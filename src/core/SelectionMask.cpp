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
  dirtyRegion_ += QRect(position, QSize(1, 1));
  return true;
}

void SelectionMask::selectRectangle(QRect rectangle, quint8 newCoverage,
                                    bool replace) {
  const QRect bounds(QPoint(), size_);
  rectangle = rectangle.normalized().intersected(bounds);
  if (replace) {
    baseCoverage_ = 0;
    tiles_.clear();
    dirtyRegion_ += bounds;
  }
  if (rectangle.isEmpty() || newCoverage == baseCoverage_) {
    return;
  }

  const auto first = tileIndex(rectangle.topLeft());
  const auto last = tileIndex(rectangle.bottomRight());
  for (int row = first.row; row <= last.row; ++row) {
    for (int column = first.column; column <= last.column; ++column) {
      const TileIndex index{column, row};
      auto& tile = ensureTile(index);
      const auto local = rectangle.translated(-tileOrigin(index));
      QPainter painter(&tile);
      painter.setCompositionMode(QPainter::CompositionMode_Source);
      painter.fillRect(local, QColor(newCoverage, newCoverage, newCoverage));
    }
  }
  dirtyRegion_ += rectangle;
}

void SelectionMask::clear() {
  if (isEmpty()) {
    return;
  }
  baseCoverage_ = 0;
  tiles_.clear();
  dirtyRegion_ += QRect(QPoint(), size_);
}

void SelectionMask::selectAll() {
  if (baseCoverage_ == 255 && tiles_.isEmpty()) {
    return;
  }
  baseCoverage_ = 255;
  tiles_.clear();
  dirtyRegion_ += QRect(QPoint(), size_);
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
