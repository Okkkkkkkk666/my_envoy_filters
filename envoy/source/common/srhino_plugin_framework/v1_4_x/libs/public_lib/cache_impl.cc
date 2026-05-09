#include "source/common/srhino_plugin_framework/v1_4_x/libs/public_lib/cache_impl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Libs {
namespace PublicLib {

std::string PublicLibCacheImpl::getRule(const std::string& policy_id, const std::string& rule_id) {
  auto& cache = Cache::instance();
  cache.readLock(policy_id);

  try {
    std::string result = "";

    auto policy_iter = cache.find(policy_id);
    if (policy_iter == cache.end(policy_id)) {
      cache.readUnlock(policy_id);
      return result;
    }

    auto& rule_table = (*policy_iter)->value;
    auto rule_iter = rule_table->find(rule_id);
    if (rule_iter != rule_table->end()) {
      result = rule_iter->second;
    }

    cache.readUnlock(policy_id);
    return result;
  } catch (...) {
    cache.readUnlock(policy_id);
    throw;
  }
}

void PublicLibCacheImpl::insertRule(const std::string& policy_id, const std::string& rule_id,
                                    const std::string& rule_data) {
  auto& cache = Cache::instance();
  cache.writeLock(policy_id);

  try {
    auto policy_iter = cache.find(policy_id);
    if (policy_iter == cache.end(policy_id)) {

      RuleTableSharedPtr rule_table = std::make_shared<RuleTable>();
      rule_table->emplace(rule_id, rule_data);
      cache.insert(policy_id, rule_table);
    } else {

      auto& rule_table = (*policy_iter)->value;
      rule_table->emplace(rule_id, rule_data);
    }

    cache.writeUnlock(policy_id);
  } catch (...) {
    cache.writeUnlock(policy_id);
    throw;
  }
}

void PublicLibCacheImpl::updateRule(const std::string& policy_id, const std::string& rule_id,
                                    const std::string& rule_data) {
  auto& cache = Cache::instance();
  cache.writeLock(policy_id);

  try {
    auto policy_iter = cache.find(policy_id);
    if (policy_iter == cache.end(policy_id)) {

      // policy_id不存在则新增
      RuleTableSharedPtr rule_table = std::make_shared<RuleTable>();
      rule_table->emplace(rule_id, rule_data);
      cache.insert(policy_id, rule_table);
    } else {

      auto& rule_table = (*policy_iter)->value;
      (*rule_table)[rule_id] = rule_data;
    }

    cache.writeUnlock(policy_id);
  } catch (...) {
    cache.writeUnlock(policy_id);
    throw;
  }
}

void PublicLibCacheImpl::delRule(const std::string& policy_id, const std::string& rule_id) {
  auto& cache = Cache::instance();
  cache.writeLock(policy_id);

  try {
    auto policy_iter = cache.find(policy_id);
    if (policy_iter == cache.end(policy_id)) {
      cache.writeUnlock(policy_id);
      return;
    }

    auto& rule_table = (*policy_iter)->value;
    rule_table->erase(rule_id);

    cache.writeUnlock(policy_id);
    return;
  } catch (...) {
    cache.writeUnlock(policy_id);
    throw;
  }
}

void PublicLibCacheImpl::delPolicy(const std::string& policy_id) {
  auto& cache = Cache::instance();
  cache.writeLock(policy_id);

  try {

    auto policy_iter = cache.find(policy_id);
    if (policy_iter != cache.end(policy_id)) {
      cache.erase(policy_iter);
    }

    cache.writeUnlock(policy_id);
  } catch (...) {
    cache.writeUnlock(policy_id);
    throw;
  }
}

} // namespace PublicLib
} // namespace Libs
} // namespace v1_4_x
} // namespace SrhinoPluginFramework