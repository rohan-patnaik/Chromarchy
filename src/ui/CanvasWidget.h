#pragma once

#include <QAbstractScrollArea>
#include <QPoint>
#include <QTransform>

namespace chromarchy {

class Document;

class CanvasWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  static constexpr double minimumZoom = 0.01;
  static constexpr double maximumZoom = 32.0;
  static constexpr double pixelGridMinimumZoom = 8.0;
  static constexpr int pixelGridOpacity = 112;

  explicit CanvasWidget(const Document* document, QWidget* parent = nullptr);

  [[nodiscard]] double zoom() const noexcept;
  void setZoom(double zoom);
  void fitToViewport();
  [[nodiscard]] int rotationDegreesClockwise() const noexcept;
  void rotateClockwise();
  void rotateCounterclockwise();
  void resetRotation();
  [[nodiscard]] bool pixelGridEnabled() const noexcept;
  [[nodiscard]] bool pixelGridVisible() const noexcept;
  void setPixelGridEnabled(bool enabled);
  [[nodiscard]] QRect visibleDocumentRect() const;
  void documentChanged();

signals:
  void zoomChanged(double zoom);
  void rotationChanged(int degreesClockwise);
  void pixelGridChanged(bool enabled);
  void selectionRequested(QRect rectangle);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  [[nodiscard]] QPointF canvasOrigin() const;
  [[nodiscard]] QSize rotatedDocumentSize() const;
  [[nodiscard]] QTransform documentToCanvasTransform() const;
  [[nodiscard]] QPointF documentPositionF(QPointF viewportPosition) const;
  [[nodiscard]] QPoint documentPosition(QPoint viewportPosition) const;
  void setRotationQuarterTurns(int quarterTurns);
  void centerDocumentPosition(QPointF documentPosition);
  void refreshAccessibleDescription();
  void updateScrollBars();

  const Document* document_ = nullptr;  // Non-owning; parent DocumentView owns it.
  double zoom_ = 1.0;
  int rotationQuarterTurns_ = 0;
  bool pixelGridEnabled_ = false;
  bool panning_ = false;
  QPoint panStart_;
  QPoint scrollStart_;
  bool selecting_ = false;
  QPoint selectionStart_;
  QPoint selectionEnd_;
};

}  // namespace chromarchy
