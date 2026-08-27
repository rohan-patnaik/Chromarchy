#include "ui/CanvasWidget.h"

#include "core/Document.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace chromarchy {
namespace {

QImage selectionOverlay(const QImage& coverage) {
  QImage overlay(coverage.size(), QImage::Format_RGBA8888_Premultiplied);
  overlay.fill(Qt::transparent);
  for (int y = 0; y < coverage.height(); ++y) {
    const auto* maskLine = coverage.constScanLine(y);
    auto* overlayLine = overlay.scanLine(y);
    for (int x = 0; x < coverage.width(); ++x) {
      const auto alpha = static_cast<quint8>(maskLine[x] * 64 / 255);
      overlayLine[x * 4] = static_cast<quint8>(32 * alpha / 255);
      overlayLine[x * 4 + 1] = static_cast<quint8>(160 * alpha / 255);
      overlayLine[x * 4 + 2] = static_cast<quint8>(220 * alpha / 255);
      overlayLine[x * 4 + 3] = alpha;
    }
  }
  return overlay;
}

}  // namespace

CanvasWidget::CanvasWidget(const Document* document, QWidget* parent)
    : QAbstractScrollArea(parent), document_(document) {
  setObjectName(QStringLiteral("canvas"));
  setAccessibleName(QStringLiteral("Image canvas"));
  setAccessibleDescription(
      QStringLiteral("Scrollable view of the current image document"));
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  viewport()->setCursor(Qt::ArrowCursor);
  viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
  updateScrollBars();
}

double CanvasWidget::zoom() const noexcept {
  return zoom_;
}

void CanvasWidget::setZoom(double zoom) {
  zoom = std::clamp(zoom, 0.01, 32.0);
  if (qFuzzyCompare(zoom_, zoom)) {
    return;
  }

  const auto oldOrigin = canvasOrigin();
  const QPointF center(viewport()->width() / 2.0, viewport()->height() / 2.0);
  const QPointF documentCenter = (center - oldOrigin) / zoom_;
  zoom_ = zoom;
  updateScrollBars();
  const auto content = QSizeF(document_->size()) * zoom_;
  if (content.width() > viewport()->width()) {
    horizontalScrollBar()->setValue(
        qRound(documentCenter.x() * zoom_ - viewport()->width() / 2.0));
  }
  if (content.height() > viewport()->height()) {
    verticalScrollBar()->setValue(
        qRound(documentCenter.y() * zoom_ - viewport()->height() / 2.0));
  }
  viewport()->update();
  emit zoomChanged(zoom_);
}

QRect CanvasWidget::visibleDocumentRect() const {
  if (!document_) {
    return {};
  }
  const auto origin = canvasOrigin();
  const QRectF viewportRect(QPointF(), QSizeF(viewport()->size()));
  const QRectF canvasRect(origin, QSizeF(document_->size()) * zoom_);
  const auto visible = viewportRect.intersected(canvasRect);
  if (visible.isEmpty()) {
    return {};
  }
  const auto relative = visible.translated(-origin);
  const int left = qMax(0, static_cast<int>(std::floor(relative.left() / zoom_)));
  const int top = qMax(0, static_cast<int>(std::floor(relative.top() / zoom_)));
  const int right = qMin(document_->size().width(),
                         static_cast<int>(std::ceil(relative.right() / zoom_)));
  const int bottom = qMin(document_->size().height(),
                          static_cast<int>(std::ceil(relative.bottom() / zoom_)));
  return QRect(left, top, qMax(0, right - left), qMax(0, bottom - top));
}

void CanvasWidget::documentChanged() {
  viewport()->update();
}

void CanvasWidget::paintEvent(QPaintEvent*) {
  QPainter painter(viewport());
  painter.fillRect(viewport()->rect(), QColor(28, 30, 34));
  if (!document_) {
    return;
  }

  const auto origin = canvasOrigin();
  const QRectF canvasRect(origin, QSizeF(document_->size()) * zoom_);
  QPixmap checker(16, 16);
  checker.fill(QColor(210, 212, 216));
  QPainter checkerPainter(&checker);
  checkerPainter.fillRect(0, 0, 8, 8, QColor(238, 239, 241));
  checkerPainter.fillRect(8, 8, 8, 8, QColor(238, 239, 241));
  checkerPainter.end();
  painter.fillRect(canvasRect, QBrush(checker));

  const auto sourceRect = visibleDocumentRect();
  if (sourceRect.isEmpty()) {
    return;
  }
  painter.save();
  painter.translate(origin);
  painter.scale(zoom_, zoom_);
  painter.setClipRect(sourceRect);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ < 1.0);
  document_->paintComposite(painter, sourceRect);

  const auto& selection = document_->selection();
  const auto selectionTiles = selection.tileSnapshots(sourceRect);
  QRegion baseRegion(sourceRect);
  for (const auto& tile : selectionTiles) {
    baseRegion -= QRect(tile.origin, tile.pixels.size());
  }
  if (selection.baseCoverage() > 0) {
    const QColor baseColor(32, 160, 220,
                           selection.baseCoverage() * 64 / 255);
    for (const auto& rectangle : baseRegion) {
      painter.fillRect(rectangle, baseColor);
    }
  }
  for (const auto& tile : selectionTiles) {
    painter.drawImage(tile.origin, selectionOverlay(tile.pixels));
  }
  painter.restore();

  if (selecting_) {
    const auto selection = QRect(selectionStart_, selectionEnd_).normalized();
    const QRectF preview(origin.x() + selection.x() * zoom_,
                         origin.y() + selection.y() * zoom_,
                         selection.width() * zoom_, selection.height() * zoom_);
    painter.setPen(QPen(QColor(80, 200, 240), 1.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(preview);
  }
  painter.setPen(QColor(0, 0, 0, 160));
  painter.drawRect(canvasRect);
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  updateScrollBars();
}

void CanvasWidget::wheelEvent(QWheelEvent* event) {
  if (event->modifiers().testFlag(Qt::ControlModifier)) {
    setZoom(zoom_ * std::pow(1.0015, event->angleDelta().y()));
    event->accept();
    return;
  }
  QAbstractScrollArea::wheelEvent(event);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = true;
    panStart_ = event->position().toPoint();
    scrollStart_ = {horizontalScrollBar()->value(), verticalScrollBar()->value()};
    viewport()->setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  if (event->button() == Qt::LeftButton) {
    const auto position = documentPosition(event->position().toPoint());
    if (QRect(QPoint(), document_->size()).contains(position)) {
      selecting_ = true;
      selectionStart_ = position;
      selectionEnd_ = position;
      viewport()->update();
      event->accept();
      return;
    }
  }
  QAbstractScrollArea::mousePressEvent(event);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
  if (panning_) {
    const auto delta = event->position().toPoint() - panStart_;
    horizontalScrollBar()->setValue(scrollStart_.x() - delta.x());
    verticalScrollBar()->setValue(scrollStart_.y() - delta.y());
    event->accept();
    return;
  }
  if (selecting_) {
    auto position = documentPosition(event->position().toPoint());
    position.setX(qBound(0, position.x(), document_->size().width() - 1));
    position.setY(qBound(0, position.y(), document_->size().height() - 1));
    selectionEnd_ = position;
    viewport()->update();
    event->accept();
    return;
  }
  QAbstractScrollArea::mouseMoveEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (panning_ && event->button() == Qt::MiddleButton) {
    panning_ = false;
    viewport()->setCursor(Qt::ArrowCursor);
    event->accept();
    return;
  }
  if (selecting_ && event->button() == Qt::LeftButton) {
    selecting_ = false;
    const auto rectangle = QRect(selectionStart_, selectionEnd_).normalized();
    viewport()->update();
    emit selectionRequested(rectangle);
    event->accept();
    return;
  }
  QAbstractScrollArea::mouseReleaseEvent(event);
}

QPointF CanvasWidget::canvasOrigin() const {
  if (!document_) {
    return {};
  }
  const auto content = QSizeF(document_->size()) * zoom_;
  const double x = content.width() < viewport()->width()
                       ? (viewport()->width() - content.width()) / 2.0
                       : -horizontalScrollBar()->value();
  const double y = content.height() < viewport()->height()
                       ? (viewport()->height() - content.height()) / 2.0
                       : -verticalScrollBar()->value();
  return {x, y};
}

QPoint CanvasWidget::documentPosition(QPoint viewportPosition) const {
  if (!document_) {
    return {-1, -1};
  }
  const auto relative = QPointF(viewportPosition) - canvasOrigin();
  return {static_cast<int>(std::floor(relative.x() / zoom_)),
          static_cast<int>(std::floor(relative.y() / zoom_))};
}

void CanvasWidget::updateScrollBars() {
  if (!document_) {
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(0, 0);
    return;
  }
  const auto content = QSizeF(document_->size()) * zoom_;
  horizontalScrollBar()->setPageStep(viewport()->width());
  verticalScrollBar()->setPageStep(viewport()->height());
  horizontalScrollBar()->setRange(
      0, qMax(0, qCeil(content.width()) - viewport()->width()));
  verticalScrollBar()->setRange(
      0, qMax(0, qCeil(content.height()) - viewport()->height()));
}

}  // namespace chromarchy
