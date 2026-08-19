#include "core/ImageIO.h"

#include <QFile>
#include <QImage>
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
  QVERIFY(document->layerAt(0)->pixels().setPixelColor(
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

QTEST_APPLESS_MAIN(ImageIOTest)

#include "ImageIOTest.moc"
