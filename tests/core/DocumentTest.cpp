#include "core/Document.h"

#include <QTest>

using chromarchy::Document;

namespace {

bool colorsWithinRounding(QColor left, QColor right) {
  return qAbs(left.red() - right.red()) <= 1 &&
         qAbs(left.green() - right.green()) <= 1 &&
         qAbs(left.blue() - right.blue()) <= 1 &&
         qAbs(left.alpha() - right.alpha()) <= 1;
}

}  // namespace

class DocumentTest final : public QObject {
  Q_OBJECT

private slots:
  void validatesCanvasSize();
  void managesLayerOwnershipAndOrder();
  void duplicateUsesCopyOnWritePixels();
  void compositesVisibilityAndOpacity();
  void mergeAndFlattenPreserveComposite();
  void locksPreventDestructiveLayerOperations();
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
  QVERIFY(document->layerAt(0)->pixels().setPixelColor(QPoint(2, 3), Qt::red));
  const auto originalId = document->layerAt(0)->id();

  QVERIFY(document->duplicateLayer(0));
  QCOMPARE(document->layerCount(), 2);
  QVERIFY(document->layerAt(1)->id() != originalId);
  QCOMPARE(document->layerAt(1)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
  QVERIFY(document->layerAt(1)->pixels().setPixelColor(QPoint(2, 3), Qt::blue));
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
}

void DocumentTest::compositesVisibilityAndOpacity() {
  auto document = Document::create(QSize(8, 8));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->pixels().setPixelColor(QPoint(1, 1), Qt::red));
  const auto top = document->addLayer(QStringLiteral("Top"));
  QVERIFY(document->layerAt(top)->pixels().setPixelColor(QPoint(1, 1), Qt::blue));

  QCOMPARE(document->composite().pixelColor(QPoint(1, 1)), QColor(Qt::blue));
  document->layerAt(top)->setVisible(false);
  QCOMPARE(document->composite().pixelColor(QPoint(1, 1)), QColor(Qt::red));
  document->layerAt(top)->setVisible(true);
  document->layerAt(top)->setOpacity(0.5);
  const auto blended = document->composite().pixelColor(QPoint(1, 1));
  QVERIFY(blended.red() >= 126 && blended.red() <= 129);
  QVERIFY(blended.blue() >= 126 && blended.blue() <= 129);
}

void DocumentTest::mergeAndFlattenPreserveComposite() {
  auto document = Document::create(QSize(512, 512));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->pixels().setPixelColor(QPoint(300, 300),
                                                       Qt::red));
  const auto middle = document->addLayer(QStringLiteral("Middle"));
  QVERIFY(document->layerAt(middle)->pixels().setPixelColor(QPoint(300, 300),
                                                            Qt::green));
  document->layerAt(middle)->setOpacity(0.4);
  const auto top = document->addLayer(QStringLiteral("Top"));
  QVERIFY(document->layerAt(top)->pixels().setPixelColor(QPoint(300, 300),
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

QTEST_APPLESS_MAIN(DocumentTest)

#include "DocumentTest.moc"
