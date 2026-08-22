#include "core/PixelStorage.h"
#include "core/TiledImage.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <limits>
#include <span>

using chromarchy::AlphaMode;
using chromarchy::ByteOrder;
using chromarchy::ChannelLayout;
using chromarchy::ChannelMeaning;
using chromarchy::PixelFormat;
using chromarchy::PixelStorageLayout;
using chromarchy::PixelTile;
using chromarchy::SampleFormat;

namespace {

std::span<const std::byte> bytesOf(const QByteArray& bytes) {
  return {reinterpret_cast<const std::byte*>(bytes.constData()),
          static_cast<std::size_t>(bytes.size())};
}

QByteArray byteArray(const QJsonArray& values) {
  QByteArray bytes;
  bytes.reserve(values.size());
  for (const auto value : values) {
    bytes.push_back(static_cast<char>(value.toInt()));
  }
  return bytes;
}

}  // namespace

class PixelStorageTest final : public QObject {
  Q_OBJECT

private slots:
  void describesSupportedSampleAndChannelContracts();
  void rejectsIncompatibleAlphaAndEndianContracts();
  void matchesCheckedLayoutFixtures();
  void rejectsInvalidOverflowingAndOverBudgetLayouts();
  void convertsIndependentRgba8AlphaVectors();
  void rejectsInvalidPremultipliedSamples();
  void honorsCheckedSourceAndDestinationStrides();
  void rejectsHostileRgba8LayoutsAndPayloads();
  void roundTripsQImageWithoutChangingLiveFormat();
  void allocatesBoundedHighDepthTiles();
  void accessesHighDepthSamplesWithCopyOnWrite();
  void handlesPartiallyOverlappingSelfWrites();
  void roundTripsHighDepthPersistenceSeam();
  void keepsRgba8ConversionSeamExplicit();
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

void PixelStorageTest::convertsIndependentRgba8AlphaVectors() {
  QFile fixture(QStringLiteral(CHROMARCHY_SOURCE_DIR
                               "/tests/fixtures/rgba8-alpha-vectors.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto document = QJsonDocument::fromJson(fixture.readAll());
  QVERIFY(document.isArray());

  for (const auto& value : document.array()) {
    const auto object = value.toObject();
    const auto straight = byteArray(object["straight"].toArray());
    const auto premultiplied = byteArray(object["premultiplied"].toArray());
    const auto expectedStraight =
        byteArray(object["unpremultiplied"].toArray());
    const auto straightLayout = PixelStorageLayout::create(
        QSize(straight.size() / 4, 1), PixelFormat::rgba8Straight());
    const auto premultipliedLayout = PixelStorageLayout::create(
        QSize(premultiplied.size() / 4, 1),
        PixelFormat::rgba8Premultiplied());
    QVERIFY2(straightLayout, qPrintable(object["name"].toString()));
    QVERIFY(premultipliedLayout);

    const auto encoded = chromarchy::convertRgba8(
        bytesOf(straight), *straightLayout, AlphaMode::Premultiplied);
    QVERIFY(encoded);
    QCOMPARE(encoded->bytes, premultiplied);

    const auto decoded = chromarchy::convertRgba8(
        bytesOf(premultiplied), *premultipliedLayout, AlphaMode::Straight);
    QVERIFY(decoded);
    QCOMPARE(decoded->bytes, expectedStraight);
  }
}

void PixelStorageTest::rejectsInvalidPremultipliedSamples() {
  const auto layout = PixelStorageLayout::create(
      QSize(4, 1), PixelFormat::rgba8Premultiplied());
  QVERIFY(layout);

  const QByteArray valid = QByteArray::fromHex(
      "0000000001010101ffffffff808080ff");
  const auto copied = chromarchy::convertRgba8(
      bytesOf(valid), *layout, AlphaMode::Premultiplied);
  QVERIFY(copied);
  QCOMPARE(copied->bytes, valid);
  QVERIFY(chromarchy::convertRgba8(bytesOf(valid), *layout,
                                   AlphaMode::Straight));

  const QList<QByteArray> invalidSamples = {
      QByteArray::fromHex("01000000"), QByteArray::fromHex("02010101"),
      QByteArray::fromHex("01020101"), QByteArray::fromHex("01010201")};
  const auto singlePixelLayout = PixelStorageLayout::create(
      QSize(1, 1), PixelFormat::rgba8Premultiplied());
  QVERIFY(singlePixelLayout);
  for (const auto& invalid : invalidSamples) {
    QVERIFY(!chromarchy::convertRgba8(bytesOf(invalid), *singlePixelLayout,
                                      AlphaMode::Premultiplied));
    QVERIFY(!chromarchy::convertRgba8(bytesOf(invalid), *singlePixelLayout,
                                      AlphaMode::Straight));
    QVERIFY(!chromarchy::rgba8ImageFromBytes(bytesOf(invalid),
                                             *singlePixelLayout, 4));
  }
}

void PixelStorageTest::honorsCheckedSourceAndDestinationStrides() {
  const auto sourceLayout = PixelStorageLayout::createWithRowStride(
      QSize(2, 2), PixelFormat::rgba8Straight(), 12);
  QVERIFY(sourceLayout);
  QByteArray source(24, static_cast<char>(0xee));
  const QByteArray firstRow = QByteArray::fromHex("ff00008000ff00ff");
  const QByteArray secondRow = QByteArray::fromHex("0000ffffffffff00");
  std::copy(firstRow.cbegin(), firstRow.cend(), source.begin());
  std::copy(secondRow.cbegin(), secondRow.cend(), source.begin() + 12);

  const auto converted = chromarchy::convertRgba8(
      bytesOf(source), *sourceLayout, AlphaMode::Premultiplied, 16, 32);
  QVERIFY(converted);
  QCOMPARE(converted->layout.packedRowBytes(), quint64(8));
  QCOMPARE(converted->layout.rowStrideBytes(), quint64(16));
  QCOMPARE(converted->layout.allocationBytes(), quint64(32));
  QCOMPARE(converted->bytes.mid(0, 8), QByteArray::fromHex("8000008000ff00ff"));
  QCOMPARE(converted->bytes.mid(8, 8), QByteArray(8, '\0'));
  QCOMPARE(converted->bytes.mid(16, 8), QByteArray::fromHex("0000ffff00000000"));
  QCOMPARE(converted->bytes.mid(24, 8), QByteArray(8, '\0'));
}

void PixelStorageTest::rejectsHostileRgba8LayoutsAndPayloads() {
  const auto rgba8 = PixelFormat::rgba8Premultiplied();
  QVERIFY(!PixelStorageLayout::createWithRowStride(QSize(2, 1), rgba8, 7));
  QVERIFY(!PixelStorageLayout::createWithRowStride(
      QSize(1, std::numeric_limits<int>::max()), rgba8,
      std::numeric_limits<quint64>::max()));
  QVERIFY(!PixelStorageLayout::createWithRowStride(QSize(2, 2), rgba8, 8, 15));
  constexpr quint64 smallBudget = 1024;
  QVERIFY(!PixelStorageLayout::create(
      QSize(std::numeric_limits<int>::max(), 1), rgba8, 1, smallBudget));
  QVERIFY(!PixelStorageLayout::createWithRowStride(
      QSize(std::numeric_limits<int>::max(), 1), rgba8,
      static_cast<quint64>(std::numeric_limits<int>::max()) * 4U,
      smallBudget));

  const auto layout = PixelStorageLayout::create(QSize(2, 1), rgba8);
  QVERIFY(layout);
  const QByteArray exact(8, '\x01');
  const QByteArray shortPayload(7, '\x01');
  const QByteArray trailingPayload(9, '\x01');
  QVERIFY(!chromarchy::convertRgba8(bytesOf(shortPayload), *layout,
                                    AlphaMode::Straight));
  QVERIFY(!chromarchy::convertRgba8(bytesOf(trailingPayload), *layout,
                                    AlphaMode::Straight));
  QVERIFY(!chromarchy::convertRgba8(bytesOf(exact), *layout, AlphaMode::None));
  QVERIFY(!chromarchy::convertRgba8(bytesOf(exact), *layout,
                                    AlphaMode::Straight, 3));
  QVERIFY(!chromarchy::convertRgba8(bytesOf(exact), *layout,
                                    AlphaMode::Straight, 1, 7));

  const PixelFormat rgba16{SampleFormat::UnsignedInteger16, ChannelLayout::RGBA,
                           AlphaMode::Premultiplied, ByteOrder::LittleEndian};
  const auto rgba16Layout = PixelStorageLayout::create(QSize(1, 1), rgba16);
  QVERIFY(rgba16Layout);
  QVERIFY(!chromarchy::convertRgba8(bytesOf(exact), *rgba16Layout,
                                    AlphaMode::Straight));
}

void PixelStorageTest::roundTripsQImageWithoutChangingLiveFormat() {
  QImage source(3, 1, QImage::Format_RGBA8888);
  const QByteArray straight = QByteArray::fromHex(
      "ff80407f0a141e80ffffffff");
  std::copy(straight.cbegin(), straight.cend(),
            reinterpret_cast<char*>(source.bits()));

  const auto encoded = chromarchy::rgba8BytesFromImage(
      source, AlphaMode::Premultiplied, 1, 12);
  QVERIFY(encoded);
  QCOMPARE(encoded->layout.format(), PixelFormat::rgba8Premultiplied());
  QCOMPARE(encoded->bytes, QByteArray::fromHex("7f40207f050a0f80ffffffff"));

  const auto restored = chromarchy::rgba8ImageFromBytes(
      bytesOf(encoded->bytes), encoded->layout, 12);
  QVERIFY(restored);
  QCOMPARE(restored->format(), QImage::Format_RGBA8888_Premultiplied);
  QCOMPARE(restored->pixelColor(0, 0).rgba(), qRgba(255, 129, 64, 127));
  QCOMPARE(restored->pixelColor(1, 0).rgba(), qRgba(10, 20, 30, 128));
  QCOMPARE(restored->pixelColor(2, 0).rgba(), qRgba(255, 255, 255, 255));

  QImage unsupported(1, 1, QImage::Format_ARGB32);
  QVERIFY(!chromarchy::rgba8BytesFromImage(unsupported,
                                           AlphaMode::Premultiplied));
}

void PixelStorageTest::allocatesBoundedHighDepthTiles() {
  const QList<QPair<PixelFormat, quint64>> formats = {
      {{SampleFormat::UnsignedInteger16, ChannelLayout::Gray, AlphaMode::None,
        ByteOrder::BigEndian},
       256ULL * 256ULL * 2ULL},
      {{SampleFormat::UnsignedInteger16, ChannelLayout::RGBA,
        AlphaMode::Straight, ByteOrder::LittleEndian},
       256ULL * 256ULL * 8ULL},
      {{SampleFormat::Float16, ChannelLayout::RGBA, AlphaMode::Straight,
        ByteOrder::BigEndian},
       256ULL * 256ULL * 8ULL},
      {{SampleFormat::Float32, ChannelLayout::RGBA, AlphaMode::Straight,
        ByteOrder::LittleEndian},
       PixelTile::maximumAllocationBytes}};

  for (const auto& [format, expectedBytes] : formats) {
    const auto tile = PixelTile::create(format, expectedBytes);
    QVERIFY(tile);
    QCOMPARE(tile->format(), format);
    QCOMPARE(tile->layout().dimensions(), QSize(256, 256));
    QCOMPARE(tile->layout().allocationBytes(), expectedBytes);
    QCOMPARE(tile->packedBytes().size(), expectedBytes);
    QVERIFY(tile->isZero());
    QVERIFY(!PixelTile::create(format, expectedBytes - 1));
  }

  const PixelFormat unsupportedPremultiplied16{
      SampleFormat::UnsignedInteger16, ChannelLayout::RGBA,
      AlphaMode::Premultiplied, ByteOrder::LittleEndian};
  QVERIFY(!PixelTile::create(unsupportedPremultiplied16));
}

void PixelStorageTest::accessesHighDepthSamplesWithCopyOnWrite() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::RGBA, AlphaMode::Straight,
                           ByteOrder::LittleEndian};
  auto original = PixelTile::create(format);
  QVERIFY(original);
  auto copy = *original;
  QCOMPARE(copy.packedBytes().data(), original->packedBytes().data());

  const QPoint position(255, 128);
  const auto sample = QByteArray::fromHex("34127856bc9af0de");
  QVERIFY(copy.setPixelBytes(position, bytesOf(sample)));
  QCOMPARE(QByteArray(reinterpret_cast<const char*>(copy.pixelBytes(position).data()),
                      static_cast<qsizetype>(copy.pixelBytes(position).size())),
           sample);
  QVERIFY(copy.packedBytes().data() != original->packedBytes().data());
  QVERIFY(original->isZero());
  QVERIFY(!copy.isZero());
  QVERIFY(!copy.setPixelBytes(position, bytesOf(sample)));
  QVERIFY(copy.pixelBytes(QPoint(-1, 0)).empty());
  QVERIFY(copy.pixelBytes(QPoint(PixelTile::extent, 0)).empty());
  QVERIFY(!copy.setPixelBytes(position, bytesOf(QByteArray::fromHex("0000"))));

  const QByteArray zeroSample(sample.size(), '\0');
  QVERIFY(copy.setPixelBytes(position, bytesOf(zeroSample)));
  QVERIFY(copy.isZero());
}

void PixelStorageTest::handlesPartiallyOverlappingSelfWrites() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::RGBA, AlphaMode::Straight,
                           ByteOrder::LittleEndian};
  auto original = PixelTile::create(format);
  QVERIFY(original);
  const auto firstPixel = QByteArray::fromHex("0102030405060708");
  QVERIFY(original->setPixelBytes(QPoint(0, 0), bytesOf(firstPixel)));

  auto copy = *original;
  const auto aliasedSource = copy.packedBytes().subspan(7, 8);
  QVERIFY(copy.setPixelBytes(QPoint(1, 0), aliasedSource));

  const auto expected = QByteArray::fromHex("0800000000000000");
  QCOMPARE(QByteArray(
               reinterpret_cast<const char*>(
                   copy.pixelBytes(QPoint(1, 0)).data()),
               static_cast<qsizetype>(copy.pixelBytes(QPoint(1, 0)).size())),
           expected);
  QCOMPARE(QByteArray(
               reinterpret_cast<const char*>(
                   original->pixelBytes(QPoint(0, 0)).data()),
               static_cast<qsizetype>(
                   original->pixelBytes(QPoint(0, 0)).size())),
           firstPixel);
  QCOMPARE(QByteArray(
               reinterpret_cast<const char*>(
                   original->pixelBytes(QPoint(1, 0)).data()),
               static_cast<qsizetype>(
                   original->pixelBytes(QPoint(1, 0)).size())),
           QByteArray(8, '\0'));
  QVERIFY(copy.packedBytes().data() != original->packedBytes().data());
}

void PixelStorageTest::roundTripsHighDepthPersistenceSeam() {
  const PixelFormat format{SampleFormat::Float32, ChannelLayout::RGB,
                           AlphaMode::None, ByteOrder::BigEndian};
  auto tile = PixelTile::create(format);
  QVERIFY(tile);
  const auto first = QByteArray::fromHex("3f8000004000000040400000");
  const auto last = QByteArray::fromHex("bf800000000000007f7fffff");
  QVERIFY(tile->setPixelBytes(QPoint(0, 0), bytesOf(first)));
  QVERIFY(tile->setPixelBytes(QPoint(255, 255), bytesOf(last)));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("tile.payload"));
  QSaveFile output(path);
  QVERIFY(output.open(QIODevice::WriteOnly));
  const auto packed = tile->packedBytes();
  QCOMPARE(output.write(reinterpret_cast<const char*>(packed.data()),
                        static_cast<qsizetype>(packed.size())),
           static_cast<qint64>(packed.size()));
  QVERIFY(output.commit());

  QFile input(path);
  QVERIFY(input.open(QIODevice::ReadOnly));
  const auto reopenedBytes = input.readAll();
  const auto reopened = PixelTile::fromPackedBytes(format,
                                                    bytesOf(reopenedBytes));
  QVERIFY(reopened);
  QVERIFY(std::equal(reopened->packedBytes().begin(),
                     reopened->packedBytes().end(), packed.begin()));
  QCOMPARE(QByteArray(
               reinterpret_cast<const char*>(
                   reopened->pixelBytes(QPoint(255, 255)).data()),
               static_cast<qsizetype>(
                   reopened->pixelBytes(QPoint(255, 255)).size())),
           last);

  auto truncated = reopenedBytes;
  truncated.chop(1);
  QVERIFY(!PixelTile::fromPackedBytes(format, bytesOf(truncated)));
  auto trailing = reopenedBytes;
  trailing.append('\0');
  QVERIFY(!PixelTile::fromPackedBytes(format, bytesOf(trailing)));
  QVERIFY(!reopened->toRgba8Premultiplied());
}

void PixelStorageTest::keepsRgba8ConversionSeamExplicit() {
  auto tile = PixelTile::create(PixelFormat::rgba8Straight());
  QVERIFY(tile);
  const auto straight = QByteArray::fromHex("ff80407f");
  QVERIFY(tile->setPixelBytes(QPoint(0, 0), bytesOf(straight)));
  const auto converted = tile->toRgba8Premultiplied();
  QVERIFY(converted);
  QCOMPARE(converted->layout.format(), PixelFormat::rgba8Premultiplied());
  QCOMPARE(converted->bytes.first(4), QByteArray::fromHex("7f40207f"));

  auto premultiplied = PixelTile::create(PixelFormat::rgba8Premultiplied());
  QVERIFY(premultiplied);
  const auto invalid = QByteArray::fromHex("02010101");
  QVERIFY(!premultiplied->setPixelBytes(QPoint(0, 0), bytesOf(invalid)));
  QVERIFY(premultiplied->isZero());
  QByteArray hostile(
      reinterpret_cast<const char*>(premultiplied->packedBytes().data()),
      static_cast<qsizetype>(premultiplied->packedBytes().size()));
  std::copy(invalid.cbegin(), invalid.cend(), hostile.begin());
  QVERIFY(!PixelTile::fromPackedBytes(PixelFormat::rgba8Premultiplied(),
                                      bytesOf(hostile)));
}

QTEST_APPLESS_MAIN(PixelStorageTest)

#include "PixelStorageTest.moc"
