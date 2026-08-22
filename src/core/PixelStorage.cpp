#include "core/PixelStorage.h"

#include <algorithm>
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

quint8 premultiply(quint8 color, quint8 alpha) noexcept {
  return static_cast<quint8>((static_cast<quint32>(color) * alpha + 127U) /
                             255U);
}

quint8 unpremultiply(quint8 color, quint8 alpha) noexcept {
  if (alpha == 0) {
    return 0;
  }
  return static_cast<quint8>(std::min<quint32>(
      255U, (static_cast<quint32>(color) * 255U + alpha / 2U) / alpha));
}

void convertRows(std::span<const std::byte> source,
                 const PixelStorageLayout& sourceLayout,
                 AlphaMode destinationAlpha, quint8* destinationBytes,
                 quint64 destinationRowStride) noexcept {
  const auto* sourceBytes =
      reinterpret_cast<const quint8*>(source.data());
  const auto width = sourceLayout.dimensions().width();
  const auto height = sourceLayout.dimensions().height();
  for (int y = 0; y < height; ++y) {
    const auto* sourceRow =
        sourceBytes + static_cast<quint64>(y) * sourceLayout.rowStrideBytes();
    auto* destinationRow = destinationBytes +
                           static_cast<quint64>(y) * destinationRowStride;
    for (int x = 0; x < width; ++x) {
      const auto* sourcePixel = sourceRow + x * 4;
      auto* destinationPixel = destinationRow + x * 4;
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

std::optional<Rgba8Buffer> convertRgba8(
    std::span<const std::byte> source, const PixelStorageLayout& sourceLayout,
    AlphaMode destinationAlpha, quint64 destinationRowAlignment,
    quint64 maximumAllocationBytes) {
  if (!isSupportedRgba8(sourceLayout.format()) ||
      (destinationAlpha != AlphaMode::Straight &&
       destinationAlpha != AlphaMode::Premultiplied) ||
      source.size() != sourceLayout.allocationBytes()) {
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
