#include "RecentDocuments.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSettings>

QStringList RecentDocuments::paths() {
  QSettings settings;
  const auto stored = settings.value(settingsKey()).toStringList();
  QStringList sanitized;
  sanitized.reserve(
      qMin<qsizetype>(stored.size(), static_cast<qsizetype>(maximumEntries)));
  QSet<QString> seen;
  const auto candidateCount =
      qMin<qsizetype>(stored.size(), static_cast<qsizetype>(maximumEntries));
  for (qsizetype index = 0; index < candidateCount; ++index) {
    const auto &candidate = stored.at(index);
    const auto normalized = normalizedPath(candidate);
    if (normalized.isEmpty() || seen.contains(normalized) ||
        !QFileInfo(normalized).isFile()) {
      continue;
    }
    seen.insert(normalized);
    sanitized.push_back(normalized);
    if (sanitized.size() == maximumEntries) {
      break;
    }
  }
  if (sanitized != stored) {
    store(sanitized);
  }
  return sanitized;
}

void RecentDocuments::add(const QString &filePath) {
  const auto normalized = normalizedPath(filePath);
  if (normalized.isEmpty() || !QFileInfo(normalized).isFile()) {
    return;
  }
  auto current = paths();
  current.removeAll(normalized);
  current.prepend(normalized);
  if (current.size() > maximumEntries) {
    current.resize(maximumEntries);
  }
  store(current);
}

void RecentDocuments::remove(const QString &filePath) {
  const auto normalized = normalizedPath(filePath);
  if (normalized.isEmpty()) {
    return;
  }
  auto current = paths();
  if (current.removeAll(normalized) != 0) {
    store(current);
  }
}

void RecentDocuments::clear() {
  QSettings settings;
  settings.remove(settingsKey());
  settings.sync();
}

QString RecentDocuments::settingsKey() {
  return QStringLiteral("documents/recentFiles");
}

QString RecentDocuments::normalizedPath(const QString &filePath) {
  if (filePath.isEmpty() || filePath.size() > maximumPathUtf8Bytes ||
      !QDir::isAbsolutePath(filePath)) {
    return {};
  }
  const auto normalized =
      QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
  if (normalized.isEmpty() ||
      normalized.toUtf8().size() > maximumPathUtf8Bytes) {
    return {};
  }
  return normalized;
}

void RecentDocuments::store(const QStringList &paths) {
  QSettings settings;
  if (paths.isEmpty()) {
    settings.remove(settingsKey());
  } else {
    settings.setValue(settingsKey(), paths);
  }
  settings.sync();
}
