#include "source/common/srhino_plugin_framework/v1_3_x/libs/encode/cryptopp_impl.h"

extern const char build_scm_revision[];
extern const char build_scm_status[];
const char build_scm_revision[] = "";
const char build_scm_status[] = "";

namespace SrhinoPluginFramework {
namespace v1_3_x {
namespace Libs {

extern "C" {

#define EXPORT __attribute__((visibility("default")))
EXPORT void* createCryptopp() { return new Encode::CryptoppImpl(); }
}

} // namespace Libs
} // namespace v1_3_x
} // namespace SrhinoPluginFramework