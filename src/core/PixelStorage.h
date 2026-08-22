#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVector>
#include <QtTypes>

#include <cstddef>
#include <limits>
#include <optional>
#include <span>

namespace chromarchy {

enum class SampleFormat : quint8 {
  UnsignedInteger8,
  UnsignedInteger16,
  Float16,
  Float32,
};

enum class ChannelLayout : quint8 {
  Gray,
  GrayAlpha,
  RGB,
  RGBA,
};

enum class ChannelMeaning : quint8 {
  Gray,
  Red,
  Green,
  Blue,
  Alpha,
};

enum class AlphaMode : quint8 {
  None,
  Straight,
  Premultiplied,
};

enum class ByteOrder : quint8 {
  NotApplicable,
  LittleEndian,
  BigEndian,
};

struct PixelFormat final {
  SampleFormat sample = SampleFormat::UnsignedInteger8;
  ChannelLayout channels = ChannelLayout::RGBA;
  AlphaMode alpha = AlphaMode::Premultiplied;
  ByteOrder byteOrder = ByteOrder::NotApplicable;

  [[nodiscard]] static constexpr PixelFormat rgba8Premultiplied() noexcept {
    return {};
  }

  [[nodiscard]] static constexpr PixelFormat rgba8Straight() noexcept {
    return {.sample = SampleFormat::UnsignedInteger8,
            .channels = ChannelLayout::RGBA,
            .alpha = AlphaMode::Straight,
            .byteOrder = ByteOrder::NotApplicable};
  }

  [[nodiscard]] constexpr bool isValid() const noexcept {
    if (channelCount() == 0 || bitsPerSample() == 0) {
      return false;
    }
    switch (alpha) {
      case AlphaMode::None:
      case AlphaMode::Straight:
      case AlphaMode::Premultiplied:
        break;
      default:
        return false;
    }
    switch (byteOrder) {
      case ByteOrder::NotApplicable:
      case ByteOrder::LittleEndian:
      case ByteOrder::BigEndian:
        break;
      default:
        return false;
    }
    const bool alphaChannel = hasAlpha();
    const bool byteOrderApplies = bytesPerSample() > 1;
    return alphaChannel == (alpha != AlphaMode::None) &&
           byteOrderApplies == (byteOrder != ByteOrder::NotApplicable);
  }

  [[nodiscard]] constexpr quint8 channelCount() const noexcept {
    switch (channels) {
      case ChannelLayout::Gray:
        return 1;
      case ChannelLayout::GrayAlpha:
        return 2;
      case ChannelLayout::RGB:
        return 3;
      case ChannelLayout::RGBA:
        return 4;
    }
    return 0;
  }

  [[nodiscard]] constexpr bool hasAlpha() const noexcept {
    return channels == ChannelLayout::GrayAlpha || channels == ChannelLayout::RGBA;
  }

  [[nodiscard]] constexpr bool isFloatingPoint() const noexcept {
    return sample == SampleFormat::Float16 || sample == SampleFormat::Float32;
  }

  [[nodiscard]] constexpr quint8 bitsPerSample() const noexcept {
    switch (sample) {
      case SampleFormat::UnsignedInteger8:
        return 8;
      case SampleFormat::UnsignedInteger16:
      case SampleFormat::Float16:
        return 16;
      case SampleFormat::Float32:
        return 32;
    }
    return 0;
  }

  [[nodiscard]] constexpr quint8 bytesPerSample() const noexcept {
    return bitsPerSample() / 8;
  }

  [[nodiscard]] constexpr quint8 bytesPerPixel() const noexcept {
    return channelCount() * bytesPerSample();
  }

  [[nodiscard]] constexpr std::optional<ChannelMeaning> channelMeaning(
      quint8 index) const noexcept {
    if (index >= channelCount()) {
      return std::nullopt;
    }
    if (channels == ChannelLayout::Gray || channels == ChannelLayout::GrayAlpha) {
      return index == 0 ? ChannelMeaning::Gray : ChannelMeaning::Alpha;
    }
    if (index == 0) {
      return ChannelMeaning::Red;
    }
    if (index == 1) {
      return ChannelMeaning::Green;
    }
    if (index == 2) {
      return ChannelMeaning::Blue;
    }
    return ChannelMeaning::Alpha;
  }

  friend bool operator==(const PixelFormat&, const PixelFormat&) = default;
};

class PixelStorageLayout final {
public:
  [[nodiscard]] static std::optional<PixelStorageLayout> create(
      QSize dimensions, PixelFormat format, quint64 rowAlignment = 1,
      quint64 maximumAllocationBytes = std::numeric_limits<quint64>::max()) noexcept;
  [[nodiscard]] static std::optional<PixelStorageLayout> createWithRowStride(
      QSize dimensions, PixelFormat format, quint64 rowStrideBytes,
      quint64 maximumAllocationBytes = std::numeric_limits<quint64>::max()) noexcept;

  [[nodiscard]] QSize dimensions() const noexcept;
  [[nodiscard]] PixelFormat format() const noexcept;
  [[nodiscard]] quint64 packedRowBytes() const noexcept;
  [[nodiscard]] quint64 rowStrideBytes() const noexcept;
  [[nodiscard]] quint64 allocationBytes() const noexcept;

private:
  PixelStorageLayout(QSize dimensions, PixelFormat format, quint64 packedRowBytes,
                     quint64 rowStrideBytes, quint64 allocationBytes) noexcept;

  QSize dimensions_;
  PixelFormat format_;
  quint64 packedRowBytes_ = 0;
  quint64 rowStrideBytes_ = 0;
  quint64 allocationBytes_ = 0;
};

struct Rgba8Buffer final {
  PixelStorageLayout layout;
  QByteArray bytes;
};

class PixelTile final {
public:
  static constexpr int extent = 256;
  static constexpr quint64 maximumAllocationBytes =
      static_cast<quint64>(extent) * extent * 4U * 4U;

  [[nodiscard]] static std::optional<PixelTile> create(
      PixelFormat format,
      quint64 allocationLimitBytes = maximumAllocationBytes);
  [[nodiscard]] static std::optional<PixelTile> fromPackedBytes(
      PixelFormat format, std::span<const std::byte> source,
      quint64 allocationLimitBytes = maximumAllocationBytes);

  [[nodiscard]] PixelFormat format() const noexcept;
  [[nodiscard]] const PixelStorageLayout& layout() const noexcept;
  [[nodiscard]] std::span<const std::byte> packedBytes() const noexcept;
  [[nodiscard]] std::span<const std::byte> pixelBytes(
      QPoint position) const noexcept;
  bool setPixelBytes(QPoint position, std::span<const std::byte> source);
  [[nodiscard]] bool isZero() const noexcept;
  [[nodiscard]] std::optional<Rgba8Buffer> toRgba8Premultiplied(
      quint64 allocationLimitBytes = maximumAllocationBytes) const;

private:
  friend class SparsePixelTileStore;

  PixelTile(PixelStorageLayout layout, QByteArray bytes);

  PixelStorageLayout layout_;
  QByteArray bytes_;
};

struct PixelTileIndex final {
  quint32 column = 0;
  quint32 row = 0;

  friend bool operator==(const PixelTileIndex&, const PixelTileIndex&) = default;
};

size_t qHash(const PixelTileIndex& index, size_t seed = 0) noexcept;

enum class PixelTileWriteResult : quint8 {
  Changed,
  Unchanged,
  Rejected,
};

struct PixelTileSnapshot final {
  PixelTileIndex index;
  QByteArray packedBytes;

  friend bool operator==(const PixelTileSnapshot&, const PixelTileSnapshot&) =
      default;
};

struct PixelRegionBuffer final {
  PixelStorageLayout layout;
  QByteArray bytes;
};

struct PixelTileDeltaRecord final {
  PixelTileIndex index;
  std::optional<QByteArray> before;
  std::optional<QByteArray> after;

  friend bool operator==(const PixelTileDeltaRecord&,
                         const PixelTileDeltaRecord&) = default;
};

enum class PixelTileDeltaDirection : quint8 {
  Forward,
  Reverse,
};

class SparsePixelTileStore final {
public:
  static constexpr quint64 hardMaximumResidentBytes =
      16ULL * 1024ULL * 1024ULL;
  static constexpr quint64 hardMaximumResidentTiles = 64;
  static constexpr quint64 hardMaximumRegionBytes =
      16ULL * 1024ULL * 1024ULL;
  static constexpr quint64 hardMaximumRegionTiles = 64;
  static constexpr quint64 hardMaximumDeltaBytes =
      16ULL * 1024ULL * 1024ULL;
  static constexpr quint64 hardMaximumDeltaRecords = 64;

  [[nodiscard]] static std::optional<SparsePixelTileStore> create(
      QSize dimensions, PixelFormat format,
      quint64 maximumResidentBytes = hardMaximumResidentBytes,
      quint64 maximumResidentTiles = hardMaximumResidentTiles);

  [[nodiscard]] QSize dimensions() const noexcept;
  [[nodiscard]] PixelFormat format() const noexcept;
  [[nodiscard]] quint64 maximumResidentBytes() const noexcept;
  [[nodiscard]] quint64 maximumResidentTiles() const noexcept;
  [[nodiscard]] quint64 residentDecodedBytes() const noexcept;
  [[nodiscard]] qsizetype allocatedTileCount() const noexcept;
  [[nodiscard]] bool containsTileIndex(PixelTileIndex index) const noexcept;
  [[nodiscard]] std::optional<QByteArray> pixelBytes(QPoint position) const;
  [[nodiscard]] std::optional<std::span<const std::byte>> packedTileBytes(
      PixelTileIndex index) const noexcept;
  [[nodiscard]] QVector<PixelTileSnapshot> tileSnapshots() const;
  [[nodiscard]] static std::optional<SparsePixelTileStore> fromTileSnapshots(
      QSize dimensions, PixelFormat format,
      const QVector<PixelTileSnapshot>& snapshots,
      quint64 maximumResidentBytes = hardMaximumResidentBytes,
      quint64 maximumResidentTiles = hardMaximumResidentTiles);
  [[nodiscard]] std::optional<PixelRegionBuffer> readRegion(
      QRect region, quint64 rowAlignment = 1,
      quint64 maximumAllocationBytes = hardMaximumRegionBytes) const;
  PixelTileWriteResult writeRegion(
      QRect region, std::span<const std::byte> source,
      quint64 sourceRowStrideBytes,
      quint64 maximumAllocationBytes = hardMaximumRegionBytes);
  [[nodiscard]] std::optional<QVector<PixelTileDeltaRecord>> tileDeltaTo(
      const SparsePixelTileStore& after,
      quint64 maximumPayloadBytes = hardMaximumDeltaBytes,
      quint64 maximumRecordCount = hardMaximumDeltaRecords) const;
  PixelTileWriteResult applyTileDelta(
      const QVector<PixelTileDeltaRecord>& records,
      PixelTileDeltaDirection direction,
      quint64 maximumPayloadBytes = hardMaximumDeltaBytes,
      quint64 maximumRecordCount = hardMaximumDeltaRecords);
  PixelTileWriteResult setPixelBytes(QPoint position,
                                     std::span<const std::byte> source);

private:
  SparsePixelTileStore(QSize dimensions, PixelFormat format,
                       quint64 tileAllocationBytes,
                       quint64 maximumResidentBytes,
                       quint64 maximumResidentTiles) noexcept;

  [[nodiscard]] bool contains(QPoint position) const noexcept;
  [[nodiscard]] static PixelTileIndex tileIndex(QPoint position) noexcept;
  [[nodiscard]] static QPoint tilePosition(QPoint position) noexcept;

  QSize dimensions_;
  PixelFormat format_;
  quint64 tileAllocationBytes_ = 0;
  quint64 maximumResidentBytes_ = 0;
  quint64 maximumResidentTiles_ = 0;
  quint64 residentDecodedBytes_ = 0;
  QHash<PixelTileIndex, PixelTile> tiles_;
};

[[nodiscard]] std::optional<Rgba8Buffer> convertRgba8(
    std::span<const std::byte> source, const PixelStorageLayout& sourceLayout,
    AlphaMode destinationAlpha, quint64 destinationRowAlignment = 1,
    quint64 maximumAllocationBytes = std::numeric_limits<quint64>::max());

[[nodiscard]] std::optional<Rgba8Buffer> rgba8BytesFromImage(
    const QImage& image, AlphaMode destinationAlpha,
    quint64 destinationRowAlignment = 1,
    quint64 maximumAllocationBytes = std::numeric_limits<quint64>::max());

[[nodiscard]] std::optional<QImage> rgba8ImageFromBytes(
    std::span<const std::byte> source, const PixelStorageLayout& sourceLayout,
    quint64 maximumAllocationBytes = std::numeric_limits<quint64>::max());

}  // namespace chromarchy
