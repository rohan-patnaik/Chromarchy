#include "core/PixelStorage.h"

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
