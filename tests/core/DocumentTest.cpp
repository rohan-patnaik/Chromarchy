#include "core/Document.h"

#include <QTest>

#include <limits>
#include <cmath>

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
  void mergeAndFlattenPreserveComposite();
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
