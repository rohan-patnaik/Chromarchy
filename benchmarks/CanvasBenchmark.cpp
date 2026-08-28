#include "core/Document.h"
#include "ui/CanvasWidget.h"

#include <QImage>
#include <QTest>

using chromarchy::CanvasWidget;
using chromarchy::Document;

class CanvasBenchmark final : public QObject {
  Q_OBJECT

private slots:
  void paintSparseDocumentAtMinimumZoom();
  void paintRotatedSparseDocumentAtMinimumZoom();
};

void CanvasBenchmark::paintSparseDocumentAtMinimumZoom() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(1000, 1000),
                                               Qt::red));
  QVERIFY(document->layerAt(0)->setPixelColor(
      QPoint(299'000, 299'000), Qt::blue));
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(0.01);
  QTest::qWait(1);
  QImage frame(canvas.size(), QImage::Format_RGBA8888_Premultiplied);

  QBENCHMARK {
    frame.fill(Qt::transparent);
    canvas.render(&frame);
  }
}

void CanvasBenchmark::paintRotatedSparseDocumentAtMinimumZoom() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(1000, 299'000),
                                               Qt::red));
  QVERIFY(document->layerAt(0)->setPixelColor(
      QPoint(299'000, 1000), Qt::blue));
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(0.01);
  canvas.rotateClockwise();
  QTest::qWait(1);
  QImage frame(canvas.size(), QImage::Format_RGBA8888_Premultiplied);

  QBENCHMARK {
    frame.fill(Qt::transparent);
    canvas.render(&frame);
  }
}

QTEST_MAIN(CanvasBenchmark)

#include "CanvasBenchmark.moc"
