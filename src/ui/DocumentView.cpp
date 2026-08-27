#include "ui/DocumentView.h"

#include "ui/CanvasWidget.h"

#include <QFileInfo>
#include <QVBoxLayout>

#include <utility>

namespace chromarchy {

DocumentView::DocumentView(Document document, QString displayName,
                           QString filePath, bool modified, QWidget* parent)
    : QWidget(parent),
      document_(std::move(document)),
      displayName_(std::move(displayName)),
      filePath_(std::move(filePath)),
      modified_(modified) {
  setObjectName(QStringLiteral("documentView"));
  setAccessibleName(QStringLiteral("Document %1").arg(displayName_));
  setAccessibleDescription(QStringLiteral("Image document editing view"));
  if (!modified_) {
    savedStateId_ = history_.currentStateId();
  }
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  canvas_ = new CanvasWidget(&document_, this);
  layout->addWidget(canvas_);
  connect(canvas_, &CanvasWidget::selectionRequested, this,
          [this](QRect rectangle) {
            performCommand(QStringLiteral("Select rectangle"),
                           [rectangle](Document& document) {
                             return document.selection().selectRectangle(
                                 rectangle);
                           });
          });
}

Document& DocumentView::document() noexcept {
  return document_;
}

const Document& DocumentView::document() const noexcept {
  return document_;
}

CanvasWidget* DocumentView::canvas() const noexcept {
  return canvas_;
}

const QString& DocumentView::filePath() const noexcept {
  return filePath_;
}

const QString& DocumentView::displayName() const noexcept {
  return displayName_;
}

bool DocumentView::isModified() const noexcept {
  return modified_;
}

void DocumentView::setModified(bool modified) {
  if (modified) {
    savedStateId_.reset();
  } else {
    savedStateId_ = history_.currentStateId();
  }
  if (modified_ == modified) {
    return;
  }
  modified_ = modified;
  emit titleChanged(tabTitle());
}

void DocumentView::refreshModifiedFromHistory() {
  const bool modified =
      !savedStateId_ || *savedStateId_ != history_.currentStateId();
  if (modified_ == modified) {
    return;
  }
  modified_ = modified;
  emit titleChanged(tabTitle());
}

bool DocumentView::performCommand(
    const QString& description,
    const std::function<bool(Document&)>& mutation) {
  auto after = document_;
  if (!mutation(after)) {
    return false;
  }
  auto command = std::make_unique<SnapshotCommand>(description, document_,
                                                    std::move(after));
  if (!history_.execute(std::move(command), document_)) {
    emit commandFailed(
        QStringLiteral("%1 exceeded the bounded undo-history budget; no changes "
                       "were applied.")
            .arg(description));
    return false;
  }
  refreshModifiedFromHistory();
  canvas_->documentChanged();
  emit historyChanged();
  return true;
}

bool DocumentView::undo() {
  if (!history_.undo(document_)) {
    return false;
  }
  refreshModifiedFromHistory();
  canvas_->documentChanged();
  emit historyChanged();
  return true;
}

bool DocumentView::redo() {
  if (!history_.redo(document_)) {
    return false;
  }
  refreshModifiedFromHistory();
  canvas_->documentChanged();
  emit historyChanged();
  return true;
}

const DocumentHistory& DocumentView::history() const noexcept {
  return history_;
}

QString DocumentView::tabTitle() const {
  return modified_ ? displayName_ + QStringLiteral(" •") : displayName_;
}

NativeDocumentWriteResult DocumentView::save(const QString& filePath) {
  auto result = NativeDocumentCodec::save(document_, filePath);
  if (result) {
    filePath_ = QFileInfo(filePath).absoluteFilePath();
    displayName_ = QFileInfo(filePath).fileName();
    setAccessibleName(QStringLiteral("Document %1").arg(displayName_));
    savedStateId_ = history_.currentStateId();
    modified_ = false;
    emit titleChanged(tabTitle());
  }
  return result;
}

}  // namespace chromarchy
