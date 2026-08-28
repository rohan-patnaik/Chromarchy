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
  refreshAccessibleDescription();
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
  zoom = std::clamp(zoom, minimumZoom, maximumZoom);
  if (qFuzzyCompare(zoom_, zoom)) {
    return;
  }

  const QPointF center(viewport()->width() / 2.0, viewport()->height() / 2.0);
  const QPointF documentCenter = documentPositionF(center);
  zoom_ = zoom;
  updateScrollBars();
  centerDocumentPosition(documentCenter);
  refreshAccessibleDescription();
  viewport()->update();
  emit zoomChanged(zoom_);
}

void CanvasWidget::fitToViewport() {
  const auto rotatedSize = rotatedDocumentSize();
  if (rotatedSize.isEmpty() || viewport()->width() <= 0 ||
      viewport()->height() <= 0) {
    return;
  }
  const double horizontal = static_cast<double>(viewport()->width()) /
                            rotatedSize.width();
  const double vertical = static_cast<double>(viewport()->height()) /
                          rotatedSize.height();
  setZoom(qMin(horizontal, vertical));
}

int CanvasWidget::rotationDegreesClockwise() const noexcept {
  return rotationQuarterTurns_ * 90;
}

void CanvasWidget::rotateClockwise() {
  setRotationQuarterTurns(rotationQuarterTurns_ + 1);
}

void CanvasWidget::rotateCounterclockwise() {
  setRotationQuarterTurns(rotationQuarterTurns_ - 1);
}

void CanvasWidget::resetRotation() {
  setRotationQuarterTurns(0);
}

bool CanvasWidget::pixelGridEnabled() const noexcept {
  return pixelGridEnabled_;
}

bool CanvasWidget::pixelGridVisible() const noexcept {
  return pixelGridEnabled_ && zoom_ >= pixelGridMinimumZoom;
}

void CanvasWidget::setPixelGridEnabled(bool enabled) {
  if (pixelGridEnabled_ == enabled) {
    return;
  }
  pixelGridEnabled_ = enabled;
  refreshAccessibleDescription();
  viewport()->update();
  emit pixelGridChanged(enabled);
}

QRect CanvasWidget::visibleDocumentRect() const {
  if (!document_) {
    return {};
  }
  const auto origin = canvasOrigin();
  const QRectF viewportRect(QPointF(), QSizeF(viewport()->size()));
  const QRectF canvasRect(origin, QSizeF(rotatedDocumentSize()) * zoom_);
  const auto visible = viewportRect.intersected(canvasRect);
  if (visible.isEmpty()) {
    return {};
  }
  const auto relative = visible.translated(-origin);
  const QRectF visibleCanvas(relative.x() / zoom_, relative.y() / zoom_,
                             relative.width() / zoom_,
                             relative.height() / zoom_);
  bool invertible = false;
  const auto canvasToDocument =
      documentToCanvasTransform().inverted(&invertible);
  if (!invertible) {
    return {};
  }
  const auto documentRect = canvasToDocument.mapRect(visibleCanvas).intersected(
      QRectF(QPointF(), QSizeF(document_->size())));
  const int left =
      qMax(0, static_cast<int>(std::floor(documentRect.left())));
  const int top = qMax(0, static_cast<int>(std::floor(documentRect.top())));
  const int right = qMin(document_->size().width(),
                         static_cast<int>(std::ceil(documentRect.right())));
  const int bottom = qMin(document_->size().height(),
                          static_cast<int>(std::ceil(documentRect.bottom())));
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
  const QRectF canvasRect(origin, QSizeF(rotatedDocumentSize()) * zoom_);
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
  painter.setTransform(documentToCanvasTransform(), true);
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

  if (pixelGridVisible()) {
    painter.setPen(
        QPen(QColor(0, 0, 0, pixelGridOpacity), 0.0, Qt::SolidLine));
    const int right = sourceRect.x() + sourceRect.width();
    const int bottom = sourceRect.y() + sourceRect.height();
    for (int x = sourceRect.x(); x <= right; ++x) {
      painter.drawLine(QPointF(x, sourceRect.y()), QPointF(x, bottom));
    }
    for (int y = sourceRect.y(); y <= bottom; ++y) {
      painter.drawLine(QPointF(sourceRect.x(), y), QPointF(right, y));
    }
  }
  painter.restore();

  if (selecting_) {
    const auto selection = QRect(selectionStart_, selectionEnd_).normalized();
    const auto canvasPreview = documentToCanvasTransform().mapRect(
        QRectF(selection.x(), selection.y(), selection.width(),
               selection.height()));
    const QRectF preview(origin.x() + canvasPreview.x() * zoom_,
                         origin.y() + canvasPreview.y() * zoom_,
                         canvasPreview.width() * zoom_,
                         canvasPreview.height() * zoom_);
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
  const auto content = QSizeF(rotatedDocumentSize()) * zoom_;
  const double x = content.width() < viewport()->width()
                       ? (viewport()->width() - content.width()) / 2.0
                       : -horizontalScrollBar()->value();
  const double y = content.height() < viewport()->height()
                       ? (viewport()->height() - content.height()) / 2.0
                       : -verticalScrollBar()->value();
  return {x, y};
}

QSize CanvasWidget::rotatedDocumentSize() const {
  if (!document_) {
    return {};
  }
  const auto size = document_->size();
  return rotationQuarterTurns_ % 2 == 0 ? size
                                        : QSize(size.height(), size.width());
}

QTransform CanvasWidget::documentToCanvasTransform() const {
  if (!document_) {
    return {};
  }
  const auto width = document_->size().width();
  const auto height = document_->size().height();
  switch (rotationQuarterTurns_) {
    case 1:
      return QTransform(0.0, 1.0, -1.0, 0.0, height, 0.0);
    case 2:
      return QTransform(-1.0, 0.0, 0.0, -1.0, width, height);
    case 3:
      return QTransform(0.0, -1.0, 1.0, 0.0, 0.0, width);
    default:
      return {};
  }
}

QPointF CanvasWidget::documentPositionF(QPointF viewportPosition) const {
  if (!document_) {
    return {-1.0, -1.0};
  }
  const auto canvasPosition =
      (viewportPosition - canvasOrigin()) / zoom_;
  bool invertible = false;
  const auto canvasToDocument =
      documentToCanvasTransform().inverted(&invertible);
  return invertible ? canvasToDocument.map(canvasPosition)
                    : QPointF(-1.0, -1.0);
}

QPoint CanvasWidget::documentPosition(QPoint viewportPosition) const {
  const auto position = documentPositionF(viewportPosition);
  return {static_cast<int>(std::floor(position.x())),
          static_cast<int>(std::floor(position.y()))};
}

void CanvasWidget::setRotationQuarterTurns(int quarterTurns) {
  quarterTurns = ((quarterTurns % 4) + 4) % 4;
  if (rotationQuarterTurns_ == quarterTurns) {
    return;
  }
  const QPointF center(viewport()->width() / 2.0, viewport()->height() / 2.0);
  const auto documentCenter = documentPositionF(center);
  rotationQuarterTurns_ = quarterTurns;
  updateScrollBars();
  centerDocumentPosition(documentCenter);
  refreshAccessibleDescription();
  viewport()->update();
  emit rotationChanged(rotationDegreesClockwise());
}

void CanvasWidget::centerDocumentPosition(QPointF documentPosition) {
  if (!document_) {
    return;
  }
  const auto canvasPosition =
      documentToCanvasTransform().map(documentPosition) * zoom_;
  horizontalScrollBar()->setValue(
      qRound(canvasPosition.x() - viewport()->width() / 2.0));
  verticalScrollBar()->setValue(
      qRound(canvasPosition.y() - viewport()->height() / 2.0));
}

void CanvasWidget::refreshAccessibleDescription() {
  setAccessibleDescription(
      QStringLiteral("Scrollable view of the current image document. View "
                     "zoom %1 percent; rotation %2 degrees clockwise. Pixel "
                     "grid %3.")
          .arg(qRound(zoom_ * 100.0))
          .arg(rotationDegreesClockwise())
          .arg(pixelGridEnabled_
                   ? (pixelGridVisible()
                          ? QStringLiteral("enabled and visible")
                          : QStringLiteral("enabled; visible from 800% zoom"))
                   : QStringLiteral("disabled")));
}

void CanvasWidget::updateScrollBars() {
  if (!document_) {
    horizontalScrollBar()->setRange(0, 0);
    verticalScrollBar()->setRange(0, 0);
    return;
  }
  const auto content = QSizeF(rotatedDocumentSize()) * zoom_;
  horizontalScrollBar()->setPageStep(viewport()->width());
  verticalScrollBar()->setPageStep(viewport()->height());
  horizontalScrollBar()->setRange(
      0, qMax(0, qCeil(content.width()) - viewport()->width()));
  verticalScrollBar()->setRange(
      0, qMax(0, qCeil(content.height()) - viewport()->height()));
}

}  // namespace chromarchy
