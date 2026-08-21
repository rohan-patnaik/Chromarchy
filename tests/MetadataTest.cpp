#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

namespace {

QByteArray readRepositoryFile(const QString& relativePath) {
  QFile file(QStringLiteral(CHROMARCHY_SOURCE_DIR "/") + relativePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.readAll();
}

}  // namespace

class MetadataTest final : public QObject {
  Q_OBJECT

private slots:
  void manifestDescribesMenuPlugin();
  void launcherReportsMissingBinary();
  void launcherIsDetachedFromShellLifecycle();
  void launcherHelperReportsEarlyExit();
};

void MetadataTest::manifestDescribesMenuPlugin() {
  const auto contents = readRepositoryFile(QStringLiteral("manifest.json"));
  QVERIFY2(!contents.isEmpty(), "manifest.json must be readable");

  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(contents, &error);
  QCOMPARE(error.error, QJsonParseError::NoError);
  QVERIFY(document.isObject());

  const auto manifest = document.object();
  QCOMPARE(manifest.value(QStringLiteral("schemaVersion")).toInt(), 1);
  QCOMPARE(manifest.value(QStringLiteral("id")).toString(),
           QStringLiteral("io.github.rohan-patnaik.chromarchy"));
  QVERIFY(manifest.value(QStringLiteral("kinds")).toArray().contains(
      QStringLiteral("menu")));
  QCOMPARE(manifest.value(QStringLiteral("entryPoints"))
               .toObject()
               .value(QStringLiteral("menu"))
               .toString(),
           QStringLiteral("Plugin.qml"));
}

void MetadataTest::launcherReportsMissingBinary() {
  const auto contents = readRepositoryFile(QStringLiteral("Plugin.qml"));
  QVERIFY2(!contents.isEmpty(), "Plugin.qml must be readable");
  QVERIFY(contents.contains("scripts/launch-chromarchy"));
  const auto helper = readRepositoryFile(QStringLiteral("scripts/launch-chromarchy"));
  QVERIFY(helper.contains("command -v chromarchy"));
  QVERIFY(helper.contains("Chromarchy is not installed"));
  QVERIFY(helper.contains("notify-send"));
  QVERIFY(helper.contains("exit 127"));
}

void MetadataTest::launcherIsDetachedFromShellLifecycle() {
  const auto contents = readRepositoryFile(QStringLiteral("Plugin.qml"));
  QVERIFY2(!contents.isEmpty(), "Plugin.qml must be readable");
  QVERIFY(contents.contains("Quickshell.execDetached(["));
  QVERIFY2(!contents.contains("Process {"),
           "A tracked Process is killed when the Quickshell plugin reloads");
  QVERIFY(!contents.contains(".running"));
}

void MetadataTest::launcherHelperReportsEarlyExit() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QFile fake(directory.filePath(QStringLiteral("chromarchy")));
  QVERIFY(fake.open(QIODevice::WriteOnly));
  QCOMPARE(fake.write("#!/bin/sh\nexit 42\n"), 18);
  fake.close();
  QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeOwner));

  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("PATH"),
                     directory.path() + QStringLiteral(":/usr/bin:/bin"));
  process.setProcessEnvironment(environment);
  process.start(QStringLiteral("sh"),
                {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                "/scripts/launch-chromarchy")});
  QVERIFY(process.waitForFinished(5'000));
  QCOMPARE(process.exitCode(), 42);
  const auto error = process.readAllStandardError();
  QVERIFY(error.contains("Chromarchy exited unexpectedly"));
  QVERIFY(error.contains("status 42"));
}

QTEST_APPLESS_MAIN(MetadataTest)

#include "MetadataTest.moc"
