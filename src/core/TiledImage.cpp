#include "core/TiledImage.h"

#include <QPainter>

#include <algorithm>
#include <utility>

namespace chromarchy {

size_t qHash(const TileIndex& index, size_t seed) noexcept {
  return qHashMulti(seed, index.column, index.row);
}

TiledImage::TiledImage(QSize size) : size_(size) {}

TiledImage TiledImage::fromImage(const QImage& image) {
  if (image.isNull()) {
    return {};
  }

  const auto source = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
  TiledImage tiled(source.size());
  const int columns = (source.width() + tileExtent - 1) / tileExtent;
  const int rows = (source.height() + tileExtent - 1) / tileExtent;

  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const TileIndex index{column, row};
      const QRect sourceRect(tileOrigin(index), QSize(tileExtent, tileExtent));
      const auto clipped = sourceRect.intersected(source.rect());
      const auto fragment = source.copy(clipped);

      bool hasVisiblePixel = !fragment.hasAlphaChannel();
      for (int y = 0; !hasVisiblePixel && y < fragment.height(); ++y) {
        const auto* scanline = fragment.constScanLine(y);
        for (int x = 0; x < fragment.width(); ++x) {
          if (scanline[x * 4 + 3] != 0) {
            hasVisiblePixel = true;
            break;
          }
        }
      }
      if (!hasVisiblePixel) {
        continue;
      }

      auto& tile = tiled.ensureTile(index);
      QPainter painter(&tile);
      painter.setCompositionMode(QPainter::CompositionMode_Source);
      painter.drawImage(QPoint(), fragment);
    }
  }
  tiled.dirtyRegion_ = QRegion(QRect(QPoint(), tiled.size_));
  return tiled;
}

QSize TiledImage::size() const noexcept {
  return size_;
}

PixelFormat TiledImage::pixelFormat() const noexcept {
  return PixelFormat::rgba8Premultiplied();
}

std::optional<PixelStorageLayout> TiledImage::tileStorageLayout(
    quint64 maximumAllocationBytes) noexcept {
  return PixelStorageLayout::create(QSize(tileExtent, tileExtent),
                                    PixelFormat::rgba8Premultiplied(), 1,
                                    maximumAllocationBytes);
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

QVector<TileSnapshot> TiledImage::tileSnapshots(QRect region) const {
  QVector<TileSnapshot> snapshots;
  region = region.intersected(QRect(QPoint(), size_));
  if (region.isEmpty()) {
    return snapshots;
  }

  const auto first = tileIndex(region.topLeft());
  const auto last = tileIndex(region.bottomRight());
  snapshots.reserve(qMin<qsizetype>(
      tiles_.size(), static_cast<qsizetype>(last.column - first.column + 1) *
                         (last.row - first.row + 1)));
  for (int row = first.row; row <= last.row; ++row) {
    for (int column = first.column; column <= last.column; ++column) {
      const TileIndex index{column, row};
      const auto tile = tiles_.constFind(index);
      if (tile != tiles_.cend()) {
        snapshots.push_back({tileOrigin(index), tile.value()});
      }
    }
  }
  return snapshots;
}

QVector<StorageBlock> TiledImage::storageBlocks() const {
  QVector<StorageBlock> blocks;
  blocks.reserve(tiles_.size());
  for (const auto& tile : tiles_) {
    blocks.push_back({static_cast<quint64>(tile.cacheKey()),
                      static_cast<quint64>(tile.sizeInBytes())});
  }
  return blocks;
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
  if (color.alpha() == 0 && isZeroTile(tile)) {
    tiles_.remove(index);
  }
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

bool TiledImage::isZeroTile(const QImage& tile) noexcept {
  const auto* begin = tile.constBits();
  return begin != nullptr &&
         std::all_of(begin, begin + tile.sizeInBytes(),
                     [](uchar byte) { return byte == 0; });
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
