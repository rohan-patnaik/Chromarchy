#include "core/TiledImage.h"

#include <QPainter>

#include <utility>

namespace chromarchy {

size_t qHash(const TileIndex& index, size_t seed) noexcept {
  return qHashMulti(seed, index.column, index.row);
}

TiledImage::TiledImage(QSize size) : size_(size) {}

QSize TiledImage::size() const noexcept {
  return size_;
}

bool TiledImage::isValid() const noexcept {
  return size_.width() > 0 && size_.height() > 0;
}

qsizetype TiledImage::allocatedTileCount() const noexcept {
  return tiles_.size();
}

QColor TiledImage::pixelColor(QPoint position) const {
  if (!contains(position)) {
    return Qt::transparent;
  }

  const auto index = tileIndex(position);
  const auto tile = tiles_.constFind(index);
  if (tile == tiles_.cend()) {
    return Qt::transparent;
  }
  return tile->pixelColor(position - tileOrigin(index));
}

QVector<TileSnapshot> TiledImage::tileSnapshots() const {
  QVector<TileSnapshot> snapshots;
  snapshots.reserve(tiles_.size());
  for (auto tile = tiles_.cbegin(); tile != tiles_.cend(); ++tile) {
    snapshots.push_back({tileOrigin(tile.key()), tile.value()});
  }
  return snapshots;
}

QImage TiledImage::render(QRect region) const {
  if (!isValid()) {
    return {};
  }

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
  for (auto tile = tiles_.cbegin(); tile != tiles_.cend(); ++tile) {
    painter.drawImage(tileOrigin(tile.key()) - region.topLeft(), tile.value());
  }
  return output;
}

bool TiledImage::setPixelColor(QPoint position, const QColor& color) {
  if (!contains(position) || pixelColor(position) == color) {
    return false;
  }

  const auto index = tileIndex(position);
  if (color.alpha() == 0 && !tiles_.contains(index)) {
    return false;
  }

  auto& tile = ensureTile(index);
  tile.setPixelColor(position - tileOrigin(index), color);
  dirtyRegion_ += QRect(position, QSize(1, 1));
  return true;
}

void TiledImage::clear() {
  if (tiles_.isEmpty()) {
    return;
  }
  tiles_.clear();
  dirtyRegion_ += QRect(QPoint(), size_);
}

QRegion TiledImage::dirtyRegion() const {
  return dirtyRegion_;
}

QRegion TiledImage::takeDirtyRegion() {
  return std::exchange(dirtyRegion_, QRegion{});
}

bool TiledImage::contains(QPoint position) const noexcept {
  return position.x() >= 0 && position.y() >= 0 &&
         position.x() < size_.width() && position.y() < size_.height();
}

TileIndex TiledImage::tileIndex(QPoint position) noexcept {
  return {position.x() / tileExtent, position.y() / tileExtent};
}

QPoint TiledImage::tileOrigin(TileIndex index) noexcept {
  return {index.column * tileExtent, index.row * tileExtent};
}

QImage& TiledImage::ensureTile(TileIndex index) {
  auto tile = tiles_.find(index);
  if (tile != tiles_.end()) {
    return tile.value();
  }

  QImage pixels(tileExtent, tileExtent, QImage::Format_RGBA8888_Premultiplied);
  pixels.fill(Qt::transparent);
  return tiles_.insert(index, std::move(pixels)).value();
}

}  // namespace chromarchy
