#include "core/PixelStorage.h"
#include "core/TiledImage.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <limits>

using chromarchy::AlphaMode;
using chromarchy::ByteOrder;
using chromarchy::ChannelLayout;
using chromarchy::ChannelMeaning;
using chromarchy::PixelFormat;
using chromarchy::PixelStorageLayout;
using chromarchy::SampleFormat;

class PixelStorageTest final : public QObject {
  Q_OBJECT

private slots:
  void describesSupportedSampleAndChannelContracts();
  void rejectsIncompatibleAlphaAndEndianContracts();
  void matchesCheckedLayoutFixtures();
  void rejectsInvalidOverflowingAndOverBudgetLayouts();
};

void PixelStorageTest::describesSupportedSampleAndChannelContracts() {
  const PixelFormat rgba16{SampleFormat::UnsignedInteger16, ChannelLayout::RGBA,
                           AlphaMode::Straight, ByteOrder::LittleEndian};
  QVERIFY(rgba16.isValid());
  QVERIFY(!rgba16.isFloatingPoint());
  QCOMPARE(rgba16.bitsPerSample(), 16);
  QCOMPARE(rgba16.bytesPerPixel(), 8);
  QCOMPARE(*rgba16.channelMeaning(0), ChannelMeaning::Red);
  QCOMPARE(*rgba16.channelMeaning(3), ChannelMeaning::Alpha);
  QVERIFY(!rgba16.channelMeaning(4));

  const PixelFormat grayFloat{SampleFormat::Float32, ChannelLayout::Gray,
                              AlphaMode::None, ByteOrder::BigEndian};
  QVERIFY(grayFloat.isValid());
  QVERIFY(grayFloat.isFloatingPoint());
  QCOMPARE(grayFloat.bitsPerSample(), 32);
  QCOMPARE(*grayFloat.channelMeaning(0), ChannelMeaning::Gray);

  const PixelFormat grayAlphaFloat{SampleFormat::Float16,
                                   ChannelLayout::GrayAlpha,
                                   AlphaMode::Premultiplied,
                                   ByteOrder::LittleEndian};
  QVERIFY(grayAlphaFloat.isValid());
  QCOMPARE(*grayAlphaFloat.channelMeaning(1), ChannelMeaning::Alpha);
}

void PixelStorageTest::rejectsIncompatibleAlphaAndEndianContracts() {
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger8, ChannelLayout::RGB,
                        AlphaMode::Straight, ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger8, ChannelLayout::RGBA,
                        AlphaMode::None, ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger16, ChannelLayout::RGBA,
                        AlphaMode::Straight, ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::Float32, ChannelLayout::RGB,
                        AlphaMode::None, ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger8, ChannelLayout::RGBA,
                        AlphaMode::Premultiplied, ByteOrder::LittleEndian}
                .isValid()));
  QVERIFY(!(PixelFormat{static_cast<SampleFormat>(255), ChannelLayout::RGBA,
                        AlphaMode::Premultiplied, ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger8,
                        static_cast<ChannelLayout>(255), AlphaMode::None,
                        ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::UnsignedInteger8, ChannelLayout::RGBA,
                        static_cast<AlphaMode>(255), ByteOrder::NotApplicable}
                .isValid()));
  QVERIFY(!(PixelFormat{SampleFormat::Float16, ChannelLayout::RGBA,
                        AlphaMode::Straight, static_cast<ByteOrder>(255)}
                .isValid()));
}

void PixelStorageTest::matchesCheckedLayoutFixtures() {
  QFile fixture(QStringLiteral(CHROMARCHY_SOURCE_DIR
                               "/tests/fixtures/pixel-storage-layouts.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto document = QJsonDocument::fromJson(fixture.readAll());
  QVERIFY(document.isArray());

  for (const auto& value : document.array()) {
    const auto object = value.toObject();
    const auto sample = static_cast<SampleFormat>(object["sample"].toInt());
    const auto channels = static_cast<ChannelLayout>(object["channels"].toInt());
    const auto alpha = static_cast<AlphaMode>(object["alpha"].toInt());
    const auto byteOrder = static_cast<ByteOrder>(object["byteOrder"].toInt());
    const PixelFormat format{sample, channels, alpha, byteOrder};
    const auto layout = PixelStorageLayout::create(
        QSize(object["width"].toInt(), object["height"].toInt()), format,
        static_cast<quint64>(object["rowAlignment"].toInteger()));
    QVERIFY2(layout, qPrintable(object["name"].toString()));
    QCOMPARE(layout->packedRowBytes(),
             static_cast<quint64>(object["packedRowBytes"].toInteger()));
    QCOMPARE(layout->rowStrideBytes(),
             static_cast<quint64>(object["rowStrideBytes"].toInteger()));
    QCOMPARE(layout->allocationBytes(),
             static_cast<quint64>(object["allocationBytes"].toInteger()));
  }
}

void PixelStorageTest::rejectsInvalidOverflowingAndOverBudgetLayouts() {
  const auto rgba8 = PixelFormat::rgba8Premultiplied();
  QVERIFY(!PixelStorageLayout::create(QSize(), rgba8));
  QVERIFY(!PixelStorageLayout::create(QSize(-1, 1), rgba8));
  QVERIFY(!PixelStorageLayout::create(QSize(1, 1), rgba8, 0));
  QVERIFY(!PixelStorageLayout::create(QSize(1, 1), rgba8, 3));

  const PixelFormat rgbaFloat{SampleFormat::Float32, ChannelLayout::RGBA,
                              AlphaMode::Premultiplied,
                              ByteOrder::LittleEndian};
  QVERIFY(!PixelStorageLayout::create(
      QSize(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()),
      rgbaFloat));

  const auto exact = PixelStorageLayout::create(QSize(256, 256), rgba8, 64,
                                                 256ULL * 256ULL * 4ULL);
  QVERIFY(exact);
  const auto engineTile = chromarchy::TiledImage::tileStorageLayout();
  QVERIFY(engineTile);
  QCOMPARE(engineTile->format(), rgba8);
  QCOMPARE(engineTile->allocationBytes(), exact->allocationBytes());
  QVERIFY(!PixelStorageLayout::create(QSize(256, 256), rgba8, 64,
                                      exact->allocationBytes() - 1));
}

QTEST_APPLESS_MAIN(PixelStorageTest)

#include "PixelStorageTest.moc"
