#include "core/PixelStorage.h"

#include <limits>

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

}  // namespace chromarchy
