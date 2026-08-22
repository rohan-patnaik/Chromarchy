#include "core/NativeDocumentCodec.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace chromarchy {
namespace {

constexpr char magic[] = {'C', 'H', 'R', 'M', 'D', 'C', '0', '1'};
constexpr quint32 maximumNameBytes = 4'096;
constexpr qsizetype tileByteCount =
    TiledImage::tileExtent * TiledImage::tileExtent * 4;
constexpr quint32 maximumCompressedTileBytes =
    static_cast<quint32>(tileByteCount + 4'096);
constexpr qsizetype selectionTileByteCount =
    TiledImage::tileExtent * TiledImage::tileExtent;
constexpr quint32 maximumCompressedSelectionTileBytes =
    static_cast<quint32>(selectionTileByteCount + 4'096);

void configureStream(QDataStream& stream) {
  stream.setVersion(QDataStream::Qt_6_6);
  stream.setByteOrder(QDataStream::LittleEndian);
}

bool writeBytes(QDataStream& stream, const QByteArray& bytes) {
  stream << static_cast<quint32>(bytes.size());
  return stream.writeRawData(bytes.constData(), bytes.size()) == bytes.size();
}

bool readBytes(QDataStream& stream, quint32 maximumSize, QByteArray& bytes) {
  quint32 size = 0;
  stream >> size;
  if (stream.status() != QDataStream::Ok || size > maximumSize) {
    return false;
  }
  bytes.resize(static_cast<qsizetype>(size));
  return stream.readRawData(bytes.data(), static_cast<qsizetype>(size)) ==
         static_cast<qsizetype>(size);
}

std::span<const std::byte> bytesOf(const QByteArray& bytes) {
  return {reinterpret_cast<const std::byte*>(bytes.constData()),
          static_cast<std::size_t>(bytes.size())};
}

std::optional<QByteArray> tileBytes(const QImage& image) {
  const auto normalized =
      image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
  const auto encoded = rgba8BytesFromImage(
      normalized, AlphaMode::Premultiplied, 1,
      static_cast<quint64>(tileByteCount));
  if (!encoded || encoded->bytes.size() != tileByteCount) {
    return std::nullopt;
  }
  return encoded->bytes;
}

bool isValidCompressedPayload(const QByteArray& bytes, qsizetype expectedBytes,
                              quint32 maximumBytes) {
  if (bytes.size() < 4 || bytes.size() > maximumBytes) {
    return false;
  }
  return qFromBigEndian<quint32>(
             reinterpret_cast<const uchar*>(bytes.constData())) ==
         expectedBytes;
}

QString streamError(const QString& detail) {
  return QStringLiteral("Invalid Chromarchy document: %1").arg(detail);
}

QString saveLimitError(const QString& detail) {
  return QStringLiteral("Chromarchy document was not saved: %1. The existing "
                        "destination was preserved.")
      .arg(detail);
}

bool exceedsWrittenFileLimit(const QSaveFile& output) {
  return output.pos() < 0 ||
         static_cast<quint64>(output.pos()) >
             NativeDocumentCodec::maximumNativeFileBytes;
}

}  // namespace

NativeDocumentWriteResult NativeDocumentCodec::save(const Document& document,
                                                     const QString& filePath) {
  if (document.layers_.isEmpty() ||
      document.layers_.size() > NativeDocumentCodec::maximumLayerCount) {
    return {.error = QStringLiteral("The document layer count cannot be saved.")};
  }

  quint64 aggregateTileCount = document.selection_.tiles_.size();
  quint64 aggregateDecodedBytes =
      aggregateTileCount * static_cast<quint64>(selectionTileByteCount);
  for (const auto& layer : document.layers_) {
    const auto layerTileCount =
        static_cast<quint64>(layer.pixels_.allocatedTileCount());
    if (layerTileCount > NativeDocumentCodec::maximumStoredTileCount ||
        aggregateTileCount >
        NativeDocumentCodec::maximumStoredTileCount - layerTileCount) {
      return {.error = saveLimitError(QStringLiteral(
                  "stored tile count exceeds the %1-tile limit")
                  .arg(NativeDocumentCodec::maximumStoredTileCount))};
    }
    aggregateTileCount += layerTileCount;
    const auto layerDecodedBytes =
        layerTileCount * static_cast<quint64>(tileByteCount);
    if (layerDecodedBytes >
            NativeDocumentCodec::maximumDecodedStorageBytes ||
        aggregateDecodedBytes >
        NativeDocumentCodec::maximumDecodedStorageBytes - layerDecodedBytes) {
      return {.error = saveLimitError(QStringLiteral(
                  "decoded tile storage exceeds the %1-byte limit")
                  .arg(NativeDocumentCodec::maximumDecodedStorageBytes))};
    }
    aggregateDecodedBytes += layerDecodedBytes;
  }
  if (aggregateTileCount > NativeDocumentCodec::maximumStoredTileCount ||
      aggregateDecodedBytes >
          NativeDocumentCodec::maximumDecodedStorageBytes) {
    return {.error = saveLimitError(QStringLiteral("aggregate tile storage limit"))};
  }

  QSaveFile output(filePath);
  if (!output.open(QIODevice::WriteOnly)) {
    return {.error = output.errorString()};
  }

  QDataStream stream(&output);
  configureStream(stream);
  if (stream.writeRawData(magic, sizeof(magic)) != sizeof(magic)) {
    output.cancelWriting();
    return {.error = output.errorString()};
  }
  stream << formatVersion << document.size_.width() << document.size_.height()
         << static_cast<quint32>(document.layers_.size())
         << document.activeLayerIndex_;
  quint64 aggregateCompressedBytes = 0;

  for (const auto& layer : document.layers_) {
    if (!std::isfinite(layer.opacity_) || layer.opacity_ < 0.0 ||
        layer.opacity_ > 1.0 || layer.pixels_.size() != document.size_) {
      output.cancelWriting();
      return {.error = QStringLiteral(
                  "A layer has invalid opacity or pixel dimensions.")};
    }
    const auto id = layer.id_.toRfc4122();
    const auto name = layer.name_.toUtf8();
    if (name.size() > maximumNameBytes) {
      output.cancelWriting();
      return {.error = QStringLiteral("A layer name is too long to save.")};
    }
    if (!writeBytes(stream, id) || !writeBytes(stream, name)) {
      output.cancelWriting();
      return {.error = output.errorString()};
    }

    stream << static_cast<quint8>(layer.visible_)
           << static_cast<quint8>(layer.locked_) << layer.opacity_;
    auto tiles = layer.pixels_.tileSnapshots();
    std::ranges::sort(tiles, [](const auto& left, const auto& right) {
      return left.origin.y() < right.origin.y() ||
             (left.origin.y() == right.origin.y() &&
              left.origin.x() < right.origin.x());
    });
    stream << static_cast<quint32>(tiles.size());
    for (const auto& tile : tiles) {
      stream << tile.origin.x() << tile.origin.y();
      const auto raw = tileBytes(tile.pixels);
      if (!raw) {
        output.cancelWriting();
        return {.error = QStringLiteral("A layer has an invalid pixel tile.")};
      }
      const auto compressed = qCompress(*raw, 6);
      if (compressed.size() > maximumCompressedTileBytes ||
          aggregateCompressedBytes >
              NativeDocumentCodec::maximumCompressedStorageBytes -
                  static_cast<quint64>(compressed.size()) ||
          !writeBytes(stream, compressed)) {
        output.cancelWriting();
        return {.error = saveLimitError(QStringLiteral(
                    "compressed tile storage exceeds the %1-byte limit")
                    .arg(NativeDocumentCodec::maximumCompressedStorageBytes))};
      }
      aggregateCompressedBytes += static_cast<quint64>(compressed.size());
      if (exceedsWrittenFileLimit(output)) {
        output.cancelWriting();
        return {.error = saveLimitError(QStringLiteral(
                    "written output exceeds the %1-byte file limit")
                    .arg(NativeDocumentCodec::maximumNativeFileBytes))};
      }
    }
  }

  stream << document.selection_.baseCoverage_;
  QVector<TileSnapshot> selectionTiles;
  selectionTiles.reserve(document.selection_.tiles_.size());
  for (auto tile = document.selection_.tiles_.cbegin();
       tile != document.selection_.tiles_.cend(); ++tile) {
    selectionTiles.push_back(
        {SelectionMask::tileOrigin(tile.key()), tile.value()});
  }
  std::ranges::sort(selectionTiles, [](const auto& left, const auto& right) {
    return left.origin.y() < right.origin.y() ||
           (left.origin.y() == right.origin.y() &&
            left.origin.x() < right.origin.x());
  });
  stream << static_cast<quint32>(selectionTiles.size());
  for (const auto& tile : selectionTiles) {
    stream << tile.origin.x() << tile.origin.y();
    const QByteArray raw(
        reinterpret_cast<const char*>(tile.pixels.constBits()),
        static_cast<qsizetype>(tile.pixels.sizeInBytes()));
    const auto compressed = qCompress(raw, 6);
    if (compressed.size() > maximumCompressedSelectionTileBytes ||
        aggregateCompressedBytes >
            NativeDocumentCodec::maximumCompressedStorageBytes -
                static_cast<quint64>(compressed.size()) ||
        !writeBytes(stream, compressed)) {
      output.cancelWriting();
      return {.error = saveLimitError(QStringLiteral(
                  "compressed tile storage exceeds the %1-byte limit")
                  .arg(NativeDocumentCodec::maximumCompressedStorageBytes))};
    }
    aggregateCompressedBytes += static_cast<quint64>(compressed.size());
    if (exceedsWrittenFileLimit(output)) {
      output.cancelWriting();
      return {.error = saveLimitError(QStringLiteral(
                  "written output exceeds the %1-byte file limit")
                  .arg(NativeDocumentCodec::maximumNativeFileBytes))};
    }
  }

  if (stream.status() != QDataStream::Ok) {
    output.cancelWriting();
    return {.error = output.errorString()};
  }
  if (!output.commit()) {
    return {.error = output.errorString()};
  }
  return {.success = true};
}

NativeDocumentLoadResult NativeDocumentCodec::load(const QString& filePath) {
  QFile input(filePath);
  if (!input.open(QIODevice::ReadOnly)) {
    return {.error = input.errorString()};
  }
  if (input.size() < static_cast<qint64>(sizeof(magic)) ||
      static_cast<quint64>(input.size()) >
          NativeDocumentCodec::maximumNativeFileBytes) {
    return {.error = streamError(QStringLiteral("file size limit"))};
  }

  QDataStream stream(&input);
  configureStream(stream);
  char header[sizeof(magic)]{};
  if (stream.readRawData(header, sizeof(header)) != sizeof(header) ||
      std::memcmp(header, magic, sizeof(magic)) != 0) {
    return {.error = streamError(QStringLiteral("file signature"))};
  }

  quint32 version = 0;
  int width = 0;
  int height = 0;
  quint32 layerCount = 0;
  int activeLayer = -1;
  stream >> version >> width >> height >> layerCount >> activeLayer;
  if (version == 0 || version > formatVersion) {
    return {.error = streamError(QStringLiteral("unsupported version %1").arg(version))};
  }
  if (layerCount == 0 || layerCount > NativeDocumentCodec::maximumLayerCount ||
      activeLayer < 0 || activeLayer >= static_cast<int>(layerCount)) {
    return {.error = streamError(QStringLiteral("layer table"))};
  }

  auto document = Document::create(QSize(width, height));
  if (!document) {
    return {.error = streamError(QStringLiteral("canvas dimensions"))};
  }
  document->layers_.clear();
  document->layers_.reserve(layerCount);

  const quint64 columns =
      (static_cast<quint64>(width) + TiledImage::tileExtent - 1) /
      TiledImage::tileExtent;
  const quint64 rows =
      (static_cast<quint64>(height) + TiledImage::tileExtent - 1) /
      TiledImage::tileExtent;
  const quint64 maximumTileCount = columns * rows;
  quint64 aggregateTileCount = 0;
  quint64 aggregateCompressedBytes = 0;
  quint64 aggregateDecodedBytes = 0;

  for (quint32 layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
    QByteArray idBytes;
    QByteArray nameBytes;
    if (!readBytes(stream, 16, idBytes) || idBytes.size() != 16 ||
        !readBytes(stream, maximumNameBytes, nameBytes)) {
      return {.error = streamError(QStringLiteral("layer identity"))};
    }

    quint8 visible = 0;
    quint8 locked = 0;
    double opacity = 0.0;
    quint32 tileCount = 0;
    stream >> visible >> locked >> opacity >> tileCount;
    if (stream.status() != QDataStream::Ok || visible > 1 || locked > 1 ||
        !std::isfinite(opacity) || opacity < 0.0 || opacity > 1.0 ||
        tileCount > maximumTileCount) {
      return {.error = streamError(QStringLiteral("layer properties"))};
    }
    aggregateTileCount += tileCount;
    const auto layerDecodedBytes = static_cast<quint64>(tileCount) *
                                   static_cast<quint64>(tileByteCount);
    if (aggregateTileCount > NativeDocumentCodec::maximumStoredTileCount ||
        aggregateDecodedBytes >
            NativeDocumentCodec::maximumDecodedStorageBytes - layerDecodedBytes) {
      return {.error = streamError(QStringLiteral("aggregate tile storage limit"))};
    }
    aggregateDecodedBytes += layerDecodedBytes;

    document->layers_.emplaceBack(QString::fromUtf8(nameBytes), document->size_);
    auto& layer = document->layers_.back();
    layer.id_ = QUuid::fromRfc4122(idBytes);
    layer.visible_ = visible != 0;
    layer.locked_ = locked != 0;
    layer.opacity_ = opacity;

    QSet<TileIndex> seenTiles;
    for (quint32 tileNumber = 0; tileNumber < tileCount; ++tileNumber) {
      int x = 0;
      int y = 0;
      stream >> x >> y;
      const TileIndex index{x / TiledImage::tileExtent,
                            y / TiledImage::tileExtent};
      if (stream.status() != QDataStream::Ok || x < 0 || y < 0 ||
          x % TiledImage::tileExtent != 0 || y % TiledImage::tileExtent != 0 ||
          x >= width || y >= height || seenTiles.contains(index)) {
        return {.error = streamError(QStringLiteral("tile location"))};
      }
      seenTiles.insert(index);

      QByteArray compressed;
      if (!readBytes(stream, maximumCompressedTileBytes, compressed) ||
          !isValidCompressedPayload(compressed, tileByteCount,
                                    maximumCompressedTileBytes)) {
        return {.error = streamError(QStringLiteral("tile payload"))};
      }
      if (aggregateCompressedBytes >
          NativeDocumentCodec::maximumCompressedStorageBytes -
              static_cast<quint64>(compressed.size())) {
        return {.error = streamError(QStringLiteral("compressed storage limit"))};
      }
      aggregateCompressedBytes += static_cast<quint64>(compressed.size());
      const auto pixels = qUncompress(compressed);
      if (pixels.size() != tileByteCount) {
        return {.error = streamError(QStringLiteral("tile decompression"))};
      }

      const auto layout = TiledImage::tileStorageLayout(tileByteCount);
      auto tile = layout ? rgba8ImageFromBytes(bytesOf(pixels), *layout,
                                               tileByteCount)
                         : std::nullopt;
      if (!tile) {
        return {.error = streamError(QStringLiteral("tile conversion"))};
      }
      layer.pixels_.tiles_.insert(index, std::move(*tile));
    }
  }

  if (version >= 2) {
    quint8 baseCoverage = 0;
    quint32 selectionTileCount = 0;
    stream >> baseCoverage >> selectionTileCount;
    if (stream.status() != QDataStream::Ok ||
        selectionTileCount > maximumTileCount) {
      return {.error = streamError(QStringLiteral("selection properties"))};
    }
    aggregateTileCount += selectionTileCount;
    const auto selectionDecodedBytes =
        static_cast<quint64>(selectionTileCount) *
        static_cast<quint64>(selectionTileByteCount);
    if (aggregateTileCount > NativeDocumentCodec::maximumStoredTileCount ||
        aggregateDecodedBytes >
            NativeDocumentCodec::maximumDecodedStorageBytes -
                selectionDecodedBytes) {
      return {.error = streamError(QStringLiteral("aggregate selection storage limit"))};
    }
    aggregateDecodedBytes += selectionDecodedBytes;
    document->selection_.baseCoverage_ = baseCoverage;
    QSet<TileIndex> seenSelectionTiles;
    for (quint32 tileNumber = 0; tileNumber < selectionTileCount; ++tileNumber) {
      int x = 0;
      int y = 0;
      stream >> x >> y;
      const TileIndex index{x / TiledImage::tileExtent,
                            y / TiledImage::tileExtent};
      if (stream.status() != QDataStream::Ok || x < 0 || y < 0 ||
          x % TiledImage::tileExtent != 0 || y % TiledImage::tileExtent != 0 ||
          x >= width || y >= height || seenSelectionTiles.contains(index)) {
        return {.error = streamError(QStringLiteral("selection tile location"))};
      }
      seenSelectionTiles.insert(index);

      QByteArray compressed;
      if (!readBytes(stream, maximumCompressedSelectionTileBytes, compressed) ||
          !isValidCompressedPayload(compressed, selectionTileByteCount,
                                    maximumCompressedSelectionTileBytes)) {
        return {.error = streamError(QStringLiteral("selection tile payload"))};
      }
      if (aggregateCompressedBytes >
          NativeDocumentCodec::maximumCompressedStorageBytes -
              static_cast<quint64>(compressed.size())) {
        return {.error = streamError(QStringLiteral("compressed storage limit"))};
      }
      aggregateCompressedBytes += static_cast<quint64>(compressed.size());
      const auto coverage = qUncompress(compressed);
      if (coverage.size() != selectionTileByteCount) {
        return {.error = streamError(
                    QStringLiteral("selection tile decompression"))};
      }
      QImage tile(TiledImage::tileExtent, TiledImage::tileExtent,
                  QImage::Format_Grayscale8);
      std::memcpy(tile.bits(), coverage.constData(), selectionTileByteCount);
      document->selection_.tiles_.insert(index, std::move(tile));
    }
  }

  if (stream.status() != QDataStream::Ok || !input.atEnd()) {
    return {.error = streamError(QStringLiteral("trailing or truncated data"))};
  }
  document->activeLayerIndex_ = activeLayer;
  return {.document = std::move(document)};
}

}  // namespace chromarchy
