#pragma once

#include "core/TiledImage.h"

#include <QHash>
#include <QImage>
#include <QRegion>
#include <QSize>

namespace chromarchy {

class SelectionMask final {
public:
  explicit SelectionMask(QSize size = {});

  [[nodiscard]] QSize size() const noexcept;
  [[nodiscard]] quint8 coverage(QPoint position) const noexcept;
  [[nodiscard]] qsizetype allocatedTileCount() const noexcept;
  [[nodiscard]] bool isEmpty() const noexcept;

  bool setCoverage(QPoint position, quint8 coverage);
  void selectRectangle(QRect rectangle, quint8 coverage = 255,
                       bool replace = true);
  void clear();
  void selectAll();
  void invert();

  [[nodiscard]] QRegion dirtyRegion() const;
  QRegion takeDirtyRegion();

private:
  [[nodiscard]] bool contains(QPoint position) const noexcept;
  [[nodiscard]] static TileIndex tileIndex(QPoint position) noexcept;
  [[nodiscard]] static QPoint tileOrigin(TileIndex index) noexcept;
  QImage& ensureTile(TileIndex index);

  QSize size_;
  quint8 baseCoverage_ = 0;
  QHash<TileIndex, QImage> tiles_;
  QRegion dirtyRegion_;
};

}  // namespace chromarchy
