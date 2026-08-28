#pragma once

#include <QString>
#include <QStringList>

class RecentDocuments final {
public:
  static constexpr int maximumEntries = 20;
  static constexpr qsizetype maximumPathUtf8Bytes = 4096;

  [[nodiscard]] QStringList paths();
  void add(const QString &filePath);
  void remove(const QString &filePath);
  void clear();

  [[nodiscard]] static QString settingsKey();

private:
  [[nodiscard]] static QString normalizedPath(const QString &filePath);
  static void store(const QStringList &paths);
};
