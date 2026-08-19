#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("Chromarchy"));
  QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
  QApplication::setOrganizationName(QStringLiteral("Chromarchy"));
  QApplication::setOrganizationDomain(QStringLiteral("io.github.rohan-patnaik"));
  QLoggingCategory::setFilterRules(QStringLiteral("chromarchy.*.debug=false"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Offline-first professional raster image editor"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(QStringLiteral("files"),
                               QStringLiteral("Images or documents to open."),
                               QStringLiteral("[files...]"));
  parser.process(app);

  MainWindow window;
  window.show();
  for (const auto& path : parser.positionalArguments()) {
    window.openFile(path);
  }
  return app.exec();
}
