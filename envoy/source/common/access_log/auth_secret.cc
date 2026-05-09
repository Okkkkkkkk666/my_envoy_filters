#include "auth_secret.h"
#include "source/common/common/base64.h"

#include <fstream>

CAuthSecret::CAuthSecret() {
  env_filepath_ = "/opt/public/.env";
  loadEnvFile(env_filepath_);
}

void CAuthSecret::loadEnvFile(const std::string& env_filepath) {
  std::ifstream file(env_filepath);
  if (!file.is_open()) {
    ENVOY_LOG(error, "Failed to open env file: {}", env_filepath);
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    size_t pos = line.find('=');
    if (pos != std::string::npos) {
      std::string env_key = line.substr(0, pos);
      std::string env_value = line.substr(pos + 1);
      env_map_[env_key] = env_value;
    }
  }
}

std::string CAuthSecret::readEnvValue(const std::string& key) {
  auto it = env_map_.find(key);
  if (it != env_map_.end()) {
    return it->second;
  }
  return "";
}

std::string CAuthSecret::getAuthSecretByEnv(const std::string& component_name) {
  std::stringstream ss;
  ss << "ip=" << readEnvValue("MasterIP")
     << "&name=" << (component_name)
     << "&node_id=" <<  readEnvValue("NodeUID")
     << "&sn=" << readEnvValue("GS_EE")
     << "&suits=" << readEnvValue("Suits");


  return Envoy::Base64::encode(ss.str().c_str(), ss.str().length());
}

// int test() {
//   std::string auth_secret = g_auth_secret->getAuthSecretByEnv("ata");
//   LOG(info, "AuthSecret: {}", auth_secret);
// }