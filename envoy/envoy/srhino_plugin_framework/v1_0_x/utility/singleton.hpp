#pragma once

#include <mutex>

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Utility {
template <class T> class Singleton {
public:
  static T& instance() {
    std::call_once(once_flag_, [&]() {
      static T obj;
      instance_ = &obj;
    });

    return *instance_;
  }

private:
  static std::once_flag once_flag_;
  static T* instance_;
};

template <class T> std::once_flag Singleton<T>::once_flag_;
template <class T> T* Singleton<T>::instance_ = nullptr;
} // namespace Utility
} // namespace v1_0_x
} // namespace SrhinoPluginFramework