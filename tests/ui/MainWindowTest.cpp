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
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <algorithm>

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

void verifyAccessibleLayerItem(QListWidget* layers, int row,
                               const QString& name, bool checked) {
  QApplication::processEvents();
  const auto* listInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(listInterface);
  QVERIFY(row >= 0 && row < listInterface->childCount());
  const auto* itemInterface = listInterface->child(row);
  QVERIFY(itemInterface);
  QCOMPARE(itemInterface->text(QAccessible::Name), name);
  QCOMPARE(itemInterface->text(QAccessible::Description),
           checked ? QStringLiteral("Visible pixel layer; press Space to hide")
                   : QStringLiteral("Hidden pixel layer; press Space to show"));
  const auto state = itemInterface->state();
  QVERIFY(state.checkable);
  QCOMPARE(state.checked, checked);
}

QVector<chromarchy::StorageBlock> sortedStorageBlocks(
    const chromarchy::Document& document) {
  auto blocks = document.storageBlocks();
  std::sort(blocks.begin(), blocks.end(), [](const auto& left, const auto& right) {
    return left.key < right.key ||
           (left.key == right.key && left.bytes < right.bytes);
  });
  return blocks;
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
  void renamesLayerByKeyboardAndPersistsUnicode();
  void togglesLayerVisibilityByKeyboardAndPersistsComposite();
  void editsLayerOpacityByKeyboardAndPersistsComposite();
  void togglesLayerLockByKeyboardAndPersistsEnforcement();
  void reordersLayerByKeyboardAndPersistsComposite();
  void createsSparseLayerByKeyboardAndPersists();
  void duplicatesLayerByKeyboardWithCowAndPersists();
  void mergesLayerByKeyboardAndPersistsComposite();
  void flattensByKeyboardAndPersistsComposite();
  void removesLayerByKeyboardAndPersists();

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

void MainWindowTest::renamesLayerByKeyboardAndPersistsUnicode() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("rename-layer.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Initial layer"));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* renameAction = requiredChild<QAction>(window, "renameLayerAction");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(canvas);
  QVERIFY(renameAction);
  QVERIFY(renameAction->isEnabled());
  QCOMPARE(renameAction->shortcut(), QKeySequence(Qt::Key_F2));
  QVERIFY(document);
  QVERIFY(!document->isModified());

  QString dialogError;
  QTimer::singleShot(0, &window, [&dialogError] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    if (!dialog || dialog->objectName() != QStringLiteral("renameLayerDialog") ||
        !hasAccessibleMetadata(dialog)) {
      dialogError = QStringLiteral("Rename dialog metadata is incomplete");
      if (dialog) {
        dialog->reject();
      }
      return;
    }
    auto* editor = dialog->findChild<QLineEdit*>(QStringLiteral("layerNameEditor"));
    auto* buttons = dialog->findChild<QDialogButtonBox*>(
        QStringLiteral("renameLayerButtons"));
    if (!editor || !buttons || !hasAccessibleMetadata(editor) ||
        !hasAccessibleMetadata(buttons) || dialog->focusWidget() != editor ||
        editor->maxLength() != static_cast<int>(
                                   chromarchy::NativeDocumentCodec::maximumLayerNameBytes)) {
      dialogError = QStringLiteral("Rename controls are incomplete");
      dialog->reject();
      return;
    }
    QTest::keyClick(editor, Qt::Key_Escape);
  });
  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_F2);
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Initial layer"));
  QVERIFY(!document->isModified());

  const QString overBudgetName(2'049, QChar(0x0800));
  QVERIFY(overBudgetName.toUtf8().size() >
          chromarchy::NativeDocumentCodec::maximumLayerNameBytes);
  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTimer::singleShot(0, &window, [&dialogError, overBudgetName] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    auto* editor = dialog ? dialog->findChild<QLineEdit*>(
                                QStringLiteral("layerNameEditor"))
                          : nullptr;
    auto* buttons = dialog ? dialog->findChild<QDialogButtonBox*>(
                                 QStringLiteral("renameLayerButtons"))
                           : nullptr;
    auto* rename = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    if (!dialog || !editor || !rename) {
      dialogError = QStringLiteral("Bounded rename editor was not modal");
      if (dialog) {
        dialog->reject();
      }
      return;
    }
    editor->setText(overBudgetName);
    rename->click();
  });
  QTest::keyClick(canvas, Qt::Key_F2);
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Initial layer"));
  QVERIFY(!document->isModified());

  const auto renamed = QStringLiteral("Σ local layer");
  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTimer::singleShot(0, &window, [&dialogError, renamed] {
    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
    auto* editor = dialog ? dialog->findChild<QLineEdit*>(
                                QStringLiteral("layerNameEditor"))
                          : nullptr;
    auto* buttons = dialog ? dialog->findChild<QDialogButtonBox*>(
                                 QStringLiteral("renameLayerButtons"))
                           : nullptr;
    auto* rename = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    if (!dialog || !editor || !rename) {
      dialogError = QStringLiteral("Rename editor was not modal");
      if (dialog) {
        dialog->reject();
      }
      return;
    }
    editor->selectAll();
    editor->setText(renamed);
    if (editor->text() != renamed) {
      dialogError = QStringLiteral("Rename editor did not retain Unicode");
      dialog->reject();
      return;
    }
    QTimer::singleShot(500, dialog, [dialog, &dialogError] {
      if (dialogError.isEmpty()) {
        dialogError = QStringLiteral("Rename action did not close dialog");
      }
      dialog->reject();
    });
    rename->click();
    if (dialog->result() != QDialog::Accepted) {
      dialogError = QStringLiteral("Rename action did not accept dialog");
      dialog->reject();
    }
  });
  QTest::keyClick(canvas, Qt::Key_F2);
  QVERIFY2(dialogError.isEmpty(), qPrintable(dialogError));
  QCOMPARE(document->document().layerAt(0)->name(), renamed);
  QVERIFY(document->isModified());

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Initial layer"));
  QVERIFY(!document->isModified());
  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerAt(0)->name(), renamed);
  QVERIFY(document->isModified());
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerAt(0)->name(), renamed);
}

void MainWindowTest::togglesLayerVisibilityByKeyboardAndPersistsComposite() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-visibility.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Base red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Top blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(2, 3), QColor(10, 20, 230, 255)));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(layers);
  QVERIFY(document);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QVERIFY(!document->isModified());
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Top blue"), true);
  auto* retainedItem = layers->item(0);
  const auto* retainedListInterface =
      QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedItem);
  QVERIFY(retainedListInterface);
  const auto* retainedItemInterface = retainedListInterface->child(0);
  QVERIFY(retainedItemInterface);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 20, 230, 255));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  layers->setFocus();
  QCOMPARE(QApplication::focusWidget(), layers);
  QTest::keyClick(layers, Qt::Key_Space);
  QVERIFY(!document->document().layerAt(topIndex)->isVisible());
  QVERIFY(document->isModified());
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Top blue"), false);
  QCOMPARE(layers->item(0), retainedItem);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers)->child(0),
           retainedItemInterface);
  QCOMPARE(retainedItemInterface->text(QAccessible::Description),
           QStringLiteral("Hidden pixel layer; press Space to show"));
  QVERIFY(!retainedItemInterface->state().checked);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));

  QTest::keyClick(layers, Qt::Key_Z, Qt::ControlModifier);
  QVERIFY(document->document().layerAt(topIndex)->isVisible());
  QVERIFY(!document->isModified());
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Top blue"), true);
  QCOMPARE(layers->item(0), retainedItem);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers)->child(0),
           retainedItemInterface);
  QCOMPARE(retainedItemInterface->text(QAccessible::Description),
           QStringLiteral("Visible pixel layer; press Space to hide"));
  QVERIFY(retainedItemInterface->state().checked);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 20, 230, 255));

  QTest::keyClick(layers, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QVERIFY(!document->document().layerAt(topIndex)->isVisible());
  QVERIFY(document->isModified());
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Top blue"), false);
  QTest::keyClick(layers, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QVERIFY(!reopened.document->layerAt(topIndex)->isVisible());
  QCOMPARE(reopened.document->composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
}

void MainWindowTest::editsLayerOpacityByKeyboardAndPersistsComposite() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-opacity.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Base red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Top blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(2, 3), QColor(10, 20, 230, 255)));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* opacity = requiredChild<QDoubleSpinBox>(window, "layerOpacity");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(opacity);
  QVERIFY(canvas);
  QVERIFY(document);
  QCOMPARE(opacity->value(), 100.0);
  QVERIFY(!document->isModified());

  auto* retainedOpacityInterface =
      QAccessible::queryAccessibleInterface(opacity);
  QVERIFY(retainedOpacityInterface);
  auto* retainedValueInterface = retainedOpacityInterface->valueInterface();
  QVERIFY(retainedValueInterface);
  QCOMPARE(retainedValueInterface->minimumValue().toDouble(), 0.0);
  QCOMPARE(retainedValueInterface->maximumValue().toDouble(), 100.0);
  QCOMPARE(retainedValueInterface->currentValue().toDouble(), 100.0);

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  opacity->setFocus();
  QCOMPARE(QApplication::focusWidget(), opacity);
  opacity->selectAll();
  QTest::keyClicks(opacity, QStringLiteral("37.5"));
  QTest::keyClick(opacity, Qt::Key_Return);
  QCOMPARE(document->document().layerAt(topIndex)->opacity(), 0.375);
  QVERIFY(document->isModified());
  QCOMPARE(QAccessible::queryAccessibleInterface(opacity),
           retainedOpacityInterface);
  QCOMPARE(retainedOpacityInterface->valueInterface(), retainedValueInterface);
  QCOMPARE(retainedValueInterface->currentValue().toDouble(), 37.5);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(142, 13, 99, 255));

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerAt(topIndex)->opacity(), 1.0);
  QVERIFY(!document->isModified());
  QCOMPARE(retainedValueInterface->currentValue().toDouble(), 100.0);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 20, 230, 255));

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerAt(topIndex)->opacity(), 0.375);
  QVERIFY(document->isModified());
  QCOMPARE(retainedValueInterface->currentValue().toDouble(), 37.5);
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerAt(topIndex)->opacity(), 0.375);
  QCOMPARE(reopened.document->composite().pixelColor(QPoint(2, 3)),
           QColor(142, 13, 99, 255));
}

void MainWindowTest::togglesLayerLockByKeyboardAndPersistsEnforcement() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-lock.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Protected pixels"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* lock = requiredChild<QCheckBox>(window, "layerLock");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(lock);
  QVERIFY(canvas);
  QVERIFY(document);
  QVERIFY(!lock->isChecked());
  QVERIFY(!document->isModified());

  auto* retainedLockInterface = QAccessible::queryAccessibleInterface(lock);
  QVERIFY(retainedLockInterface);
  QCOMPARE(retainedLockInterface->text(QAccessible::Name),
           QStringLiteral("Lock layer pixels"));
  QCOMPARE(retainedLockInterface->text(QAccessible::Description),
           QStringLiteral("Prevent pixel changes on the selected layer"));
  QVERIFY(retainedLockInterface->state().checkable);
  QVERIFY(!retainedLockInterface->state().checked);

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  lock->setFocus();
  QCOMPARE(QApplication::focusWidget(), lock);
  QTest::keyClick(lock, Qt::Key_Space);
  QVERIFY(document->document().layerAt(0)->isLocked());
  QVERIFY(document->isModified());
  QCOMPARE(QAccessible::queryAccessibleInterface(lock),
           retainedLockInterface);
  QVERIFY(retainedLockInterface->state().checked);
  QVERIFY(!document->document().layerAt(0)->setPixelColor(
      QPoint(2, 3), QColor(1, 2, 3, 255)));
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QVERIFY(!document->document().layerAt(0)->isLocked());
  QVERIFY(!document->isModified());
  QVERIFY(!retainedLockInterface->state().checked);

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QVERIFY(document->document().layerAt(0)->isLocked());
  QVERIFY(document->isModified());
  QVERIFY(retainedLockInterface->state().checked);
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QVERIFY(reopened.document->layerAt(0)->isLocked());
  auto reopenedDocument = *reopened.document;
  QVERIFY(!reopenedDocument.layerAt(0)->setPixelColor(
      QPoint(2, 3), QColor(1, 2, 3, 255)));
  QCOMPARE(reopenedDocument.composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
}

void MainWindowTest::reordersLayerByKeyboardAndPersistsComposite() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-reorder.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Base red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  const auto middleIndex = source->addLayer(QStringLiteral("Middle green"));
  QVERIFY(source->layerAt(middleIndex)->setPixelColor(
      QPoint(2, 3), QColor(10, 220, 20, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Top blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(2, 3), QColor(10, 20, 230, 255)));
  QVERIFY(source->setActiveLayerIndex(middleIndex));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(document);
  QCOMPARE(layers->count(), 3);
  QCOMPARE(layers->currentRow(), 1);
  QCOMPARE(document->document().activeLayerIndex(), middleIndex);
  QVERIFY(!document->isModified());
  const auto originalBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 20, 230, 255));

  auto* listInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(listInterface);
  auto* retainedTopRowInterface = listInterface->child(0);
  auto* retainedMiddleRowInterface = listInterface->child(1);
  QVERIFY(retainedTopRowInterface);
  QVERIFY(retainedMiddleRowInterface);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Top blue"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_BracketRight,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QCOMPARE(document->document().layerAt(topIndex)->name(),
           QStringLiteral("Middle green"));
  QCOMPARE(layers->currentRow(), 0);
  QVERIFY(document->isModified());
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers), listInterface);
  QCOMPARE(listInterface->child(0), retainedTopRowInterface);
  QCOMPARE(listInterface->child(1), retainedMiddleRowInterface);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Top blue"));
  QVERIFY(retainedTopRowInterface->state().selected);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 220, 20, 255));

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().activeLayerIndex(), middleIndex);
  QCOMPARE(layers->currentRow(), 1);
  QVERIFY(!document->isModified());
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Top blue"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));
  QVERIFY(retainedMiddleRowInterface->state().selected);
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(10, 20, 230, 255));

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QCOMPARE(layers->currentRow(), 0);
  QVERIFY(document->isModified());
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->activeLayerIndex(), topIndex);
  QCOMPARE(reopened.document->layerAt(topIndex)->name(),
           QStringLiteral("Middle green"));
  QCOMPARE(reopened.document->layerAt(middleIndex)->name(),
           QStringLiteral("Top blue"));
  QCOMPARE(reopened.document->composite().pixelColor(QPoint(2, 3)),
           QColor(10, 220, 20, 255));
}

void MainWindowTest::createsSparseLayerByKeyboardAndPersists() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-create.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Existing pixels"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(document);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(layers->count(), 1);
  QVERIFY(!document->isModified());
  const auto originalBlocks = sortedStorageBlocks(document->document());
  auto* retainedListInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedListInterface);
  auto* retainedFirstRowInterface = retainedListInterface->child(0);
  QVERIFY(retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Existing pixels"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_N,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QCOMPARE(document->document().layerAt(1)->name(),
           QStringLiteral("Layer 2"));
  QCOMPARE(document->document().layerAt(1)->pixels().allocatedTileCount(), 0);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Layer 2"), true);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers),
           retainedListInterface);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Layer 2"));
  QCOMPARE(retainedListInterface->child(1)->text(QAccessible::Name),
           QStringLiteral("Existing pixels"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(document->isModified());
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));

  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Existing pixels"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(!document->isModified());
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QCOMPARE(document->document().layerAt(1)->name(),
           QStringLiteral("Layer 2"));
  QCOMPARE(document->document().layerAt(1)->pixels().allocatedTileCount(), 0);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  verifyAccessibleLayerItem(layers, 0, QStringLiteral("Layer 2"), true);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Layer 2"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(document->isModified());
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 2);
  QCOMPARE(reopened.document->activeLayerIndex(), 1);
  QCOMPARE(reopened.document->layerAt(1)->name(), QStringLiteral("Layer 2"));
  QCOMPARE(reopened.document->layerAt(1)->pixels().allocatedTileCount(), 0);
  QCOMPARE(reopened.document->composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
}

void MainWindowTest::duplicatesLayerByKeyboardWithCowAndPersists() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-duplicate.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Original pixels"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(document);
  QCOMPARE(document->document().layerCount(), 1);
  const auto originalBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(originalBlocks.size(), 1);
  QVERIFY(!document->isModified());

  auto* retainedListInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedListInterface);
  auto* retainedFirstRowInterface = retainedListInterface->child(0);
  QVERIFY(retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Original pixels"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_J, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QCOMPARE(document->document().layerAt(1)->name(),
           QStringLiteral("Original pixels copy"));
  QCOMPARE(document->document().layerAt(1)->pixels().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
  const auto duplicatedBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(duplicatedBlocks.size(), 2);
  QCOMPARE(duplicatedBlocks[0], originalBlocks[0]);
  QCOMPARE(duplicatedBlocks[1], originalBlocks[0]);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers),
           retainedListInterface);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Original pixels copy"));
  QCOMPARE(retainedListInterface->child(1)->text(QAccessible::Name),
           QStringLiteral("Original pixels"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(document->isModified());
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));

  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Original pixels"));
  QVERIFY(!document->isModified());

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  const auto redoneBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(redoneBlocks.size(), 2);
  QCOMPARE(redoneBlocks[0], originalBlocks[0]);
  QCOMPARE(redoneBlocks[1], originalBlocks[0]);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Original pixels copy"));
  QVERIFY(document->isModified());
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 2);
  QCOMPARE(reopened.document->activeLayerIndex(), 1);
  QCOMPARE(reopened.document->layerAt(0)->name(),
           QStringLiteral("Original pixels"));
  QCOMPARE(reopened.document->layerAt(1)->name(),
           QStringLiteral("Original pixels copy"));
  QCOMPARE(reopened.document->layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->layerAt(1)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->layerAt(0)->pixels().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
  QCOMPARE(reopened.document->layerAt(1)->pixels().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
  QCOMPARE(reopened.document->composite().pixelColor(QPoint(2, 3)),
           QColor(220, 10, 20, 255));
}

void MainWindowTest::mergesLayerByKeyboardAndPersistsComposite() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-merge.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Lower red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(2, 3),
                                            QColor(220, 10, 20, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Upper blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(2, 3), QColor(20, 40, 220, 160)));
  QVERIFY(source->layerAt(topIndex)->setOpacity(0.5));
  const auto expectedComposite = source->composite();
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* lock = requiredChild<QCheckBox>(window, "layerLock");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  auto* mergeAction = requiredChild<QAction>(window, "mergeDownAction");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(lock);
  QVERIFY(document);
  QVERIFY(mergeAction);
  QCOMPARE(mergeAction->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_E));
  QVERIFY(mergeAction->isEnabled());
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  const auto originalBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(originalBlocks.size(), 2);
  QVERIFY(!document->isModified());

  auto* retainedListInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedListInterface);
  auto* retainedFirstRowInterface = retainedListInterface->child(0);
  QVERIFY(retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  lock->setFocus();
  QTest::keyClick(lock, Qt::Key_Space);
  QVERIFY(document->document().layerAt(topIndex)->isLocked());
  QVERIFY(document->isModified());
  QVERIFY(!mergeAction->isEnabled());
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_E, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QVERIFY(!document->document().layerAt(topIndex)->isLocked());
  QVERIFY(!document->isModified());
  QVERIFY(mergeAction->isEnabled());

  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Down);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  lock->setFocus();
  QTest::keyClick(lock, Qt::Key_Space);
  QVERIFY(document->document().layerAt(0)->isLocked());
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Up);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QVERIFY(!mergeAction->isEnabled());
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_E, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QVERIFY(!document->document().layerAt(0)->isLocked());
  QVERIFY(!document->isModified());
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Up);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QVERIFY(mergeAction->isEnabled());

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_E, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Upper blue"));
  QCOMPARE(document->document().layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(sortedStorageBlocks(document->document()).size(), 1);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers),
           retainedListInterface);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(document->isModified());
  QVERIFY(!mergeAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QCOMPARE(retainedListInterface->child(1)->text(QAccessible::Name),
           QStringLiteral("Lower red"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(!document->isModified());
  QVERIFY(mergeAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QVERIFY(document->isModified());
  QVERIFY(!mergeAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());
  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 1);
  QCOMPARE(reopened.document->activeLayerIndex(), 0);
  QCOMPARE(reopened.document->layerAt(0)->name(),
           QStringLiteral("Upper blue"));
  QCOMPARE(reopened.document->layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->composite(), expectedComposite);
}

void MainWindowTest::flattensByKeyboardAndPersistsComposite() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-flatten.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Lower red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(1, 1),
                                            QColor(220, 10, 20, 255)));
  const auto middleIndex = source->addLayer(QStringLiteral("Middle green"));
  QVERIFY(source->layerAt(middleIndex)->setPixelColor(
      QPoint(2, 2), QColor(10, 220, 30, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Upper blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(3, 3), QColor(20, 40, 220, 255)));
  const auto expectedComposite = source->composite();
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* lock = requiredChild<QCheckBox>(window, "layerLock");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  auto* flattenAction = requiredChild<QAction>(window, "flattenAction");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(lock);
  QVERIFY(document);
  QVERIFY(flattenAction);
  QVERIFY(flattenAction->isEnabled());
  QCOMPARE(document->document().layerCount(), 3);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  const auto originalBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(originalBlocks.size(), 3);
  QVERIFY(!document->isModified());

  auto* retainedListInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedListInterface);
  auto* retainedFirstRowInterface = retainedListInterface->child(0);
  QVERIFY(retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Down);
  QCOMPARE(document->document().activeLayerIndex(), middleIndex);
  lock->setFocus();
  QTest::keyClick(lock, Qt::Key_Space);
  QVERIFY(document->document().layerAt(middleIndex)->isLocked());
  QVERIFY(document->isModified());
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Up);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QVERIFY(!document->document().layerAt(topIndex)->isLocked());
  QVERIFY(!flattenAction->isEnabled());
  const auto lockedHistorySize = document->history().size();
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_L, Qt::AltModifier);
  QTRY_VERIFY(QApplication::activePopupWidget());
  auto* lockedLayerMenu =
      qobject_cast<QMenu*>(QApplication::activePopupWidget());
  QVERIFY(lockedLayerMenu);
  QTest::keyClick(lockedLayerMenu, Qt::Key_F);
  QCOMPARE(document->document().layerCount(), 3);
  QCOMPARE(document->history().size(), lockedHistorySize);
  if (QApplication::activePopupWidget()) {
    QTest::keyClick(QApplication::activePopupWidget(), Qt::Key_Escape);
  }
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QVERIFY(!document->document().layerAt(middleIndex)->isLocked());
  QVERIFY(!document->isModified());
  QVERIFY(flattenAction->isEnabled());
  layers->setFocus();
  QTest::keyClick(layers, Qt::Key_Up);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_L, Qt::AltModifier);
  QTRY_VERIFY(QApplication::activePopupWidget());
  auto* layerMenu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
  QVERIFY(layerMenu);
  QCOMPARE(layerMenu->title(), QStringLiteral("&Layer"));
  QTest::keyClick(layerMenu, Qt::Key_F);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Flattened"));
  QCOMPARE(document->document().layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(sortedStorageBlocks(document->document()).size(), 1);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers),
           retainedListInterface);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Flattened"));
  QVERIFY(retainedFirstRowInterface->state().selected);
  QVERIFY(document->isModified());
  QVERIFY(!flattenAction->isEnabled());

  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 3);
  QCOMPARE(document->document().activeLayerIndex(), topIndex);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 3);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QCOMPARE(retainedListInterface->child(1)->text(QAccessible::Name),
           QStringLiteral("Middle green"));
  QCOMPARE(retainedListInterface->child(2)->text(QAccessible::Name),
           QStringLiteral("Lower red"));
  QVERIFY(!document->isModified());
  QVERIFY(flattenAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().activeLayerIndex(), 0);
  QCOMPARE(document->document().composite(), expectedComposite);
  QCOMPARE(layers->count(), 1);
  QCOMPARE(retainedListInterface->child(0), retainedFirstRowInterface);
  QCOMPARE(retainedFirstRowInterface->text(QAccessible::Name),
           QStringLiteral("Flattened"));
  QVERIFY(document->isModified());
  QVERIFY(!flattenAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());
  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 1);
  QCOMPARE(reopened.document->activeLayerIndex(), 0);
  QCOMPARE(reopened.document->layerAt(0)->name(),
           QStringLiteral("Flattened"));
  QCOMPARE(reopened.document->layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->composite(), expectedComposite);
}

void MainWindowTest::removesLayerByKeyboardAndPersists() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto documentPath =
      directory.filePath(QStringLiteral("layer-remove.chromarchy"));
  auto source = chromarchy::Document::create(QSize(8, 6));
  QVERIFY(source);
  source->layerAt(0)->setName(QStringLiteral("Lower red"));
  QVERIFY(source->layerAt(0)->setPixelColor(QPoint(1, 1),
                                            QColor(220, 10, 20, 255)));
  const auto middleIndex = source->addLayer(QStringLiteral("Middle green"));
  QVERIFY(source->layerAt(middleIndex)->setPixelColor(
      QPoint(2, 2), QColor(10, 220, 30, 255)));
  const auto topIndex = source->addLayer(QStringLiteral("Upper blue"));
  QVERIFY(source->layerAt(topIndex)->setPixelColor(
      QPoint(3, 3), QColor(20, 40, 220, 255)));
  QVERIFY(source->setActiveLayerIndex(middleIndex));
  const auto originalComposite = source->composite();
  QVERIFY(chromarchy::NativeDocumentCodec::save(*source, documentPath));

  MainWindow window;
  QVERIFY(window.openFile(documentPath));
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* layers = requiredChild<QListWidget>(window, "layersList");
  auto* canvas = requiredChild<chromarchy::CanvasWidget>(window, "canvas");
  auto* document =
      requiredChild<chromarchy::DocumentView>(window, "documentView");
  auto* removeAction = requiredChild<QAction>(window, "removeLayerAction");
  QVERIFY(layers);
  QVERIFY(canvas);
  QVERIFY(document);
  QVERIFY(removeAction);
  QCOMPARE(removeAction->shortcut(), QKeySequence::Delete);
  QVERIFY(removeAction->isEnabled());
  QCOMPARE(document->document().layerCount(), 3);
  QCOMPARE(document->document().activeLayerIndex(), middleIndex);
  QCOMPARE(layers->currentRow(), 1);
  const auto originalBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(originalBlocks.size(), 3);
  const auto removedBlocks =
      document->document().layerAt(middleIndex)->pixels().storageBlocks();
  QCOMPARE(removedBlocks.size(), 1);
  QVERIFY(!document->isModified());

  auto* retainedListInterface = QAccessible::queryAccessibleInterface(layers);
  QVERIFY(retainedListInterface);
  auto* retainedTopRowInterface = retainedListInterface->child(0);
  auto* retainedMiddleRowInterface = retainedListInterface->child(1);
  QVERIFY(retainedTopRowInterface);
  QVERIFY(retainedMiddleRowInterface);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));

  window.activateWindow();
  QVERIFY(QTest::qWaitForWindowActive(&window));
  canvas->setFocus();
  QTest::keyClick(canvas, Qt::Key_Delete);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Lower red"));
  QCOMPARE(document->document().layerAt(1)->name(),
           QStringLiteral("Upper blue"));
  const auto remainingBlocks = sortedStorageBlocks(document->document());
  QCOMPARE(remainingBlocks.size(), 2);
  QVERIFY(!remainingBlocks.contains(removedBlocks[0]));
  QVERIFY(originalBlocks.contains(remainingBlocks[0]));
  QVERIFY(originalBlocks.contains(remainingBlocks[1]));
  QCOMPARE(document->document().composite().pixelColor(QPoint(1, 1)),
           QColor(220, 10, 20, 255));
  QCOMPARE(document->document().composite().pixelColor(QPoint(2, 2)),
           QColor(Qt::transparent));
  QCOMPARE(document->document().composite().pixelColor(QPoint(3, 3)),
           QColor(20, 40, 220, 255));
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(QAccessible::queryAccessibleInterface(layers),
           retainedListInterface);
  QCOMPARE(retainedListInterface->child(0), retainedTopRowInterface);
  QCOMPARE(retainedListInterface->child(1), retainedMiddleRowInterface);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Lower red"));
  QVERIFY(retainedTopRowInterface->state().selected);
  QVERIFY(document->isModified());
  QVERIFY(removeAction->isEnabled());

  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 3);
  QCOMPARE(document->document().activeLayerIndex(), middleIndex);
  QCOMPARE(document->document().composite(), originalComposite);
  QCOMPARE(sortedStorageBlocks(document->document()), originalBlocks);
  QCOMPARE(layers->count(), 3);
  QCOMPARE(layers->currentRow(), 1);
  QCOMPARE(retainedListInterface->child(0), retainedTopRowInterface);
  QCOMPARE(retainedListInterface->child(1), retainedMiddleRowInterface);
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Middle green"));
  QVERIFY(retainedMiddleRowInterface->state().selected);
  QVERIFY(!document->isModified());

  QTest::keyClick(canvas, Qt::Key_Z,
                  Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QCOMPARE(sortedStorageBlocks(document->document()), remainingBlocks);
  QCOMPARE(layers->count(), 2);
  QCOMPARE(layers->currentRow(), 0);
  QCOMPARE(retainedListInterface->child(0), retainedTopRowInterface);
  QCOMPARE(retainedListInterface->child(1), retainedMiddleRowInterface);
  QCOMPARE(retainedTopRowInterface->text(QAccessible::Name),
           QStringLiteral("Upper blue"));
  QCOMPARE(retainedMiddleRowInterface->text(QAccessible::Name),
           QStringLiteral("Lower red"));
  QVERIFY(document->isModified());

  const auto savedComposite = document->document().composite();
  QTest::keyClick(canvas, Qt::Key_S, Qt::ControlModifier);
  QVERIFY(!document->isModified());
  QTest::keyClick(canvas, Qt::Key_Delete);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->document().layerAt(0)->name(),
           QStringLiteral("Lower red"));
  QVERIFY(document->isModified());
  QVERIFY(!removeAction->isEnabled());
  const auto singleLayerHistorySize = document->history().size();
  QTest::keyClick(canvas, Qt::Key_Delete);
  QCOMPARE(document->document().layerCount(), 1);
  QCOMPARE(document->history().size(), singleLayerHistorySize);
  QTest::keyClick(canvas, Qt::Key_Z, Qt::ControlModifier);
  QCOMPARE(document->document().layerCount(), 2);
  QCOMPARE(document->document().activeLayerIndex(), 1);
  QVERIFY(!document->isModified());
  QVERIFY(removeAction->isEnabled());

  const auto reopened = chromarchy::NativeDocumentCodec::load(documentPath);
  QVERIFY2(reopened, qPrintable(reopened.error));
  QCOMPARE(reopened.document->layerCount(), 2);
  QCOMPARE(reopened.document->activeLayerIndex(), 1);
  QCOMPARE(reopened.document->layerAt(0)->name(),
           QStringLiteral("Lower red"));
  QCOMPARE(reopened.document->layerAt(1)->name(),
           QStringLiteral("Upper blue"));
  QCOMPARE(reopened.document->layerAt(0)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->layerAt(1)->pixels().allocatedTileCount(), 1);
  QCOMPARE(reopened.document->composite(), savedComposite);
}

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
