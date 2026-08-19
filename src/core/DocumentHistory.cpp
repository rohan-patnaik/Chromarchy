#include "core/DocumentHistory.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <utility>

namespace chromarchy {

SnapshotCommand::SnapshotCommand(QString description, Document before,
                                 Document after)
    : description_(std::move(description)),
      before_(std::move(before)),
      after_(std::move(after)),
      metadataBytes_(before_.estimatedMetadataBytes() +
                     after_.estimatedMetadataBytes()) {
  QHash<quint64, quint64> uniqueBlocks;
  for (const auto& block : before_.storageBlocks()) {
    uniqueBlocks.insert(block.key, block.bytes);
  }
  for (const auto& block : after_.storageBlocks()) {
    uniqueBlocks.insert(block.key, block.bytes);
  }
  storageBlocks_.reserve(uniqueBlocks.size());
  for (auto block = uniqueBlocks.cbegin(); block != uniqueBlocks.cend(); ++block) {
    storageBlocks_.push_back({block.key(), block.value()});
  }
}

void SnapshotCommand::redo(Document& document) {
  document = after_;
}

void SnapshotCommand::undo(Document& document) {
  document = before_;
}

quint64 SnapshotCommand::metadataBytes() const noexcept {
  return metadataBytes_;
}

const QVector<StorageBlock>& SnapshotCommand::storageBlocks() const noexcept {
  return storageBlocks_;
}

const QString& SnapshotCommand::description() const noexcept {
  return description_;
}

DocumentHistory::DocumentHistory(quint64 byteBudget, qsizetype commandLimit)
    : byteBudget_(byteBudget), commandLimit_(qMax<qsizetype>(1, commandLimit)) {}

bool DocumentHistory::execute(std::unique_ptr<DocumentCommand> command,
                              Document& document) {
  if (!command) {
    return false;
  }

  command->redo(document);
  QSet<quint64> currentBlocks;
  for (const auto& block : document.storageBlocks()) {
    currentBlocks.insert(block.key);
  }
  quint64 commandBytes = command->metadataBytes();
  for (const auto& block : command->storageBlocks()) {
    if (!currentBlocks.contains(block.key)) {
      commandBytes += block.bytes;
    }
  }
  if (commandBytes > byteBudget_) {
    command->undo(document);
    return false;
  }

  discardRedo();
  commands_.push_back(std::move(command));
  cursor_ = static_cast<qsizetype>(commands_.size());
  recalculateEstimatedBytes(document);
  trimToLimits(document);
  return true;
}

bool DocumentHistory::undo(Document& document) {
  if (!canUndo()) {
    return false;
  }
  --cursor_;
  commands_[static_cast<size_t>(cursor_)]->undo(document);
  recalculateEstimatedBytes(document);
  return true;
}

bool DocumentHistory::redo(Document& document) {
  if (!canRedo()) {
    return false;
  }
  commands_[static_cast<size_t>(cursor_)]->redo(document);
  ++cursor_;
  recalculateEstimatedBytes(document);
  return true;
}

void DocumentHistory::clear() {
  commands_.clear();
  cursor_ = 0;
  estimatedBytes_ = 0;
}

bool DocumentHistory::canUndo() const noexcept {
  return cursor_ > 0;
}

bool DocumentHistory::canRedo() const noexcept {
  return cursor_ < static_cast<qsizetype>(commands_.size());
}

QString DocumentHistory::undoDescription() const {
  return canUndo() ? commands_[static_cast<size_t>(cursor_ - 1)]->description()
                   : QString{};
}

QString DocumentHistory::redoDescription() const {
  return canRedo() ? commands_[static_cast<size_t>(cursor_)]->description()
                   : QString{};
}

qsizetype DocumentHistory::size() const noexcept {
  return static_cast<qsizetype>(commands_.size());
}

quint64 DocumentHistory::estimatedBytes() const noexcept {
  return estimatedBytes_;
}

void DocumentHistory::discardRedo() {
  while (static_cast<qsizetype>(commands_.size()) > cursor_) {
    commands_.pop_back();
  }
}

void DocumentHistory::recalculateEstimatedBytes(const Document& document) {
  QSet<quint64> currentBlocks;
  for (const auto& block : document.storageBlocks()) {
    currentBlocks.insert(block.key);
  }
  QHash<quint64, quint64> retainedBlocks;
  estimatedBytes_ = 0;
  for (const auto& command : commands_) {
    estimatedBytes_ += command->metadataBytes();
    for (const auto& block : command->storageBlocks()) {
      if (!currentBlocks.contains(block.key)) {
        retainedBlocks.insert(block.key, block.bytes);
      }
    }
  }
  for (const auto bytes : retainedBlocks) {
    estimatedBytes_ += bytes;
  }
}

void DocumentHistory::trimToLimits(const Document& document) {
  while (!commands_.empty() &&
         (static_cast<qsizetype>(commands_.size()) > commandLimit_ ||
          estimatedBytes_ > byteBudget_)) {
    commands_.erase(commands_.begin());
    cursor_ = qMax<qsizetype>(0, cursor_ - 1);
    recalculateEstimatedBytes(document);
  }
}

}  // namespace chromarchy
