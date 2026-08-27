#include "core/NativeDocumentCodec.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

#include <limits>
#include <optional>

using chromarchy::Document;
using chromarchy::NativeDocumentCodec;

namespace {

QByteArray firstPackedTile(const QByteArray& nativeBytes) {
  QBuffer input;
  input.setData(nativeBytes);
  if (!input.open(QIODevice::ReadOnly)) {
    return {};
  }
  QDataStream stream(&input);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  if (stream.skipRawData(8) != 8) {
    return {};
  }
  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  stream >> version >> width >> height >> layerCount >> activeLayer;
  quint32 idSize = 0;
  stream >> idSize;
  if (idSize != 16 || stream.skipRawData(idSize) != static_cast<int>(idSize)) {
    return {};
  }
  quint32 nameSize = 0;
  stream >> nameSize;
  if (stream.skipRawData(nameSize) != static_cast<int>(nameSize)) {
    return {};
  }
  quint8 visible = 0;
  quint8 locked = 0;
  double opacity = 0.0;
  quint32 tileCount = 0;
  int x = 0;
  int y = 0;
  quint32 compressedSize = 0;
  stream >> visible >> locked >> opacity >> tileCount >> x >> y
         >> compressedSize;
  if (stream.status() != QDataStream::Ok || version < 1 || version > 2 ||
      width != 4 || height != 2 || layerCount != 1 || activeLayer != 0 ||
      visible != 1 || locked != 0 || opacity != 1.0 || tileCount != 1 ||
      x != 0 || y != 0 || compressedSize > 4096) {
    return {};
  }
  QByteArray compressed(static_cast<qsizetype>(compressedSize), '\0');
  if (stream.readRawData(compressed.data(), compressed.size()) !=
      compressed.size()) {
    return {};
  }
  return qUncompress(compressed);
}

std::optional<quint32> firstLayerTileCount(const QByteArray& nativeBytes) {
  QBuffer input;
  input.setData(nativeBytes);
  if (!input.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  QDataStream stream(&input);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  if (stream.skipRawData(8) != 8) {
    return std::nullopt;
  }
  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  quint32 idSize = 0;
  stream >> version >> width >> height >> layerCount >> activeLayer >> idSize;
  if (idSize != 16 || stream.skipRawData(idSize) != static_cast<int>(idSize)) {
    return std::nullopt;
  }
  quint32 nameSize = 0;
  stream >> nameSize;
  if (stream.skipRawData(nameSize) != static_cast<int>(nameSize)) {
    return std::nullopt;
  }
  quint8 visible = 0;
  quint8 locked = 0;
  double opacity = 0.0;
  quint32 tileCount = 0;
  stream >> visible >> locked >> opacity >> tileCount;
  if (stream.status() != QDataStream::Ok || version < 1 || version > 2 ||
      width != 4 || height != 2 || layerCount != 1 || activeLayer != 0 ||
      visible != 1 || locked != 0 || opacity != 1.0) {
    return std::nullopt;
  }
  return tileCount;
}

std::optional<quint32> selectionTileCount(const QByteArray& nativeBytes) {
  QBuffer input;
  input.setData(nativeBytes);
  if (!input.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  QDataStream stream(&input);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  QByteArray magic(8, '\0');
  if (stream.readRawData(magic.data(), magic.size()) != magic.size() ||
      magic != QByteArrayLiteral("CHRMDC01")) {
    return std::nullopt;
  }
  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  stream >> version >> width >> height >> layerCount >> activeLayer;
  if (version != 2 || width != 4 || height != 2 || layerCount != 1 ||
      activeLayer != 0) {
    return std::nullopt;
  }
  for (quint32 layerNumber = 0; layerNumber < layerCount; ++layerNumber) {
    quint32 idSize = 0;
    quint32 nameSize = 0;
    stream >> idSize;
    if (idSize != 16 ||
        stream.skipRawData(idSize) != static_cast<int>(idSize)) {
      return std::nullopt;
    }
    stream >> nameSize;
    if (nameSize > 4096 ||
        stream.skipRawData(nameSize) != static_cast<int>(nameSize)) {
      return std::nullopt;
    }
    quint8 visible = 0;
    quint8 locked = 0;
    double opacity = 0.0;
    quint32 pixelTileCount = 0;
    stream >> visible >> locked >> opacity >> pixelTileCount;
    if (stream.status() != QDataStream::Ok || visible != 1 || locked != 0 ||
        opacity != 1.0 || pixelTileCount > 64) {
      return std::nullopt;
    }
    for (quint32 tileNumber = 0; tileNumber < pixelTileCount; ++tileNumber) {
      int x = 0;
      int y = 0;
      quint32 compressedSize = 0;
      stream >> x >> y >> compressedSize;
      if (compressedSize > 4096 ||
          stream.skipRawData(compressedSize) !=
              static_cast<int>(compressedSize)) {
        return std::nullopt;
      }
    }
  }
  quint8 baseCoverage = 0;
  quint32 count = 0;
  stream >> baseCoverage >> count;
  if (stream.status() != QDataStream::Ok) {
    return std::nullopt;
  }
  return count;
}

}  // namespace

class NativeDocumentCodecTest final : public QObject {
  Q_OBJECT

private slots:
  void preservesLayeredDocument();
  void loadsVersionOneWithoutSelection();
  void loadsFixedBaselineRgba8Fixtures();
  void canonicalizesLegacyZeroTilesWithoutChangingSource();
  void canonicalizesLegacyBaseSelectionWithoutChangingSource();
  void rejectsInvalidPremultipliedNativeTile();
  void rejectsCorruptAndTruncatedDocuments();
  void rejectsNonFiniteLayerOpacity();
  void rejectsOversizedSparseFileBeforeParsing();
  void rejectsAggregateTileStorageBeforePayloadDecode();
  void exactAggregateTileLimitRoundTripsWithinFileBound();
  void overLimitSavePreservesExistingDestination();
};

void NativeDocumentCodecTest::preservesLayeredDocument() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("roundtrip.chromarchy"));

  auto document = Document::create(QSize(700, 400));
  QVERIFY(document);
  auto* base = document->layerAt(0);
  base->setName(QStringLiteral("Base / colour"));
  QVERIFY(base->setPixelColor(QPoint(699, 399),
                              QColor(20, 40, 60, 255)));
  base->setLocked(true);
  const auto baseId = base->id();

  const auto inkIndex = document->addLayer(QStringLiteral("Ink"));
  auto* ink = document->layerAt(inkIndex);
  ink->setOpacity(0.625);
  ink->setVisible(false);
  QVERIFY(ink->setPixelColor(QPoint(257, 10),
                             QColor(200, 100, 50, 180)));
  const auto expectedInkPixel = ink->pixels().pixelColor(QPoint(257, 10));
  const auto inkId = ink->id();
  document->selection().selectAll();
  QVERIFY(document->selection().setCoverage(QPoint(5, 6), 32));

  const auto written = NativeDocumentCodec::save(*document, path);
  QVERIFY2(written, qPrintable(written.error));
  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));

  QCOMPARE(loaded.document->size(), QSize(700, 400));
  QCOMPARE(loaded.document->layerCount(), 2);
  QCOMPARE(loaded.document->activeLayerIndex(), 1);
  QCOMPARE(loaded.document->layerAt(0)->id(), baseId);
  QCOMPARE(loaded.document->layerAt(0)->name(), QStringLiteral("Base / colour"));
  QVERIFY(loaded.document->layerAt(0)->isLocked());
  QCOMPARE(loaded.document->layerAt(0)->pixels().pixelColor(QPoint(699, 399)),
           QColor(20, 40, 60, 255));
  QCOMPARE(loaded.document->layerAt(1)->id(), inkId);
  QCOMPARE(loaded.document->layerAt(1)->opacity(), 0.625);
  QVERIFY(!loaded.document->layerAt(1)->isVisible());
  QCOMPARE(loaded.document->layerAt(1)->pixels().pixelColor(QPoint(257, 10)),
           expectedInkPixel);
  QCOMPARE(loaded.document->selection().coverage(QPoint(5, 6)), 32);
  QCOMPARE(loaded.document->selection().coverage(QPoint(600, 300)), 255);
  QCOMPARE(loaded.document->selection().allocatedTileCount(), 1);
}

void NativeDocumentCodecTest::loadsVersionOneWithoutSelection() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("version-one.chromarchy"));
  auto document = Document::create(QSize(40, 30));
  QVERIFY(document);
  document->layerAt(0)->setName(QStringLiteral("Legacy"));
  QVERIFY(NativeDocumentCodec::save(*document, path));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadWrite));
  auto bytes = file.readAll();
  QVERIFY(bytes.size() > 17);
  qToLittleEndian<quint32>(1, reinterpret_cast<uchar*>(bytes.data() + 8));
  bytes.chop(5);  // v2 empty-selection base coverage and tile count.
  QVERIFY(file.resize(0));
  QVERIFY(file.seek(0));
  QCOMPARE(file.write(bytes), bytes.size());
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->size(), QSize(40, 30));
  QCOMPARE(loaded.document->layerAt(0)->name(), QStringLiteral("Legacy"));
  QVERIFY(loaded.document->selection().isEmpty());
}

void NativeDocumentCodecTest::loadsFixedBaselineRgba8Fixtures() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/native-rgba8-baseline.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto fixtureDocument = QJsonDocument::fromJson(fixture.readAll());
  QVERIFY(fixtureDocument.isObject());
  const auto fixtureObject = fixtureDocument.object();
  QCOMPARE(fixtureObject["baselineRevision"].toString(),
           QStringLiteral("341ebfea38aa9e50fd62c57ebd62f0ba95216ae4"));

  const auto versionOneBytes = QByteArray::fromBase64(
      fixtureObject["versionOneBase64"].toString().toLatin1());
  const auto versionTwoBytes = QByteArray::fromBase64(
      fixtureObject["versionTwoBase64"].toString().toLatin1());
  QCOMPARE(QCryptographicHash::hash(versionOneBytes,
                                    QCryptographicHash::Sha256)
               .toHex(),
           fixtureObject["versionOneSha256"].toString().toLatin1());
  QCOMPARE(QCryptographicHash::hash(versionTwoBytes,
                                    QCryptographicHash::Sha256)
               .toHex(),
           fixtureObject["versionTwoSha256"].toString().toLatin1());
  QCOMPARE(firstPackedTile(versionOneBytes).first(16),
           QByteArray::fromHex(
               fixtureObject["packedPremultipliedPrefixHex"]
                   .toString()
                   .toLatin1()));
  QCOMPARE(firstPackedTile(versionTwoBytes).first(16),
           QByteArray::fromHex(
               fixtureObject["packedPremultipliedPrefixHex"]
                   .toString()
                   .toLatin1()));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QList<QPair<QString, QByteArray>> versions = {
      {QStringLiteral("v1"), versionOneBytes},
      {QStringLiteral("v2"), versionTwoBytes}};
  for (const auto& [name, sourceBytes] : versions) {
    const auto sourcePath = directory.filePath(name + ".chromarchy");
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(sourceBytes), sourceBytes.size());
    source.close();

    const auto loaded = NativeDocumentCodec::load(sourcePath);
    QVERIFY2(loaded, qPrintable(loaded.error));
    QCOMPARE(loaded.document->size(), QSize(4, 2));
    QCOMPARE(loaded.document->layerAt(0)->name(),
             QStringLiteral("Baseline RGBA8"));
    QCOMPARE(loaded.document->layerAt(0)
                 ->pixels()
                 .pixelColor(QPoint(0, 0))
                 .rgba(),
             qRgba(255, 255, 0, 1));
    QCOMPARE(loaded.document->layerAt(0)
                 ->pixels()
                 .pixelColor(QPoint(1, 0))
                 .rgba(),
             qRgba(10, 20, 30, 128));
    QCOMPARE(loaded.document->layerAt(0)
                 ->pixels()
                 .pixelColor(QPoint(2, 0))
                 .rgba(),
             qRgba(1, 128, 255, 254));
    QCOMPARE(loaded.document->layerAt(0)
                 ->pixels()
                 .pixelColor(QPoint(3, 0))
                 .rgba(),
             qRgba(1, 2, 3, 255));

    const auto reopenedPath = directory.filePath(name + "-reopen.chromarchy");
    QVERIFY(NativeDocumentCodec::save(*loaded.document, reopenedPath));
    QFile reopenedFile(reopenedPath);
    QVERIFY(reopenedFile.open(QIODevice::ReadOnly));
    QCOMPARE(reopenedFile.readAll(), versionTwoBytes);
    const auto reopened = NativeDocumentCodec::load(reopenedPath);
    QVERIFY2(reopened, qPrintable(reopened.error));
  }
}

void NativeDocumentCodecTest::canonicalizesLegacyZeroTilesWithoutChangingSource() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/native-rgba8-baseline.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto fixtureObject = QJsonDocument::fromJson(fixture.readAll()).object();
  const auto sourceBytes = QByteArray::fromBase64(
      fixtureObject["legacyZeroVersionTwoBase64"].toString().toLatin1());
  QCOMPARE(QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256)
               .toHex(),
           fixtureObject["legacyZeroVersionTwoSha256"].toString().toLatin1());
  QCOMPARE(firstLayerTileCount(sourceBytes), std::optional<quint32>(1));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto sourcePath =
      directory.filePath(QStringLiteral("legacy-zero.chromarchy"));
  QFile source(sourcePath);
  QVERIFY(source.open(QIODevice::WriteOnly));
  QCOMPARE(source.write(sourceBytes), sourceBytes.size());
  source.close();

  const auto loaded = NativeDocumentCodec::load(sourcePath);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->layerCount(), 1);
  QCOMPARE(loaded.document->layerAt(0)->name(),
           QStringLiteral("Legacy Zero Tile"));
  QCOMPARE(loaded.document->layerAt(0)->pixels().allocatedTileCount(), 0);
  QCOMPARE(loaded.document->composite().pixelColor(QPoint()),
           QColor(Qt::transparent));
  QVERIFY(source.open(QIODevice::ReadOnly));
  QCOMPARE(source.readAll(), sourceBytes);
  source.close();

  const auto canonicalPath =
      directory.filePath(QStringLiteral("canonical-zero.chromarchy"));
  const auto saved = NativeDocumentCodec::save(*loaded.document, canonicalPath);
  QVERIFY2(saved, qPrintable(saved.error));
  QFile canonical(canonicalPath);
  QVERIFY(canonical.open(QIODevice::ReadOnly));
  const auto canonicalBytes = canonical.readAll();
  QCOMPARE(firstLayerTileCount(canonicalBytes), std::optional<quint32>(0));
  const auto reopened = NativeDocumentCodec::load(canonicalPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 1);
  QCOMPARE(reopened.document->layerAt(0)->pixels().allocatedTileCount(), 0);
  QCOMPARE(reopened.document->composite().pixelColor(QPoint()),
           QColor(Qt::transparent));
}

void NativeDocumentCodecTest::canonicalizesLegacyBaseSelectionWithoutChangingSource() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/native-rgba8-baseline.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto fixtureObject = QJsonDocument::fromJson(fixture.readAll()).object();
  const auto sourceBytes = QByteArray::fromBase64(
      fixtureObject["legacyBaseSelectionVersionTwoBase64"]
          .toString()
          .toLatin1());
  QCOMPARE(QCryptographicHash::hash(sourceBytes, QCryptographicHash::Sha256)
               .toHex(),
           fixtureObject["legacyBaseSelectionVersionTwoSha256"]
               .toString()
               .toLatin1());
  QCOMPARE(selectionTileCount(sourceBytes), std::optional<quint32>(1));

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto sourcePath =
      directory.filePath(QStringLiteral("legacy-base-selection.chromarchy"));
  QFile source(sourcePath);
  QVERIFY(source.open(QIODevice::WriteOnly));
  QCOMPARE(source.write(sourceBytes), sourceBytes.size());
  source.close();

  const auto loaded = NativeDocumentCodec::load(sourcePath);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->selection().baseCoverage(), 255);
  QCOMPARE(loaded.document->selection().allocatedTileCount(), 0);
  QCOMPARE(loaded.document->selection().coverage(QPoint(3, 1)), 255);
  QVERIFY(source.open(QIODevice::ReadOnly));
  QCOMPARE(source.readAll(), sourceBytes);
  source.close();

  const auto canonicalPath = directory.filePath(
      QStringLiteral("canonical-base-selection.chromarchy"));
  const auto saved = NativeDocumentCodec::save(*loaded.document, canonicalPath);
  QVERIFY2(saved, qPrintable(saved.error));
  QFile canonical(canonicalPath);
  QVERIFY(canonical.open(QIODevice::ReadOnly));
  QCOMPARE(selectionTileCount(canonical.readAll()),
           std::optional<quint32>(0));
  const auto reopened = NativeDocumentCodec::load(canonicalPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->selection().baseCoverage(), 255);
  QCOMPARE(reopened.document->selection().allocatedTileCount(), 0);
  QCOMPARE(reopened.document->selection().coverage(QPoint(3, 1)), 255);
}

void NativeDocumentCodecTest::rejectsInvalidPremultipliedNativeTile() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/native-rgba8-baseline.json"));
  QVERIFY2(fixture.open(QIODevice::ReadOnly), qPrintable(fixture.errorString()));
  const auto fixtureObject = QJsonDocument::fromJson(fixture.readAll()).object();
  const auto hostileBytes = QByteArray::fromBase64(
      fixtureObject["hostileVersionTwoBase64"].toString().toLatin1());
  QCOMPARE(QCryptographicHash::hash(hostileBytes, QCryptographicHash::Sha256)
               .toHex(),
           fixtureObject["hostileVersionTwoSha256"].toString().toLatin1());

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto sourcePath = directory.filePath(QStringLiteral("hostile.chromarchy"));
  QFile source(sourcePath);
  QVERIFY(source.open(QIODevice::WriteOnly));
  QCOMPARE(source.write(hostileBytes), hostileBytes.size());
  source.close();

  const auto loaded = NativeDocumentCodec::load(sourcePath);
  QVERIFY(!loaded);
  QVERIFY(loaded.error.contains(QStringLiteral("tile conversion")));
  QVERIFY(source.open(QIODevice::ReadOnly));
  QCOMPARE(source.readAll(), hostileBytes);
}

void NativeDocumentCodecTest::rejectsCorruptAndTruncatedDocuments() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto corruptPath = directory.filePath(QStringLiteral("corrupt.chromarchy"));
  QFile corrupt(corruptPath);
  QVERIFY(corrupt.open(QIODevice::WriteOnly));
  QCOMPARE(corrupt.write("not-a-document"), 14);
  corrupt.close();
  QVERIFY(!NativeDocumentCodec::load(corruptPath));

  auto document = Document::create(QSize(10, 10));
  QVERIFY(document);
  const auto truncatedPath =
      directory.filePath(QStringLiteral("truncated.chromarchy"));
  QVERIFY(NativeDocumentCodec::save(*document, truncatedPath));
  QFile truncated(truncatedPath);
  QVERIFY(truncated.open(QIODevice::ReadWrite));
  QVERIFY(truncated.resize(truncated.size() - 1));
  truncated.close();
  QVERIFY(!NativeDocumentCodec::load(truncatedPath));
}

void NativeDocumentCodecTest::rejectsNonFiniteLayerOpacity() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("non-finite.chromarchy"));
  auto document = Document::create(QSize(10, 10));
  QVERIFY(document);
  QVERIFY(NativeDocumentCodec::save(*document, path));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadWrite));
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  char magic[8]{};
  QCOMPARE(stream.readRawData(magic, sizeof(magic)), sizeof(magic));
  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  stream >> version >> width >> height >> layerCount >> activeLayer;
  quint32 idSize = 0;
  stream >> idSize;
  QCOMPARE(stream.skipRawData(idSize), static_cast<int>(idSize));
  quint32 nameSize = 0;
  stream >> nameSize;
  QCOMPARE(stream.skipRawData(nameSize), static_cast<int>(nameSize));
  quint8 visible = 0;
  quint8 locked = 0;
  stream >> visible >> locked;
  const auto opacityOffset = file.pos();
  QVERIFY(opacityOffset > 0);
  QVERIFY(file.seek(opacityOffset));
  QDataStream writer(&file);
  writer.setVersion(QDataStream::Qt_6_6);
  writer.setByteOrder(QDataStream::LittleEndian);
  writer << std::numeric_limits<double>::quiet_NaN();
  QCOMPARE(writer.status(), QDataStream::Ok);
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY(!loaded);
  QVERIFY(loaded.error.contains(QStringLiteral("layer properties")));
}

void NativeDocumentCodecTest::rejectsOversizedSparseFileBeforeParsing() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("oversized.chromarchy"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.resize(
      static_cast<qint64>(NativeDocumentCodec::maximumNativeFileBytes + 1)));
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY(!loaded);
  QVERIFY(loaded.error.contains(QStringLiteral("file size limit")));
}

void NativeDocumentCodecTest::rejectsAggregateTileStorageBeforePayloadDecode() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("tile-budget.chromarchy"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
  QCOMPARE(stream.writeRawData("CHRMDC01", 8), 8);
  stream << quint32(2) << 300'000 << 300'000 << quint32(1) << 0;
  const auto id = QUuid::createUuid().toRfc4122();
  stream << quint32(id.size());
  QCOMPARE(stream.writeRawData(id.constData(), id.size()), id.size());
  stream << quint32(0) << quint8(1) << quint8(0) << 1.0
         << quint32(NativeDocumentCodec::maximumStoredTileCount + 1);
  file.close();

  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY(!loaded);
  QVERIFY(loaded.error.contains(QStringLiteral("aggregate tile storage limit")));
}

void NativeDocumentCodecTest::exactAggregateTileLimitRoundTripsWithinFileBound() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("exact-limit.chromarchy"));
  auto document = Document::create(QSize(2048, 2048));
  QVERIFY(document);
  for (quint64 index = 0;
       index < NativeDocumentCodec::maximumStoredTileCount; ++index) {
    const QPoint position(static_cast<int>(index % 8) * 256,
                          static_cast<int>(index / 8) * 256);
    QVERIFY(document->layerAt(0)->setPixelColor(position, QColor(1, 2, 3, 255)));
  }

  const auto saved = NativeDocumentCodec::save(*document, path);
  QVERIFY2(saved, qPrintable(saved.error));
  const QFileInfo output(path);
  QVERIFY(output.size() > 0);
  QVERIFY(static_cast<quint64>(output.size()) <=
          NativeDocumentCodec::maximumNativeFileBytes);
  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->layerAt(0)->pixels().allocatedTileCount(),
           static_cast<qsizetype>(NativeDocumentCodec::maximumStoredTileCount));
}

void NativeDocumentCodecTest::overLimitSavePreservesExistingDestination() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("preserved.chromarchy"));
  const QByteArray original("existing destination bytes");
  QFile destination(path);
  QVERIFY(destination.open(QIODevice::WriteOnly));
  QCOMPARE(destination.write(original), original.size());
  destination.close();

  auto document = Document::create(QSize(2304, 2048));
  QVERIFY(document);
  for (quint64 index = 0;
       index <= NativeDocumentCodec::maximumStoredTileCount; ++index) {
    const QPoint position(static_cast<int>(index % 9) * 256,
                          static_cast<int>(index / 9) * 256);
    QVERIFY(document->layerAt(0)->setPixelColor(position, Qt::red));
  }

  const auto saved = NativeDocumentCodec::save(*document, path);
  QVERIFY(!saved);
  QVERIFY(saved.error.contains(QStringLiteral("64-tile limit")));
  QVERIFY(saved.error.contains(QStringLiteral("destination was preserved")));
  QVERIFY(destination.open(QIODevice::ReadOnly));
  QCOMPARE(destination.readAll(), original);
}

QTEST_APPLESS_MAIN(NativeDocumentCodecTest)

#include "NativeDocumentCodecTest.moc"
