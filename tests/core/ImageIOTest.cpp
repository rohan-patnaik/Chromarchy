#include "core/ImageIO.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>
#include <QTest>

using chromarchy::Document;
using chromarchy::ImageIO;

class ImageIOTest final : public QObject {
  Q_OBJECT

private slots:
  void opensImageIntoRealPixelTiles();
  void exportsCompositeAtomically();
  void rejectsUnknownOrDamagedInput();
  void roundTripsSupportedFormats_data();
  void roundTripsSupportedFormats();
  void exportStripsSourceMetadataByDefault();
  void oversizedSparseExportFailsBeforeAllocation();
  void oversizedDeclaredImportFailsBeforeDecode();
  void oversizedSparseInputFileFailsBeforeDecode();
};

void ImageIOTest::opensImageIntoRealPixelTiles() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("source.png"));

  QImage source(300, 270, QImage::Format_RGBA8888);
  source.fill(Qt::transparent);
  source.setPixelColor(QPoint(299, 269), QColor(12, 34, 56, 255));
  QVERIFY(source.save(path));

  auto result = ImageIO::open(path);
  QVERIFY2(result, qPrintable(result.error));
  QCOMPARE(result.document->size(), source.size());
  QCOMPARE(result.document->layerAt(0)->name(), QStringLiteral("source"));
  QCOMPARE(result.document->layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(result.document->layerAt(0)->pixels().pixelColor(QPoint(299, 269)),
           QColor(12, 34, 56, 255));
  QVERIFY(result.document->layerAt(0)->pixels().dirtyRegion().isEmpty());
}

void ImageIOTest::exportsCompositeAtomically() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("export.png"));

  auto document = Document::create(QSize(16, 12));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(
      QPoint(4, 5), QColor(100, 120, 140, 200)));

  const auto result = ImageIO::exportComposite(*document, path);
  QVERIFY2(result, qPrintable(result.error));
  const QImage exported(path);
  QCOMPARE(exported.size(), QSize(16, 12));
  const auto pixel = exported.pixelColor(QPoint(4, 5));
  QCOMPARE(pixel.alpha(), 200);
  QVERIFY(qAbs(pixel.red() - 100) <= 1);
  QVERIFY(qAbs(pixel.green() - 120) <= 1);
  QVERIFY(qAbs(pixel.blue() - 140) <= 1);
}

void ImageIOTest::rejectsUnknownOrDamagedInput() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("damaged.png"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write("not an image"), 12);
  file.close();

  const auto result = ImageIO::open(path);
  QVERIFY(!result);
  QVERIFY(!result.error.isEmpty());
}

void ImageIOTest::roundTripsSupportedFormats_data() {
  QTest::addColumn<QByteArray>("format");
  QTest::addColumn<QString>("suffix");
  QTest::addColumn<int>("maximumChannelError");
  QTest::newRow("png") << QByteArray("png") << QStringLiteral("png") << 1;
  QTest::newRow("jpeg") << QByteArray("jpeg") << QStringLiteral("jpg") << 4;
  QTest::newRow("tiff") << QByteArray("tiff") << QStringLiteral("tiff") << 1;
  QTest::newRow("webp") << QByteArray("webp") << QStringLiteral("webp") << 2;
  QTest::newRow("openexr") << QByteArray("exr") << QStringLiteral("exr") << 2;
}

void ImageIOTest::roundTripsSupportedFormats() {
  QFETCH(QByteArray, format);
  QFETCH(QString, suffix);
  QFETCH(int, maximumChannelError);
  if (!QImageReader::supportedImageFormats().contains(format) ||
      !QImageWriter::supportedImageFormats().contains(format)) {
    QSKIP(qPrintable(QStringLiteral("Qt codec '%1' is unavailable")
                         .arg(QString::fromLatin1(format))));
  }

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("roundtrip.%1").arg(suffix));
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  QImage source(64, 64, QImage::Format_RGBA8888_Premultiplied);
  source.fill(QColor(80, 120, 160, 255));
  QVERIFY(document->layerAt(0)->replacePixels(
      chromarchy::TiledImage::fromImage(source)));

  const auto exported = ImageIO::exportComposite(*document, path, 100);
  QVERIFY2(exported, qPrintable(exported.error));
  const auto opened = ImageIO::open(path);
  QVERIFY2(opened, qPrintable(opened.error));
  const auto pixel = opened.document->composite().pixelColor(QPoint(32, 32));
  QVERIFY(qAbs(pixel.red() - 80) <= maximumChannelError);
  QVERIFY(qAbs(pixel.green() - 120) <= maximumChannelError);
  QVERIFY(qAbs(pixel.blue() - 160) <= maximumChannelError);
  QCOMPARE(pixel.alpha(), 255);
}

void ImageIOTest::exportStripsSourceMetadataByDefault() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto sourcePath = directory.filePath(QStringLiteral("metadata-source.png"));
  QImage source(8, 8, QImage::Format_RGBA8888);
  source.fill(Qt::red);
  source.setText(QStringLiteral("Author"), QStringLiteral("Private Name"));
  QVERIFY(source.save(sourcePath));

  const auto opened = ImageIO::open(sourcePath);
  QVERIFY2(opened, qPrintable(opened.error));
  const auto exportPath = directory.filePath(QStringLiteral("metadata-export.png"));
  const auto exported = ImageIO::exportComposite(*opened.document, exportPath);
  QVERIFY2(exported, qPrintable(exported.error));
  QImageReader reader(exportPath);
  QVERIFY(reader.read().isNull() == false);
  QVERIFY(reader.textKeys().isEmpty());
}

void ImageIOTest::oversizedSparseExportFailsBeforeAllocation() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("oversized.png"));
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(10, 10), Qt::red));

  const auto result = ImageIO::exportComposite(*document, path);
  QVERIFY(!result);
  QVERIFY(result.error.contains(QStringLiteral("bounded export limit")));
  QVERIFY(!QFileInfo::exists(path));
}

void ImageIOTest::oversizedDeclaredImportFailsBeforeDecode() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("oversized.png"));
  // Original minimal PNG header declaring 100,000 x 100,000 RGBA8 pixels.
  // The IHDR CRC is valid; no image payload is needed because the application
  // must reject the declared allocation before asking libpng to decode it.
  const auto header = QByteArray::fromHex(
      "89504e470d0a1a0a0000000d49484452000186a0000186a00806000000a8520bc8"
      "000000004944415435af061e"
      "0000000049454e44ae426082");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(header), header.size());
  file.close();

  const auto result = ImageIO::open(path);
  QVERIFY(!result);
  QVERIFY(result.error.contains(QStringLiteral("bounded import limit")));
}

void ImageIOTest::oversizedSparseInputFileFailsBeforeDecode() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("oversized.png"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.resize(512LL * 1024LL * 1024LL + 1));
  file.close();

  const auto result = ImageIO::open(path);
  QVERIFY(!result);
  QVERIFY(result.error.contains(QStringLiteral("bounded input size limit")));
}

QTEST_APPLESS_MAIN(ImageIOTest)

#include "ImageIOTest.moc"
