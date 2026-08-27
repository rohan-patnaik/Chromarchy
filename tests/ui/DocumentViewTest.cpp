#include "core/Document.h"
#include "core/NativeDocumentCodec.h"
#include "ui/CanvasWidget.h"
#include "ui/DocumentView.h"

#include <QTemporaryDir>
#include <QTest>

using chromarchy::Document;
using chromarchy::DocumentView;
using chromarchy::NativeDocumentCodec;

class DocumentViewTest final : public QObject {
  Q_OBJECT

private slots:
  void noOpSelectionsPreserveSavedState();
  void transparentMergeElisionSurvivesUndoRedoAndSave();
  void tracksSavedRevisionAcrossUndoRedoAndBranching();
  void evictedSavedRevisionNeverAppearsClean();
};

void DocumentViewTest::noOpSelectionsPreserveSavedState() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  DocumentView view(std::move(*document), QStringLiteral("Saved"), {}, true);
  const auto path = directory.filePath(QStringLiteral("saved.chromarchy"));
  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());

  QVERIFY(view.performCommand(QStringLiteral("Select all"), [](Document& changed) {
    return changed.selection().selectAll();
  }));
  QVERIFY(view.isModified());
  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());
  const auto historySize = view.history().size();

  QVERIFY(!view.performCommand(QStringLiteral("Select all"), [](Document& changed) {
    return changed.selection().selectAll();
  }));
  QVERIFY(!view.isModified());
  QCOMPARE(view.history().size(), historySize);

  QVERIFY(view.performCommand(QStringLiteral("Deselect"), [](Document& changed) {
    return changed.selection().clear();
  }));
  QVERIFY(view.save(path));
  QVERIFY(!view.performCommand(QStringLiteral("Deselect"), [](Document& changed) {
    return changed.selection().clear();
  }));
  QVERIFY(!view.isModified());

  const QRect rectangle(10, 12, 30, 32);
  view.canvas()->selectionRequested(rectangle);
  QVERIFY(view.isModified());
  QVERIFY(view.save(path));
  const auto rectangleHistorySize = view.history().size();

  view.canvas()->selectionRequested(rectangle);
  QVERIFY(!view.isModified());
  QCOMPARE(view.history().size(), rectangleHistorySize);
}

void DocumentViewTest::transparentMergeElisionSurvivesUndoRedoAndSave() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto document = Document::create(QSize(512, 512));
  QVERIFY(document);
  QVERIFY(document->layerAt(0)->setPixelColor(QPoint(300, 300), Qt::red));
  document->layerAt(0)->setOpacity(0.0);
  const auto top = document->addLayer(QStringLiteral("Top"));
  QVERIFY(document->layerAt(top)->setPixelColor(QPoint(4, 5), Qt::blue));
  document->layerAt(top)->setOpacity(0.0);

  DocumentView view(std::move(*document), QStringLiteral("Transparent"), {},
                    true);
  const auto path = directory.filePath(QStringLiteral("merged.chromarchy"));
  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());
  QVERIFY(view.performCommand(QStringLiteral("Merge layer down"),
                              [top](Document& changed) {
                                return changed.mergeLayerDown(top);
                              }));
  QVERIFY(view.isModified());
  QCOMPARE(view.document().layerCount(), 1);
  QCOMPARE(view.document().layerAt(0)->pixels().allocatedTileCount(), 0);

  QVERIFY(view.undo());
  QCOMPARE(view.document().layerCount(), 2);
  QCOMPARE(view.document().layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(view.document().layerAt(1)->pixels().allocatedTileCount(), 1);
  QVERIFY(view.redo());
  QCOMPARE(view.document().layerCount(), 1);
  QCOMPARE(view.document().layerAt(0)->pixels().allocatedTileCount(), 0);

  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());
  const auto loaded = NativeDocumentCodec::load(path);
  QVERIFY2(loaded, qPrintable(loaded.error));
  QCOMPARE(loaded.document->layerCount(), 1);
  QCOMPARE(loaded.document->layerAt(0)->pixels().allocatedTileCount(), 0);
  QCOMPARE(loaded.document->composite().pixelColor(QPoint(4, 5)),
           QColor(Qt::transparent));
}

void DocumentViewTest::tracksSavedRevisionAcrossUndoRedoAndBranching() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  DocumentView view(std::move(*document), QStringLiteral("Revision"), {}, true);
  QVERIFY(view.isModified());
  const auto path = directory.filePath(QStringLiteral("revision.chromarchy"));
  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());

  QVERIFY(view.performCommand(QStringLiteral("Select all"), [](Document& changed) {
    return changed.selection().selectAll();
  }));
  QVERIFY(view.isModified());
  QVERIFY(view.undo());
  QVERIFY(!view.isModified());
  QVERIFY(view.redo());
  QVERIFY(view.isModified());

  QVERIFY(view.save(path));
  QVERIFY(!view.isModified());
  QVERIFY(view.undo());
  QVERIFY(view.isModified());
  QVERIFY(view.redo());
  QVERIFY(!view.isModified());

  QVERIFY(view.undo());
  QVERIFY(view.performCommand(QStringLiteral("Select rectangle"),
                              [](Document& changed) {
                                return changed.selection().selectRectangle(
                                    QRect(2, 3, 10, 11));
                              }));
  QVERIFY(view.isModified());
  QVERIFY(!view.history().canRedo());

  const auto missingPath =
      directory.filePath(QStringLiteral("missing/revision.chromarchy"));
  QVERIFY(!view.save(missingPath));
  QVERIFY(view.isModified());

  auto cleanDocument = Document::create(QSize(64, 64));
  QVERIFY(cleanDocument);
  DocumentView cleanView(std::move(*cleanDocument), QStringLiteral("Clean"), {},
                         false);
  QVERIFY(!cleanView.isModified());
  QVERIFY(cleanView.performCommand(QStringLiteral("Select all"),
                                   [](Document& changed) {
                                     return changed.selection().selectAll();
                                   }));
  QVERIFY(cleanView.isModified());
  QVERIFY(cleanView.undo());
  QVERIFY(!cleanView.isModified());
}

void DocumentViewTest::evictedSavedRevisionNeverAppearsClean() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  auto document = Document::create(QSize(64, 64));
  QVERIFY(document);
  DocumentView view(std::move(*document), QStringLiteral("Evicted"), {}, true);
  const auto path = directory.filePath(QStringLiteral("evicted.chromarchy"));
  QVERIFY(view.save(path));

  for (int index = 0;
       index <= chromarchy::DocumentHistory::defaultCommandLimit; ++index) {
    const QPoint position(index % 64, index / 64);
    QVERIFY(view.performCommand(QStringLiteral("Move selection"),
                                [position](Document& changed) {
                                  return changed.selection().selectRectangle(
                                      QRect(position, QSize(1, 1)));
                                }));
  }
  QCOMPARE(view.history().size(),
           chromarchy::DocumentHistory::defaultCommandLimit);
  QVERIFY(view.isModified());

  while (view.undo()) {
    QVERIFY(view.isModified());
  }
  QVERIFY(view.isModified());
}

QTEST_MAIN(DocumentViewTest)

#include "DocumentViewTest.moc"
