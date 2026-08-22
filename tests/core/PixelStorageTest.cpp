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
using chromarchy::PixelTileIndex;
using chromarchy::PixelTileSnapshot;
using chromarchy::PixelTileWriteResult;
using chromarchy::SampleFormat;
using chromarchy::SparsePixelTileStore;

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
  void ownsSparseTypedTilesWithinHardBudgets();
  void rejectsSparseWritesAtomically();
  void mutatesSparseTilesWithCopyOnWrite();
  void elidesZeroTypedTilesAndPreservesCopies();
  void preservesSparseHighDepthByteOrder();
  void exportsSparseSnapshotsInDeterministicOrder();
  void roundTripsOwningSparseSnapshots();
  void rejectsHostileSparseSnapshotsAtomically();
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

void PixelStorageTest::ownsSparseTypedTilesWithinHardBudgets() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::RGBA, AlphaMode::Straight,
                           ByteOrder::LittleEndian};
  constexpr quint64 tileBytes = 256ULL * 256ULL * 8ULL;
  auto store = SparsePixelTileStore::create(QSize(257, 257), format,
                                             tileBytes * 2, 2);
  QVERIFY(store);
  QCOMPARE(store->dimensions(), QSize(257, 257));
  QCOMPARE(store->format(), format);
  QCOMPARE(store->maximumResidentBytes(), tileBytes * 2);
  QCOMPARE(store->maximumResidentTiles(), quint64(2));
  QCOMPARE(store->residentDecodedBytes(), quint64(0));
  QCOMPARE(store->allocatedTileCount(), 0);
  QVERIFY(store->containsTileIndex(PixelTileIndex{0, 0}));
  QVERIFY(store->containsTileIndex(PixelTileIndex{1, 1}));
  QVERIFY(!store->containsTileIndex(PixelTileIndex{2, 0}));

  const auto absent = store->pixelBytes(QPoint(256, 256));
  QVERIFY(absent);
  QCOMPARE(*absent, QByteArray(8, '\0'));
  QCOMPARE(store->setPixelBytes(QPoint(256, 256), bytesOf(*absent)),
           PixelTileWriteResult::Unchanged);
  QCOMPARE(store->allocatedTileCount(), 0);
  QVERIFY(!store->pixelBytes(QPoint(257, 256)));
  QVERIFY(!store->pixelBytes(QPoint(-1, 0)));

  QVERIFY(!SparsePixelTileStore::create(QSize(), format));
  QVERIFY(!SparsePixelTileStore::create(
      QSize(1, 1), format, SparsePixelTileStore::hardMaximumResidentBytes + 1,
      1));
  QVERIFY(!SparsePixelTileStore::create(
      QSize(1, 1), format, tileBytes,
      SparsePixelTileStore::hardMaximumResidentTiles + 1));
  QVERIFY(!SparsePixelTileStore::create(QSize(1, 1), format, tileBytes - 1,
                                         1));

  auto huge = SparsePixelTileStore::create(
      QSize(std::numeric_limits<int>::max(),
            std::numeric_limits<int>::max()),
      format, tileBytes, 1);
  QVERIFY(huge);
  QVERIFY(huge->containsTileIndex(PixelTileIndex{8'388'607, 8'388'607}));
  QVERIFY(!huge->containsTileIndex(PixelTileIndex{8'388'608, 0}));
  QCOMPARE(huge->allocatedTileCount(), 0);
}

void PixelStorageTest::rejectsSparseWritesAtomically() {
  const PixelFormat format{SampleFormat::Float32, ChannelLayout::RGBA,
                           AlphaMode::Straight, ByteOrder::LittleEndian};
  constexpr quint64 tileBytes = PixelTile::maximumAllocationBytes;
  auto store =
      SparsePixelTileStore::create(QSize(512, 256), format, tileBytes, 2);
  QVERIFY(store);
  const auto sample = QByteArray::fromHex(
      "0000803f000000400000404000008040");
  QCOMPARE(store->setPixelBytes(QPoint(0, 0), bytesOf(sample)),
           PixelTileWriteResult::Changed);
  QCOMPARE(store->residentDecodedBytes(), tileBytes);
  QCOMPARE(store->allocatedTileCount(), 1);
  const auto firstTile = store->packedTileBytes(PixelTileIndex{0, 0});
  QVERIFY(firstTile);
  const QByteArray before(reinterpret_cast<const char*>(firstTile->data()),
                          static_cast<qsizetype>(firstTile->size()));

  QCOMPARE(store->setPixelBytes(QPoint(256, 0), bytesOf(sample)),
           PixelTileWriteResult::Rejected);
  QCOMPARE(store->setPixelBytes(QPoint(1, 0),
                                bytesOf(QByteArray::fromHex("0000"))),
           PixelTileWriteResult::Rejected);
  QCOMPARE(store->setPixelBytes(QPoint(std::numeric_limits<int>::max(), 0),
                                bytesOf(sample)),
           PixelTileWriteResult::Rejected);
  QCOMPARE(store->residentDecodedBytes(), tileBytes);
  QCOMPARE(store->allocatedTileCount(), 1);
  const auto unchanged = store->packedTileBytes(PixelTileIndex{0, 0});
  QVERIFY(unchanged);
  QCOMPARE(QByteArray(reinterpret_cast<const char*>(unchanged->data()),
                      static_cast<qsizetype>(unchanged->size())),
           before);
  QVERIFY(!store->packedTileBytes(PixelTileIndex{1, 0}));

  const PixelFormat gray16{SampleFormat::UnsignedInteger16,
                           ChannelLayout::Gray, AlphaMode::None,
                           ByteOrder::LittleEndian};
  constexpr quint64 grayTileBytes = 256ULL * 256ULL * 2ULL;
  auto tileLimited = SparsePixelTileStore::create(
      QSize(512, 256), gray16, grayTileBytes * 2, 1);
  QVERIFY(tileLimited);
  const auto graySample = QByteArray::fromHex("0100");
  QCOMPARE(tileLimited->setPixelBytes(QPoint(0, 0), bytesOf(graySample)),
           PixelTileWriteResult::Changed);
  QCOMPARE(tileLimited->setPixelBytes(QPoint(256, 0), bytesOf(graySample)),
           PixelTileWriteResult::Rejected);
  QCOMPARE(tileLimited->allocatedTileCount(), 1);
  QCOMPARE(tileLimited->residentDecodedBytes(), grayTileBytes);

  auto premultiplied = SparsePixelTileStore::create(
      QSize(256, 256), PixelFormat::rgba8Premultiplied(), 256 * 256 * 4, 1);
  QVERIFY(premultiplied);
  const auto hostile = QByteArray::fromHex("02010101");
  QCOMPARE(premultiplied->setPixelBytes(QPoint(0, 0), bytesOf(hostile)),
           PixelTileWriteResult::Rejected);
  QCOMPARE(premultiplied->allocatedTileCount(), 0);
  QCOMPARE(premultiplied->residentDecodedBytes(), quint64(0));
}

void PixelStorageTest::mutatesSparseTilesWithCopyOnWrite() {
  const PixelFormat format{SampleFormat::Float32, ChannelLayout::RGBA,
                           AlphaMode::Straight, ByteOrder::LittleEndian};
  constexpr quint64 tileBytes = PixelTile::maximumAllocationBytes;
  auto original = SparsePixelTileStore::create(QSize(512, 256), format,
                                                tileBytes * 2, 2);
  QVERIFY(original);
  const auto first = QByteArray::fromHex(
      "0000803f000000400000404000008040");
  const auto second = QByteArray::fromHex(
      "0000a0400000c0400000e04000000041");
  const auto third = QByteArray::fromHex(
      "00001041000020410000304100004041");
  QCOMPARE(original->setPixelBytes(QPoint(0, 0), bytesOf(first)),
           PixelTileWriteResult::Changed);
  QCOMPARE(original->setPixelBytes(QPoint(256, 0), bytesOf(first)),
           PixelTileWriteResult::Changed);
  const auto* uniqueFirstPointer =
      original->packedTileBytes(PixelTileIndex{0, 0})->data();
  const auto* uniqueSecondPointer =
      original->packedTileBytes(PixelTileIndex{1, 0})->data();

  QCOMPARE(original->setPixelBytes(QPoint(1, 0), bytesOf(second)),
           PixelTileWriteResult::Changed);
  QCOMPARE(original->packedTileBytes(PixelTileIndex{0, 0})->data(),
           uniqueFirstPointer);
  QCOMPARE(original->packedTileBytes(PixelTileIndex{1, 0})->data(),
           uniqueSecondPointer);

  auto copy = *original;
  QCOMPARE(copy.packedTileBytes(PixelTileIndex{0, 0})->data(),
           uniqueFirstPointer);
  QCOMPARE(copy.packedTileBytes(PixelTileIndex{1, 0})->data(),
           uniqueSecondPointer);
  QCOMPARE(copy.setPixelBytes(QPoint(2, 0), bytesOf(third)),
           PixelTileWriteResult::Changed);
  QVERIFY(copy.packedTileBytes(PixelTileIndex{0, 0})->data() !=
          original->packedTileBytes(PixelTileIndex{0, 0})->data());
  QCOMPARE(copy.packedTileBytes(PixelTileIndex{1, 0})->data(),
           original->packedTileBytes(PixelTileIndex{1, 0})->data());
  QCOMPARE(*original->pixelBytes(QPoint(2, 0)), QByteArray(16, '\0'));
  QCOMPARE(*copy.pixelBytes(QPoint(2, 0)), third);
  QCOMPARE(*original->pixelBytes(QPoint(1, 0)), second);
}

void PixelStorageTest::elidesZeroTypedTilesAndPreservesCopies() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::Gray, AlphaMode::None,
                           ByteOrder::BigEndian};
  constexpr quint64 tileBytes = 256ULL * 256ULL * 2ULL;
  auto original =
      SparsePixelTileStore::create(QSize(256, 256), format, tileBytes, 1);
  QVERIFY(original);
  const auto nonzero = QByteArray::fromHex("1234");
  QCOMPARE(original->setPixelBytes(QPoint(4, 5), bytesOf(nonzero)),
           PixelTileWriteResult::Changed);
  auto copy = *original;
  const auto originalPacked = original->packedTileBytes(PixelTileIndex{0, 0});
  const auto copyPacked = copy.packedTileBytes(PixelTileIndex{0, 0});
  QVERIFY(originalPacked);
  QVERIFY(copyPacked);
  QCOMPARE(originalPacked->data(), copyPacked->data());

  const QByteArray zero(2, '\0');
  QCOMPARE(copy.setPixelBytes(QPoint(4, 5), bytesOf(zero)),
           PixelTileWriteResult::Changed);
  QCOMPARE(copy.allocatedTileCount(), 0);
  QCOMPARE(copy.residentDecodedBytes(), quint64(0));
  QCOMPARE(original->allocatedTileCount(), 1);
  QCOMPARE(original->residentDecodedBytes(), tileBytes);
  QCOMPARE(*original->pixelBytes(QPoint(4, 5)), nonzero);
  QCOMPARE(*copy.pixelBytes(QPoint(4, 5)), zero);
}

void PixelStorageTest::preservesSparseHighDepthByteOrder() {
  const PixelFormat bigEndian{SampleFormat::UnsignedInteger16,
                              ChannelLayout::RGBA, AlphaMode::Straight,
                              ByteOrder::BigEndian};
  const PixelFormat littleEndian{SampleFormat::UnsignedInteger16,
                                 ChannelLayout::RGBA, AlphaMode::Straight,
                                 ByteOrder::LittleEndian};
  constexpr quint64 tileBytes = 256ULL * 256ULL * 8ULL;
  auto big =
      SparsePixelTileStore::create(QSize(256, 256), bigEndian, tileBytes, 1);
  auto little = SparsePixelTileStore::create(QSize(256, 256), littleEndian,
                                              tileBytes, 1);
  QVERIFY(big);
  QVERIFY(little);
  const auto sample = QByteArray::fromHex("123456789abcdef0");
  QCOMPARE(big->setPixelBytes(QPoint(255, 255), bytesOf(sample)),
           PixelTileWriteResult::Changed);
  QCOMPARE(little->setPixelBytes(QPoint(255, 255), bytesOf(sample)),
           PixelTileWriteResult::Changed);
  QCOMPARE(*big->pixelBytes(QPoint(255, 255)), sample);
  QCOMPARE(*little->pixelBytes(QPoint(255, 255)), sample);
  QCOMPARE(big->format().byteOrder, ByteOrder::BigEndian);
  QCOMPARE(little->format().byteOrder, ByteOrder::LittleEndian);
}

void PixelStorageTest::exportsSparseSnapshotsInDeterministicOrder() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::Gray, AlphaMode::None,
                           ByteOrder::BigEndian};
  constexpr quint64 tileBytes = 256ULL * 256ULL * 2ULL;
  auto store = SparsePixelTileStore::create(QSize(768, 512), format,
                                             tileBytes * 4, 4);
  QVERIFY(store);
  const QList<QPair<QPoint, QByteArray>> writes = {
      {QPoint(512, 256), QByteArray::fromHex("0400")},
      {QPoint(0, 256), QByteArray::fromHex("0300")},
      {QPoint(256, 0), QByteArray::fromHex("0200")},
      {QPoint(0, 0), QByteArray::fromHex("0100")}};
  for (const auto& [position, bytes] : writes) {
    QCOMPARE(store->setPixelBytes(position, bytesOf(bytes)),
             PixelTileWriteResult::Changed);
  }

  auto snapshots = store->tileSnapshots();
  QCOMPARE(snapshots.size(), 4);
  QCOMPARE(snapshots[0].index, PixelTileIndex(0, 0));
  QCOMPARE(snapshots[1].index, PixelTileIndex(1, 0));
  QCOMPARE(snapshots[2].index, PixelTileIndex(0, 1));
  QCOMPARE(snapshots[3].index, PixelTileIndex(2, 1));
  QVERIFY(snapshots == store->tileSnapshots());

  const auto firstRecordBytes = snapshots[0].packedBytes;
  const auto replacement = QByteArray::fromHex("0500");
  QCOMPARE(store->setPixelBytes(QPoint(0, 0), bytesOf(replacement)),
           PixelTileWriteResult::Changed);
  QCOMPARE(snapshots[0].packedBytes, firstRecordBytes);
  snapshots[0].packedBytes[0] = '\x7f';
  QCOMPARE(*store->pixelBytes(QPoint(0, 0)), replacement);
}

void PixelStorageTest::roundTripsOwningSparseSnapshots() {
  const PixelFormat format{SampleFormat::UnsignedInteger16,
                           ChannelLayout::RGBA, AlphaMode::Straight,
                           ByteOrder::BigEndian};
  constexpr quint64 tileBytes = 256ULL * 256ULL * 8ULL;
  auto source = SparsePixelTileStore::create(QSize(512, 512), format,
                                              tileBytes * 2, 2);
  QVERIFY(source);
  const auto first = QByteArray::fromHex("123456789abcdef0");
  const auto second = QByteArray::fromHex("fedcba9876543210");
  QCOMPARE(source->setPixelBytes(QPoint(255, 255), bytesOf(first)),
           PixelTileWriteResult::Changed);
  QCOMPARE(source->setPixelBytes(QPoint(256, 256), bytesOf(second)),
           PixelTileWriteResult::Changed);

  const auto canonicalSnapshots = source->tileSnapshots();
  auto snapshots = canonicalSnapshots;
  std::reverse(snapshots.begin(), snapshots.end());
  const auto snapshotsBefore = snapshots;
  auto reopened = SparsePixelTileStore::fromTileSnapshots(
      source->dimensions(), format, snapshots, tileBytes * 2, 2);
  QVERIFY(reopened);
  QVERIFY(snapshots == snapshotsBefore);
  QCOMPARE(reopened->format(), format);
  QCOMPARE(reopened->residentDecodedBytes(), tileBytes * 2);
  QVERIFY(reopened->tileSnapshots() == canonicalSnapshots);
  QCOMPARE(*reopened->pixelBytes(QPoint(255, 255)), first);
  QCOMPARE(*reopened->pixelBytes(QPoint(256, 256)), second);

  auto copy = *reopened;
  const auto changed = QByteArray::fromHex("0102030405060708");
  QCOMPARE(copy.setPixelBytes(QPoint(255, 255), bytesOf(changed)),
           PixelTileWriteResult::Changed);
  QCOMPARE(*reopened->pixelBytes(QPoint(255, 255)), first);
  QCOMPARE(*copy.pixelBytes(QPoint(255, 255)), changed);
}

void PixelStorageTest::rejectsHostileSparseSnapshotsAtomically() {
  const auto rgba8 = PixelFormat::rgba8Premultiplied();
  constexpr qsizetype tileBytes = 256 * 256 * 4;
  QByteArray valid(tileBytes, '\0');
  const auto validSample = QByteArray::fromHex("01010101");
  std::copy(validSample.cbegin(), validSample.cend(), valid.begin());
  const QVector<PixelTileSnapshot> baseline = {
      {{0, 0}, valid},
      {{1, 0}, valid},
  };
  const auto baselineBefore = baseline;
  QVERIFY(SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, baseline, tileBytes * 2, 2));
  QVERIFY(baseline == baselineBefore);

  auto duplicate = baseline;
  duplicate[1].index = {0, 0};
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, duplicate, tileBytes * 2, 2));
  auto outOfGrid = baseline;
  outOfGrid[1].index = {2, 0};
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, outOfGrid, tileBytes * 2, 2));
  auto truncated = baseline;
  truncated[0].packedBytes.chop(1);
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, truncated, tileBytes * 2, 2));
  auto trailing = baseline;
  trailing[0].packedBytes.append('\0');
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, trailing, tileBytes * 2, 2));
  auto noncanonicalZero = baseline;
  noncanonicalZero[0].packedBytes.fill('\0');
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, noncanonicalZero, tileBytes * 2, 2));
  auto hostilePremultiplied = baseline;
  const auto invalid = QByteArray::fromHex("02010101");
  std::copy(invalid.cbegin(), invalid.cend(),
            hostilePremultiplied[0].packedBytes.begin());
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, hostilePremultiplied, tileBytes * 2, 2));
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, baseline, tileBytes, 2));
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, baseline, tileBytes * 2, 1));
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, baseline,
      SparsePixelTileStore::hardMaximumResidentBytes + 1, 2));
  QVERIFY(!SparsePixelTileStore::fromTileSnapshots(
      QSize(512, 256), rgba8, baseline, tileBytes * 2,
      SparsePixelTileStore::hardMaximumResidentTiles + 1));
  QVERIFY(baseline == baselineBefore);
}

QTEST_APPLESS_MAIN(PixelStorageTest)

#include "PixelStorageTest.moc"
