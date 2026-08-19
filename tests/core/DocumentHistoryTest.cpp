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
  DocumentHistory tooSmall(1024, 10);
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

QTEST_APPLESS_MAIN(DocumentHistoryTest)

#include "DocumentHistoryTest.moc"
