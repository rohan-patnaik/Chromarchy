#include "RecentDocuments.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class RecentDocumentsTest final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void boundsDeduplicatesAndPersistsPathsOnly();
  void normalizesHostileSettingsAndPrunesMissing();

private:
  QTemporaryDir settingsDirectory_;
};

void RecentDocumentsTest::initTestCase() {
  QVERIFY(settingsDirectory_.isValid());
  QCoreApplication::setOrganizationName(QStringLiteral("ChromarchyTests"));
  QCoreApplication::setApplicationName(QStringLiteral("RecentDocumentsTest"));
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     settingsDirectory_.path());
}

void RecentDocumentsTest::init() {
  QSettings settings;
  settings.clear();
  settings.sync();
}

void RecentDocumentsTest::boundsDeduplicatesAndPersistsPathsOnly() {
  QTemporaryDir files;
  QVERIFY(files.isValid());
  QStringList created;
  const QByteArray privatePayload("CONFIDENTIAL_CONTENT_SENTINEL");
  for (int index = 0; index < 25; ++index) {
    const auto path = files.filePath(QStringLiteral("document-%1.chromarchy")
                                         .arg(index, 2, 10, QLatin1Char('0')));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(privatePayload), privatePayload.size());
    file.close();
    created.push_back(path);
  }

  RecentDocuments recents;
  for (const auto &path : created) {
    recents.add(path);
  }
  auto paths = recents.paths();
  QCOMPARE(paths.size(), RecentDocuments::maximumEntries);
  QCOMPARE(paths.constFirst(), created.constLast());
  QCOMPARE(paths.constLast(), created.at(5));

  recents.add(created.at(10));
  paths = recents.paths();
  QCOMPARE(paths.size(), RecentDocuments::maximumEntries);
  QCOMPARE(paths.constFirst(), created.at(10));
  QCOMPARE(paths.count(created.at(10)), 1);

  QSettings settings;
  QCOMPARE(settings.value(RecentDocuments::settingsKey()).toStringList(),
           paths);
  QFile settingsFile(settings.fileName());
  QVERIFY(settingsFile.open(QIODevice::ReadOnly));
  QVERIFY(!settingsFile.readAll().contains("CONFIDENTIAL_CONTENT_SENTINEL"));

  recents.clear();
  QVERIFY(recents.paths().isEmpty());
  QVERIFY(!settings.contains(RecentDocuments::settingsKey()));
}

void RecentDocumentsTest::normalizesHostileSettingsAndPrunesMissing() {
  QFile fixture(QStringLiteral(
      CHROMARCHY_SOURCE_DIR "/tests/fixtures/recent-documents-hostile.json"));
  QVERIFY(fixture.open(QIODevice::ReadOnly));
  QJsonParseError parseError;
  const auto root =
      QJsonDocument::fromJson(fixture.readAll(), &parseError).object();
  QCOMPARE(parseError.error, QJsonParseError::NoError);
  QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
  QCOMPARE(root.value(QStringLiteral("maximumEntries")).toInt(),
           RecentDocuments::maximumEntries);
  QCOMPARE(root.value(QStringLiteral("maximumPathUtf8Bytes")).toInt(),
           static_cast<int>(RecentDocuments::maximumPathUtf8Bytes));
  const auto hostileKinds =
      root.value(QStringLiteral("hostileKinds")).toArray();
  QCOMPARE(hostileKinds.size(), 5);

  QTemporaryDir files;
  QVERIFY(files.isValid());
  const auto validPath = files.filePath(QStringLiteral("kept.chromarchy"));
  QFile valid(validPath);
  QVERIFY(valid.open(QIODevice::WriteOnly));
  valid.close();
  const auto missingPath = files.filePath(QStringLiteral("missing.chromarchy"));
  const QString overlongPath =
      QStringLiteral("/") +
      QString(RecentDocuments::maximumPathUtf8Bytes, QLatin1Char('x'));
  QVERIFY(overlongPath.toUtf8().size() > RecentDocuments::maximumPathUtf8Bytes);

  QSettings settings;
  QStringList hostileStored{QString{},   QStringLiteral("relative.chromarchy"),
                            missingPath, validPath,
                            validPath,   overlongPath};
  while (hostileStored.size() < 200) {
    hostileStored.push_back(missingPath);
  }
  settings.setValue(RecentDocuments::settingsKey(), hostileStored);
  settings.sync();

  RecentDocuments recents;
  QCOMPARE(recents.paths(), QStringList{validPath});
  QCOMPARE(settings.value(RecentDocuments::settingsKey()).toStringList(),
           QStringList{validPath});

  QVERIFY(QFile::remove(validPath));
  QVERIFY(recents.paths().isEmpty());
  QVERIFY(!settings.contains(RecentDocuments::settingsKey()));

  recents.add(QStringLiteral("relative.chromarchy"));
  recents.add(missingPath);
  recents.add(overlongPath);
  QVERIFY(recents.paths().isEmpty());
}

QTEST_APPLESS_MAIN(RecentDocumentsTest)

#include "RecentDocumentsTest.moc"
