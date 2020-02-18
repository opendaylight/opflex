#if HAVE_CONFIG_H
#  include <config.h>
#endif
#include "opflex/version/Version.h"

namespace opflex {
namespace version {

const char *getGitHash() { return GITHASH; }
const char *getVersionSummary() { return PACKAGE_VERSION; }

} 
} // namespace version

