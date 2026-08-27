#include "core/SelectionMask.h"

#include <QPainter>

#include <utility>

namespace chromarchy {

SelectionMask::SelectionMask(QSize size) : size_(size) {}

QSize SelectionMask::size() const noexcept {
  return size_;
}

quint8 SelectionMask::coverage(QPoint position) const noexcept {
  if (!contains(position)) {
    return 0;
  }
  const auto index = tileIndex(position);
  const auto tile = tiles_.constFind(index);
  if (tile == tiles_.cend()) {
    return baseCoverage_;
  }
  return tile->constScanLine(position.y() % TiledImage::tileExtent)
             [position.x() % TiledImage::tileExtent];
}

qsizetype SelectionMask::allocatedTileCount() const noexcept {
  return tiles_.size();
}

bool SelectionMask::isEmpty() const noexcept {
  if (baseCoverage_ != 0) {
    return false;
  }
  for (const auto& tile : tiles_) {
    for (int y = 0; y < tile.height(); ++y) {
      const auto* scanline = tile.constScanLine(y);
      for (int x = 0; x < tile.width(); ++x) {
        if (scanline[x] != 0) {
          return false;
        }
      }
    }
  }
  return true;
}

quint8 SelectionMask::baseCoverage() const noexcept {
  return baseCoverage_;
}

QVector<TileSnapshot> SelectionMask::tileSnapshots(QRect region) const {
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

QVector<StorageBlock> SelectionMask::storageBlocks() const {
  QVector<StorageBlock> blocks;
  blocks.reserve(tiles_.size());
  for (const auto& tile : tiles_) {
    blocks.push_back({static_cast<quint64>(tile.cacheKey()),
                      static_cast<quint64>(tile.sizeInBytes())});
  }
  return blocks;
}

QImage SelectionMask::render(QRect region) const {
  const QRect bounds(QPoint(), size_);
  if (region.isNull()) {
    region = bounds;
  } else {
    region = region.intersected(bounds);
  }
  if (region.isEmpty()) {
    return {};
  }

  QImage output(region.size(), QImage::Format_Grayscale8);
  output.fill(baseCoverage_);
  QPainter painter(&output);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  for (auto tile = tiles_.cbegin(); tile != tiles_.cend(); ++tile) {
    painter.drawImage(tileOrigin(tile.key()) - region.topLeft(), tile.value());
  }
  return output;
}

bool SelectionMask::setCoverage(QPoint position, quint8 newCoverage) {
  if (!contains(position) || coverage(position) == newCoverage) {
    return false;
  }
  const auto index = tileIndex(position);
  if (newCoverage == baseCoverage_ && !tiles_.contains(index)) {
    return false;
  }
  auto& tile = ensureTile(index);
  tile.scanLine(position.y() % TiledImage::tileExtent)
      [position.x() % TiledImage::tileExtent] = newCoverage;
  if (newCoverage == baseCoverage_ && isBaseTile(tile, baseCoverage_)) {
    tiles_.remove(index);
  }
  dirtyRegion_ += QRect(position, QSize(1, 1));
  return true;
}

bool SelectionMask::selectRectangle(QRect rectangle, quint8 newCoverage,
                                    bool replace) {
  const QRect bounds(QPoint(), size_);
  rectangle = rectangle.normalized().intersected(bounds);
  if (replace) {
    SelectionMask replacement(size_);
    replacement.selectRectangle(rectangle, newCoverage, false);
    if (baseCoverage_ == replacement.baseCoverage_ &&
        tiles_ == replacement.tiles_) {
      return false;
    }
    baseCoverage_ = replacement.baseCoverage_;
    tiles_ = std::move(replacement.tiles_);
    dirtyRegion_ += bounds;
    return true;
  }
  if (rectangle.isEmpty()) {
    return false;
  }

  bool changed = false;
  const auto first = tileIndex(rectangle.topLeft());
  const auto last = tileIndex(rectangle.bottomRight());
  for (int row = first.row; row <= last.row; ++row) {
    for (int column = first.column; column <= last.column; ++column) {
      const TileIndex index{column, row};
      if (!tiles_.contains(index) && newCoverage == baseCoverage_) {
        continue;
      }
      auto& tile = ensureTile(index);
      const auto before = tile;
      const auto local = rectangle.translated(-tileOrigin(index));
      {
        QPainter painter(&tile);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(local, QColor(newCoverage, newCoverage, newCoverage));
      }
      changed = changed || tile != before;
      if (newCoverage == baseCoverage_ && isBaseTile(tile, baseCoverage_)) {
        tiles_.remove(index);
      }
    }
  }
  if (changed) {
    dirtyRegion_ += rectangle;
  }
  return changed;
}

bool SelectionMask::clear() {
  if (isEmpty()) {
    return false;
  }
  baseCoverage_ = 0;
  tiles_.clear();
  dirtyRegion_ += QRect(QPoint(), size_);
  return true;
}

bool SelectionMask::selectAll() {
  if (baseCoverage_ == 255 && tiles_.isEmpty()) {
    return false;
  }
  baseCoverage_ = 255;
  tiles_.clear();
  dirtyRegion_ += QRect(QPoint(), size_);
  return true;
}

void SelectionMask::invert() {
  baseCoverage_ = 255 - baseCoverage_;
  for (auto& tile : tiles_) {
    tile.detach();
    for (int y = 0; y < tile.height(); ++y) {
      auto* scanline = tile.scanLine(y);
      for (int x = 0; x < tile.width(); ++x) {
        scanline[x] = 255 - scanline[x];
      }
    }
  }
  dirtyRegion_ += QRect(QPoint(), size_);
}

QRegion SelectionMask::dirtyRegion() const {
  return dirtyRegion_;
}

QRegion SelectionMask::takeDirtyRegion() {
  return std::exchange(dirtyRegion_, QRegion{});
}

bool SelectionMask::contains(QPoint position) const noexcept {
  return position.x() >= 0 && position.y() >= 0 &&
         position.x() < size_.width() && position.y() < size_.height();
}

TileIndex SelectionMask::tileIndex(QPoint position) noexcept {
  return {position.x() / TiledImage::tileExtent,
          position.y() / TiledImage::tileExtent};
}

QPoint SelectionMask::tileOrigin(TileIndex index) noexcept {
  return {index.column * TiledImage::tileExtent,
          index.row * TiledImage::tileExtent};
}

bool SelectionMask::isBaseTile(const QImage& tile,
                               quint8 baseCoverage) noexcept {
  if (tile.isNull() || tile.format() != QImage::Format_Grayscale8) {
    return false;
  }
  for (int y = 0; y < tile.height(); ++y) {
    const auto* scanline = tile.constScanLine(y);
    for (int x = 0; x < tile.width(); ++x) {
      if (scanline[x] != baseCoverage) {
        return false;
      }
    }
  }
  return true;
}

QImage& SelectionMask::ensureTile(TileIndex index) {
  auto tile = tiles_.find(index);
  if (tile != tiles_.end()) {
    return tile.value();
  }
  QImage coverage(TiledImage::tileExtent, TiledImage::tileExtent,
                  QImage::Format_Grayscale8);
  coverage.fill(baseCoverage_);
  return tiles_.insert(index, std::move(coverage)).value();
}

}  // namespace chromarchy
