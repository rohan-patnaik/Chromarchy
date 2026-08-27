#pragma once

#include "core/Document.h"

#include <QString>

#include <memory>
#include <vector>

namespace chromarchy {

class DocumentCommand {
public:
  virtual ~DocumentCommand() = default;

  virtual void redo(Document& document) = 0;
  virtual void undo(Document& document) = 0;
  [[nodiscard]] virtual quint64 metadataBytes() const noexcept = 0;
  [[nodiscard]] virtual const QVector<StorageBlock>& storageBlocks() const noexcept = 0;
  [[nodiscard]] virtual const QString& description() const noexcept = 0;
};

class SnapshotCommand final : public DocumentCommand {
public:
  SnapshotCommand(QString description, Document before, Document after);

  void redo(Document& document) override;
  void undo(Document& document) override;
  [[nodiscard]] quint64 metadataBytes() const noexcept override;
  [[nodiscard]] const QVector<StorageBlock>& storageBlocks() const noexcept override;
  [[nodiscard]] const QString& description() const noexcept override;

private:
  QString description_;
  Document before_;
  Document after_;
  quint64 metadataBytes_ = 0;
  QVector<StorageBlock> storageBlocks_;
};

class DocumentHistory final {
public:
  static constexpr quint64 defaultByteBudget = 512ULL * 1024ULL * 1024ULL;
  static constexpr qsizetype defaultCommandLimit = 200;

  explicit DocumentHistory(quint64 byteBudget = defaultByteBudget,
                           qsizetype commandLimit = defaultCommandLimit);

  bool execute(std::unique_ptr<DocumentCommand> command, Document& document);
  bool undo(Document& document);
  bool redo(Document& document);
  void clear();

  [[nodiscard]] bool canUndo() const noexcept;
  [[nodiscard]] bool canRedo() const noexcept;
  [[nodiscard]] QString undoDescription() const;
  [[nodiscard]] QString redoDescription() const;
  [[nodiscard]] qsizetype size() const noexcept;
  [[nodiscard]] quint64 estimatedBytes() const noexcept;
  [[nodiscard]] quint64 currentStateId() const noexcept;

private:
  void discardRedo();
  void recalculateEstimatedBytes(const Document& document);
  void trimToLimits(const Document& document);

  std::vector<std::unique_ptr<DocumentCommand>> commands_;
  std::vector<quint64> stateIds_{0};
  qsizetype cursor_ = 0;
  quint64 nextStateId_ = 1;
  quint64 estimatedBytes_ = 0;
  quint64 byteBudget_;
  qsizetype commandLimit_;
};

}  // namespace chromarchy
