#include "core/Document.h"

#include <QPainter>
#include <QSet>

#include <utility>

namespace chromarchy {

std::optional<Document> Document::create(QSize size) {
  if (size.width() <= 0 || size.height() <= 0 ||
      size.width() > maximumDimension || size.height() > maximumDimension) {
    return std::nullopt;
  }
  return Document(size);
}

Document::Document(QSize size) : size_(size), selection_(size) {
  addLayer(QStringLiteral("Layer 1"));
}

QSize Document::size() const noexcept {
  return size_;
}

qsizetype Document::layerCount() const noexcept {
  return layers_.size();
}

quint64 Document::estimatedStorageBytes() const noexcept {
  quint64 bytes = estimatedMetadataBytes();
  for (const auto& block : storageBlocks()) {
    bytes += block.bytes;
  }
  return bytes;
}

quint64 Document::estimatedMetadataBytes() const noexcept {
  quint64 bytes = sizeof(Document);
  for (const auto& layer : layers_) {
    bytes += sizeof(Layer) +
             static_cast<quint64>(layer.name().size()) * sizeof(QChar);
  }
  return bytes;
}

QVector<StorageBlock> Document::storageBlocks() const {
  QVector<StorageBlock> blocks;
  for (const auto& layer : layers_) {
    blocks += layer.pixels().storageBlocks();
  }
  blocks += selection_.storageBlocks();
  return blocks;
}

int Document::activeLayerIndex() const noexcept {
  return activeLayerIndex_;
}

const SelectionMask& Document::selection() const noexcept {
  return selection_;
}

SelectionMask& Document::selection() noexcept {
  return selection_;
}

bool Document::setActiveLayerIndex(int index) noexcept {
  if (!containsLayer(index)) {
    return false;
  }
  activeLayerIndex_ = index;
  return true;
}

const Layer* Document::layerAt(int index) const noexcept {
  return containsLayer(index) ? &layers_[index] : nullptr;
}

Layer* Document::layerAt(int index) noexcept {
  return containsLayer(index) ? &layers_[index] : nullptr;
}

int Document::addLayer(QString name) {
  layers_.emplaceBack(std::move(name), size_);
  activeLayerIndex_ = static_cast<int>(layers_.size() - 1);
  return activeLayerIndex_;
}

bool Document::duplicateLayer(int index) {
  if (!containsLayer(index)) {
    return false;
  }
  auto duplicate = layers_[index].duplicate(
      QStringLiteral("%1 copy").arg(layers_[index].name()));
  layers_.insert(index + 1, std::move(duplicate));
  activeLayerIndex_ = index + 1;
  return true;
}

bool Document::removeLayer(int index) {
  if (!containsLayer(index) || layers_.size() == 1) {
    return false;
  }
  layers_.removeAt(index);
  if (activeLayerIndex_ > index) {
    --activeLayerIndex_;
  } else if (activeLayerIndex_ == index) {
    activeLayerIndex_ = qMin(index, static_cast<int>(layers_.size() - 1));
  }
  return true;
}

bool Document::moveLayer(int from, int to) {
  if (!containsLayer(from) || !containsLayer(to) || from == to) {
    return false;
  }
  layers_.move(from, to);
  if (activeLayerIndex_ == from) {
    activeLayerIndex_ = to;
  } else if (from < activeLayerIndex_ && activeLayerIndex_ <= to) {
    --activeLayerIndex_;
  } else if (to <= activeLayerIndex_ && activeLayerIndex_ < from) {
    ++activeLayerIndex_;
  }
  return true;
}

bool Document::mergeLayerDown(int index) {
  if (!containsLayer(index) || index == 0 || layers_[index].isLocked() ||
      layers_[index - 1].isLocked()) {
    return false;
  }

  const auto& lower = layers_[index - 1];
  const auto& upper = layers_[index];
  Layer merged(upper.name(), size_);
  merged.setVisible(lower.isVisible() || upper.isVisible());

  QSet<TileIndex> tileIndices;
  if (lower.isVisible()) {
    for (auto tile = lower.pixels().tiles_.cbegin();
         tile != lower.pixels().tiles_.cend(); ++tile) {
      tileIndices.insert(tile.key());
    }
  }
  if (upper.isVisible()) {
    for (auto tile = upper.pixels().tiles_.cbegin();
         tile != upper.pixels().tiles_.cend(); ++tile) {
      tileIndices.insert(tile.key());
    }
  }

  for (const auto tileIndex : tileIndices) {
    QImage output(TiledImage::tileExtent, TiledImage::tileExtent,
                  QImage::Format_RGBA8888_Premultiplied);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    const auto lowerTile = lower.pixels().tiles_.constFind(tileIndex);
    if (lower.isVisible() && lowerTile != lower.pixels().tiles_.cend()) {
      painter.setOpacity(lower.opacity());
      painter.drawImage(QPoint(), lowerTile.value());
    }
    const auto upperTile = upper.pixels().tiles_.constFind(tileIndex);
    if (upper.isVisible() && upperTile != upper.pixels().tiles_.cend()) {
      painter.setOpacity(upper.opacity());
      painter.drawImage(QPoint(), upperTile.value());
    }
    painter.end();
    if (!TiledImage::isZeroTile(output)) {
      merged.pixels_.tiles_.insert(tileIndex, std::move(output));
    }
  }

  layers_[index - 1] = std::move(merged);
  layers_.removeAt(index);
  activeLayerIndex_ = index - 1;
  return true;
}

bool Document::flatten() {
  if (layers_.size() < 2) {
    return false;
  }
  for (const auto& layer : layers_) {
    if (layer.isLocked()) {
      return false;
    }
  }

  QSet<TileIndex> tileIndices;
  for (const auto& layer : layers_) {
    if (!layer.isVisible()) {
      continue;
    }
    for (auto tile = layer.pixels().tiles_.cbegin();
         tile != layer.pixels().tiles_.cend(); ++tile) {
      tileIndices.insert(tile.key());
    }
  }

  Layer flattened(QStringLiteral("Flattened"), size_);
  for (const auto tileIndex : tileIndices) {
    QImage output(TiledImage::tileExtent, TiledImage::tileExtent,
                  QImage::Format_RGBA8888_Premultiplied);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    for (const auto& layer : layers_) {
      if (!layer.isVisible()) {
        continue;
      }
      const auto tile = layer.pixels().tiles_.constFind(tileIndex);
      if (tile == layer.pixels().tiles_.cend()) {
        continue;
      }
      painter.setOpacity(layer.opacity());
      painter.drawImage(QPoint(), tile.value());
    }
    painter.end();
    if (!TiledImage::isZeroTile(output)) {
      flattened.pixels_.tiles_.insert(tileIndex, std::move(output));
    }
  }

  layers_.clear();
  layers_.push_back(std::move(flattened));
  activeLayerIndex_ = 0;
  return true;
}

QImage Document::composite(QRect region) const {
  const QRect bounds(QPoint(), size_);
  if (region.isNull()) {
    region = bounds;
  } else {
    region = region.intersected(bounds);
  }
  if (region.isEmpty()) {
    return {};
  }

  QImage output(region.size(), QImage::Format_RGBA8888_Premultiplied);
  output.fill(Qt::transparent);
  QPainter painter(&output);
  for (const auto& layer : layers_) {
    if (!layer.isVisible() || layer.opacity() <= 0.0) {
      continue;
    }
    painter.setOpacity(layer.opacity());
    for (const auto& tile : layer.pixels().tileSnapshots(region)) {
      painter.drawImage(tile.origin - region.topLeft(), tile.pixels);
    }
  }
  return output;
}

void Document::paintComposite(QPainter& painter, QRect region) const {
  region = region.intersected(QRect(QPoint(), size_));
  if (region.isEmpty()) {
    return;
  }

  painter.save();
  for (const auto& layer : layers_) {
    if (!layer.isVisible() || layer.opacity() <= 0.0) {
      continue;
    }
    painter.setOpacity(layer.opacity());
    for (const auto& tile : layer.pixels().tileSnapshots(region)) {
      painter.drawImage(tile.origin, tile.pixels);
    }
  }
  painter.restore();
}

bool Document::containsLayer(int index) const noexcept {
  return index >= 0 && index < layers_.size();
}

}  // namespace chromarchy
