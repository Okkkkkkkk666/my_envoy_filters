#pragma once
#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include "string_utils.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
struct Shuffle {};

class ShuffleRewrite {
public:
  ShuffleRewrite(const Shuffle& shuffle);

public:
  std::string shuffle(const std::string& data) const;
};
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy