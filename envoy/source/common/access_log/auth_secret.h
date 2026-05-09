#pragma once

#include "source/common/common/logger.h"
#include <string>
#include <map>

#define g_auth_secret (CAuthSecret::getInstance())

class CAuthSecret : Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
private:
  CAuthSecret();
  void loadEnvFile(const std::string& env_filepath);

public:
  static CAuthSecret* getInstance() {
      static CAuthSecret instance;
      return &instance;
  }

  std::string readEnvValue(const std::string& key);
  // 返回给定组件名称的 nsqd 认证密钥
  std::string getAuthSecretByEnv(const std::string& component_name);

private:
  std::map<std::string, std::string> env_map_;
  std::string env_filepath_;
};
