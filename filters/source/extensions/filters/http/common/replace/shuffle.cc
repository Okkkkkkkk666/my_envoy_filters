#include "shuffle.h"
namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace Replaces {
ShuffleRewrite::ShuffleRewrite(const Shuffle& shuffle) { (void)shuffle; }

std::string ShuffleRewrite::shuffle(const std::string& data) const{

  std::random_device rd;
  std::mt19937 g(rd());
  std::vector<wchar_t> new_data;
  size_t len = getWideCharCount(data);
  std::wstring wide_data = stringToWString(data);
  for (size_t i = 0; i < len; i++) {
    new_data.emplace_back(wide_data[i]);
  }
  std::shuffle(new_data.begin(), new_data.end(), g);
  std::wstring shuffled_str(new_data.begin(), new_data.end());
  std::string shuffled = wstringToString(shuffled_str);

  return shuffled;
}
} // namespace Replaces
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy