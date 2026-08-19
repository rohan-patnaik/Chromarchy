#include "core/NativeDocumentCodec.h"

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <limits>

using chromarchy::Document;
using chromarchy::NativeDocumentCodec;

class NativeDocumentCodecTest final : public QObject {
  Q_OBJECT

private slots:
  void preservesLayeredDocument();
  void loadsVersionOneWithoutSelection();
  void rejectsCorruptAndTruncatedDocuments();
  void rejectsNonFiniteLayerOpacity();
};

void NativeDocumentCodecTest::preservesLayeredDocument() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("roundtrip.chromarchy"));

  auto document = Document::create(QSize(700, 400));
  QVERIFY(document);
  auto* base = document->layerAt(0);
  base->setName(QStringLiteral("Base / colour"));
  QVERIFY(base->setPixelColor(QPoint(699, 399),
                              QColor(20, 40, 60, 255)));
  base->setLocked(true);
  const auto baseId = base->id();

  const auto inkIndex = document->addLayer(QStringLiteral("Ink"));
  auto* ink = document->layerAt(inkIndex);
  ink->setOpacity(0.625);
  ink->setVisible(false);
  QVERIFY(ink->setPixelColor(QPoint(257, 10),
                             QColor(200, 100, 50, 180)));
  const auto expectedInkPixel = ink->pixels().pixelColor(QPoint(257, 10));
  const auto inkId = ink->id();
  document->selection().selectAll();
  QVERIFY(document->selection().setCoverage(QPoint(5, 6), 32));

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
  QCOMPARE(loaded.document->selection().coverage(QPoint(5, 6)), 32);
  QCOMPARE(loaded.document->selection().coverage(QPoint(600, 300)), 255);
  QCOMPARE(loaded.document->selection().allocatedTileCount(), 1);
}

void NativeDocumentCodecTest::loadsVersionOneWithoutSelection() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("version-one.chromarchy"));
  auto document = Document::create(QSize(40, 30));
  QVERIFY(document);
  document->layerAt(0)->setName(QStringLiteral("Legacy"));
  QVERIFY(NativeDocumentCodec::save(*document, path));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadWrite));
  auto bytes = file.readAll();
  QVERIFY(bytes.size() > 17);
  qToLittleEndian<quint32>(1, reinterpret_cast<uchar*>(bytes.data() + 8));
  bytes.chop(5);  // v2 empty-selection base coverage and tile count.
  QVERIFY(file.resize(0));
  QVERIFY(file.seek(0));
  QCOMPARE(file.write(bytes), bytes.size());
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->size(), QSize(40, 30));
  QCOMPARE(loaded.document->layerAt(0)->name(), QStringLiteral("Legacy"));
  QVERIFY(loaded.document->selection().isEmpty());
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

void NativeDocumentCodecTest::rejectsNonFiniteLayerOpacity() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("non-finite.chromarchy"));
  auto document = Document::create(QSize(10, 10));
  QVERIFY(document);
  QVERIFY(NativeDocumentCodec::save(*document, path));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadWrite));
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  char magic[8]{};
  QCOMPARE(stream.readRawData(magic, sizeof(magic)), sizeof(magic));
  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  stream >> version >> width >> height >> layerCount >> activeLayer;
  quint32 idSize = 0;
  stream >> idSize;
  QCOMPARE(stream.skipRawData(idSize), static_cast<int>(idSize));
  quint32 nameSize = 0;
  stream >> nameSize;
  QCOMPARE(stream.skipRawData(nameSize), static_cast<int>(nameSize));
  quint8 visible = 0;
  quint8 locked = 0;
  stream >> visible >> locked;
  const auto opacityOffset = file.pos();
  QVERIFY(opacityOffset > 0);
  QVERIFY(file.seek(opacityOffset));
  QDataStream writer(&file);
  writer.setVersion(QDataStream::Qt_6_6);
  writer.setByteOrder(QDataStream::LittleEndian);
  writer << std::numeric_limits<double>::quiet_NaN();
  QCOMPARE(writer.status(), QDataStream::Ok);
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY(!loaded);
  QVERIFY(loaded.error.contains(QStringLiteral("layer properties")));
}

QTEST_APPLESS_MAIN(NativeDocumentCodecTest)

#include "NativeDocumentCodecTest.moc"
