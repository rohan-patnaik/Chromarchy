#include "core/Document.h"
#include "core/NativeDocumentCodec.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <limits>

using chromarchy::Document;

namespace {

bool colorsWithinRounding(QColor left, QColor right) {
  return qAbs(left.red() - right.red()) <= 1 &&
         qAbs(left.green() - right.green()) <= 1 &&
         qAbs(left.blue() - right.blue()) <= 1 &&
         qAbs(left.alpha() - right.alpha()) <= 1;
}

QColor sourceOverReference(QColor destination, QColor source,
                           double layerOpacity) {
  const double sourceAlpha = source.alphaF() * layerOpacity;
  const double destinationAlpha = destination.alphaF();
  const double outputAlpha = sourceAlpha + destinationAlpha * (1.0 - sourceAlpha);
  if (outputAlpha == 0.0) {
    return Qt::transparent;
  }
  const auto channel = [&](double destinationChannel, double sourceChannel) {
    return (sourceChannel * sourceAlpha +
            destinationChannel * destinationAlpha * (1.0 - sourceAlpha)) /
           outputAlpha;
  };
  return QColor::fromRgbF(channel(destination.redF(), source.redF()),
                          channel(destination.greenF(), source.greenF()),
                          channel(destination.blueF(), source.blueF()),
                          outputAlpha);
}

}  // namespace

class DocumentTest final : public QObject {
  Q_OBJECT

private slots:
  void validatesCanvasSize();
  void managesLayerOwnershipAndOrder();
  void duplicateUsesCopyOnWritePixels();
  void compositesVisibilityAndOpacity();
  void matchesOriginalSourceOverReferenceVectors();
  void fullImageCompositeIsRepeatableAcrossTileBoundary();
  void matchesIndependentCompositeGoldenAfterNativeRoundTrip();
  void mergeAndFlattenPreserveComposite();
  void mergeAndFlattenElideTransparentTiles();
  void locksPreventDestructiveLayerOperations();
  void rejectsInvalidLayerState();
};

void DocumentTest::validatesCanvasSize() {
  QVERIFY(!Document::create(QSize()));
  QVERIFY(!Document::create(QSize(Document::maximumDimension + 1, 10)));

  auto document = Document::create(QSize(800, 600));
  QVERIFY(document);
  QCOMPARE(document->size(), QSize(800, 600));
  QCOMPARE(document->layerCount(), 1);
  QCOMPARE(document->activeLayerIndex(), 0);
}

void DocumentTest::managesLayerOwnershipAndOrder() {
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  QCOMPARE(document->addLayer(QStringLiteral("Ink")), 1);
  QCOMPARE(document->addLayer(QStringLiteral("Highlights")), 2);
  QVERIFY(document->moveLayer(2, 0));
  QCOMPARE(document->layerAt(0)->name(), QStringLiteral("Highlights"));
  QCOMPARE(document->activeLayerIndex(), 0);
  QVERIFY(document->removeLayer(1));
  QVERIFY(document->removeLayer(1));
  QVERIFY(!document->removeLayer(0));
  QVERIFY(document->layerAt(-1) == nullptr);
}

void DocumentTest::duplicateUsesCopyOnWritePixels() {
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(2, 3), Qt::red));
  const auto originalId = document->layerAt(0)->id();

  QVERIFY(document->duplicateLayer(0));
  QCOMPARE(document->layerCount(), 2);
  QVERIFY(document->layerAt(1)->id() != originalId);
  QCOMPARE(document->layerAt(1)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
  QVERIFY(document->layerAt(1)->setPixelColor(QPoint(2, 3), Qt::blue));
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
}

void DocumentTest::compositesVisibilityAndOpacity() {
  auto document = Document::create(QSize(8, 8));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(1, 1), Qt::red));
  const auto top = document->addLayer(QStringLiteral("Top"));
  QVERIFY(document->layerAt(top)->setPixelColor(QPoint(1, 1), Qt::blue));

  QCOMPARE(document->composite().pixelColor(QPoint(1, 1)), QColor(Qt::blue));
  document->layerAt(top)->setVisible(false);
  QCOMPARE(document->composite().pixelColor(QPoint(1, 1)), QColor(Qt::red));
  document->layerAt(top)->setVisible(true);
  document->layerAt(top)->setOpacity(0.5);
  const auto blended = document->composite().pixelColor(QPoint(1, 1));
  QVERIFY(blended.red() >= 126 && blended.red() <= 129);
  QVERIFY(blended.blue() >= 126 && blended.blue() <= 129);
}

void DocumentTest::matchesOriginalSourceOverReferenceVectors() {
  struct Vector final {
    QColor destination;
    QColor source;
    double opacity;
  };
  const QVector<Vector> vectors{
      {QColor(10, 20, 30, 0), QColor(200, 100, 50, 0), 1.0},
      {QColor(20, 80, 160, 255), QColor(240, 40, 10, 128), 1.0},
      {QColor(20, 80, 160, 96), QColor(240, 40, 10, 192), 0.375},
      {QColor(255, 255, 255, 255), QColor(0, 0, 0, 255), 0.0},
      {QColor(4, 9, 250, 1), QColor(252, 17, 2, 254), 0.999},
  };

  for (const auto& vector : vectors) {
    auto document = Document::create(QSize(1, 1));
    QVERIFY(document);
    QVERIFY(document->layerAt(0)->setPixelColor(QPoint(), vector.destination) ||
            vector.destination.alpha() == 0);
    const auto top = document->addLayer(QStringLiteral("Source"));
    QVERIFY(document->layerAt(top)->setPixelColor(QPoint(), vector.source) ||
            vector.source.alpha() == 0);
    QVERIFY(document->layerAt(top)->setOpacity(vector.opacity) ||
            qFuzzyCompare(document->layerAt(top)->opacity(), vector.opacity));
    const auto actual = document->composite().pixelColor(QPoint());
    const auto expected = sourceOverReference(vector.destination, vector.source,
                                              vector.opacity);
    QVERIFY2(colorsWithinRounding(actual, expected),
             qPrintable(QStringLiteral("actual=%1 expected=%2")
                            .arg(actual.name(QColor::HexArgb),
                                 expected.name(QColor::HexArgb))));
  }
}

void DocumentTest::fullImageCompositeIsRepeatableAcrossTileBoundary() {
  auto document = Document::create(QSize(258, 3));
  QVERIFY(document);
  const QVector<QPoint> positions{{0, 0}, {255, 1}, {256, 1}, {257, 2}};
  for (const auto& position : positions) {
    QVERIFY(document->layerAt(0)->setPixelColor(position,
                                                QColor(20, 80, 160, 96)));
  }
  const auto top = document->addLayer(QStringLiteral("Source"));
  for (const auto& position : positions) {
    QVERIFY(document->layerAt(top)->setPixelColor(position,
                                                  QColor(240, 40, 10, 192)));
  }
  QVERIFY(document->layerAt(top)->setOpacity(0.375));

  const auto first = document->composite();
  const auto second = document->composite();
  QCOMPARE(first, second);
  const auto expected = sourceOverReference(QColor(20, 80, 160, 96),
                                            QColor(240, 40, 10, 192), 0.375);
  for (const auto& position : positions) {
    QVERIFY(colorsWithinRounding(first.pixelColor(position), expected));
  }

  QVERIFY(document->moveLayer(top, 0));
  const auto reversed = document->composite();
  QVERIFY(reversed != first);
}

void DocumentTest::matchesIndependentCompositeGoldenAfterNativeRoundTrip() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/composite-source-over-golden.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  QJsonParseError parseError;
  const auto fixtureDocument =
      QJsonDocument::fromJson(fixture.readAll(), &parseError);
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QVERIFY(fixtureDocument.isObject());
  const auto root = fixtureDocument.object();
  QCOMPARE(root["schemaVersion"].toInt(), 1);

  const auto sizeValues = root["size"].toArray();
  QCOMPARE(sizeValues.size(), 2);
  const QSize size(sizeValues[0].toInt(), sizeValues[1].toInt());
  QCOMPARE(size, QSize(257, 2));
  auto document = Document::create(size);
  QVERIFY(document);

  const auto layers = root["layers"].toArray();
  QVERIFY(!layers.isEmpty() && layers.size() <= 8);
  for (qsizetype layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
    const auto layerObject = layers[layerIndex].toObject();
    const auto documentIndex =
        layerIndex == 0
            ? 0
            : document->addLayer(layerObject["name"].toString());
    auto* layer = document->layerAt(documentIndex);
    QVERIFY(layer);
    if (layerIndex == 0) {
      layer->setName(layerObject["name"].toString());
    }
    layer->setVisible(layerObject["visible"].toBool());
    QVERIFY(layer->setOpacity(layerObject["opacity"].toDouble()) ||
            layer->opacity() == layerObject["opacity"].toDouble());

    const auto pixels = layerObject["pixels"].toArray();
    QVERIFY(pixels.size() <= 64);
    QSet<QPoint> positions;
    for (const auto pixelValue : pixels) {
      const auto pixel = pixelValue.toArray();
      QCOMPARE(pixel.size(), 6);
      const QPoint position(pixel[0].toInt(-1), pixel[1].toInt(-1));
      QVERIFY(QRect(QPoint(), size).contains(position));
      QVERIFY(!positions.contains(position));
      positions.insert(position);
      for (int channel = 2; channel < 6; ++channel) {
        QVERIFY(pixel[channel].isDouble());
        QVERIFY(pixel[channel].toInt(-1) >= 0 &&
                pixel[channel].toInt(-1) <= 255);
      }
      QVERIFY(layer->setPixelColor(
          position, QColor(pixel[2].toInt(), pixel[3].toInt(),
                           pixel[4].toInt(), pixel[5].toInt())));
    }
  }

  const auto defaultPixel = root["expectedDefaultRgba8"].toArray();
  QCOMPARE(defaultPixel.size(), 4);
  for (const auto channel : defaultPixel) {
    QVERIFY(channel.isDouble());
    QVERIFY(channel.toInt(-1) >= 0 && channel.toInt(-1) <= 255);
  }
  QImage expected(size, QImage::Format_RGBA8888);
  expected.fill(QColor(defaultPixel[0].toInt(), defaultPixel[1].toInt(),
                       defaultPixel[2].toInt(), defaultPixel[3].toInt()));
  const auto expectedPixels = root["expectedPixels"].toArray();
  QVERIFY(expectedPixels.size() <= 64);
  QSet<QPoint> expectedPositions;
  for (const auto pixelValue : expectedPixels) {
    const auto pixel = pixelValue.toArray();
    QCOMPARE(pixel.size(), 6);
    const QPoint position(pixel[0].toInt(-1), pixel[1].toInt(-1));
    QVERIFY(QRect(QPoint(), size).contains(position));
    QVERIFY(!expectedPositions.contains(position));
    expectedPositions.insert(position);
    for (int channel = 2; channel < 6; ++channel) {
      QVERIFY(pixel[channel].isDouble());
      QVERIFY(pixel[channel].toInt(-1) >= 0 &&
              pixel[channel].toInt(-1) <= 255);
    }
    expected.setPixelColor(position, QColor(pixel[2].toInt(), pixel[3].toInt(),
                                            pixel[4].toInt(),
                                            pixel[5].toInt()));
  }
  const QByteArray expectedBytes(
      reinterpret_cast<const char*>(expected.constBits()),
      static_cast<qsizetype>(expected.sizeInBytes()));
  QCOMPARE(QCryptographicHash::hash(expectedBytes, QCryptographicHash::Sha256)
               .toHex(),
           root["expectedRgba8Sha256"].toString().toLatin1());

  const auto verifyGolden = [&](const QImage& actual,
                                const QImage& reference) {
    QCOMPARE(actual.size(), reference.size());
    for (int y = 0; y < reference.height(); ++y) {
      for (int x = 0; x < reference.width(); ++x) {
        const auto actualColor = actual.pixelColor(x, y);
        const auto expectedColor = reference.pixelColor(x, y);
        const auto withinCompoundedRounding =
            qAbs(actualColor.red() - expectedColor.red()) <= 2 &&
            qAbs(actualColor.green() - expectedColor.green()) <= 2 &&
            qAbs(actualColor.blue() - expectedColor.blue()) <= 2 &&
            qAbs(actualColor.alpha() - expectedColor.alpha()) <= 2;
        QVERIFY2(withinCompoundedRounding,
                 qPrintable(QStringLiteral("at %1,%2 actual=%3 expected=%4")
                                .arg(x)
                                .arg(y)
                                .arg(actualColor.name(QColor::HexArgb),
                                     expectedColor.name(QColor::HexArgb))));
      }
    }
  };

  const auto first = document->composite();
  QCOMPARE(document->composite(), first);
  verifyGolden(first, expected);

  const QRect boundaryRegion(254, 0, 3, 2);
  const auto region = document->composite(boundaryRegion);
  verifyGolden(region, expected.copy(boundaryRegion));

  QImage painted(size, QImage::Format_RGBA8888_Premultiplied);
  painted.fill(Qt::transparent);
  QPainter painter(&painted);
  document->paintComposite(painter, QRect(QPoint(), size));
  painter.end();
  QCOMPARE(painted, first);

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto nativePath =
      directory.filePath(QStringLiteral("reference.chromarchy"));
  const auto saved =
      chromarchy::NativeDocumentCodec::save(*document, nativePath);
  QVERIFY2(saved, qPrintable(saved.error));
  QCOMPARE(document->composite(), first);
  const auto reopened = chromarchy::NativeDocumentCodec::load(nativePath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->composite(), first);
  verifyGolden(reopened.document->composite(), expected);

  auto flattened = *reopened.document;
  QVERIFY(flattened.flatten());
  QCOMPARE(flattened.composite(), first);
  const auto flattenedPath =
      directory.filePath(QStringLiteral("flattened.chromarchy"));
  const auto flattenedSave =
      chromarchy::NativeDocumentCodec::save(flattened, flattenedPath);
  QVERIFY2(flattenedSave, qPrintable(flattenedSave.error));
  const auto reopenedFlattened =
      chromarchy::NativeDocumentCodec::load(flattenedPath);
  QVERIFY2(reopenedFlattened, qPrintable(reopenedFlattened.error));
  QCOMPARE(reopenedFlattened.document->composite(), first);

  auto merged = *reopened.document;
  while (merged.layerCount() > 1) {
    QVERIFY(merged.mergeLayerDown(static_cast<int>(merged.layerCount() - 1)));
    verifyGolden(merged.composite(), expected);
  }
  const auto mergedComposite = merged.composite();
  const auto mergedPath =
      directory.filePath(QStringLiteral("merged.chromarchy"));
  const auto mergedSave =
      chromarchy::NativeDocumentCodec::save(merged, mergedPath);
  QVERIFY2(mergedSave, qPrintable(mergedSave.error));
  const auto reopenedMerged = chromarchy::NativeDocumentCodec::load(mergedPath);
  QVERIFY2(reopenedMerged, qPrintable(reopenedMerged.error));
  QCOMPARE(reopenedMerged.document->composite(), mergedComposite);
}

void DocumentTest::mergeAndFlattenPreserveComposite() {
  auto document = Document::create(QSize(512, 512));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(300, 300),
                                               Qt::red));
  const auto middle = document->addLayer(QStringLiteral("Middle"));
  QVERIFY(document->layerAt(middle)->setPixelColor(QPoint(300, 300),
                                                    Qt::green));
  document->layerAt(middle)->setOpacity(0.4);
  const auto top = document->addLayer(QStringLiteral("Top"));
  QVERIFY(document->layerAt(top)->setPixelColor(QPoint(300, 300),
                                                 Qt::blue));
  document->layerAt(top)->setOpacity(0.25);
  const auto beforeMerge = document->composite().pixelColor(QPoint(300, 300));

  QVERIFY(document->mergeLayerDown(top));
  QCOMPARE(document->layerCount(), 2);
  QVERIFY(colorsWithinRounding(
      document->composite().pixelColor(QPoint(300, 300)), beforeMerge));
  const auto beforeFlatten = document->composite().pixelColor(QPoint(300, 300));
  QVERIFY(document->flatten());
  QCOMPARE(document->layerCount(), 1);
  QCOMPARE(document->layerAt(0)->name(), QStringLiteral("Flattened"));
  QVERIFY(colorsWithinRounding(
      document->composite().pixelColor(QPoint(300, 300)), beforeFlatten));
}

void DocumentTest::mergeAndFlattenElideTransparentTiles() {
  auto mergeDocument = Document::create(QSize(512, 512));
  QVERIFY(mergeDocument);
  QVERIFY(mergeDocument->layerAt(0)->setPixelColor(QPoint(300, 300), Qt::red));
  mergeDocument->layerAt(0)->setOpacity(0.0);
  const auto top = mergeDocument->addLayer(QStringLiteral("Top"));
  QVERIFY(mergeDocument->layerAt(top)->setPixelColor(QPoint(4, 5), Qt::blue));
  mergeDocument->layerAt(top)->setOpacity(0.0);

  QVERIFY(mergeDocument->mergeLayerDown(top));
  QCOMPARE(mergeDocument->layerCount(), 1);
  QCOMPARE(mergeDocument->layerAt(0)->pixels().allocatedTileCount(), 0);
  QCOMPARE(mergeDocument->composite().pixelColor(QPoint(300, 300)),
           QColor(Qt::transparent));

  auto flattenDocument = Document::create(QSize(512, 512));
  QVERIFY(flattenDocument);
  QVERIFY(flattenDocument->layerAt(0)->setPixelColor(QPoint(300, 300),
                                                     Qt::green));
  flattenDocument->layerAt(0)->setOpacity(0.0);
  const auto second = flattenDocument->addLayer(QStringLiteral("Second"));
  QVERIFY(flattenDocument->layerAt(second)->setPixelColor(QPoint(4, 5),
                                                           Qt::yellow));
  flattenDocument->layerAt(second)->setOpacity(0.0);

  QVERIFY(flattenDocument->flatten());
  QCOMPARE(flattenDocument->layerCount(), 1);
  QCOMPARE(flattenDocument->layerAt(0)->pixels().allocatedTileCount(), 0);
  QCOMPARE(flattenDocument->composite().pixelColor(QPoint(4, 5)),
           QColor(Qt::transparent));
}

void DocumentTest::locksPreventDestructiveLayerOperations() {
  auto document = Document::create(QSize(32, 32));
  QVERIFY(document);
  document->addLayer(QStringLiteral("Locked"));
  document->layerAt(1)->setLocked(true);
  QVERIFY(!document->mergeLayerDown(1));
  QVERIFY(!document->flatten());
  QCOMPARE(document->layerCount(), 2);
}

void DocumentTest::rejectsInvalidLayerState() {
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  auto* layer = document->layerAt(0);
  QVERIFY(layer->setOpacity(0.5));
  QVERIFY(!layer->setOpacity(std::numeric_limits<double>::quiet_NaN()));
  QVERIFY(!layer->setOpacity(std::numeric_limits<double>::infinity()));
  QCOMPARE(layer->opacity(), 0.5);

  QVERIFY(!layer->replacePixels(chromarchy::TiledImage(QSize(32, 32))));
  QCOMPARE(layer->pixels().size(), QSize(64, 64));
  QVERIFY(layer->replacePixels(chromarchy::TiledImage(QSize(64, 64))));
}

QTEST_APPLESS_MAIN(DocumentTest)

#include "DocumentTest.moc"
