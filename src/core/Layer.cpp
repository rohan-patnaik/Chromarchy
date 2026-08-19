#include "core/Layer.h"

#include <algorithm>
#include <utility>

namespace chromarchy {

Layer::Layer(QString name, QSize size)
    : name_(std::move(name)), pixels_(size) {}

const QUuid& Layer::id() const noexcept {
  return id_;
}

const QString& Layer::name() const noexcept {
  return name_;
}

void Layer::setName(QString name) {
  name_ = std::move(name);
}

bool Layer::isVisible() const noexcept {
  return visible_;
}

void Layer::setVisible(bool visible) noexcept {
  visible_ = visible;
}

double Layer::opacity() const noexcept {
  return opacity_;
}

void Layer::setOpacity(double opacity) noexcept {
  opacity_ = std::clamp(opacity, 0.0, 1.0);
}

bool Layer::isLocked() const noexcept {
  return locked_;
}

void Layer::setLocked(bool locked) noexcept {
  locked_ = locked;
}

const TiledImage& Layer::pixels() const noexcept {
  return pixels_;
}

TiledImage& Layer::pixels() noexcept {
  return pixels_;
}

Layer Layer::duplicate(QString name) const {
  Layer copy(std::move(name), pixels_.size());
  copy.pixels_ = pixels_;
  copy.visible_ = visible_;
  copy.locked_ = locked_;
  copy.opacity_ = opacity_;
  return copy;
}

}  // namespace chromarchy
