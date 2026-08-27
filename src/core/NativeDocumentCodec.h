#pragma once

#include "core/Document.h"

#include <QString>

#include <optional>

namespace chromarchy {

struct NativeDocumentLoadResult final {
  std::optional<Document> document;
  QString error;

  [[nodiscard]] explicit operator bool() const noexcept {
    return document.has_value();
  }
};

struct NativeDocumentWriteResult final {
  bool success = false;
  QString error;

  [[nodiscard]] explicit operator bool() const noexcept {
    return success;
  }
};

class NativeDocumentCodec final {
public:
  static constexpr quint32 formatVersion = 2;
  static constexpr auto extension = ".chromarchy";
  static constexpr quint32 maximumLayerCount = 10'000;
  static constexpr quint32 maximumLayerNameBytes = 4'096;
  static constexpr quint64 maximumNativeFileBytes = 64ULL * 1024ULL * 1024ULL;
  static constexpr quint64 maximumCompressedStorageBytes =
      32ULL * 1024ULL * 1024ULL;
  static constexpr quint64 maximumDecodedStorageBytes =
      16ULL * 1024ULL * 1024ULL;
  static constexpr quint64 maximumStoredTileCount = 64;

  [[nodiscard]] static NativeDocumentWriteResult save(
      const Document& document, const QString& filePath);
  [[nodiscard]] static NativeDocumentLoadResult load(const QString& filePath);
};

}  // namespace chromarchy
