#include "core/Document.h"
#include "ui/CanvasWidget.h"

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
  QVERIFY(document->layerAt(0)->pixels().setPixelColor(QPoint(1000, 1000),
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

QTEST_MAIN(CanvasWidgetTest)

#include "CanvasWidgetTest.moc"
