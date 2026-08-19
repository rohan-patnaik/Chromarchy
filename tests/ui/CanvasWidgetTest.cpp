#include "core/Document.h"
#include "ui/CanvasWidget.h"

#include <QScrollBar>
#include <QTest>

using chromarchy::CanvasWidget;
using chromarchy::Document;

class CanvasWidgetTest final : public QObject {
  Q_OBJECT

private slots:
  void clampsZoomAndTracksVisibleRegion();
  void visibleCompositeStaysBoundedByViewport();
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

QTEST_MAIN(CanvasWidgetTest)

#include "CanvasWidgetTest.moc"
