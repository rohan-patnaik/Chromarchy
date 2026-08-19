#pragma once

#include <QMainWindow>

class QAction;
class QCloseEvent;
class QListWidget;
class QListWidgetItem;
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
  void removeLayer();
  void updateActions();
  void showError(const QString& title, const QString& detail);

  QTabWidget* tabs_ = nullptr;       // Owned by QObject parent.
  QListWidget* layers_ = nullptr;    // Owned by QObject parent.
  QAction* saveAction_ = nullptr;    // Owned by QObject parent.
  QAction* saveAsAction_ = nullptr;  // Owned by QObject parent.
  QAction* exportAction_ = nullptr;  // Owned by QObject parent.
  QAction* closeAction_ = nullptr;   // Owned by QObject parent.
  QAction* addLayerAction_ = nullptr;
  QAction* duplicateLayerAction_ = nullptr;
  QAction* removeLayerAction_ = nullptr;
  bool updatingLayers_ = false;
  int untitledCounter_ = 1;
};
