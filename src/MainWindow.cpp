#include "MainWindow.h"

#include "core/ImageIO.h"
#include "core/NativeDocumentCodec.h"
#include "ui/CanvasWidget.h"
#include "ui/DocumentView.h"

#include <QAbstractButton>
#include <QAction>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <utility>

using chromarchy::Document;
using chromarchy::DocumentView;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setObjectName(QStringLiteral("mainWindow"));
  setAccessibleName(QStringLiteral("Chromarchy workspace"));
  setAccessibleDescription(QStringLiteral("Local image editing workspace"));
  setWindowTitle(QStringLiteral("Chromarchy — local image studio"));
  resize(1280, 800);
  setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);

  tabs_ = new QTabWidget(this);
  tabs_->setObjectName(QStringLiteral("documentTabs"));
  tabs_->setAccessibleName(QStringLiteral("Open documents"));
  tabs_->setAccessibleDescription(
      QStringLiteral("Switch between open image documents"));
  tabs_->setDocumentMode(true);
  tabs_->setMovable(true);
  tabs_->setTabsClosable(true);
  tabs_->setUsesScrollButtons(true);
  setCentralWidget(tabs_);
  connect(tabs_, &QTabWidget::currentChanged, this, [this] {
    refreshLayers();
    updateActions();
  });
  connect(tabs_, &QTabWidget::tabCloseRequested, this,
          &MainWindow::closeDocumentTab);

  createActions();
  createLayersDock();
  menuBar()->setObjectName(QStringLiteral("mainMenu"));
  menuBar()->setAccessibleName(QStringLiteral("Application menu"));
  statusBar()->setObjectName(QStringLiteral("statusBar"));
  statusBar()->setAccessibleName(QStringLiteral("Workspace status"));
  statusBar()->showMessage(QStringLiteral("Ready"));

  QSettings settings;
  restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
  restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
  updateActions();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions() {
  auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  auto* newAction = fileMenu->addAction(QStringLiteral("&New…"), this,
                                        &MainWindow::newDocument);
  newAction->setShortcut(QKeySequence::New);
  auto* openAction = fileMenu->addAction(QStringLiteral("&Open…"), this,
                                         &MainWindow::chooseAndOpenFile);
  openAction->setShortcut(QKeySequence::Open);
  fileMenu->addSeparator();
  saveAction_ = fileMenu->addAction(QStringLiteral("&Save"), this, [this] {
    saveDocument(currentDocument(), false);
  });
  saveAction_->setShortcut(QKeySequence::Save);
  saveAsAction_ = fileMenu->addAction(QStringLiteral("Save &As…"), this, [this] {
    saveDocument(currentDocument(), true);
  });
  saveAsAction_->setShortcut(QKeySequence::SaveAs);
  exportAction_ = fileMenu->addAction(QStringLiteral("&Export Image…"), this,
                                      &MainWindow::exportDocument);
  exportAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
  fileMenu->addSeparator();
  closeAction_ = fileMenu->addAction(QStringLiteral("&Close Document"), this, [this] {
    closeDocumentTab(tabs_->currentIndex());
  });
  closeAction_->setObjectName(QStringLiteral("closeDocumentAction"));
  closeAction_->setShortcut(QKeySequence::Close);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close,
                      QKeySequence::Quit);

  auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
  undoAction_ = editMenu->addAction(QStringLiteral("&Undo"), this, [this] {
    if (auto* view = currentDocument()) {
      view->undo();
    }
  });
  undoAction_->setShortcut(QKeySequence::Undo);
  redoAction_ = editMenu->addAction(QStringLiteral("&Redo"), this, [this] {
    if (auto* view = currentDocument()) {
      view->redo();
    }
  });
  auto redoShortcuts = QKeySequence::keyBindings(QKeySequence::Redo);
  const QKeySequence conventionalRedo(Qt::CTRL | Qt::SHIFT | Qt::Key_Z);
  if (!redoShortcuts.contains(conventionalRedo)) {
    redoShortcuts.push_back(conventionalRedo);
  }
  redoAction_->setShortcuts(redoShortcuts);

  auto* selectMenu = menuBar()->addMenu(QStringLiteral("&Select"));
  auto* selectAll = selectMenu->addAction(QStringLiteral("Select &All"), this, [this] {
    if (auto* view = currentDocument()) {
      view->performCommand(QStringLiteral("Select all"), [](Document& document) {
        return document.selection().selectAll();
      });
    }
  });
  selectAll->setShortcut(QKeySequence::SelectAll);
  auto* deselect = selectMenu->addAction(QStringLiteral("&Deselect"), this, [this] {
    if (auto* view = currentDocument()) {
      view->performCommand(QStringLiteral("Deselect"), [](Document& document) {
        return document.selection().clear();
      });
    }
  });
  deselect->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
  auto* invertSelection =
      selectMenu->addAction(QStringLiteral("&Invert Selection"), this, [this] {
        if (auto* view = currentDocument()) {
          view->performCommand(QStringLiteral("Invert selection"),
                               [](Document& document) {
                                 document.selection().invert();
                                 return true;
                               });
        }
      });
  invertSelection->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

  auto* layerMenu = menuBar()->addMenu(QStringLiteral("&Layer"));
  addLayerAction_ = layerMenu->addAction(QStringLiteral("&New Pixel Layer"), this,
                                         &MainWindow::addLayer);
  addLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
  duplicateLayerAction_ = layerMenu->addAction(QStringLiteral("&Duplicate Layer"), this,
                                               &MainWindow::duplicateLayer);
  duplicateLayerAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
  renameLayerAction_ = layerMenu->addAction(QStringLiteral("&Rename Layer…"), this,
                                            &MainWindow::renameLayer);
  renameLayerAction_->setObjectName(QStringLiteral("renameLayerAction"));
  renameLayerAction_->setShortcut(QKeySequence(Qt::Key_F2));
  removeLayerAction_ = layerMenu->addAction(QStringLiteral("&Remove Layer"), this,
                                            &MainWindow::removeLayer);
  removeLayerAction_->setObjectName(QStringLiteral("removeLayerAction"));
  removeLayerAction_->setShortcut(QKeySequence::Delete);
  moveLayerUpAction_ = layerMenu->addAction(QStringLiteral("Move Layer &Up"), this,
                                            &MainWindow::moveLayerUp);
  moveLayerUpAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
  moveLayerDownAction_ = layerMenu->addAction(
      QStringLiteral("Move Layer &Down"), this, &MainWindow::moveLayerDown);
  moveLayerDownAction_->setShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));
  layerMenu->addSeparator();
  mergeDownAction_ = layerMenu->addAction(QStringLiteral("Merge &Down"), this,
                                          &MainWindow::mergeLayerDown);
  mergeDownAction_->setObjectName(QStringLiteral("mergeDownAction"));
  mergeDownAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  flattenAction_ = layerMenu->addAction(QStringLiteral("&Flatten Document"), this,
                                        &MainWindow::flattenDocument);
  flattenAction_->setObjectName(QStringLiteral("flattenAction"));

  auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
  auto zoomAction = [this, viewMenu](const QString& text,
                                     const QKeySequence& shortcut,
                                     double factor) {
    auto* action = viewMenu->addAction(text, this, [this, factor] {
      if (auto* view = currentDocument()) {
        view->canvas()->setZoom(view->canvas()->zoom() * factor);
      }
    });
    action->setShortcut(shortcut);
  };
  zoomAction(QStringLiteral("Zoom &In"), QKeySequence::ZoomIn, 1.25);
  zoomAction(QStringLiteral("Zoom &Out"), QKeySequence::ZoomOut, 0.8);
  auto* actualPixels = viewMenu->addAction(QStringLiteral("&Actual Pixels"), this, [this] {
    if (auto* view = currentDocument()) {
      view->canvas()->setZoom(1.0);
    }
  });
  actualPixels->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
}

void MainWindow::createLayersDock() {
  auto* dock = new QDockWidget(QStringLiteral("Layers"), this);
  dock->setObjectName(QStringLiteral("layersDock"));
  dock->setAccessibleName(QStringLiteral("Layers panel"));
  dock->setAccessibleDescription(
      QStringLiteral("Manage layers in the current document"));
  auto* panel = new QWidget(dock);
  panel->setObjectName(QStringLiteral("layersPanel"));
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(4, 4, 4, 4);
  auto* toolbar = new QToolBar(panel);
  toolbar->setObjectName(QStringLiteral("layerActionsToolbar"));
  toolbar->setAccessibleName(QStringLiteral("Layer actions"));
  toolbar->setAccessibleDescription(
      QStringLiteral("Create, duplicate, or remove the selected layer"));
  toolbar->setIconSize(QSize(16, 16));
  toolbar->addAction(addLayerAction_);
  toolbar->addAction(duplicateLayerAction_);
  toolbar->addAction(removeLayerAction_);
  layout->addWidget(toolbar);

  layers_ = new QListWidget(panel);
  layers_->setObjectName(QStringLiteral("layersList"));
  layers_->setAccessibleName(QStringLiteral("Document layers"));
  layers_->setAccessibleDescription(QStringLiteral(
      "Select, rename, and toggle visibility for document layers"));
  layers_->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(layers_);
  connect(layers_, &QListWidget::currentRowChanged, this,
          &MainWindow::layerSelectionChanged);
  connect(layers_, &QListWidget::itemChanged, this,
          &MainWindow::layerItemChanged);

  auto* properties = new QFormLayout;
  opacity_ = new QDoubleSpinBox(panel);
  opacity_->setObjectName(QStringLiteral("layerOpacity"));
  opacity_->setAccessibleName(QStringLiteral("Layer opacity"));
  opacity_->setAccessibleDescription(
      QStringLiteral("Set opacity for the selected layer as a percentage"));
  opacity_->setRange(0.0, 100.0);
  opacity_->setDecimals(1);
  opacity_->setSuffix(QStringLiteral(" %"));
  opacity_->setKeyboardTracking(false);
  connect(opacity_, &QDoubleSpinBox::editingFinished, this,
          &MainWindow::commitLayerOpacity);
  properties->addRow(QStringLiteral("Opacity"), opacity_);
  layerLocked_ = new QCheckBox(QStringLiteral("Lock pixels"), panel);
  layerLocked_->setObjectName(QStringLiteral("layerLock"));
  layerLocked_->setAccessibleName(QStringLiteral("Lock layer pixels"));
  layerLocked_->setAccessibleDescription(
      QStringLiteral("Prevent pixel changes on the selected layer"));
  connect(layerLocked_, &QCheckBox::toggled, this,
          &MainWindow::setLayerLocked);
  properties->addRow(layerLocked_);
  layout->addLayout(properties);
  QWidget::setTabOrder(layers_, opacity_);
  QWidget::setTabOrder(opacity_, layerLocked_);
  dock->setWidget(panel);
  addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::newDocument() {
  QDialog dialog(this);
  dialog.setObjectName(QStringLiteral("newDocumentDialog"));
  dialog.setAccessibleName(QStringLiteral("New image document"));
  dialog.setAccessibleDescription(
      QStringLiteral("Choose bounded pixel dimensions for a new document"));
  dialog.setWindowTitle(QStringLiteral("New Document"));
  auto* layout = new QFormLayout(&dialog);
  auto* width = new QSpinBox(&dialog);
  width->setObjectName(QStringLiteral("newDocumentWidth"));
  width->setAccessibleName(QStringLiteral("Document width"));
  width->setAccessibleDescription(
      QStringLiteral("Width of the new document in pixels"));
  width->setRange(1, Document::maximumDimension);
  width->setValue(1600);
  width->setSuffix(QStringLiteral(" px"));
  auto* height = new QSpinBox(&dialog);
  height->setObjectName(QStringLiteral("newDocumentHeight"));
  height->setAccessibleName(QStringLiteral("Document height"));
  height->setAccessibleDescription(
      QStringLiteral("Height of the new document in pixels"));
  height->setRange(1, Document::maximumDimension);
  height->setValue(1200);
  height->setSuffix(QStringLiteral(" px"));
  layout->addRow(QStringLiteral("Width"), width);
  layout->addRow(QStringLiteral("Height"), height);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->setObjectName(QStringLiteral("newDocumentButtons"));
  buttons->setAccessibleName(QStringLiteral("New document actions"));
  buttons->setAccessibleDescription(
      QStringLiteral("Create the document or cancel without changes"));
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addRow(buttons);
  QWidget::setTabOrder(width, height);
  QWidget::setTabOrder(height, buttons);
  width->setFocus();
  width->selectAll();
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  auto document = Document::create(QSize(width->value(), height->value()));
  if (!document) {
    showError(QStringLiteral("New Document"),
              QStringLiteral("Those canvas dimensions are not supported."));
    return;
  }
  addDocumentTab(new DocumentView(
      std::move(*document), QStringLiteral("Untitled %1").arg(untitledCounter_++),
      {}, true, tabs_));
}

void MainWindow::chooseAndOpenFile() {
  const auto path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Open Image"), {},
      QStringLiteral("Images (*.chromarchy *.png *.jpg *.jpeg *.tif *.tiff *.webp *.exr);;"
                     "Chromarchy Documents (*.chromarchy);;All Files (*)"));
  if (!path.isEmpty()) {
    openFile(path);
  }
}

bool MainWindow::openFile(const QString& filePath) {
  const QFileInfo info(filePath);
  if (info.suffix().compare(QStringLiteral("chromarchy"), Qt::CaseInsensitive) == 0) {
    auto result = chromarchy::NativeDocumentCodec::load(filePath);
    if (!result) {
      showError(QStringLiteral("Could Not Open Document"), result.error);
      return false;
    }
    addDocumentTab(new DocumentView(std::move(*result.document), info.fileName(),
                                    info.absoluteFilePath(), false, tabs_));
    return true;
  }

  auto result = chromarchy::ImageIO::open(filePath);
  if (!result) {
    showError(QStringLiteral("Could Not Open Image"), result.error);
    return false;
  }
  addDocumentTab(new DocumentView(std::move(*result.document), info.fileName(), {},
                                  true, tabs_));
  return true;
}

bool MainWindow::saveDocument(DocumentView* view, bool choosePath) {
  if (!view) {
    return false;
  }
  auto path = view->filePath();
  if (choosePath || path.isEmpty()) {
    path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save Chromarchy Document"), path,
        QStringLiteral("Chromarchy Documents (*.chromarchy)"));
    if (path.isEmpty()) {
      return false;
    }
    if (!path.endsWith(QStringLiteral(".chromarchy"), Qt::CaseInsensitive)) {
      path += QStringLiteral(".chromarchy");
    }
  }

  const auto result = view->save(path);
  if (!result) {
    showError(QStringLiteral("Could Not Save Document"), result.error);
    return false;
  }
  statusBar()->showMessage(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()),
                           3000);
  return true;
}

void MainWindow::exportDocument() {
  auto* view = currentDocument();
  if (!view) {
    return;
  }
  const auto path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Export Image"), {},
      QStringLiteral("PNG (*.png);;JPEG (*.jpg *.jpeg);;TIFF (*.tif *.tiff);;"
                     "WebP (*.webp);;OpenEXR (*.exr)"));
  if (path.isEmpty()) {
    return;
  }
  const auto result = chromarchy::ImageIO::exportComposite(view->document(), path);
  if (!result) {
    showError(QStringLiteral("Could Not Export Image"), result.error);
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(path).fileName()), 3000);
}

void MainWindow::addDocumentTab(DocumentView* view) {
  const auto index = tabs_->addTab(view, view->tabTitle());
  tabs_->setCurrentIndex(index);
  connect(view, &DocumentView::titleChanged, this,
          [this, view](const QString& title) {
            const auto tab = tabs_->indexOf(view);
            if (tab >= 0) {
              tabs_->setTabText(tab, title);
            }
          });
  connect(view->canvas(), &chromarchy::CanvasWidget::zoomChanged, this,
          [this](double zoom) {
            statusBar()->showMessage(
                QStringLiteral("Zoom %1%").arg(qRound(zoom * 100.0)));
          });
  connect(view, &DocumentView::historyChanged, this, [this] {
    refreshLayers();
    updateActions();
  });
  connect(view, &DocumentView::commandFailed, this,
          [this](const QString& detail) {
            showError(QStringLiteral("Could Not Record Edit"), detail);
          });
  refreshLayers();
  updateActions();
}

void MainWindow::closeDocumentTab(int index) {
  if (index < 0) {
    return;
  }
  auto* view = qobject_cast<DocumentView*>(tabs_->widget(index));
  if (!view || !canClose(view)) {
    return;
  }
  tabs_->removeTab(index);
  view->deleteLater();
}

bool MainWindow::canClose(DocumentView* view) {
  if (!view->isModified()) {
    return true;
  }
  QMessageBox prompt(
      QMessageBox::Warning, QStringLiteral("Unsaved Changes"),
      QStringLiteral("Save changes to %1?").arg(view->displayName()),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
  prompt.setObjectName(QStringLiteral("unsavedChangesDialog"));
  prompt.setAccessibleName(QStringLiteral("Unsaved document changes"));
  prompt.setAccessibleDescription(
      QStringLiteral("Choose whether to save changes before closing"));
  auto* saveButton = prompt.button(QMessageBox::Save);
  auto* discardButton = prompt.button(QMessageBox::Discard);
  auto* cancelButton = prompt.button(QMessageBox::Cancel);
  saveButton->setObjectName(QStringLiteral("saveChangesButton"));
  saveButton->setAccessibleName(QStringLiteral("Save changes"));
  saveButton->setAccessibleDescription(
      QStringLiteral("Save changes and close the document"));
  discardButton->setObjectName(QStringLiteral("discardChangesButton"));
  discardButton->setAccessibleName(QStringLiteral("Discard changes"));
  discardButton->setAccessibleDescription(
      QStringLiteral("Close the document without saving changes"));
  cancelButton->setObjectName(QStringLiteral("cancelCloseButton"));
  cancelButton->setAccessibleName(QStringLiteral("Cancel close"));
  cancelButton->setAccessibleDescription(
      QStringLiteral("Keep the document open"));
  QWidget::setTabOrder(saveButton, discardButton);
  QWidget::setTabOrder(discardButton, cancelButton);
  prompt.setDefaultButton(QMessageBox::Save);
  prompt.setEscapeButton(QMessageBox::Cancel);
  prompt.exec();
  const auto choice = prompt.standardButton(prompt.clickedButton());
  if (choice == QMessageBox::Cancel || choice == QMessageBox::NoButton) {
    return false;
  }
  return choice == QMessageBox::Discard || saveDocument(view, false);
}

DocumentView* MainWindow::currentDocument() const {
  return qobject_cast<DocumentView*>(tabs_->currentWidget());
}

void MainWindow::refreshLayers() {
  updatingLayers_ = true;
  auto* view = currentDocument();
  if (view) {
    const auto& document = view->document();
    const auto layerCount = static_cast<int>(document.layerCount());
    while (layers_->count() > layerCount) {
      delete layers_->takeItem(layers_->count() - 1);
    }
    while (layers_->count() < layerCount) {
      new QListWidgetItem(layers_);
    }
    for (int row = 0; row < layerCount; ++row) {
      const auto index = layerCount - row - 1;
      const auto* layer = document.layerAt(index);
      auto* item = layers_->item(row);
      item->setText(layer->name());
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
      item->setCheckState(layer->isVisible() ? Qt::Checked : Qt::Unchecked);
      item->setData(Qt::AccessibleTextRole, layer->name());
      item->setData(
          Qt::AccessibleDescriptionRole,
          layer->isVisible()
              ? QStringLiteral("Visible pixel layer; press Space to hide")
              : QStringLiteral("Hidden pixel layer; press Space to show"));
      item->setData(Qt::UserRole, index);
      if (index == document.activeLayerIndex()) {
        layers_->setCurrentItem(item);
      }
    }
    const auto* active = document.layerAt(document.activeLayerIndex());
    opacity_->setValue(active->opacity() * 100.0);
    layerLocked_->setChecked(active->isLocked());
  } else {
    layers_->clear();
  }
  opacity_->setEnabled(view != nullptr);
  layerLocked_->setEnabled(view != nullptr);
  updatingLayers_ = false;
}

void MainWindow::layerSelectionChanged(int row) {
  if (updatingLayers_ || row < 0) {
    return;
  }
  if (auto* view = currentDocument()) {
    const auto index = layers_->item(row)->data(Qt::UserRole).toInt();
    if (view->document().setActiveLayerIndex(index)) {
      const auto* layer = view->document().layerAt(index);
      updatingLayers_ = true;
      opacity_->setValue(layer->opacity() * 100.0);
      layerLocked_->setChecked(layer->isLocked());
      updatingLayers_ = false;
    }
  }
  updateActions();
}

void MainWindow::layerItemChanged(QListWidgetItem* item) {
  if (updatingLayers_ || !item) {
    return;
  }
  if (auto* view = currentDocument()) {
    const auto index = item->data(Qt::UserRole).toInt();
    const auto* layer = view->document().layerAt(index);
    const bool visible = item->checkState() == Qt::Checked;
    if (layer && layer->isVisible() != visible) {
      view->performCommand(QStringLiteral("Change layer visibility"),
                           [index, visible](Document& document) {
                             auto* changed = document.layerAt(index);
                             if (!changed) {
                               return false;
                             }
                             changed->setVisible(visible);
                             return true;
                           });
      return;
    }
    const auto name = item->text().trimmed();
    if (layer && (name.isEmpty() ||
                  name.toUtf8().size() >
                      chromarchy::NativeDocumentCodec::maximumLayerNameBytes)) {
      updatingLayers_ = true;
      item->setText(layer->name());
      updatingLayers_ = false;
      statusBar()->showMessage(
          QStringLiteral("Layer names must use 1 to %1 UTF-8 bytes.")
              .arg(chromarchy::NativeDocumentCodec::maximumLayerNameBytes),
          5000);
      return;
    }
    if (layer && !name.isEmpty() && layer->name() != name) {
      view->performCommand(QStringLiteral("Rename layer"),
                           [index, name](Document& document) {
                             auto* changed = document.layerAt(index);
                             if (!changed) {
                               return false;
                             }
                             changed->setName(name);
                             return true;
                           });
    }
  }
}

void MainWindow::addLayer() {
  if (auto* view = currentDocument()) {
    const auto name =
        QStringLiteral("Layer %1").arg(view->document().layerCount() + 1);
    view->performCommand(QStringLiteral("Add layer"),
                         [name](Document& document) {
                           document.addLayer(name);
                           return true;
                         });
  }
}

void MainWindow::duplicateLayer() {
  if (auto* view = currentDocument()) {
    const auto index = view->document().activeLayerIndex();
    view->performCommand(QStringLiteral("Duplicate layer"),
                         [index](Document& document) {
                           return document.duplicateLayer(index);
                         });
  }
}

void MainWindow::renameLayer() {
  auto* view = currentDocument();
  if (!view) {
    return;
  }
  const auto index = view->document().activeLayerIndex();
  const auto* layer = view->document().layerAt(index);
  if (!layer) {
    return;
  }
  const auto originalName = layer->name();

  QDialog dialog(this);
  dialog.setObjectName(QStringLiteral("renameLayerDialog"));
  dialog.setAccessibleName(QStringLiteral("Rename selected layer"));
  dialog.setAccessibleDescription(
      QStringLiteral("Enter a bounded name for the selected layer"));
  dialog.setWindowTitle(QStringLiteral("Rename Layer"));
  auto* layout = new QFormLayout(&dialog);
  auto* editor = new QLineEdit(originalName, &dialog);
  editor->setObjectName(QStringLiteral("layerNameEditor"));
  editor->setAccessibleName(QStringLiteral("Layer name"));
  editor->setAccessibleDescription(
      QStringLiteral("Name stored with the selected layer"));
  editor->setMaxLength(
      static_cast<int>(
          chromarchy::NativeDocumentCodec::maximumLayerNameBytes));
  layout->addRow(QStringLiteral("Layer name"), editor);
  auto* buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Rename"));
  buttons->setObjectName(QStringLiteral("renameLayerButtons"));
  buttons->setAccessibleName(QStringLiteral("Rename layer actions"));
  buttons->setAccessibleDescription(
      QStringLiteral("Apply the new name or cancel without changes"));
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addRow(buttons);
  QWidget::setTabOrder(editor, buttons);
  editor->selectAll();
  editor->setFocus();
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto name = editor->text().trimmed();
  if (name.isEmpty() ||
      name.toUtf8().size() >
          chromarchy::NativeDocumentCodec::maximumLayerNameBytes) {
    statusBar()->showMessage(
        QStringLiteral("Layer names must use 1 to %1 UTF-8 bytes.")
            .arg(chromarchy::NativeDocumentCodec::maximumLayerNameBytes),
        5000);
    return;
  }
  if (name == originalName) {
    return;
  }
  view->performCommand(QStringLiteral("Rename layer"),
                       [index, name](Document& document) {
                         auto* changed = document.layerAt(index);
                         if (!changed) {
                           return false;
                         }
                         changed->setName(name);
                         return true;
                       });
}

void MainWindow::removeLayer() {
  if (auto* view = currentDocument()) {
    const auto index = view->document().activeLayerIndex();
    view->performCommand(QStringLiteral("Remove layer"),
                         [index](Document& document) {
                           return document.removeLayer(index);
                         });
  }
}

void MainWindow::mergeLayerDown() {
  if (auto* view = currentDocument()) {
    const auto index = view->document().activeLayerIndex();
    view->performCommand(QStringLiteral("Merge layer down"),
                         [index](Document& document) {
                           return document.mergeLayerDown(index);
                         });
  }
}

void MainWindow::flattenDocument() {
  if (auto* view = currentDocument()) {
    view->performCommand(QStringLiteral("Flatten document"),
                         [](Document& document) {
                           return document.flatten();
                         });
  }
}

void MainWindow::moveLayerUp() {
  if (auto* view = currentDocument()) {
    const auto from = view->document().activeLayerIndex();
    view->performCommand(QStringLiteral("Move layer up"),
                         [from](Document& document) {
                           return document.moveLayer(from, from + 1);
                         });
  }
}

void MainWindow::moveLayerDown() {
  if (auto* view = currentDocument()) {
    const auto from = view->document().activeLayerIndex();
    view->performCommand(QStringLiteral("Move layer down"),
                         [from](Document& document) {
                           return document.moveLayer(from, from - 1);
                         });
  }
}

void MainWindow::commitLayerOpacity() {
  if (updatingLayers_) {
    return;
  }
  if (auto* view = currentDocument()) {
    const auto index = view->document().activeLayerIndex();
    const auto value = opacity_->value() / 100.0;
    const auto* layer = view->document().layerAt(index);
    if (!layer || qFuzzyCompare(layer->opacity(), value)) {
      return;
    }
    view->performCommand(QStringLiteral("Change layer opacity"),
                         [index, value](Document& document) {
                           auto* changed = document.layerAt(index);
                           if (!changed) {
                             return false;
                           }
                           return changed->setOpacity(value);
                         });
  }
}

void MainWindow::setLayerLocked(bool locked) {
  if (updatingLayers_) {
    return;
  }
  if (auto* view = currentDocument()) {
    const auto index = view->document().activeLayerIndex();
    const auto* layer = view->document().layerAt(index);
    if (!layer || layer->isLocked() == locked) {
      return;
    }
    view->performCommand(QStringLiteral("Change layer lock"),
                         [index, locked](Document& document) {
                           auto* changed = document.layerAt(index);
                           if (!changed) {
                             return false;
                           }
                           changed->setLocked(locked);
                           return true;
                         });
  }
}

void MainWindow::updateActions() {
  const auto* view = currentDocument();
  const bool hasDocument = view != nullptr;
  saveAction_->setEnabled(hasDocument);
  saveAsAction_->setEnabled(hasDocument);
  exportAction_->setEnabled(hasDocument);
  closeAction_->setEnabled(hasDocument);
  undoAction_->setEnabled(hasDocument && view->history().canUndo());
  redoAction_->setEnabled(hasDocument && view->history().canRedo());
  undoAction_->setText(hasDocument && view->history().canUndo()
                           ? QStringLiteral("&Undo %1").arg(
                                 view->history().undoDescription())
                           : QStringLiteral("&Undo"));
  redoAction_->setText(hasDocument && view->history().canRedo()
                           ? QStringLiteral("&Redo %1").arg(
                                 view->history().redoDescription())
                           : QStringLiteral("&Redo"));
  addLayerAction_->setEnabled(hasDocument);
  duplicateLayerAction_->setEnabled(hasDocument);
  renameLayerAction_->setEnabled(hasDocument);
  removeLayerAction_->setEnabled(hasDocument && view->document().layerCount() > 1);
  const auto activeLayer = hasDocument ? view->document().activeLayerIndex() : -1;
  const auto* mergeUpper =
      hasDocument ? view->document().layerAt(activeLayer) : nullptr;
  const auto* mergeLower =
      hasDocument ? view->document().layerAt(activeLayer - 1) : nullptr;
  mergeDownAction_->setEnabled(mergeUpper && mergeLower &&
                               !mergeUpper->isLocked() &&
                               !mergeLower->isLocked());
  bool canFlatten = hasDocument && view->document().layerCount() > 1;
  if (canFlatten) {
    for (int index = 0;
         index < static_cast<int>(view->document().layerCount()); ++index) {
      if (view->document().layerAt(index)->isLocked()) {
        canFlatten = false;
        break;
      }
    }
  }
  flattenAction_->setEnabled(canFlatten);
  moveLayerUpAction_->setEnabled(
      hasDocument && view->document().activeLayerIndex() <
                         view->document().layerCount() - 1);
  moveLayerDownAction_->setEnabled(
      hasDocument && view->document().activeLayerIndex() > 0);
}

void MainWindow::showError(const QString& title, const QString& detail) {
  QMessageBox::critical(this, title,
                        detail.isEmpty() ? QStringLiteral("An unknown error occurred.")
                                         : detail);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  for (int index = 0; index < tabs_->count(); ++index) {
    auto* view = qobject_cast<DocumentView*>(tabs_->widget(index));
    if (view && !canClose(view)) {
      event->ignore();
      return;
    }
  }
  QSettings settings;
  settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
  settings.setValue(QStringLiteral("window/state"), saveState());
  event->accept();
}
