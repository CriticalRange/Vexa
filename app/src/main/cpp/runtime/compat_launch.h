#pragma once

#include "../common/paths.h"
#include "../common/status.h"
#include <jni.h>
#include <string>
#include <vector>

namespace Vexa::Runtime {
// Shared by the Android launch entry and FEX's native re-exec trampoline.
Vexa::Common::Result LaunchGuest(JNIEnv *env, Vexa::Common::Paths paths,
                               std::vector<std::string> environment,
                               std::vector<std::string> arguments);
void StopCompatibilitySession();
void SetStandaloneLoader();
}
