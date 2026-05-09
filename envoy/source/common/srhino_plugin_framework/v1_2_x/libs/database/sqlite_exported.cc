#include "source/common/srhino_plugin_framework/v1_2_x/libs/database/sqlite_db_factory_impl.h"

extern const char build_scm_revision[];
extern const char build_scm_status[];
const char build_scm_revision[] = "";
const char build_scm_status[] = "";

namespace SrhinoPluginFramework {
namespace v1_2_x {
namespace Libs {

extern "C" {

#define EXPORT __attribute__((visibility("default")))
EXPORT void* createSqliteDbFactory() { return new Database::SqliteDbFactoryImpl(); }
}

} // namespace Libs
} // namespace v1_2_x
} // namespace SrhinoPluginFramework