#include "core/DocumentHistory.h"

#include <QTest>

#include <memory>

using chromarchy::Document;
using chromarchy::DocumentHistory;
using chromarchy::SnapshotCommand;

namespace {

std::unique_ptr<SnapshotCommand> pixelCommand(const Document& document,
                                              QPoint position, QColor color,
                                              QString description) {
  auto after = document;
  after.layerAt(0)->pixels().setPixelColor(position, color);
  return std::make_unique<SnapshotCommand>(std::move(description), document,
                                           std::move(after));
}

}  // namespace

class DocumentHistoryTest final : public QObject {
  Q_OBJECT

private slots:
  void executesUndoAndRedo();
  void newCommandDiscardsRedoBranch();
  void enforcesMemoryAndCountBounds();
  void largePixelStorageDoesNotBlockMetadataUndo();
};

void DocumentHistoryTest::executesUndoAndRedo() {
  auto document = Document::create(QSize(32, 32));
  QVERIFY(document);
  DocumentHistory history;
  QVERIFY(history.execute(pixelCommand(*document, QPoint(2, 3), Qt::red,
                                       QStringLiteral("Paint pixel")),
                          *document));
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
  QCOMPARE(history.undoDescription(), QStringLiteral("Paint pixel"));
  QVERIFY(history.undo(*document));
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::transparent));
  QCOMPARE(history.redoDescription(), QStringLiteral("Paint pixel"));
  QVERIFY(history.redo(*document));
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(Qt::red));
}

void DocumentHistoryTest::newCommandDiscardsRedoBranch() {
  auto document = Document::create(QSize(32, 32));
  QVERIFY(document);
  DocumentHistory history;
  QVERIFY(history.execute(pixelCommand(*document, QPoint(1, 1), Qt::red,
                                       QStringLiteral("Red")),
                          *document));
  QVERIFY(history.undo(*document));
  QVERIFY(history.execute(pixelCommand(*document, QPoint(1, 1), Qt::blue,
                                       QStringLiteral("Blue")),
                          *document));
  QVERIFY(!history.canRedo());
  QCOMPARE(history.size(), 1);
  QCOMPARE(document->layerAt(0)->pixels().pixelColor(QPoint(1, 1)),
           QColor(Qt::blue));
}

void DocumentHistoryTest::enforcesMemoryAndCountBounds() {
  auto document = Document::create(QSize(32, 32));
  QVERIFY(document);
  DocumentHistory tooSmall(1, 10);
  QVERIFY(!tooSmall.execute(pixelCommand(*document, QPoint(1, 1), Qt::red,
                                         QStringLiteral("Too large")),
                            *document));

  DocumentHistory bounded(DocumentHistory::defaultByteBudget, 2);
  for (int index = 0; index < 3; ++index) {
    QVERIFY(bounded.execute(
        pixelCommand(*document, QPoint(index, 0), Qt::red,
                     QStringLiteral("Command %1").arg(index)),
        *document));
  }
  QCOMPARE(bounded.size(), 2);
  QVERIFY(bounded.estimatedBytes() <= DocumentHistory::defaultByteBudget);
}

void DocumentHistoryTest::largePixelStorageDoesNotBlockMetadataUndo() {
  auto document = Document::create(QSize(20'000, 20'000));
  QVERIFY(document);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      QVERIFY(document->layerAt(0)->pixels().setPixelColor(
          QPoint(column * 256, row * 256), Qt::red));
    }
  }
  QVERIFY(document->estimatedStorageBytes() > 4ULL * 1024ULL * 1024ULL);

  auto renamed = *document;
  renamed.layerAt(0)->setName(QStringLiteral("Metadata edit"));
  DocumentHistory history(64 * 1024, 10);
  QVERIFY(history.execute(std::make_unique<SnapshotCommand>(
                              QStringLiteral("Rename layer"), *document,
                              std::move(renamed)),
                          *document));
  QCOMPARE(document->layerAt(0)->name(), QStringLiteral("Metadata edit"));
  QVERIFY(history.estimatedBytes() < 64 * 1024);
  QVERIFY(history.undo(*document));
  QCOMPARE(document->layerAt(0)->name(), QStringLiteral("Layer 1"));
}

QTEST_APPLESS_MAIN(DocumentHistoryTest)

#include "DocumentHistoryTest.moc"
