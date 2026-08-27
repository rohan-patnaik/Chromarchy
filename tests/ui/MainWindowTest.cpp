#include "MainWindow.h"

#include "ui/CanvasWidget.h"
#include "ui/DocumentView.h"

#include <QAccessible>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QListWidget>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

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

}  // namespace

class MainWindowTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void exposesCoreWorkspaceAccessibility();
  void supportsCoreKeyboardWorkflow();

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

QTEST_MAIN(MainWindowTest)

#include "MainWindowTest.moc"
