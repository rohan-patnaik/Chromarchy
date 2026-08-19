#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRegion>
#include <QSize>
#include <QVector>

namespace chromarchy {

struct TileIndex final {
  int column = 0;
  int row = 0;

  friend bool operator==(const TileIndex&, const TileIndex&) = default;
};

size_t qHash(const TileIndex& index, size_t seed = 0) noexcept;

struct TileSnapshot final {
  QPoint origin;
  QImage pixels;
};

class TiledImage final {
public:
  static constexpr int tileExtent = 256;

  explicit TiledImage(QSize size = {});

  [[nodiscard]] QSize size() const noexcept;
  [[nodiscard]] bool isValid() const noexcept;
  [[nodiscard]] qsizetype allocatedTileCount() const noexcept;
  [[nodiscard]] QColor pixelColor(QPoint position) const;
  [[nodiscard]] QVector<TileSnapshot> tileSnapshots() const;
  [[nodiscard]] QImage render(QRect region = {}) const;

  bool setPixelColor(QPoint position, const QColor& color);
  void clear();

  [[nodiscard]] QRegion dirtyRegion() const;
  QRegion takeDirtyRegion();

private:
  [[nodiscard]] bool contains(QPoint position) const noexcept;
  [[nodiscard]] static TileIndex tileIndex(QPoint position) noexcept;
  [[nodiscard]] static QPoint tileOrigin(TileIndex index) noexcept;
  QImage& ensureTile(TileIndex index);

  QSize size_;
  QHash<TileIndex, QImage> tiles_;
  QRegion dirtyRegion_;
};

}  // namespace chromarchy
