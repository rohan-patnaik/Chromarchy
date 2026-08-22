#pragma once

#include <QSize>
#include <QtTypes>

#include <limits>
#include <optional>

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

}  // namespace chromarchy
