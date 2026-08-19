#include "core/Document.h"

#include <QPainter>

#include <utility>

namespace chromarchy {

std::optional<Document> Document::create(QSize size) {
  if (size.width() <= 0 || size.height() <= 0 ||
      size.width() > maximumDimension || size.height() > maximumDimension) {
    return std::nullopt;
  }
  return Document(size);
}

Document::Document(QSize size) : size_(size) {
  addLayer(QStringLiteral("Layer 1"));
}

QSize Document::size() const noexcept {
  return size_;
}

qsizetype Document::layerCount() const noexcept {
  return layers_.size();
}

quint64 Document::estimatedStorageBytes() const noexcept {
  constexpr quint64 tileBytes =
      TiledImage::tileExtent * TiledImage::tileExtent * 4ULL;
  quint64 bytes = sizeof(Document);
  for (const auto& layer : layers_) {
    bytes += sizeof(Layer) +
             static_cast<quint64>(layer.name().size()) * sizeof(QChar) +
             static_cast<quint64>(layer.pixels().allocatedTileCount()) * tileBytes;
  }
  return bytes;
}

int Document::activeLayerIndex() const noexcept {
  return activeLayerIndex_;
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
    for (const auto& tile : layer.pixels().tileSnapshots()) {
      painter.drawImage(tile.origin - region.topLeft(), tile.pixels);
    }
  }
  return output;
}

bool Document::containsLayer(int index) const noexcept {
  return index >= 0 && index < layers_.size();
}

}  // namespace chromarchy
