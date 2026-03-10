//
// Created by critical on 10.03.2026.
//

#include "launch.h"
#include <dlfcn.h>
#include <FEXCore/Config/Config.h>

namespace Vexa::Runtime {
    static void *g_fex_handle = nullptr;

    LaunchResult StartRuntime(const Vexa::Common::PreflightPaths &paths) {
        (void) paths; // TODO: remove when real launch

        if (g_fex_handle) {
            return {0, "Already started", ""};
        }

        g_fex_handle = dlopen("libFEXCore.so", RTLD_NOW);
        if (!g_fex_handle) {
            const char *err = dlerror();
            return {7, "Failed to load libFEXCore.so", err ? err : "unknown"};
        }

        // Initializing FEX runtime here.
        FEXCore::Config::Initialize();
        FEXCore::Config::Load();

        return {0, "OK", ""};
    }

    void StopRuntime() {
        if (g_fex_handle) {
            dlclose(g_fex_handle);
            g_fex_handle = nullptr;
        }
    }
}