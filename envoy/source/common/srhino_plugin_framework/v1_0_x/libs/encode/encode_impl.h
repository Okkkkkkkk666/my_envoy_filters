#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/srhino_plugin_framework/v1_0_x/libs/encode/encode.h"

namespace SrhinoPluginFramework {
namespace v1_0_x {
namespace Libs {
namespace Encode {
class EncodeImpl : public Encode {
public:
  DecompressorSharedPtr createDecompressor(Decompressor::Type type) override;
  CompressorSharedPtr createCompressor(Compressor::Type type) override;
  OpenSslSharedPtr createOpenSsl() override;
  CryptoppSharedPtr createCryptopp() override;
};

} // namespace Encode
} // namespace Libs
} // namespace v1_0_x
} // namespace SrhinoPluginFramework