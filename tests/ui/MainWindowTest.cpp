#include "MainWindow.h"

#include "core/NativeDocumentCodec.h"
#include "ui/CanvasWidget.h"
#include "ui/DocumentView.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QListWidget>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

namespace {

template <typename Widget>
Widget* requiredChild(QObject& parent, const char* objectName) {
  return parent.findChild<Widget*>(QString::fromLatin1(objectName));
}

void verifyAccessibleWidget(QWidget* widget, const QString& name) {
  QVERIFY(widget);
  const auto* interface = QAccessible::queryAccessibleInterface(widget);
  QVERIFY(interface);
  QCOMPARE(interface->text(QAccessible::Name), name);
  QVERIFY(!interface->text(QAccessible::Description).isEmpty());
  QVERIFY(interface->role() != QAccessible::NoRole);
}

bool hasAccessibleMetadata(QWidget* widget) {
  const auto* interface = QAccessible::queryAccessibleInterface(widget);
  return interface && !interface->text(QAccessible::Name).isEmpty() &&
         !interface->text(QAccessible::Description).isEmpty() &&
         interface->role() != QAccessible::NoRole;
}

bool hasStandardShortcut(const QAction* action,
                         QKeySequence::StandardKey standardKey) {
  const auto bindings = QKeySequence::keyBindings(standardKey);
  for (const auto& shortcut : action->shortcuts()) {
    if (bindings.contains(shortcut)) {
      return true;
    }
  }
  return false;
}

void cancelPrompt(QMessageBox* prompt) {
  if (auto* cancel = prompt->button(QMessageBox::Cancel)) {
    cancel->click();
  } else {
    prompt->reject();
  }
}

}  // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void exposesCoreWorkspaceAccessibility();
  void supportsCoreKeyboardWorkflow();
  void createsAndCancelsNewDocumentByKeyboard();
  void cancelsAndDiscardsClosePromptByKeyboard();
  void savesDirtyDocumentBeforeCloseByKeyboard();

private:
  QTemporaryDir settingsDirectory_;
};

void MainWindowTest::initTestCase() {
  QVERIFY(settingsDirectory_.isValid());
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     settingsDirectory_.path());
}

void MainWindowTest::exposesCoreWorkspaceAccessibility() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto imagePath = directory.filePath(QStringLiteral("accessible.png"));
  QImage image(QSize(8, 6), QImage::Format_RGBA8888);
  image.fill(QColor(12, 34, 56, 200));
  QVERIFY(image.save(imagePath));

  MainWindow window;
  QVERIFY(window.openFile(imagePath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  verifyAccessibleWidget(&window, QStringLiteral("Chromarchy workspace"));
  verifyAccessibleWidget(requiredChild<QTabWidget>(window, "documentTabs"),
                         QStringLiteral("Open documents"));
  verifyAccessibleWidget(requiredChild<QListWidget>(window, "layersList"),
                         QStringLiteral("Document layers"));
  verifyAccessibleWidget(requiredChild<QDoubleSpinBox>(window, "layerOpacity"),
                         QStringLiteral("Layer opacity"));
  verifyAccessibleWidget(requiredChild<QCheckBox>(window, "layerLock"),
                         QStringLiteral("Lock layer pixels"));
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(document);
  verifyAccessibleWidget(document,
                         QStringLiteral("Document accessible.png"));
  verifyAccessibleWidget(
      requiredChild<chromarchy::CanvasWidget>(window, "canvas"),
      QStringLiteral("Image canvas"));

  const auto savedPath =
      directory.filePath(QStringLiteral("renamed.chromarchy"));
  QVERIFY(document->save(savedPath));
  verifyAccessibleWidget(document,
                         QStringLiteral("Document renamed.chromarchy"));
}

void MainWindowTest::supportsCoreKeyboardWorkflow() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto imagePath = directory.filePath(QStringLiteral("keyboard.png"));
  QImage image(QSize(8, 6), QImage::Format_RGBA8888);
  image.fill(Qt::transparent);
  QVERIFY(image.save(imagePath));

  MainWindow window;
  QVERIFY(window.openFile(imagePath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));

  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* opacity = requiredChild<QDoubleSpinBox>(window, "layerOpacity");
  auto* lock = requiredChild<QCheckBox>(window, "layerLock");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(layers);
  QVERIFY(opacity);
  QVERIFY(lock);
  QVERIFY(canvas);
  QVERIFY(document);

  layers->setFocus();
  QCOMPARE(QApplication::focusWidget(), layers);
  QTest::keyClick(layers, Qt::Key_Tab);
  QCOMPARE(QApplication::focusWidget(), opacity);
  QTest::keyClick(opacity, Qt::Key_Tab);
  QCOMPARE(QApplication::focusWidget(), lock);

  canvas->setFocus();
  QCOMPARE(QApplication::focusWidget(), canvas);
  QTest::keyClick(canvas, Qt::Key_A, Qt::ControlModifier);
  QCOMPARE(document->document().selection().baseCoverage(), quint8{255});

  QCOMPARE(layers->count(), 1);
  QTest::keyClick(canvas, Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(layers->count(), 2);
  QTest::keyClick(canvas, Qt::Key_J, Qt::ControlModifier);
  QCOMPARE(layers->count(), 3);
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(layers->count(), 2);
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(layers->count(), 3);
}

void MainWindowTest::createsAndCancelsNewDocumentByKeyboard() {
  MainWindow window;
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* tabs = requiredChild<QTabWidget>(window, "documentTabs");
  QVERIFY(tabs);
  QCOMPARE(tabs->count(), 0);

  QString dialogError;
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* dialog =
        qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog) {
      dialogError = QStringLiteral("New Document dialog was not modal");
      return;
    }
    if (dialog->objectName() != QStringLiteral("newDocumentDialog") ||
        dialog->accessibleName() != QStringLiteral("New image document") ||
        !hasAccessibleMetadata(dialog)) {
      dialogError = QStringLiteral("New Document dialog metadata is incomplete");
      dialog->reject();
      return;
    }
    auto* width = requiredChild<QSpinBox>(*dialog, "newDocumentWidth");
    auto* height = requiredChild<QSpinBox>(*dialog, "newDocumentHeight");
    auto* buttons =
        requiredChild<QDialogButtonBox>(*dialog, "newDocumentButtons");
    if (!width || !height || !buttons || !hasAccessibleMetadata(width) ||
        !hasAccessibleMetadata(height) || !hasAccessibleMetadata(buttons)) {
      dialogError = QStringLiteral("New Document controls are incomplete");
      dialog->reject();
      return;
    }
    width->setFocus();
    width->selectAll();
    QTest::keyClicks(width, QStringLiteral("320"));
    QTest::keyClick(width, Qt::Key_Tab);
    if (dialog->focusWidget() != height) {
      dialogError = QStringLiteral("Width does not tab to height");
      dialog->reject();
      return;
    }
    height->selectAll();
    QTest::keyClicks(height, QStringLiteral("240"));
    QTimer::singleShot(500, dialog, [dialog, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Return did not close New Document");
      }
      dialog->reject();
    });
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Return);
  });

  QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(tabs->count(), 1);
  auto* document = qobject_cast<chromarchy::DocumentView*>(tabs->currentWidget());
  QVERIFY(document);
  QCOMPARE(document->document().size(), QSize(320, 240));
  QVERIFY(document->isModified());
  QCOMPARE(document->accessibleName(), QStringLiteral("Document Untitled 1"));

  dialogError.clear();
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* dialog =
        qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog) {
      dialogError = QStringLiteral("Cancel dialog was not modal");
      return;
    }
    auto* width = requiredChild<QSpinBox>(*dialog, "newDocumentWidth");
    if (!width || dialog->focusWidget() != width) {
      dialogError = QStringLiteral("Width was not initially focused");
      dialog->reject();
      return;
    }
    QTimer::singleShot(500, dialog, [dialog, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Escape did not close New Document");
      }
      dialog->reject();
    });
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Escape);
  });
  QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier);
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(tabs->count(), 1);
}

void MainWindowTest::cancelsAndDiscardsClosePromptByKeyboard() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto imagePath = directory.filePath(QStringLiteral("dirty.png"));
  QImage image(QSize(8, 6), QImage::Format_RGBA8888);
  image.fill(QColor(12, 34, 56, 200));
  QVERIFY(image.save(imagePath));

  MainWindow window;
  QVERIFY(window.openFile(imagePath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* tabs = requiredChild<QTabWidget>(window, "documentTabs");
  auto* closeAction =
      requiredChild<QAction>(window, "closeDocumentAction");
  QVERIFY(tabs);
  QVERIFY(closeAction);
  QVERIFY(closeAction->isEnabled());
  QVERIFY(hasStandardShortcut(closeAction, QKeySequence::Close));
  QCOMPARE(tabs->count(), 1);

  QString dialogError;
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!prompt) {
      dialogError = QStringLiteral("Unsaved Changes prompt was not modal");
      return;
    }
    auto* save = prompt->button(QMessageBox::Save);
    auto* discard = prompt->button(QMessageBox::Discard);
    auto* cancel = prompt->button(QMessageBox::Cancel);
    if (prompt->objectName() != QStringLiteral("unsavedChangesDialog") ||
        !hasAccessibleMetadata(prompt) || !save || !discard || !cancel ||
        !hasAccessibleMetadata(save) || !hasAccessibleMetadata(discard) ||
        !hasAccessibleMetadata(cancel) ||
        save->objectName() != QStringLiteral("saveChangesButton") ||
        discard->objectName() != QStringLiteral("discardChangesButton") ||
        cancel->objectName() != QStringLiteral("cancelCloseButton")) {
      dialogError = QStringLiteral("Unsaved Changes metadata is incomplete");
      cancelPrompt(prompt);
      return;
    }
    if (prompt->focusWidget() != save) {
      dialogError = QStringLiteral("Save was not initially focused");
      cancelPrompt(prompt);
      return;
    }
    QTimer::singleShot(500, prompt, [prompt, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Escape did not cancel close");
      }
      cancelPrompt(prompt);
    });
    QTest::keyClick(prompt->focusWidget(), Qt::Key_Escape);
  });
  const auto closeBindings = QKeySequence::keyBindings(QKeySequence::Close);
  QVERIFY(!closeBindings.isEmpty());
  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  tabs->currentWidget()->setFocus();
  QTest::keySequence(tabs->currentWidget(), closeBindings.constFirst());
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(tabs->count(), 1);

  dialogError.clear();
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!prompt) {
      dialogError = QStringLiteral("Discard prompt was not modal");
      return;
    }
    auto* save = prompt->button(QMessageBox::Save);
    auto* discard = prompt->button(QMessageBox::Discard);
    if (!save || !discard || prompt->focusWidget() != save) {
      dialogError = QStringLiteral("Discard prompt focus was incomplete");
      cancelPrompt(prompt);
      return;
    }
    QTest::keyClick(prompt->focusWidget(), Qt::Key_Tab);
    if (prompt->focusWidget() != discard) {
      dialogError = QStringLiteral("Save does not tab to Discard");
      cancelPrompt(prompt);
      return;
    }
    QTimer::singleShot(500, prompt, [prompt, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Space did not discard changes");
      }
      cancelPrompt(prompt);
    });
    QTest::keyClick(prompt->focusWidget(), Qt::Key_Space);
    if (prompt->clickedButton() != discard) {
      dialogError = QStringLiteral("Space did not activate Discard");
      cancelPrompt(prompt);
    }
  });
  closeAction->trigger();
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(tabs->count(), 0);
}

void MainWindowTest::savesDirtyDocumentBeforeCloseByKeyboard() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("close-save.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* tabs = requiredChild<QTabWidget>(window, "documentTabs");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* closeAction =
      requiredChild<QAction>(window, "closeDocumentAction");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(tabs);
  QVERIFY(canvas);
  QVERIFY(closeAction);
  QVERIFY(closeAction->isEnabled());
  QVERIFY(hasStandardShortcut(closeAction, QKeySequence::Close));
  QVERIFY(document);
  QVERIFY(!document->isModified());

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_A, Qt::ControlModifier);
  QVERIFY(document->isModified());
  QCOMPARE(document->document().selection().baseCoverage(), quint8{255});

  QString dialogError;
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* prompt =
        qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!prompt) {
      dialogError = QStringLiteral("Save prompt was not modal");
      return;
    }
    auto* save = prompt->button(QMessageBox::Save);
    if (!save || prompt->focusWidget() != save) {
      dialogError = QStringLiteral("Save prompt focus was incomplete");
      cancelPrompt(prompt);
      return;
    }
    QTimer::singleShot(500, prompt, [prompt, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Return did not save before close");
      }
      cancelPrompt(prompt);
    });
    QTest::keyClick(prompt->focusWidget(), Qt::Key_Return);
  });
  closeAction->trigger();
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(tabs->count(), 0);

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->selection().baseCoverage(), quint8{255});
}

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
