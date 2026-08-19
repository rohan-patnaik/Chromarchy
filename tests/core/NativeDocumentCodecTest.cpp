#include "core/NativeDocumentCodec.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using chromarchy::Document;
using chromarchy::NativeDocumentCodec;

class NativeDocumentCodecTest final : public QObject {
  Q_OBJECT

private slots:
  void preservesLayeredDocument();
  void rejectsCorruptAndTruncatedDocuments();
};

void NativeDocumentCodecTest::preservesLayeredDocument() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("roundtrip.chromarchy"));

  auto document = Document::create(QSize(700, 400));
  QVERIFY(document);
  auto* base = document->layerAt(0);
  base->setName(QStringLiteral("Base / colour"));
  base->setLocked(true);
  QVERIFY(base->pixels().setPixelColor(QPoint(699, 399),
                                      QColor(20, 40, 60, 255)));
  const auto baseId = base->id();

  const auto inkIndex = document->addLayer(QStringLiteral("Ink"));
  auto* ink = document->layerAt(inkIndex);
  ink->setOpacity(0.625);
  ink->setVisible(false);
  QVERIFY(ink->pixels().setPixelColor(QPoint(257, 10),
                                     QColor(200, 100, 50, 180)));
  const auto expectedInkPixel = ink->pixels().pixelColor(QPoint(257, 10));
  const auto inkId = ink->id();

  const auto written = NativeDocumentCodec::save(*document, path);
  QVERIFY2(written, qPrintable(written.error));
  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));

  QCOMPARE(loaded.document->size(), QSize(700, 400));
  QCOMPARE(loaded.document->layerCount(), 2);
  QCOMPARE(loaded.document->activeLayerIndex(), 1);
  QCOMPARE(loaded.document->layerAt(0)->id(), baseId);
  QCOMPARE(loaded.document->layerAt(0)->name(), QStringLiteral("Base / colour"));
  QVERIFY(loaded.document->layerAt(0)->isLocked());
  QCOMPARE(loaded.document->layerAt(0)->pixels().pixelColor(QPoint(699, 399)),
           QColor(20, 40, 60, 255));
  QCOMPARE(loaded.document->layerAt(1)->id(), inkId);
  QCOMPARE(loaded.document->layerAt(1)->opacity(), 0.625);
  QVERIFY(!loaded.document->layerAt(1)->isVisible());
  QCOMPARE(loaded.document->layerAt(1)->pixels().pixelColor(QPoint(257, 10)),
           expectedInkPixel);
}

void NativeDocumentCodecTest::rejectsCorruptAndTruncatedDocuments() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto corruptPath = directory.filePath(QStringLiteral("corrupt.chromarchy"));
  QFile corrupt(corruptPath);
  QVERIFY(corrupt.open(QIODevice::WriteOnly));
  QCOMPARE(corrupt.write("not-a-document"), 14);
  corrupt.close();
  QVERIFY(!NativeDocumentCodec::load(corruptPath));

  auto document = Document::create(QSize(10, 10));
  QVERIFY(document);
  const auto truncatedPath =
      directory.filePath(QStringLiteral("truncated.chromarchy"));
  QVERIFY(NativeDocumentCodec::save(*document, truncatedPath));
  QFile truncated(truncatedPath);
  QVERIFY(truncated.open(QIODevice::ReadWrite));
  QVERIFY(truncated.resize(truncated.size() - 1));
  truncated.close();
  QVERIFY(!NativeDocumentCodec::load(truncatedPath));
}

QTEST_APPLESS_MAIN(NativeDocumentCodecTest)

#include "NativeDocumentCodecTest.moc"
