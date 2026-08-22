#pragma once

#include "core/PixelStorage.h"
#include "core/StorageBlock.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRegion>
#include <QSize>
#include <QVector>

namespace chromarchy {

class NativeDocumentCodec;
class Document;

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
  [[nodiscard]] static TiledImage fromImage(const QImage& image);

  [[nodiscard]] QSize size() const noexcept;
  [[nodiscard]] PixelFormat pixelFormat() const noexcept;
  [[nodiscard]] static std::optional<PixelStorageLayout> tileStorageLayout(
      quint64 maximumAllocationBytes =
          std::numeric_limits<quint64>::max()) noexcept;
  [[nodiscard]] bool isValid() const noexcept;
  [[nodiscard]] qsizetype allocatedTileCount() const noexcept;
  [[nodiscard]] QColor pixelColor(QPoint position) const;
  [[nodiscard]] QVector<TileSnapshot> tileSnapshots() const;
  [[nodiscard]] QVector<TileSnapshot> tileSnapshots(QRect region) const;
  [[nodiscard]] QVector<StorageBlock> storageBlocks() const;
  [[nodiscard]] QImage render(QRect region = {}) const;

  bool setPixelColor(QPoint position, const QColor& color);
  void clear();

  [[nodiscard]] QRegion dirtyRegion() const;
  QRegion takeDirtyRegion();

private:
  friend class Document;
  friend class NativeDocumentCodec;

  [[nodiscard]] bool contains(QPoint position) const noexcept;
  [[nodiscard]] static TileIndex tileIndex(QPoint position) noexcept;
  [[nodiscard]] static QPoint tileOrigin(TileIndex index) noexcept;
  [[nodiscard]] static bool isZeroTile(const QImage& tile) noexcept;
  QImage& ensureTile(TileIndex index);

  QSize size_;
  QHash<TileIndex, QImage> tiles_;
  QRegion dirtyRegion_;
};

}  // namespace chromarchy
