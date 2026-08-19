#pragma once

#include "core/Document.h"

#include <QString>

#include <optional>

namespace chromarchy {

struct DocumentLoadResult final {
  std::optional<Document> document;
  QString error;

  [[nodiscard]] explicit operator bool() const noexcept {
    return document.has_value();
  }
};

struct ImageWriteResult final {
  bool success = false;
  QString error;

  [[nodiscard]] explicit operator bool() const noexcept {
    return success;
  }
};

class ImageIO final {
public:
  static constexpr quint64 maximumExportPixels = 64ULL * 1024ULL * 1024ULL;

  [[nodiscard]] static DocumentLoadResult open(const QString& filePath);
  [[nodiscard]] static ImageWriteResult exportComposite(
      const Document& document, const QString& filePath, int quality = -1);
};

}  // namespace chromarchy
