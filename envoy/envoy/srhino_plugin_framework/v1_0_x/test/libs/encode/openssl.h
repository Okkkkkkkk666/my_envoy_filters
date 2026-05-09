#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_0_x/libs/encode/openssl.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Test {
namespace Libs {
namespace Encode {
class MockOpenSsl : public SrhinoPluginFramework::v1_0_x::Libs::Encode::OpenSsl {
public:
  MockOpenSsl() : mock_aes_(std::make_shared<MockAes>()), mock_md5_(std::make_shared<MockMd5>()) {
    ON_CALL(*this, aes()).WillByDefault(testing::Return(mock_aes_));
    ON_CALL(*this, md5()).WillByDefault(testing::Return(mock_md5_));
  }

public:
  class MockAes : public Aes {
  public:
    MOCK_METHOD(std::string, encode, (const void*, size_t, const std::string&, Mode), (const));
    MOCK_METHOD(std::string, decode, (const void*, size_t, const std::string&, Mode), (const));
    MOCK_METHOD(void, decode,
                (const std::vector<unsigned char>&, std::vector<unsigned char>&,
                 const std::vector<uint8_t>&, const std::vector<uint8_t>&, Mode),
                (const));
  };

  class MockMd5 : public Md5 {};

public:
  MOCK_METHOD(AesSharedPtr, aes, (), (const));
  MOCK_METHOD(Md5SharedPtr, md5, (), (const));

public:
  std::shared_ptr<MockAes> mock_aes_;
  std::shared_ptr<MockMd5> mock_md5_;
};

} // namespace Encode
} // namespace Libs
} // namespace Test
} // namespace v1_0_x
} // namespace SrhinoPluginFramework