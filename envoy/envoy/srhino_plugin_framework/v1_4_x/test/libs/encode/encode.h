#pragma once

#include <gmock/gmock.h>

#include "envoy/srhino_plugin_framework/v1_4_x/libs/encode/encode.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/encode/compressor.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/encode/cryptopp.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/encode/decompressor.h"
#include "envoy/srhino_plugin_framework/v1_4_x/test/libs/encode/openssl.h"

namespace SrhinoPluginFramework {
namespace v1_4_x {
namespace Test {
namespace Libs {
namespace Encode {
using namespace SrhinoPluginFramework::v1_4_x::Libs::Encode;
using testing::_;
using testing::Return;

class MockEncode : public SrhinoPluginFramework::v1_4_x::Libs::Encode::Encode {
public:
  MockEncode() {
    EXPECT_CALL(*this, createDecompressor(_))
        .WillRepeatedly(testing::Return(std::make_shared<MockDecompressor>()));
    EXPECT_CALL(*this, createCompressor(_))
        .WillRepeatedly(testing::Return(std::make_shared<MockCompressor>()));
    EXPECT_CALL(*this, createOpenSsl)
        .WillRepeatedly(testing::Return(std::make_shared<MockOpenSsl>()));
    EXPECT_CALL(*this, createCryptopp)
        .WillRepeatedly(testing::Return(std::make_shared<MockCryptopp>()));
  }

public:
  MOCK_METHOD(DecompressorSharedPtr, createDecompressor, (Decompressor::Type type), ());
  MOCK_METHOD(CompressorSharedPtr, createCompressor, (Compressor::Type type), ());
  MOCK_METHOD(OpenSslSharedPtr, createOpenSsl, (), ());
  MOCK_METHOD(CryptoppSharedPtr, createCryptopp, (), ());
};

} // namespace Encode
} // namespace Libs
} // namespace Test
} // namespace v1_4_x
} // namespace SrhinoPluginFramework