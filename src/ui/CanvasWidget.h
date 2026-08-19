#pragma once

#include <QAbstractScrollArea>
#include <QPoint>

namespace chromarchy {

class Document;

class CanvasWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit CanvasWidget(const Document* document, QWidget* parent = nullptr);

  [[nodiscard]] double zoom() const noexcept;
  void setZoom(double zoom);
  [[nodiscard]] QRect visibleDocumentRect() const;
  void documentChanged();

signals:
  void zoomChanged(double zoom);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  [[nodiscard]] QPointF canvasOrigin() const;
  void updateScrollBars();

  const Document* document_ = nullptr;  // Non-owning; parent DocumentView owns it.
  double zoom_ = 1.0;
  bool panning_ = false;
  QPoint panStart_;
  QPoint scrollStart_;
};

}  // namespace chromarchy
