#include "core/PixelStorage.h"

#include <QSet>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace chromarchy {
namespace {

bool checkedMultiply(quint64 left, quint64 right, quint64& result) noexcept {
  if (left != 0 && right > std::numeric_limits<quint64>::max() / left) {
    return false;
  }
  result = left * right;
  return true;
}

bool checkedAlignUp(quint64 value, quint64 alignment, quint64& result) noexcept {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return false;
  }
  const quint64 remainder = value & (alignment - 1);
  if (remainder == 0) {
    result = value;
    return true;
  }
  const quint64 padding = alignment - remainder;
  if (value > std::numeric_limits<quint64>::max() - padding) {
    return false;
  }
  result = value + padding;
  return true;
}

bool isSupportedRgba8(PixelFormat format) noexcept {
  return format.sample == SampleFormat::UnsignedInteger8 &&
         format.channels == ChannelLayout::RGBA &&
         format.byteOrder == ByteOrder::NotApplicable &&
         (format.alpha == AlphaMode::Straight ||
          format.alpha == AlphaMode::Premultiplied);
}

bool isSupportedTileFormat(PixelFormat format) noexcept {
  return format.isValid() &&
         (format.alpha != AlphaMode::Premultiplied ||
          format == PixelFormat::rgba8Premultiplied());
}

std::optional<PixelStorageLayout> tileLayout(
    PixelFormat format, quint64 allocationLimitBytes) noexcept {
  if (!isSupportedTileFormat(format)) {
    return std::nullopt;
  }
  return PixelStorageLayout::create(
      QSize(PixelTile::extent, PixelTile::extent), format, 1,
      std::min(allocationLimitBytes, PixelTile::maximumAllocationBytes));
}

bool isValidPremultipliedRgba8Pixel(
    std::span<const std::byte> pixel) noexcept {
  if (pixel.size() != 4) {
    return false;
  }
  const auto* bytes = reinterpret_cast<const quint8*>(pixel.data());
  return bytes[0] <= bytes[3] && bytes[1] <= bytes[3] &&
         bytes[2] <= bytes[3];
}

struct RegionTileBounds final {
  quint64 firstColumn = 0;
  quint64 lastColumn = 0;
  quint64 firstRow = 0;
  quint64 lastRow = 0;
  quint64 tileCount = 0;
};

std::optional<RegionTileBounds> regionTileBounds(QSize dimensions,
                                                  QRect region) noexcept {
  if (region.x() < 0 || region.y() < 0 || region.width() <= 0 ||
      region.height() <= 0) {
    return std::nullopt;
  }
  const quint64 right = static_cast<quint64>(region.x()) +
                        static_cast<quint64>(region.width());
  const quint64 bottom = static_cast<quint64>(region.y()) +
                         static_cast<quint64>(region.height());
  if (right > static_cast<quint64>(dimensions.width()) ||
      bottom > static_cast<quint64>(dimensions.height())) {
    return std::nullopt;
  }

  RegionTileBounds bounds{
      .firstColumn = static_cast<quint64>(region.x()) / PixelTile::extent,
      .lastColumn = (right - 1U) / PixelTile::extent,
      .firstRow = static_cast<quint64>(region.y()) / PixelTile::extent,
      .lastRow = (bottom - 1U) / PixelTile::extent,
  };
  const quint64 columns = bounds.lastColumn - bounds.firstColumn + 1U;
  const quint64 rows = bounds.lastRow - bounds.firstRow + 1U;
  if (!checkedMultiply(columns, rows, bounds.tileCount)) {
    return std::nullopt;
  }
  return bounds;
}

quint8 premultiply(quint8 color, quint8 alpha) noexcept {
  return static_cast<quint8>((static_cast<quint32>(color) * alpha + 127U) /
                             255U);
}

quint8 unpremultiply(quint8 color, quint8 alpha) noexcept {
  if (alpha == 0) {
    return 0;
  }
  return static_cast<quint8>(
      (static_cast<quint32>(color) * 255U + alpha / 2U) / alpha);
}

bool hasValidPremultipliedSamples(
    std::span<const std::byte> source,
    const PixelStorageLayout& sourceLayout) noexcept {
  if (sourceLayout.format().alpha != AlphaMode::Premultiplied) {
    return true;
  }

  const auto* sourceBytes = reinterpret_cast<const quint8*>(source.data());
  const quint64 width = static_cast<quint64>(sourceLayout.dimensions().width());
  const quint64 height =
      static_cast<quint64>(sourceLayout.dimensions().height());
  for (quint64 y = 0; y < height; ++y) {
    const quint64 rowOffset = y * sourceLayout.rowStrideBytes();
    for (quint64 x = 0; x < width; ++x) {
      const quint64 pixelOffset = rowOffset + x * 4U;
      const auto* pixel = sourceBytes + pixelOffset;
      const quint8 alpha = pixel[3];
      if (pixel[0] > alpha || pixel[1] > alpha || pixel[2] > alpha) {
        return false;
      }
    }
  }
  return true;
}

void convertRows(std::span<const std::byte> source,
                 const PixelStorageLayout& sourceLayout,
                 AlphaMode destinationAlpha, quint8* destinationBytes,
                 quint64 destinationRowStride) noexcept {
  const auto* sourceBytes =
      reinterpret_cast<const quint8*>(source.data());
  const quint64 width = static_cast<quint64>(sourceLayout.dimensions().width());
  const quint64 height =
      static_cast<quint64>(sourceLayout.dimensions().height());
  for (quint64 y = 0; y < height; ++y) {
    const quint64 sourceRowOffset = y * sourceLayout.rowStrideBytes();
    const quint64 destinationRowOffset = y * destinationRowStride;
    for (quint64 x = 0; x < width; ++x) {
      const quint64 pixelOffset = x * 4U;
      const auto* sourcePixel = sourceBytes + sourceRowOffset + pixelOffset;
      auto* destinationPixel =
          destinationBytes + destinationRowOffset + pixelOffset;
      const quint8 alpha = sourcePixel[3];
      for (int channel = 0; channel < 3; ++channel) {
        if (sourceLayout.format().alpha == destinationAlpha) {
          destinationPixel[channel] = sourcePixel[channel];
        } else if (destinationAlpha == AlphaMode::Premultiplied) {
          destinationPixel[channel] = premultiply(sourcePixel[channel], alpha);
        } else {
          destinationPixel[channel] = unpremultiply(sourcePixel[channel], alpha);
        }
      }
      destinationPixel[3] = alpha;
    }
  }
}

}  // namespace

std::optional<PixelStorageLayout> PixelStorageLayout::create(
    QSize dimensions, PixelFormat format, quint64 rowAlignment,
    quint64 maximumAllocationBytes) noexcept {
  if (dimensions.width() <= 0 || dimensions.height() <= 0 || !format.isValid()) {
    return std::nullopt;
  }

  quint64 packedRowBytes = 0;
  quint64 rowStrideBytes = 0;
  quint64 allocationBytes = 0;
  if (!checkedMultiply(static_cast<quint64>(dimensions.width()),
                       format.bytesPerPixel(), packedRowBytes) ||
      !checkedAlignUp(packedRowBytes, rowAlignment, rowStrideBytes) ||
      !checkedMultiply(rowStrideBytes,
                       static_cast<quint64>(dimensions.height()),
                       allocationBytes) ||
      allocationBytes > maximumAllocationBytes ||
      allocationBytes > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
    return std::nullopt;
  }

  return PixelStorageLayout(dimensions, format, packedRowBytes, rowStrideBytes,
                            allocationBytes);
}

std::optional<PixelStorageLayout> PixelStorageLayout::createWithRowStride(
    QSize dimensions, PixelFormat format, quint64 rowStrideBytes,
    quint64 maximumAllocationBytes) noexcept {
  if (dimensions.width() <= 0 || dimensions.height() <= 0 || !format.isValid()) {
    return std::nullopt;
  }

  quint64 packedRowBytes = 0;
  quint64 allocationBytes = 0;
  if (!checkedMultiply(static_cast<quint64>(dimensions.width()),
                       format.bytesPerPixel(), packedRowBytes) ||
      rowStrideBytes < packedRowBytes ||
      !checkedMultiply(rowStrideBytes,
                       static_cast<quint64>(dimensions.height()),
                       allocationBytes) ||
      allocationBytes > maximumAllocationBytes ||
      allocationBytes >
          static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
    return std::nullopt;
  }

  return PixelStorageLayout(dimensions, format, packedRowBytes, rowStrideBytes,
                            allocationBytes);
}

PixelStorageLayout::PixelStorageLayout(QSize dimensions, PixelFormat format,
                                       quint64 packedRowBytes,
                                       quint64 rowStrideBytes,
                                       quint64 allocationBytes) noexcept
    : dimensions_(dimensions),
      format_(format),
      packedRowBytes_(packedRowBytes),
      rowStrideBytes_(rowStrideBytes),
      allocationBytes_(allocationBytes) {}

QSize PixelStorageLayout::dimensions() const noexcept {
  return dimensions_;
}

PixelFormat PixelStorageLayout::format() const noexcept {
  return format_;
}

quint64 PixelStorageLayout::packedRowBytes() const noexcept {
  return packedRowBytes_;
}

quint64 PixelStorageLayout::rowStrideBytes() const noexcept {
  return rowStrideBytes_;
}

quint64 PixelStorageLayout::allocationBytes() const noexcept {
  return allocationBytes_;
}

std::optional<PixelTile> PixelTile::create(PixelFormat format,
                                           quint64 allocationLimitBytes) {
  const auto layout = tileLayout(format, allocationLimitBytes);
  if (!layout) {
    return std::nullopt;
  }
  QByteArray bytes(static_cast<qsizetype>(layout->allocationBytes()), '\0');
  if (bytes.size() != static_cast<qsizetype>(layout->allocationBytes())) {
    return std::nullopt;
  }
  return PixelTile(*layout, std::move(bytes));
}

std::optional<PixelTile> PixelTile::fromPackedBytes(
    PixelFormat format, std::span<const std::byte> source,
    quint64 allocationLimitBytes) {
  const auto layout = tileLayout(format, allocationLimitBytes);
  if (!layout || source.size() != layout->allocationBytes()) {
    return std::nullopt;
  }
  if (format == PixelFormat::rgba8Premultiplied() &&
      !hasValidPremultipliedSamples(source, *layout)) {
    return std::nullopt;
  }
  QByteArray bytes(reinterpret_cast<const char*>(source.data()),
                   static_cast<qsizetype>(source.size()));
  if (bytes.size() != static_cast<qsizetype>(source.size())) {
    return std::nullopt;
  }
  return PixelTile(*layout, std::move(bytes));
}

PixelTile::PixelTile(PixelStorageLayout layout, QByteArray bytes)
    : layout_(std::move(layout)), bytes_(std::move(bytes)) {}

PixelFormat PixelTile::format() const noexcept {
  return layout_.format();
}

const PixelStorageLayout& PixelTile::layout() const noexcept {
  return layout_;
}

std::span<const std::byte> PixelTile::packedBytes() const noexcept {
  return {reinterpret_cast<const std::byte*>(bytes_.constData()),
          static_cast<std::size_t>(bytes_.size())};
}

std::span<const std::byte> PixelTile::pixelBytes(QPoint position) const noexcept {
  if (position.x() < 0 || position.y() < 0 || position.x() >= extent ||
      position.y() >= extent) {
    return {};
  }
  const quint64 bytesPerPixel = layout_.format().bytesPerPixel();
  const quint64 offset =
      (static_cast<quint64>(position.y()) * extent +
       static_cast<quint64>(position.x())) *
      bytesPerPixel;
  return {reinterpret_cast<const std::byte*>(bytes_.constData()) + offset,
          static_cast<std::size_t>(bytesPerPixel)};
}

bool PixelTile::setPixelBytes(QPoint position,
                              std::span<const std::byte> source) {
  const auto existing = pixelBytes(position);
  if (existing.empty() || source.size() != existing.size() ||
      (format() == PixelFormat::rgba8Premultiplied() &&
       !isValidPremultipliedRgba8Pixel(source)) ||
      std::equal(existing.begin(), existing.end(), source.begin())) {
    return false;
  }
  const auto offset =
      static_cast<qsizetype>(existing.data() - packedBytes().data());
  std::array<std::byte, 16> stableSource{};
  if (source.size() > stableSource.size()) {
    return false;
  }
  std::copy(source.begin(), source.end(), stableSource.begin());
  std::memcpy(bytes_.data() + offset, stableSource.data(), source.size());
  return true;
}

bool PixelTile::isZero() const noexcept {
  return std::all_of(bytes_.cbegin(), bytes_.cend(),
                     [](char byte) { return byte == 0; });
}

std::optional<Rgba8Buffer> PixelTile::toRgba8Premultiplied(
    quint64 allocationLimitBytes) const {
  if (!isSupportedRgba8(format())) {
    return std::nullopt;
  }
  return convertRgba8(packedBytes(), layout_, AlphaMode::Premultiplied, 1,
                      allocationLimitBytes);
}

size_t qHash(const PixelTileIndex& index, size_t seed) noexcept {
  return qHashMulti(seed, index.column, index.row);
}

std::optional<SparsePixelTileStore> SparsePixelTileStore::create(
    QSize dimensions, PixelFormat format, quint64 maximumResidentBytes,
    quint64 maximumResidentTiles) {
  const auto layout = tileLayout(format, PixelTile::maximumAllocationBytes);
  if (dimensions.width() <= 0 || dimensions.height() <= 0 || !layout ||
      maximumResidentBytes == 0 ||
      maximumResidentBytes > hardMaximumResidentBytes ||
      maximumResidentTiles == 0 ||
      maximumResidentTiles > hardMaximumResidentTiles ||
      layout->allocationBytes() > maximumResidentBytes) {
    return std::nullopt;
  }
  return SparsePixelTileStore(dimensions, format, layout->allocationBytes(),
                              maximumResidentBytes, maximumResidentTiles);
}

SparsePixelTileStore::SparsePixelTileStore(
    QSize dimensions, PixelFormat format, quint64 tileAllocationBytes,
    quint64 maximumResidentBytes, quint64 maximumResidentTiles) noexcept
    : dimensions_(dimensions),
      format_(format),
      tileAllocationBytes_(tileAllocationBytes),
      maximumResidentBytes_(maximumResidentBytes),
      maximumResidentTiles_(maximumResidentTiles) {}

QSize SparsePixelTileStore::dimensions() const noexcept {
  return dimensions_;
}

PixelFormat SparsePixelTileStore::format() const noexcept {
  return format_;
}

quint64 SparsePixelTileStore::maximumResidentBytes() const noexcept {
  return maximumResidentBytes_;
}

quint64 SparsePixelTileStore::maximumResidentTiles() const noexcept {
  return maximumResidentTiles_;
}

quint64 SparsePixelTileStore::residentDecodedBytes() const noexcept {
  return residentDecodedBytes_;
}

qsizetype SparsePixelTileStore::allocatedTileCount() const noexcept {
  return tiles_.size();
}

bool SparsePixelTileStore::containsTileIndex(
    PixelTileIndex index) const noexcept {
  const quint64 columns =
      (static_cast<quint64>(dimensions_.width()) + PixelTile::extent - 1U) /
      PixelTile::extent;
  const quint64 rows =
      (static_cast<quint64>(dimensions_.height()) + PixelTile::extent - 1U) /
      PixelTile::extent;
  return index.column < columns && index.row < rows;
}

std::optional<QByteArray> SparsePixelTileStore::pixelBytes(
    QPoint position) const {
  if (!contains(position)) {
    return std::nullopt;
  }
  const auto tile = tiles_.constFind(tileIndex(position));
  if (tile == tiles_.cend()) {
    return QByteArray(format_.bytesPerPixel(), '\0');
  }
  const auto pixel = tile->pixelBytes(tilePosition(position));
  return QByteArray(reinterpret_cast<const char*>(pixel.data()),
                    static_cast<qsizetype>(pixel.size()));
}

std::optional<std::span<const std::byte>>
SparsePixelTileStore::packedTileBytes(PixelTileIndex index) const noexcept {
  if (!containsTileIndex(index)) {
    return std::nullopt;
  }
  const auto tile = tiles_.constFind(index);
  if (tile == tiles_.cend()) {
    return std::nullopt;
  }
  return tile->packedBytes();
}

QVector<PixelTileSnapshot> SparsePixelTileStore::tileSnapshots() const {
  QVector<PixelTileSnapshot> snapshots;
  snapshots.reserve(tiles_.size());
  for (auto tile = tiles_.cbegin(); tile != tiles_.cend(); ++tile) {
    const auto bytes = tile->packedBytes();
    snapshots.push_back(
        {tile.key(),
         QByteArray(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<qsizetype>(bytes.size()))});
  }
  std::sort(snapshots.begin(), snapshots.end(),
            [](const PixelTileSnapshot& left,
               const PixelTileSnapshot& right) {
              return left.index.row != right.index.row
                         ? left.index.row < right.index.row
                         : left.index.column < right.index.column;
            });
  return snapshots;
}

std::optional<SparsePixelTileStore>
SparsePixelTileStore::fromTileSnapshots(
    QSize dimensions, PixelFormat format,
    const QVector<PixelTileSnapshot>& snapshots, quint64 maximumResidentBytes,
    quint64 maximumResidentTiles) {
  auto store = create(dimensions, format, maximumResidentBytes,
                      maximumResidentTiles);
  if (!store) {
    return std::nullopt;
  }
  const auto count = static_cast<quint64>(snapshots.size());
  if (count > store->maximumResidentTiles_ ||
      count > store->maximumResidentBytes_ / store->tileAllocationBytes_) {
    return std::nullopt;
  }

  QSet<PixelTileIndex> seen;
  seen.reserve(snapshots.size());
  for (const auto& snapshot : snapshots) {
    if (!store->containsTileIndex(snapshot.index) ||
        seen.contains(snapshot.index)) {
      return std::nullopt;
    }
    auto tile = PixelTile::fromPackedBytes(
        format,
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(snapshot.packedBytes.constData()),
            static_cast<std::size_t>(snapshot.packedBytes.size())),
        store->tileAllocationBytes_);
    if (!tile || tile->isZero()) {
      return std::nullopt;
    }
    seen.insert(snapshot.index);
    store->tiles_.insert(snapshot.index, std::move(*tile));
  }
  store->residentDecodedBytes_ = count * store->tileAllocationBytes_;
  return store;
}

std::optional<PixelRegionBuffer> SparsePixelTileStore::readRegion(
    QRect region, quint64 rowAlignment,
    quint64 maximumAllocationBytes) const {
  const auto bounds = regionTileBounds(dimensions_, region);
  if (!bounds || bounds->tileCount > hardMaximumRegionTiles ||
      maximumAllocationBytes == 0 ||
      maximumAllocationBytes > hardMaximumRegionBytes) {
    return std::nullopt;
  }
  const auto layout = PixelStorageLayout::create(
      region.size(), format_, rowAlignment, maximumAllocationBytes);
  if (!layout) {
    return std::nullopt;
  }

  QByteArray output(static_cast<qsizetype>(layout->allocationBytes()), '\0');
  if (output.size() != static_cast<qsizetype>(layout->allocationBytes())) {
    return std::nullopt;
  }
  auto* destination = reinterpret_cast<std::byte*>(output.data());
  const quint64 bytesPerPixel = format_.bytesPerPixel();
  const quint64 regionRight = static_cast<quint64>(region.x()) +
                              static_cast<quint64>(region.width());
  const quint64 regionBottom = static_cast<quint64>(region.y()) +
                               static_cast<quint64>(region.height());

  for (quint64 tileRow = bounds->firstRow; tileRow <= bounds->lastRow;
       ++tileRow) {
    for (quint64 tileColumn = bounds->firstColumn;
         tileColumn <= bounds->lastColumn; ++tileColumn) {
      const PixelTileIndex index{static_cast<quint32>(tileColumn),
                                 static_cast<quint32>(tileRow)};
      const auto tile = tiles_.constFind(index);
      if (tile == tiles_.cend()) {
        continue;
      }
      const quint64 tileX = tileColumn * PixelTile::extent;
      const quint64 tileY = tileRow * PixelTile::extent;
      const quint64 copyLeft =
          std::max(tileX, static_cast<quint64>(region.x()));
      const quint64 copyRight =
          std::min(tileX + PixelTile::extent, regionRight);
      const quint64 copyTop =
          std::max(tileY, static_cast<quint64>(region.y()));
      const quint64 copyBottom =
          std::min(tileY + PixelTile::extent, regionBottom);
      const quint64 copyBytes = (copyRight - copyLeft) * bytesPerPixel;
      const auto packed = tile->packedBytes();
      for (quint64 y = copyTop; y < copyBottom; ++y) {
        const quint64 sourceOffset =
            ((y - tileY) * PixelTile::extent + copyLeft - tileX) *
            bytesPerPixel;
        const quint64 destinationOffset =
            (y - static_cast<quint64>(region.y())) *
                layout->rowStrideBytes() +
            (copyLeft - static_cast<quint64>(region.x())) * bytesPerPixel;
        std::memcpy(destination + destinationOffset,
                    packed.data() + sourceOffset, copyBytes);
      }
    }
  }
  return PixelRegionBuffer{*layout, std::move(output)};
}

PixelTileWriteResult SparsePixelTileStore::writeRegion(
    QRect region, std::span<const std::byte> source,
    quint64 sourceRowStrideBytes, quint64 maximumAllocationBytes) {
  const auto bounds = regionTileBounds(dimensions_, region);
  if (!bounds || bounds->tileCount > hardMaximumRegionTiles ||
      maximumAllocationBytes == 0 ||
      maximumAllocationBytes > hardMaximumRegionBytes ||
      bounds->tileCount > maximumAllocationBytes / tileAllocationBytes_) {
    return PixelTileWriteResult::Rejected;
  }
  const auto sourceLayout = PixelStorageLayout::createWithRowStride(
      region.size(), format_, sourceRowStrideBytes, maximumAllocationBytes);
  const auto fullTileLayout = tileLayout(format_, tileAllocationBytes_);
  if (!sourceLayout || !fullTileLayout ||
      source.size() != sourceLayout->allocationBytes()) {
    return PixelTileWriteResult::Rejected;
  }

  QByteArray staged(reinterpret_cast<const char*>(source.data()),
                    static_cast<qsizetype>(source.size()));
  if (staged.size() != static_cast<qsizetype>(source.size())) {
    return PixelTileWriteResult::Rejected;
  }

  struct PendingTile final {
    PixelTileIndex index;
    bool remove = false;
    bool add = false;
    std::optional<PixelTile> replacement;
  };
  QVector<PendingTile> pending;
  pending.reserve(static_cast<qsizetype>(bounds->tileCount));
  quint64 additions = 0;
  quint64 removals = 0;
  const quint64 bytesPerPixel = format_.bytesPerPixel();
  const quint64 regionRight = static_cast<quint64>(region.x()) +
                              static_cast<quint64>(region.width());
  const quint64 regionBottom = static_cast<quint64>(region.y()) +
                               static_cast<quint64>(region.height());
  const auto* stagedBytes =
      reinterpret_cast<const std::byte*>(staged.constData());

  for (quint64 tileRow = bounds->firstRow; tileRow <= bounds->lastRow;
       ++tileRow) {
    for (quint64 tileColumn = bounds->firstColumn;
         tileColumn <= bounds->lastColumn; ++tileColumn) {
      const PixelTileIndex index{static_cast<quint32>(tileColumn),
                                 static_cast<quint32>(tileRow)};
      const auto existing = tiles_.constFind(index);
      const bool existed = existing != tiles_.cend();
      QByteArray packed(
          existed ? reinterpret_cast<const char*>(existing->packedBytes().data())
                  : nullptr,
          existed ? static_cast<qsizetype>(tileAllocationBytes_) : 0);
      if (!existed) {
        packed = QByteArray(static_cast<qsizetype>(tileAllocationBytes_), '\0');
      }
      if (packed.size() != static_cast<qsizetype>(tileAllocationBytes_)) {
        return PixelTileWriteResult::Rejected;
      }

      const quint64 tileX = tileColumn * PixelTile::extent;
      const quint64 tileY = tileRow * PixelTile::extent;
      const quint64 copyLeft =
          std::max(tileX, static_cast<quint64>(region.x()));
      const quint64 copyRight =
          std::min(tileX + PixelTile::extent, regionRight);
      const quint64 copyTop =
          std::max(tileY, static_cast<quint64>(region.y()));
      const quint64 copyBottom =
          std::min(tileY + PixelTile::extent, regionBottom);
      const quint64 copyBytes = (copyRight - copyLeft) * bytesPerPixel;
      auto* packedBytes = reinterpret_cast<std::byte*>(packed.data());
      for (quint64 y = copyTop; y < copyBottom; ++y) {
        const quint64 sourceOffset =
            (y - static_cast<quint64>(region.y())) * sourceRowStrideBytes +
            (copyLeft - static_cast<quint64>(region.x())) * bytesPerPixel;
        const quint64 destinationOffset =
            ((y - tileY) * PixelTile::extent + copyLeft - tileX) *
            bytesPerPixel;
        std::memcpy(packedBytes + destinationOffset,
                    stagedBytes + sourceOffset, copyBytes);
      }

      if (existed &&
          std::equal(existing->packedBytes().begin(),
                     existing->packedBytes().end(),
                     reinterpret_cast<const std::byte*>(packed.constData()))) {
        continue;
      }
      const bool isZero =
          std::all_of(packed.cbegin(), packed.cend(),
                      [](char byte) { return byte == 0; });
      if (isZero) {
        if (existed) {
          pending.push_back({.index = index, .remove = true});
          ++removals;
        }
        continue;
      }
      const auto packedSpan = std::span<const std::byte>(
          reinterpret_cast<const std::byte*>(packed.constData()),
          static_cast<std::size_t>(packed.size()));
      if (format_ == PixelFormat::rgba8Premultiplied() &&
          !hasValidPremultipliedSamples(packedSpan, *fullTileLayout)) {
        return PixelTileWriteResult::Rejected;
      }
      pending.push_back({.index = index,
                         .add = !existed,
                         .replacement = PixelTile(*fullTileLayout,
                                                  std::move(packed))});
      additions += !existed ? 1U : 0U;
    }
  }

  if (pending.isEmpty()) {
    return PixelTileWriteResult::Unchanged;
  }
  const quint64 finalTileCount =
      static_cast<quint64>(tiles_.size()) - removals + additions;
  if (finalTileCount > maximumResidentTiles_ ||
      finalTileCount > maximumResidentBytes_ / tileAllocationBytes_) {
    return PixelTileWriteResult::Rejected;
  }

  auto candidate = *this;
  for (auto& change : pending) {
    if (change.remove) {
      candidate.tiles_.remove(change.index);
    } else {
      candidate.tiles_.insert(change.index, std::move(*change.replacement));
    }
  }
  candidate.residentDecodedBytes_ = finalTileCount * tileAllocationBytes_;
  *this = std::move(candidate);
  return PixelTileWriteResult::Changed;
}

std::optional<QVector<PixelTileDeltaRecord>>
SparsePixelTileStore::tileDeltaTo(const SparsePixelTileStore& after,
                                  quint64 maximumPayloadBytes,
                                  quint64 maximumRecordCount) const {
  if (dimensions_ != after.dimensions_ || format_ != after.format_ ||
      maximumPayloadBytes == 0 ||
      maximumPayloadBytes > hardMaximumDeltaBytes ||
      maximumRecordCount == 0 ||
      maximumRecordCount > hardMaximumDeltaRecords) {
    return std::nullopt;
  }

  QSet<PixelTileIndex> changedIndices;
  changedIndices.reserve(tiles_.size() + after.tiles_.size());
  for (auto tile = tiles_.cbegin(); tile != tiles_.cend(); ++tile) {
    changedIndices.insert(tile.key());
  }
  for (auto tile = after.tiles_.cbegin(); tile != after.tiles_.cend(); ++tile) {
    changedIndices.insert(tile.key());
  }
  QVector<PixelTileIndex> orderedIndices(changedIndices.cbegin(),
                                         changedIndices.cend());
  std::sort(orderedIndices.begin(), orderedIndices.end(),
            [](PixelTileIndex left, PixelTileIndex right) {
              return left.row != right.row ? left.row < right.row
                                           : left.column < right.column;
            });

  QVector<PixelTileDeltaRecord> records;
  records.reserve(std::min<qsizetype>(orderedIndices.size(),
                                      static_cast<qsizetype>(maximumRecordCount)));
  quint64 payloadBytes = 0;
  for (const auto index : orderedIndices) {
    const auto beforeTile = tiles_.constFind(index);
    const auto afterTile = after.tiles_.constFind(index);
    const bool hasBefore = beforeTile != tiles_.cend();
    const bool hasAfter = afterTile != after.tiles_.cend();
    if (hasBefore && hasAfter &&
        std::equal(beforeTile->packedBytes().begin(),
                   beforeTile->packedBytes().end(),
                   afterTile->packedBytes().begin())) {
      continue;
    }
    const quint64 recordBytes =
        (hasBefore ? tileAllocationBytes_ : 0U) +
        (hasAfter ? tileAllocationBytes_ : 0U);
    if (static_cast<quint64>(records.size()) >= maximumRecordCount ||
        recordBytes > maximumPayloadBytes - payloadBytes) {
      return std::nullopt;
    }

    records.push_back(
        {.index = index,
         .before = hasBefore ? std::optional(beforeTile->bytes_)
                             : std::nullopt,
         .after = hasAfter ? std::optional(afterTile->bytes_)
                           : std::nullopt});
    payloadBytes += recordBytes;
  }
  return records;
}

PixelTileWriteResult SparsePixelTileStore::applyTileDelta(
    const QVector<PixelTileDeltaRecord>& records,
    PixelTileDeltaDirection direction, quint64 maximumPayloadBytes,
    quint64 maximumRecordCount) {
  if ((direction != PixelTileDeltaDirection::Forward &&
       direction != PixelTileDeltaDirection::Reverse) ||
      maximumPayloadBytes == 0 ||
      maximumPayloadBytes > hardMaximumDeltaBytes ||
      maximumRecordCount == 0 ||
      maximumRecordCount > hardMaximumDeltaRecords ||
      static_cast<quint64>(records.size()) > maximumRecordCount) {
    return PixelTileWriteResult::Rejected;
  }
  if (records.isEmpty()) {
    return PixelTileWriteResult::Unchanged;
  }

  const auto fullTileLayout = tileLayout(format_, tileAllocationBytes_);
  if (!fullTileLayout) {
    return PixelTileWriteResult::Rejected;
  }
  struct PendingTile final {
    PixelTileIndex index;
    bool remove = false;
    std::optional<PixelTile> replacement;
  };
  QVector<PendingTile> pending;
  pending.reserve(records.size());
  quint64 payloadBytes = 0;
  quint64 additions = 0;
  quint64 removals = 0;
  std::optional<PixelTileIndex> previousIndex;

  auto validPayload = [this, &fullTileLayout](
                          const std::optional<QByteArray>& payload) {
    if (!payload) {
      return true;
    }
    if (payload->size() != static_cast<qsizetype>(tileAllocationBytes_) ||
        std::all_of(payload->cbegin(), payload->cend(),
                    [](char byte) { return byte == 0; })) {
      return false;
    }
    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(payload->constData()),
        static_cast<std::size_t>(payload->size()));
    return format_ != PixelFormat::rgba8Premultiplied() ||
           hasValidPremultipliedSamples(bytes, *fullTileLayout);
  };

  for (const auto& record : records) {
    const bool orderedAfterPrevious =
        !previousIndex || previousIndex->row < record.index.row ||
        (previousIndex->row == record.index.row &&
         previousIndex->column < record.index.column);
    const quint64 recordBytes =
        (record.before ? tileAllocationBytes_ : 0U) +
        (record.after ? tileAllocationBytes_ : 0U);
    if (!orderedAfterPrevious || !containsTileIndex(record.index) ||
        (!record.before && !record.after) || record.before == record.after ||
        !validPayload(record.before) || !validPayload(record.after) ||
        recordBytes > maximumPayloadBytes - payloadBytes) {
      return PixelTileWriteResult::Rejected;
    }
    previousIndex = record.index;
    payloadBytes += recordBytes;

    const auto& expected = direction == PixelTileDeltaDirection::Forward
                               ? record.before
                               : record.after;
    const auto& replacement = direction == PixelTileDeltaDirection::Forward
                                  ? record.after
                                  : record.before;
    const auto current = tiles_.constFind(record.index);
    const bool currentExists = current != tiles_.cend();
    if (currentExists != expected.has_value() ||
        (expected && current->packedBytes().size() !=
                         static_cast<std::size_t>(expected->size())) ||
        (expected &&
         !std::equal(current->packedBytes().begin(),
                     current->packedBytes().end(),
                     reinterpret_cast<const std::byte*>(
                         expected->constData())))) {
      return PixelTileWriteResult::Rejected;
    }

    if (!replacement) {
      pending.push_back({.index = record.index, .remove = true});
      ++removals;
    } else {
      pending.push_back(
          {.index = record.index,
           .replacement = PixelTile(*fullTileLayout, *replacement)});
      additions += !currentExists ? 1U : 0U;
    }
  }

  const quint64 finalTileCount =
      static_cast<quint64>(tiles_.size()) - removals + additions;
  if (finalTileCount > maximumResidentTiles_ ||
      finalTileCount > maximumResidentBytes_ / tileAllocationBytes_) {
    return PixelTileWriteResult::Rejected;
  }

  auto candidate = *this;
  for (auto& change : pending) {
    if (change.remove) {
      candidate.tiles_.remove(change.index);
    } else {
      candidate.tiles_.insert(change.index, std::move(*change.replacement));
    }
  }
  candidate.residentDecodedBytes_ = finalTileCount * tileAllocationBytes_;
  *this = std::move(candidate);
  return PixelTileWriteResult::Changed;
}

PixelTileWriteResult SparsePixelTileStore::setPixelBytes(
    QPoint position, std::span<const std::byte> source) {
  if (!contains(position) || source.size() != format_.bytesPerPixel()) {
    return PixelTileWriteResult::Rejected;
  }
  const bool sourceIsZero =
      std::all_of(source.begin(), source.end(),
                  [](std::byte byte) { return byte == std::byte{}; });
  if (format_ == PixelFormat::rgba8Premultiplied() &&
      !isValidPremultipliedRgba8Pixel(source)) {
    return PixelTileWriteResult::Rejected;
  }

  const auto index = tileIndex(position);
  const auto localPosition = tilePosition(position);
  const auto existing = tiles_.constFind(index);
  if (existing == tiles_.cend()) {
    if (sourceIsZero) {
      return PixelTileWriteResult::Unchanged;
    }
    if (static_cast<quint64>(tiles_.size()) >= maximumResidentTiles_ ||
        residentDecodedBytes_ > maximumResidentBytes_ - tileAllocationBytes_) {
      return PixelTileWriteResult::Rejected;
    }
    auto candidate = PixelTile::create(format_, tileAllocationBytes_);
    if (!candidate || !candidate->setPixelBytes(localPosition, source)) {
      return PixelTileWriteResult::Rejected;
    }
    tiles_.insert(index, std::move(*candidate));
    residentDecodedBytes_ += tileAllocationBytes_;
    return PixelTileWriteResult::Changed;
  }

  const auto existingPixel = existing->pixelBytes(localPosition);
  if (std::equal(existingPixel.begin(), existingPixel.end(), source.begin())) {
    return PixelTileWriteResult::Unchanged;
  }
  auto tile = tiles_.find(index);
  if (!tile->setPixelBytes(localPosition, source)) {
    return PixelTileWriteResult::Rejected;
  }
  if (sourceIsZero && tile->isZero()) {
    tiles_.erase(tile);
    residentDecodedBytes_ -= tileAllocationBytes_;
  }
  return PixelTileWriteResult::Changed;
}

bool SparsePixelTileStore::contains(QPoint position) const noexcept {
  return position.x() >= 0 && position.y() >= 0 &&
         position.x() < dimensions_.width() &&
         position.y() < dimensions_.height();
}

PixelTileIndex SparsePixelTileStore::tileIndex(QPoint position) noexcept {
  return {static_cast<quint32>(position.x() / PixelTile::extent),
          static_cast<quint32>(position.y() / PixelTile::extent)};
}

QPoint SparsePixelTileStore::tilePosition(QPoint position) noexcept {
  return {position.x() % PixelTile::extent,
          position.y() % PixelTile::extent};
}

std::optional<Rgba8Buffer> convertRgba8(
    std::span<const std::byte> source, const PixelStorageLayout& sourceLayout,
    AlphaMode destinationAlpha, quint64 destinationRowAlignment,
    quint64 maximumAllocationBytes) {
  if (!isSupportedRgba8(sourceLayout.format()) ||
      (destinationAlpha != AlphaMode::Straight &&
       destinationAlpha != AlphaMode::Premultiplied) ||
      source.size() != sourceLayout.allocationBytes() ||
      !hasValidPremultipliedSamples(source, sourceLayout)) {
    return std::nullopt;
  }

  const PixelFormat destinationFormat =
      destinationAlpha == AlphaMode::Straight
          ? PixelFormat::rgba8Straight()
          : PixelFormat::rgba8Premultiplied();
  const auto destinationLayout = PixelStorageLayout::create(
      sourceLayout.dimensions(), destinationFormat, destinationRowAlignment,
      maximumAllocationBytes);
  if (!destinationLayout) {
    return std::nullopt;
  }

  QByteArray destination(
      static_cast<qsizetype>(destinationLayout->allocationBytes()), '\0');
  if (destination.size() !=
      static_cast<qsizetype>(destinationLayout->allocationBytes())) {
    return std::nullopt;
  }

  auto* destinationBytes =
      reinterpret_cast<quint8*>(destination.data());
  convertRows(source, sourceLayout, destinationAlpha, destinationBytes,
              destinationLayout->rowStrideBytes());

  return Rgba8Buffer{*destinationLayout, std::move(destination)};
}

std::optional<Rgba8Buffer> rgba8BytesFromImage(
    const QImage& image, AlphaMode destinationAlpha,
    quint64 destinationRowAlignment, quint64 maximumAllocationBytes) {
  PixelFormat sourceFormat;
  if (image.format() == QImage::Format_RGBA8888) {
    sourceFormat = PixelFormat::rgba8Straight();
  } else if (image.format() == QImage::Format_RGBA8888_Premultiplied) {
    sourceFormat = PixelFormat::rgba8Premultiplied();
  } else {
    return std::nullopt;
  }

  const auto sourceLayout = PixelStorageLayout::createWithRowStride(
      image.size(), sourceFormat, static_cast<quint64>(image.bytesPerLine()),
      maximumAllocationBytes);
  if (!sourceLayout || image.sizeInBytes() != sourceLayout->allocationBytes()) {
    return std::nullopt;
  }

  return convertRgba8(
      std::span<const std::byte>(
          reinterpret_cast<const std::byte*>(image.constBits()),
          static_cast<std::size_t>(image.sizeInBytes())),
      *sourceLayout, destinationAlpha, destinationRowAlignment,
      maximumAllocationBytes);
}

std::optional<QImage> rgba8ImageFromBytes(
    std::span<const std::byte> source, const PixelStorageLayout& sourceLayout,
    quint64 maximumAllocationBytes) {
  if (!isSupportedRgba8(sourceLayout.format()) ||
      source.size() != sourceLayout.allocationBytes() ||
      !hasValidPremultipliedSamples(source, sourceLayout) ||
      !PixelStorageLayout::create(
          sourceLayout.dimensions(), PixelFormat::rgba8Premultiplied(), 1,
          maximumAllocationBytes)) {
    return std::nullopt;
  }

  QImage image(sourceLayout.dimensions(),
               QImage::Format_RGBA8888_Premultiplied);
  if (image.isNull()) {
    return std::nullopt;
  }
  const auto imageLayout = PixelStorageLayout::createWithRowStride(
      image.size(), PixelFormat::rgba8Premultiplied(),
      static_cast<quint64>(image.bytesPerLine()), maximumAllocationBytes);
  if (!imageLayout || image.sizeInBytes() != imageLayout->allocationBytes()) {
    return std::nullopt;
  }
  convertRows(source, sourceLayout, AlphaMode::Premultiplied, image.bits(),
              imageLayout->rowStrideBytes());
  return image;
}

}  // namespace chromarchy
