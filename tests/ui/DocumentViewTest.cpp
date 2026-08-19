#include "core/Document.h"
#include "ui/DocumentView.h"

#include <QTemporaryDir>
#include <QTest>

using chromarchy::Document;
using chromarchy::DocumentView;

class DocumentViewTest final : public QObject {
  Q_OBJECT

private slots:
  void noOpSelectionsPreserveSavedState();
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
}

QTEST_MAIN(DocumentViewTest)

#include "DocumentViewTest.moc"
