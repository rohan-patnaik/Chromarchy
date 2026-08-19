#pragma once

#include "core/TiledImage.h"

#include <QString>
#include <QUuid>

namespace chromarchy {

class NativeDocumentCodec;

class Layer final {
public:
  Layer(QString name, QSize size);

  [[nodiscard]] const QUuid& id() const noexcept;
  [[nodiscard]] const QString& name() const noexcept;
  void setName(QString name);

  [[nodiscard]] bool isVisible() const noexcept;
  void setVisible(bool visible) noexcept;
  [[nodiscard]] double opacity() const noexcept;
  void setOpacity(double opacity) noexcept;
  [[nodiscard]] bool isLocked() const noexcept;
  void setLocked(bool locked) noexcept;

  [[nodiscard]] const TiledImage& pixels() const noexcept;
  [[nodiscard]] TiledImage& pixels() noexcept;
  [[nodiscard]] Layer duplicate(QString name) const;

private:
  friend class NativeDocumentCodec;

  QUuid id_ = QUuid::createUuid();
  QString name_;
  TiledImage pixels_;
  bool visible_ = true;
  bool locked_ = false;
  double opacity_ = 1.0;
};

}  // namespace chromarchy
