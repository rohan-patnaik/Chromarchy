#include "core/ImageIO.h"

#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

namespace chromarchy {
namespace {

QByteArray formatForPath(const QString& filePath) {
  auto suffix = QFileInfo(filePath).suffix().toLower().toLatin1();
  if (suffix == "jpg") {
    suffix = "jpeg";
  } else if (suffix == "tif") {
    suffix = "tiff";
  }
  return suffix;
}

QImage flattenForJpeg(const QImage& source) {
  QImage flattened(source.size(), QImage::Format_RGB888);
  flattened.fill(Qt::white);
  QPainter painter(&flattened);
  painter.drawImage(QPoint(), source);
  return flattened;
}

}  // namespace

DocumentLoadResult ImageIO::open(const QString& filePath) {
  const QFileInfo inputInfo(filePath);
  if (!inputInfo.isFile() || inputInfo.size() < 0 ||
      static_cast<quint64>(inputInfo.size()) > maximumImportFileBytes) {
    return {.error = QStringLiteral(
                "Image file is missing or exceeds the bounded input size limit.")};
  }
  QImageReader reader(filePath);
  reader.setAutoTransform(true);
  const auto declaredSize = reader.size();
  if (declaredSize.width() <= 0 || declaredSize.height() <= 0 ||
      declaredSize.width() > Document::maximumDimension ||
      declaredSize.height() > Document::maximumDimension) {
    return {.error = QStringLiteral("Image dimensions are invalid or exceed %1 pixels.")
                         .arg(Document::maximumDimension)};
  }
  const auto pixelCount = static_cast<quint64>(declaredSize.width()) *
                          static_cast<quint64>(declaredSize.height());
  if (pixelCount > maximumImportPixels ||
      pixelCount > maximumImportDecodedBytes / 4) {
    return {.error = QStringLiteral(
                "Image decode requires %1 megapixels, above the bounded import limit.")
                        .arg(pixelCount / 1'000'000.0, 0, 'f', 1)};
  }
  if (!reader.canRead()) {
    return {.error = reader.errorString()};
  }

  const auto image = reader.read();
  if (image.isNull()) {
    return {.error = reader.errorString()};
  }

  auto document = Document::create(image.size());
  if (!document) {
    return {.error = QStringLiteral("Could not create a document for this image.")};
  }
  auto* layer = document->layerAt(0);
  layer->setName(QFileInfo(filePath).completeBaseName());
  if (!layer->replacePixels(TiledImage::fromImage(image))) {
    return {.error = QStringLiteral(
                "Decoded pixels do not match the document canvas.")};
  }
  return {.document = std::move(document)};
}

ImageWriteResult ImageIO::exportComposite(const Document& document,
                                          const QString& filePath,
                                          int quality) {
  const auto format = formatForPath(filePath);
  if (format.isEmpty()) {
    return {.error = QStringLiteral("The export filename needs a format extension.")};
  }

  const auto pixelCount = static_cast<quint64>(document.size().width()) *
                          static_cast<quint64>(document.size().height());
  if (pixelCount > maximumExportPixels) {
    return {.error = QStringLiteral(
                "Export requires %1 megapixels, above the current %2-megapixel "
                "bounded export limit. Resize or crop the document first.")
                        .arg(pixelCount / 1'000'000.0, 0, 'f', 1)
                        .arg(maximumExportPixels / 1'000'000.0, 0, 'f', 1)};
  }

  auto image = document.composite();
  if (image.isNull()) {
    return {.error = QStringLiteral("The document has no exportable canvas.")};
  }
  if (format == "jpeg") {
    image = flattenForJpeg(image);
  }

  QSaveFile output(filePath);
  if (!output.open(QIODevice::WriteOnly)) {
    return {.error = output.errorString()};
  }

  QImageWriter writer(&output, format);
  writer.setQuality(quality);
  writer.setOptimizedWrite(true);
  if (!writer.write(image)) {
    output.cancelWriting();
    return {.error = writer.errorString()};
  }
  if (!output.commit()) {
    return {.error = output.errorString()};
  }
  return {.success = true};
}

}  // namespace chromarchy
