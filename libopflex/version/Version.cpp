#include "opflex/version/Version.h"

namespace opflex {
namespace version {

const char *getGitHash() { return GITHASH; }
const char *getVersionSummary() { return VERSIONSUMMARY; }

}  // namespace version
}  // namespace opflex
