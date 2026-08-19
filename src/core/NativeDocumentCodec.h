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
  static constexpr quint32 formatVersion = 1;
  static constexpr auto extension = ".chromarchy";

  [[nodiscard]] static NativeDocumentWriteResult save(
      const Document& document, const QString& filePath);
  [[nodiscard]] static NativeDocumentLoadResult load(const QString& filePath);
};

}  // namespace chromarchy
