#pragma once

#include "RecentDocuments.h"

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QCheckBox;
class QDoubleSpinBox;
class QDockWidget;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QTabWidget;

namespace chromarchy {
class DocumentView;
}

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  bool openFile(const QString& filePath);

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  void createActions();
  void createLayersDock();
  void newDocument();
  void chooseAndOpenFile();
  void refreshRecentDocumentsMenu();
  void scheduleRecentDocumentsMenuRefresh();
  void openRecentDocument(const QString& filePath);
  void clearRecentDocuments();
  void recordRecentDocument(const QString& filePath);
  void showOfflineHelp();
  void showAbout();
  bool saveDocument(chromarchy::DocumentView* view, bool choosePath);
  void exportDocument();
  void addDocumentTab(chromarchy::DocumentView* view);
  void closeDocumentTab(int index);
  [[nodiscard]] bool canClose(chromarchy::DocumentView* view);
  [[nodiscard]] chromarchy::DocumentView* currentDocument() const;
  void refreshLayers();
  void layerSelectionChanged(int row);
  void layerItemChanged(QListWidgetItem* item);
  void addLayer();
  void duplicateLayer();
  void renameLayer();
  void removeLayer();
  void mergeLayerDown();
  void flattenDocument();
  void moveLayerUp();
  void moveLayerDown();
  void navigateDocument(int offset);
  void focusCanvas();
  void focusLayersPanel();
  void commitLayerOpacity();
  void setLayerLocked(bool locked);
  void updateActions();
  void showError(const QString& title, const QString& detail);

  QTabWidget* tabs_ = nullptr;       // Owned by QObject parent.
  QDockWidget* layersDock_ = nullptr;  // Owned by QObject parent.
  QListWidget* layers_ = nullptr;    // Owned by QObject parent.
  QMenu* recentDocumentsMenu_ = nullptr;  // Owned by QObject parent.
  QAction* saveAction_ = nullptr;    // Owned by QObject parent.
  QAction* saveAsAction_ = nullptr;  // Owned by QObject parent.
  QAction* exportAction_ = nullptr;  // Owned by QObject parent.
  QAction* closeAction_ = nullptr;   // Owned by QObject parent.
  QAction* undoAction_ = nullptr;
  QAction* redoAction_ = nullptr;
  QAction* selectAllAction_ = nullptr;
  QAction* deselectAction_ = nullptr;
  QAction* invertSelectionAction_ = nullptr;
  QAction* fitViewAction_ = nullptr;
  QAction* rotateViewClockwiseAction_ = nullptr;
  QAction* rotateViewCounterclockwiseAction_ = nullptr;
  QAction* resetViewRotationAction_ = nullptr;
  QAction* pixelGridAction_ = nullptr;
  QAction* nextDocumentAction_ = nullptr;
  QAction* previousDocumentAction_ = nullptr;
  QAction* toggleLayersAction_ = nullptr;
  QAction* focusCanvasAction_ = nullptr;
  QAction* focusLayersAction_ = nullptr;
  QAction* addLayerAction_ = nullptr;
  QAction* duplicateLayerAction_ = nullptr;
  QAction* renameLayerAction_ = nullptr;
  QAction* removeLayerAction_ = nullptr;
  QAction* mergeDownAction_ = nullptr;
  QAction* flattenAction_ = nullptr;
  QAction* moveLayerUpAction_ = nullptr;
  QAction* moveLayerDownAction_ = nullptr;
  QDoubleSpinBox* opacity_ = nullptr;  // Owned by QObject parent.
  QCheckBox* layerLocked_ = nullptr;   // Owned by QObject parent.
  bool updatingLayers_ = false;
  bool recentDocumentsMenuRefreshPending_ = false;
  int untitledCounter_ = 1;
  RecentDocuments recentDocuments_;
};
