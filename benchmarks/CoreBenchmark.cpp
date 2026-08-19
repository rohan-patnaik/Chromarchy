#include "core/Document.h"
#include "core/TiledImage.h"

#include <QImage>
#include <QTest>

#include <optional>

using chromarchy::Document;
using chromarchy::SelectionMask;
using chromarchy::TiledImage;

class CoreBenchmark final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void compositeVisibleMegapixel();
  void copyOnWriteTileMutation();
  void invertSparseLargeSelection();

private:
  std::optional<Document> document_;
};

void CoreBenchmark::initTestCase() {
  document_ = Document::create(QSize(20'000, 20'000));
  QVERIFY(document_);

  QImage pixels(1024, 1024, QImage::Format_RGBA8888_Premultiplied);
  for (int layerIndex = 0; layerIndex < 8; ++layerIndex) {
    pixels.fill(QColor(20 + layerIndex * 20, 80 + layerIndex * 10,
                       180 - layerIndex * 15, 48));
    const auto index = layerIndex == 0
                           ? 0
                           : document_->addLayer(
                                 QStringLiteral("Benchmark %1").arg(layerIndex));
    document_->layerAt(index)->pixels() = TiledImage::fromImage(pixels);
  }
}

void CoreBenchmark::compositeVisibleMegapixel() {
  QBENCHMARK {
    const auto composite = document_->composite(QRect(0, 0, 1024, 1024));
    Q_UNUSED(composite);
  }
}

void CoreBenchmark::copyOnWriteTileMutation() {
  const auto source = document_->layerAt(0)->pixels();
  QBENCHMARK {
    auto copy = source;
    copy.setPixelColor(QPoint(500, 500), Qt::red);
  }
}

void CoreBenchmark::invertSparseLargeSelection() {
  SelectionMask selection(QSize(300'000, 300'000));
  selection.selectRectangle(QRect(1000, 1000, 512, 512));
  QBENCHMARK {
    auto copy = selection;
    copy.invert();
  }
}

QTEST_APPLESS_MAIN(CoreBenchmark)

#include "CoreBenchmark.moc"
