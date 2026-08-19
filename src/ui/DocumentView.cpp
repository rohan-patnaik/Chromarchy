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
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  canvas_ = new CanvasWidget(&document_, this);
  layout->addWidget(canvas_);
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
  if (modified_ == modified) {
    return;
  }
  modified_ = modified;
  emit titleChanged(tabTitle());
}

QString DocumentView::tabTitle() const {
  return modified_ ? displayName_ + QStringLiteral(" •") : displayName_;
}

NativeDocumentWriteResult DocumentView::save(const QString& filePath) {
  auto result = NativeDocumentCodec::save(document_, filePath);
  if (result) {
    filePath_ = QFileInfo(filePath).absoluteFilePath();
    displayName_ = QFileInfo(filePath).fileName();
    modified_ = false;
    emit titleChanged(tabTitle());
  }
  return result;
}

}  // namespace chromarchy
