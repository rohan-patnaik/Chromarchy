#include "MainWindow.h"

#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("Chromarchy — local image studio");
  resize(1280, 800);

  auto* canvas = new QLabel("Open an image or create a document", this);
  canvas->setAlignment(Qt::AlignCenter);
  canvas->setObjectName("canvasPlaceholder");
  setCentralWidget(canvas);

  auto* fileMenu = menuBar()->addMenu("&File");
  fileMenu->addAction("&New");
  fileMenu->addAction("&Open…");
  fileMenu->addSeparator();
  fileMenu->addAction("E&xit", this, &QWidget::close);

  auto* tools = addToolBar("Tools");
  tools->setMovable(true);
  tools->addAction("Move");
  tools->addAction("Select");
  tools->addAction("Brush");
  tools->addAction("Erase");

  auto* layersDock = new QDockWidget("Layers", this);
  auto* layers = new QListWidget(layersDock);
  layers->addItem("Background");
  layersDock->setWidget(layers);
  addDockWidget(Qt::RightDockWidgetArea, layersDock);

  statusBar()->showMessage("Chromarchy foundation build");
}

