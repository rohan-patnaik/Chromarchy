#pragma once

#include "core/Layer.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>

#include <optional>

namespace chromarchy {

class Document final {
public:
  static constexpr int maximumDimension = 300'000;

  [[nodiscard]] static std::optional<Document> create(QSize size);

  [[nodiscard]] QSize size() const noexcept;
  [[nodiscard]] qsizetype layerCount() const noexcept;
  [[nodiscard]] int activeLayerIndex() const noexcept;
  bool setActiveLayerIndex(int index) noexcept;

  [[nodiscard]] const Layer* layerAt(int index) const noexcept;
  [[nodiscard]] Layer* layerAt(int index) noexcept;

  int addLayer(QString name);
  bool duplicateLayer(int index);
  bool removeLayer(int index);
  bool moveLayer(int from, int to);

  [[nodiscard]] QImage composite(QRect region = {}) const;

private:
  explicit Document(QSize size);
  [[nodiscard]] bool containsLayer(int index) const noexcept;

  QSize size_;
  QVector<Layer> layers_;
  int activeLayerIndex_ = -1;
};

}  // namespace chromarchy
