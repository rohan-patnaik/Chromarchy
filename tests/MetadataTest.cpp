#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
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
  void launcherHelperReportsPermissionDeniedCandidate();
  void launcherHelperSkipsUnusableCandidate();
  void launcherHelperPreservesEmptyPathComponents();
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
  QVERIFY(helper.contains("candidate=$directory/chromarchy"));
  QVERIFY(helper.contains("Chromarchy is not installed"));
  QVERIFY(helper.contains("notify-send"));
  QVERIFY(helper.contains("exit 127"));

  QTemporaryDir emptyPath;
  QVERIFY(emptyPath.isValid());
  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("PATH"), emptyPath.path());
  process.setProcessEnvironment(environment);
  process.start(QStringLiteral("/bin/sh"),
                {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                "/scripts/launch-chromarchy")});
  QVERIFY(process.waitForFinished(5'000));
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QCOMPARE(process.exitCode(), 127);
  const auto error = process.readAllStandardError();
  QVERIFY(error.contains("Chromarchy is not installed"));
  QVERIFY(error.contains("Install the native chromarchy binary on PATH"));
}

void MetadataTest::launcherHelperReportsPermissionDeniedCandidate() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const auto path = directory.filePath(QStringLiteral("chromarchy"));
  QFile fake(path);
  QVERIFY(fake.open(QIODevice::WriteOnly));
  QCOMPARE(fake.write("#!/bin/sh\nexit 0\n"), 17);
  fake.close();
  QVERIFY(fake.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("PATH"), directory.path());
  process.setProcessEnvironment(environment);
  process.start(QStringLiteral("/bin/sh"),
                {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                "/scripts/launch-chromarchy")});
  QVERIFY(process.waitForFinished(5'000));
  QCOMPARE(process.exitCode(), 126);
  const auto error = process.readAllStandardError();
  QVERIFY(error.contains("Chromarchy cannot be launched"));
  QVERIFY(error.contains(path.toUtf8()));
  QVERIFY(error.contains("not an executable regular file"));
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
  environment.insert(QStringLiteral("PATH"), directory.path());
  process.setProcessEnvironment(environment);
  process.start(QStringLiteral("/bin/sh"),
                {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                "/scripts/launch-chromarchy")});
  QVERIFY(process.waitForFinished(5'000));
  QCOMPARE(process.exitCode(), 42);
  const auto error = process.readAllStandardError();
  QVERIFY(error.contains("Chromarchy exited unexpectedly"));
  QVERIFY(error.contains("status 42"));
}

void MetadataTest::launcherHelperSkipsUnusableCandidate() {
  QTemporaryDir unusableDirectory;
  QTemporaryDir executableDirectory;
  QVERIFY(unusableDirectory.isValid());
  QVERIFY(executableDirectory.isValid());

  QFile unusable(unusableDirectory.filePath(QStringLiteral("chromarchy")));
  QVERIFY(unusable.open(QIODevice::WriteOnly));
  QCOMPARE(unusable.write("#!/bin/sh\nexit 0\n"), 17);
  unusable.close();
  QVERIFY(unusable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

  const auto executablePath =
      executableDirectory.filePath(QStringLiteral("chromarchy"));
  QFile executable(executablePath);
  QVERIFY(executable.open(QIODevice::WriteOnly));
  QCOMPARE(executable.write("#!/bin/sh\nexit 23\n"), 18);
  executable.close();
  QVERIFY(executable.setPermissions(QFileDevice::ReadOwner |
                                    QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner));

  QProcess process;
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("PATH"),
                     unusableDirectory.path() + QStringLiteral(":") +
                         executableDirectory.path());
  process.setProcessEnvironment(environment);
  process.start(QStringLiteral("/bin/sh"),
                {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                "/scripts/launch-chromarchy")});
  QVERIFY(process.waitForFinished(5'000));
  QCOMPARE(process.exitStatus(), QProcess::NormalExit);
  QCOMPARE(process.exitCode(), 23);
  const auto error = process.readAllStandardError();
  QVERIFY(error.contains("Chromarchy exited unexpectedly"));
  QVERIFY(error.contains(executablePath.toUtf8()));
  QVERIFY(!error.contains("not an executable regular file"));
}

void MetadataTest::launcherHelperPreservesEmptyPathComponents() {
  QTemporaryDir workingDirectory;
  QVERIFY(workingDirectory.isValid());
  QFile executable(workingDirectory.filePath(QStringLiteral("chromarchy")));
  QVERIFY(executable.open(QIODevice::WriteOnly));
  QCOMPARE(executable.write("#!/bin/sh\nexit 24\n"), 18);
  executable.close();
  QVERIFY(executable.setPermissions(QFileDevice::ReadOwner |
                                    QFileDevice::WriteOwner |
                                    QFileDevice::ExeOwner));

  const QStringList pathValues = {QStringLiteral(":/definitely/not/present"),
                                  QStringLiteral("/definitely/not/present:")};
  for (const auto& pathValue : pathValues) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), pathValue);
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(workingDirectory.path());
    process.start(QStringLiteral("/bin/sh"),
                  {QStringLiteral(CHROMARCHY_SOURCE_DIR
                                  "/scripts/launch-chromarchy")});
    QVERIFY2(process.waitForFinished(5'000), qPrintable(pathValue));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 24);
    const auto error = process.readAllStandardError();
    QVERIFY2(error.contains("native application at ./chromarchy"),
             error.constData());
  }
}

QTEST_APPLESS_MAIN(MetadataTest)

#include "MetadataTest.moc"
