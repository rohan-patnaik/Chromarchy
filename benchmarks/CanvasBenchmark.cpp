#include "core/Document.h"
#include "ui/CanvasWidget.h"

#include <QImage>
#include <QScrollBar>
#include <QTest>

using chromarchy::CanvasWidget;
using chromarchy::Document;

class CanvasBenchmark final : public QObject {
  Q_OBJECT

private slots:
  void paintSparseDocumentAtMinimumZoom();
  void paintRotatedSparseDocumentAtMinimumZoom();
  void paintPixelGridOnHugeSparseDocument();
  void fitHugeSparseDocumentWithinZoomBounds();
  void panHugeSparseDocumentByKeyboard();
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

void CanvasBenchmark::paintPixelGridOnHugeSparseDocument() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(150'000, 150'000),
                                               Qt::green));
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(32.0);
  canvas.setPixelGridEnabled(true);
  QTest::qWait(1);
  QVERIFY(canvas.pixelGridVisible());
  const auto visible = canvas.visibleDocumentRect();
  QVERIFY(visible.width() + visible.height() + 2 <= 40);
  QImage frame(canvas.size(), QImage::Format_RGBA8888_Premultiplied);

  QBENCHMARK {
    frame.fill(Qt::transparent);
    canvas.render(&frame);
  }
}

void CanvasBenchmark::fitHugeSparseDocumentWithinZoomBounds() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  QTest::qWait(1);

  QBENCHMARK {
    canvas.setZoom(CanvasWidget::maximumZoom);
    canvas.fitToViewport();
  }
  QCOMPARE(canvas.zoom(), CanvasWidget::minimumZoom);
}

void CanvasBenchmark::panHugeSparseDocumentByKeyboard() {
  auto document = Document::create(QSize(300'000, 300'000));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(299'999, 299'999),
                                               Qt::yellow));
  const auto blocks = document->storageBlocks();
  CanvasWidget canvas(&*document);
  canvas.resize(640, 480);
  canvas.show();
  canvas.setZoom(CanvasWidget::minimumZoom);
  QTest::qWait(1);
  canvas.horizontalScrollBar()->setValue(
      canvas.horizontalScrollBar()->maximum() / 2);
  const auto start = canvas.horizontalScrollBar()->value();

  QBENCHMARK {
    QTest::keyClick(&canvas, Qt::Key_Right);
    QTest::keyClick(&canvas, Qt::Key_Left);
  }
  QCOMPARE(canvas.horizontalScrollBar()->value(), start);
  QCOMPARE(document->storageBlocks(), blocks);
}

QTEST_MAIN(CanvasBenchmark)

#include "CanvasBenchmark.moc"
