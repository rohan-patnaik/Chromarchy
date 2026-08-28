#include "core/Document.h"
#include "ui/CanvasWidget.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>

using chromarchy::CanvasWidget;
using chromarchy::Document;

class CanvasWidgetTest final : public QObject {
  Q_OBJECT

private slots:
  void clampsZoomAndTracksVisibleRegion();
  void visibleCompositeStaysBoundedByViewport();
  void requestsRectangleSelectionFromCanvasDrag();
  void minimumZoomLargeSparsePaintIsViewportBounded();
  void rendersIndependentQuarterTurnWithoutChangingDocument();
  void rotationAndZoomPreserveDocumentCenter();
  void keepsRepeatedRotationAndHugeSparsePaintBounded();
  void rendersIndependentPixelGridContract();
  void keepsHugeSparsePixelGridViewportBounded();
  void fitsCanvasWithinViewportAcrossRotationAndBounds();
};

void CanvasWidgetTest::clampsZoomAndTracksVisibleRegion() {
  auto document = Document::create(QSize(1000, 800));
  QVERIFY(document);
  CanvasWidget canvas(&*document);
  canvas.resize(400, 300);
  canvas.show();
  QTest::qWait(1);

  canvas.setZoom(100.0);
  QCOMPARE(canvas.zoom(), 32.0);
  canvas.setZoom(0.0001);
  QCOMPARE(canvas.zoom(), 0.01);
  QCOMPARE(canvas.visibleDocumentRect(), QRect(0, 0, 1000, 800));
}

void CanvasWidgetTest::visibleCompositeStaysBoundedByViewport() {
  auto document = Document::create(QSize(20'000, 12'000));
  QVERIFY(document);
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  QTest::qWait(1);
  canvas.setZoom(1.0);

  const auto visible = canvas.visibleDocumentRect();
  QVERIFY(visible.width() <= canvas.viewport()->width() + 1);
  QVERIFY(visible.height() <= canvas.viewport()->height() + 1);
  QVERIFY(canvas.horizontalScrollBar()->maximum() > 0);
  QVERIFY(canvas.verticalScrollBar()->maximum() > 0);
}

void CanvasWidgetTest::requestsRectangleSelectionFromCanvasDrag() {
  auto document = Document::create(QSize(200, 100));
  QVERIFY(document);
  CanvasWidget canvas(&*document);
  canvas.resize(400, 300);
  canvas.show();
  QTest::qWait(1);
  QSignalSpy selections(&canvas, &CanvasWidget::selectionRequested);

  const auto origin = canvas.viewport()->rect().center() - QPoint(100, 50);
  QTest::mousePress(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                    origin + QPoint(10, 20));
  QTest::mouseMove(canvas.viewport(), origin + QPoint(30, 40));
  QTest::mouseRelease(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                      origin + QPoint(30, 40));

  QCOMPARE(selections.size(), 1);
  const auto selection = selections.takeFirst().at(0).toRect();
  QCOMPARE(selection.size(), QSize(21, 21));
  QVERIFY(qAbs(selection.x() - 10) <= 1);
  QVERIFY(qAbs(selection.y() - 20) <= 1);
}

void CanvasWidgetTest::minimumZoomLargeSparsePaintIsViewportBounded() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(1000, 1000),
                                               Qt::red));
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(0.01);
  QTest::qWait(1);
  QVERIFY(static_cast<quint64>(canvas.visibleDocumentRect().width()) *
              canvas.visibleDocumentRect().height() >
          1'000'000'000ULL);

  QImage frame(canvas.size(), QImage::Format_RGBA8888_Premultiplied);
  frame.fill(Qt::transparent);
  canvas.render(&frame);
  QCOMPARE(frame.size(), canvas.size());
}

void CanvasWidgetTest::rendersIndependentQuarterTurnWithoutChangingDocument() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/view-rotation-quarter-turn.json"));
  QVERIFY(fixture.open(QIODevice::ReadOnly));
  QJsonParseError parseError;
  const auto root =
      QJsonDocument::fromJson(fixture.readAll(), &parseError).object();
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
  const auto sourceSize = root.value(QStringLiteral("sourceSize")).toArray();
  QCOMPARE(sourceSize.size(), 2);
  auto document =
      Document::create(QSize(sourceSize.at(0).toInt(), sourceSize.at(1).toInt()));
  QVERIFY(document);
  const auto sourceRows = root.value(QStringLiteral("sourceRows")).toArray();
  for (int y = 0; y < sourceRows.size(); ++y) {
    const auto row = sourceRows.at(y).toArray();
    for (int x = 0; x < row.size(); ++x) {
      QVERIFY(document->layerAt(0)->setPixelColor(
          QPoint(x, y), QColor(row.at(x).toString())));
    }
  }
  const auto originalBlocks = document->storageBlocks();
  const auto originalComposite = document->composite();

  CanvasWidget canvas(&*document);
  canvas.resize(120, 100);
  canvas.show();
  canvas.setZoom(12.0);
  QSignalSpy rotations(&canvas, &CanvasWidget::rotationChanged);
  QSignalSpy selections(&canvas, &CanvasWidget::selectionRequested);
  canvas.rotateClockwise();
  QCOMPARE(canvas.rotationDegreesClockwise(), 90);
  QCOMPARE(rotations.size(), 1);
  QCOMPARE(rotations.constFirst().at(0).toInt(), 90);
  QVERIFY(canvas.accessibleDescription().contains(QStringLiteral("90 degrees")));

  QImage frame(canvas.viewport()->size(),
               QImage::Format_RGBA8888_Premultiplied);
  frame.fill(Qt::transparent);
  canvas.viewport()->render(&frame);
  const auto clockwiseRows =
      root.value(QStringLiteral("clockwiseRows")).toArray();
  const QSize rotatedSize(clockwiseRows.at(0).toArray().size(),
                          clockwiseRows.size());
  const QPointF origin(
      (canvas.viewport()->width() - rotatedSize.width() * canvas.zoom()) / 2.0,
      (canvas.viewport()->height() - rotatedSize.height() * canvas.zoom()) /
          2.0);
  for (int y = 0; y < clockwiseRows.size(); ++y) {
    const auto row = clockwiseRows.at(y).toArray();
    for (int x = 0; x < row.size(); ++x) {
      const QPoint sample(
          qFloor(origin.x() + (x + 0.5) * canvas.zoom()),
          qFloor(origin.y() + (y + 0.5) * canvas.zoom()));
      QCOMPARE(frame.pixelColor(sample), QColor(row.at(x).toString()));
    }
  }

  const auto viewportPoint = [&](QPointF rotatedPixelCenter) {
    return QPoint(qRound(origin.x() + rotatedPixelCenter.x() * canvas.zoom()),
                  qRound(origin.y() +
                         rotatedPixelCenter.y() * canvas.zoom()));
  };
  QTest::mousePress(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                    viewportPoint(QPointF(2.5, 0.5)));
  QTest::mouseMove(canvas.viewport(), viewportPoint(QPointF(0.5, 0.5)));
  QTest::mouseRelease(canvas.viewport(), Qt::LeftButton, Qt::NoModifier,
                      viewportPoint(QPointF(0.5, 0.5)));
  QCOMPARE(selections.size(), 1);
  QCOMPARE(selections.takeFirst().at(0).toRect(), QRect(0, 0, 1, 3));
  QCOMPARE(document->storageBlocks(), originalBlocks);
  QCOMPARE(document->composite(), originalComposite);
  QCOMPARE(document->size(), QSize(2, 3));
}

void CanvasWidgetTest::rotationAndZoomPreserveDocumentCenter() {
  auto document = Document::create(QSize(2000, 1200));
  QVERIFY(document);
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(1.0);
  canvas.horizontalScrollBar()->setValue(700);
  canvas.verticalScrollBar()->setValue(350);
  const auto originalCenter = canvas.visibleDocumentRect().center();

  canvas.rotateClockwise();
  const auto rotatedCenter = canvas.visibleDocumentRect().center();
  QVERIFY(qAbs(rotatedCenter.x() - originalCenter.x()) <= 1);
  QVERIFY(qAbs(rotatedCenter.y() - originalCenter.y()) <= 1);
  canvas.setZoom(2.0);
  const auto zoomedCenter = canvas.visibleDocumentRect().center();
  QVERIFY(qAbs(zoomedCenter.x() - originalCenter.x()) <= 1);
  QVERIFY(qAbs(zoomedCenter.y() - originalCenter.y()) <= 1);
  canvas.rotateCounterclockwise();
  const auto resetCenter = canvas.visibleDocumentRect().center();
  QVERIFY(qAbs(resetCenter.x() - originalCenter.x()) <= 1);
  QVERIFY(qAbs(resetCenter.y() - originalCenter.y()) <= 1);
}

void CanvasWidgetTest::keepsRepeatedRotationAndHugeSparsePaintBounded() {
  auto document = Document::create(QSize(300'000, 200'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(299'999, 199'999),
                                               Qt::red));
  const auto originalBlocks = document->storageBlocks();
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(0.01);
  for (int iteration = 0; iteration < 1025; ++iteration) {
    canvas.rotateClockwise();
  }
  QCOMPARE(canvas.rotationDegreesClockwise(), 90);
  const auto visible = canvas.visibleDocumentRect();
  QVERIFY(!visible.isEmpty());
  QVERIFY(visible.width() <= qCeil(canvas.viewport()->height() / canvas.zoom()) +
                                 1);
  QVERIFY(visible.height() <= qCeil(canvas.viewport()->width() / canvas.zoom()) +
                                  1);
  QImage frame(canvas.viewport()->size(),
               QImage::Format_RGBA8888_Premultiplied);
  frame.fill(Qt::transparent);
  canvas.viewport()->render(&frame);
  QCOMPARE(frame.size(), canvas.viewport()->size());
  QCOMPARE(document->storageBlocks(), originalBlocks);
  QCOMPARE(document->size(), QSize(300'000, 200'000));
  canvas.resetRotation();
  QCOMPARE(canvas.rotationDegreesClockwise(), 0);
  QCOMPARE(document->storageBlocks(), originalBlocks);
}

void CanvasWidgetTest::rendersIndependentPixelGridContract() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/pixel-grid-contract.json"));
  QVERIFY(fixture.open(QIODevice::ReadOnly));
  QJsonParseError parseError;
  const auto root =
      QJsonDocument::fromJson(fixture.readAll(), &parseError).object();
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
  QCOMPARE(root.value(QStringLiteral("minimumZoom")).toDouble(),
           CanvasWidget::pixelGridMinimumZoom);
  QCOMPARE(root.value(QStringLiteral("lineOpacity")).toInt(),
           CanvasWidget::pixelGridOpacity);

  const auto size = root.value(QStringLiteral("documentSize")).toArray();
  auto document = Document::create(QSize(size.at(0).toInt(), size.at(1).toInt()));
  QVERIFY(document);
  const QColor pixelColor(
      root.value(QStringLiteral("documentPixelColor")).toString());
  for (int y = 0; y < document->size().height(); ++y) {
    for (int x = 0; x < document->size().width(); ++x) {
      QVERIFY(document->layerAt(0)->setPixelColor(QPoint(x, y), pixelColor));
    }
  }
  const auto originalBlocks = document->storageBlocks();
  const auto originalComposite = document->composite();

  CanvasWidget canvas(&*document);
  canvas.resize(180, 140);
  canvas.show();
  canvas.setPixelGridEnabled(true);
  QCOMPARE(canvas.pixelGridEnabled(), true);
  QCOMPARE(canvas.pixelGridVisible(), false);
  QVERIFY(canvas.accessibleDescription().contains(
      QStringLiteral("visible from 800% zoom")));
  QImage hiddenFrame(canvas.viewport()->size(),
                     QImage::Format_RGBA8888_Premultiplied);
  hiddenFrame.fill(Qt::transparent);
  canvas.viewport()->render(&hiddenFrame);
  canvas.setPixelGridEnabled(false);
  QImage disabledFrame(canvas.viewport()->size(),
                       QImage::Format_RGBA8888_Premultiplied);
  disabledFrame.fill(Qt::transparent);
  canvas.viewport()->render(&disabledFrame);
  QCOMPARE(hiddenFrame, disabledFrame);
  canvas.setPixelGridEnabled(true);
  canvas.setZoom(root.value(QStringLiteral("renderZoom")).toDouble());
  QCOMPARE(canvas.pixelGridVisible(), true);
  QVERIFY(canvas.accessibleDescription().contains(
      QStringLiteral("enabled and visible")));

  QImage gridFrame(canvas.viewport()->size(),
                   QImage::Format_RGBA8888_Premultiplied);
  gridFrame.fill(Qt::transparent);
  canvas.viewport()->render(&gridFrame);
  const QPoint origin(
      qRound((canvas.viewport()->width() -
              document->size().width() * canvas.zoom()) /
             2.0),
      qRound((canvas.viewport()->height() -
              document->size().height() * canvas.zoom()) /
             2.0));
  const QPoint interior =
      origin + QPoint(qRound(canvas.zoom() * 0.5),
                      qRound(canvas.zoom() * 0.5));
  const QPoint boundary =
      origin + QPoint(qRound(canvas.zoom()), qRound(canvas.zoom() * 0.5));
  QCOMPARE(gridFrame.pixelColor(interior), pixelColor);
  QCOMPARE(gridFrame.pixelColor(boundary),
           QColor(root.value(QStringLiteral("expectedBoundaryColor"))
                      .toString()));

  canvas.rotateClockwise();
  QVERIFY(canvas.pixelGridVisible());
  QImage rotatedFrame(canvas.viewport()->size(),
                      QImage::Format_RGBA8888_Premultiplied);
  rotatedFrame.fill(Qt::transparent);
  canvas.viewport()->render(&rotatedFrame);
  const QSize rotatedSize(document->size().height(), document->size().width());
  const QPoint rotatedOrigin(
      qRound((canvas.viewport()->width() -
              rotatedSize.width() * canvas.zoom()) /
             2.0),
      qRound((canvas.viewport()->height() -
              rotatedSize.height() * canvas.zoom()) /
             2.0));
  const QPoint rotatedBoundary =
      rotatedOrigin
      + QPoint(qRound(canvas.zoom()), qRound(canvas.zoom() * 0.5));
  QCOMPARE(rotatedFrame.pixelColor(rotatedBoundary),
           QColor(root.value(QStringLiteral("expectedBoundaryColor"))
                      .toString()));
  QCOMPARE(document->storageBlocks(), originalBlocks);
  QCOMPARE(document->composite(), originalComposite);
}

void CanvasWidgetTest::keepsHugeSparsePixelGridViewportBounded() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(299'999, 299'999),
                                               Qt::red));
  const auto originalBlocks = document->storageBlocks();
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(32.0);
  QSignalSpy changes(&canvas, &CanvasWidget::pixelGridChanged);
  for (int iteration = 0; iteration < 1025; ++iteration) {
    canvas.setPixelGridEnabled(iteration % 2 == 0);
  }
  QCOMPARE(changes.size(), 1025);
  QVERIFY(canvas.pixelGridVisible());
  const auto visible = canvas.visibleDocumentRect();
  QVERIFY(visible.width() <= qCeil(canvas.viewport()->width() / canvas.zoom()) +
                                 1);
  QVERIFY(visible.height() <=
          qCeil(canvas.viewport()->height() / canvas.zoom()) + 1);
  const auto maximumGridLines = visible.width() + visible.height() + 2;
  QVERIFY(maximumGridLines <= 40);
  QImage frame(canvas.viewport()->size(),
               QImage::Format_RGBA8888_Premultiplied);
  frame.fill(Qt::transparent);
  canvas.viewport()->render(&frame);
  QCOMPARE(frame.size(), canvas.viewport()->size());
  QCOMPARE(document->storageBlocks(), originalBlocks);
  QCOMPARE(document->size(), QSize(300'000, 300'000));
}

void CanvasWidgetTest::fitsCanvasWithinViewportAcrossRotationAndBounds() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/fit-view-contract.json"));
  QVERIFY(fixture.open(QIODevice::ReadOnly));
  QJsonParseError parseError;
  const auto root =
      QJsonDocument::fromJson(fixture.readAll(), &parseError).object();
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
  QCOMPARE(root.value(QStringLiteral("minimumZoom")).toDouble(),
           CanvasWidget::minimumZoom);
  QCOMPARE(root.value(QStringLiteral("maximumZoom")).toDouble(),
           CanvasWidget::maximumZoom);
  const auto documentSize = root.value(QStringLiteral("documentSize")).toArray();
  const auto widgetSize = root.value(QStringLiteral("widgetSize")).toArray();
  auto document = Document::create(
      QSize(documentSize.at(0).toInt(), documentSize.at(1).toInt()));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(399, 199), Qt::blue));
  const auto originalBlocks = document->storageBlocks();
  CanvasWidget canvas(&*document);
  canvas.resize(widgetSize.at(0).toInt(), widgetSize.at(1).toInt());
  canvas.show();
  canvas.setZoom(4.0);
  QSignalSpy zooms(&canvas, &CanvasWidget::zoomChanged);
  const double expectedUnrotated = qMin(
      static_cast<double>(canvas.viewport()->width()) / document->size().width(),
      static_cast<double>(canvas.viewport()->height()) /
          document->size().height());
  QCOMPARE(root.value(QStringLiteral("unrotatedLimitingAxis")).toString(),
           QStringLiteral("width"));
  QVERIFY(static_cast<double>(canvas.viewport()->width()) /
              document->size().width() <=
          static_cast<double>(canvas.viewport()->height()) /
              document->size().height());
  canvas.fitToViewport();
  QCOMPARE(canvas.zoom(), expectedUnrotated);
  QCOMPARE(canvas.visibleDocumentRect(), QRect(QPoint(), document->size()));
  QVERIFY(canvas.accessibleDescription().contains(QStringLiteral("View zoom")));

  canvas.rotateClockwise();
  canvas.setZoom(4.0);
  const double expectedRotated = qMin(
      static_cast<double>(canvas.viewport()->width()) / document->size().height(),
      static_cast<double>(canvas.viewport()->height()) / document->size().width());
  QCOMPARE(root.value(QStringLiteral("rotatedLimitingAxis")).toString(),
           QStringLiteral("height"));
  QVERIFY(static_cast<double>(canvas.viewport()->height()) /
              document->size().width() <=
          static_cast<double>(canvas.viewport()->width()) /
              document->size().height());
  canvas.fitToViewport();
  QCOMPARE(canvas.zoom(), expectedRotated);
  QCOMPARE(canvas.visibleDocumentRect(), QRect(QPoint(), document->size()));
  QVERIFY(zooms.size() >= 3);
  QCOMPARE(document->storageBlocks(), originalBlocks);

  const auto hugeSize = root.value(QStringLiteral("hugeSparseSize")).toArray();
  auto huge = Document::create(
      QSize(hugeSize.at(0).toInt(), hugeSize.at(1).toInt()));
  QVERIFY(huge);
  QVERIFY(huge->layerAt(0)->setPixelColor(QPoint(299'999, 299'999), Qt::red));
  const auto hugeBlocks = huge->storageBlocks();
  CanvasWidget hugeCanvas(&*huge);
  hugeCanvas.resize(640, 480);
  hugeCanvas.show();
  for (int iteration = 0; iteration < 1025; ++iteration) {
    hugeCanvas.fitToViewport();
  }
  QCOMPARE(root.value(QStringLiteral("hugeSparseResult")).toString(),
           QStringLiteral("minimum-zoom-clamped"));
  QCOMPARE(hugeCanvas.zoom(), CanvasWidget::minimumZoom);
  QVERIFY(hugeCanvas.visibleDocumentRect().width() < huge->size().width());
  QVERIFY(hugeCanvas.visibleDocumentRect().height() < huge->size().height());
  QCOMPARE(huge->storageBlocks(), hugeBlocks);
}

QTEST_MAIN(CanvasWidgetTest)

#include "CanvasWidgetTest.moc"
