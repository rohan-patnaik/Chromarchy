#pragma once

#include <QtTypes>

namespace chromarchy {

struct StorageBlock final {
  quint64 key = 0;
  quint64 bytes = 0;

  friend bool operator==(const StorageBlock&, const StorageBlock&) = default;
};

}  // namespace chromarchy
