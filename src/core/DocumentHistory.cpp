#include "core/DocumentHistory.h"

#include <algorithm>
#include <utility>

namespace chromarchy {

SnapshotCommand::SnapshotCommand(QString description, Document before,
                                 Document after)
    : description_(std::move(description)),
      before_(std::move(before)),
      after_(std::move(after)),
      estimatedBytes_(before_.estimatedStorageBytes() +
                      after_.estimatedStorageBytes()) {}

void SnapshotCommand::redo(Document& document) {
  document = after_;
}

void SnapshotCommand::undo(Document& document) {
  document = before_;
}

quint64 SnapshotCommand::estimatedBytes() const noexcept {
  return estimatedBytes_;
}

const QString& SnapshotCommand::description() const noexcept {
  return description_;
}

DocumentHistory::DocumentHistory(quint64 byteBudget, qsizetype commandLimit)
    : byteBudget_(byteBudget), commandLimit_(qMax<qsizetype>(1, commandLimit)) {}

bool DocumentHistory::execute(std::unique_ptr<DocumentCommand> command,
                              Document& document) {
  if (!command || command->estimatedBytes() > byteBudget_) {
    return false;
  }
  discardRedo();
  command->redo(document);
  estimatedBytes_ += command->estimatedBytes();
  commands_.push_back(std::move(command));
  cursor_ = static_cast<qsizetype>(commands_.size());
  trimToLimits();
  return true;
}

bool DocumentHistory::undo(Document& document) {
  if (!canUndo()) {
    return false;
  }
  --cursor_;
  commands_[static_cast<size_t>(cursor_)]->undo(document);
  return true;
}

bool DocumentHistory::redo(Document& document) {
  if (!canRedo()) {
    return false;
  }
  commands_[static_cast<size_t>(cursor_)]->redo(document);
  ++cursor_;
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
    estimatedBytes_ -= commands_.back()->estimatedBytes();
    commands_.pop_back();
  }
}

void DocumentHistory::trimToLimits() {
  while (!commands_.empty() &&
         (static_cast<qsizetype>(commands_.size()) > commandLimit_ ||
          estimatedBytes_ > byteBudget_)) {
    estimatedBytes_ -= commands_.front()->estimatedBytes();
    commands_.erase(commands_.begin());
    cursor_ = qMax<qsizetype>(0, cursor_ - 1);
  }
}

}  // namespace chromarchy
