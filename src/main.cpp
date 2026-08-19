#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("Chromarchy");
  QApplication::setOrganizationName("Rohan Patnaik");

  MainWindow window;
  window.show();
  return app.exec();
}

