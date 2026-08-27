#include "core/Document.h"
#include "core/PixelStorage.h"
#include "core/TiledImage.h"

#include <QImage>
#include <QTest>

#include <optional>
#include <span>

using chromarchy::AlphaMode;
using chromarchy::ByteOrder;
using chromarchy::ChannelLayout;
using chromarchy::Document;
using chromarchy::PixelFormat;
using chromarchy::PixelTileWriteResult;
using chromarchy::SampleFormat;
using chromarchy::SelectionMask;
using chromarchy::SparsePixelTileStore;
using chromarchy::TiledImage;

class CoreBenchmark final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void compositeVisibleMegapixel();
  void copyOnWriteTileMutation();
  void invertSparseLargeSelection();
  void materializeTypedQuarterMegapixel();

private:
  std::optional<Document> document_;
  std::optional<SparsePixelTileStore> typedStore_;
};

void CoreBenchmark::initTestCase() {
  document_ = Document::create(QSize(1024, 1024));
  QVERIFY(document_);

  QImage pixels(1024, 1024, QImage::Format_RGBA8888_Premultiplied);
  for (int layerIndex = 0; layerIndex < 8; ++layerIndex) {
    pixels.fill(QColor(20 + layerIndex * 20, 80 + layerIndex * 10,
                       180 - layerIndex * 15, 48));
    const auto index = layerIndex == 0
                           ? 0
                           : document_->addLayer(
                                 QStringLiteral("Benchmark %1").arg(layerIndex));
    QVERIFY(document_->layerAt(index)->replacePixels(
        TiledImage::fromImage(pixels)));
  }

  const PixelFormat typedFormat{SampleFormat::UnsignedInteger16,
                                ChannelLayout::RGBA, AlphaMode::Straight,
                                ByteOrder::LittleEndian};
  typedStore_ = SparsePixelTileStore::create(QSize(512, 512), typedFormat);
  QVERIFY(typedStore_);
  QByteArray typedPixels(512 * 512 * 8, '\0');
  for (qsizetype offset = 0; offset < typedPixels.size(); offset += 8) {
    typedPixels[offset] = '\0';
    typedPixels[offset + 1] = static_cast<char>(0x80);
    typedPixels[offset + 2] = static_cast<char>(0xff);
    typedPixels[offset + 3] = static_cast<char>(0x7f);
    typedPixels[offset + 4] = '\0';
    typedPixels[offset + 5] = static_cast<char>(0x40);
    typedPixels[offset + 6] = static_cast<char>(0xff);
    typedPixels[offset + 7] = static_cast<char>(0xff);
  }
  const auto typedBytes = std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(typedPixels.constData()),
      static_cast<std::size_t>(typedPixels.size()));
  QCOMPARE(typedStore_->writeRegion(QRect(0, 0, 512, 512), typedBytes,
                                    512ULL * 8ULL),
           PixelTileWriteResult::Changed);
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

void CoreBenchmark::materializeTypedQuarterMegapixel() {
  std::optional<chromarchy::Rgba8Buffer> converted;
  QBENCHMARK {
    converted = typedStore_->readRgba8PremultipliedRegion(
        QRect(0, 0, 512, 512));
  }
  QVERIFY(converted);
}

QTEST_APPLESS_MAIN(CoreBenchmark)

#include "CoreBenchmark.moc"
