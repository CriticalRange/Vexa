//
// Created by critical on 13.03.2026.
//

#include <Common/FEXServerClient.h>
#include <Tools/CommonTools/PortabilityInfo.h>
#include <Tools/LinuxEmulation/LinuxSyscalls/Syscalls.h>

#include "../../logging/native_log.h"

#include "client.h"
#include "logging.h"

namespace Vexa::Runtime::Init {
    Vexa::Common::Result SetupClient(JNIEnv *env,
                                     const Vexa::Common::Paths &paths) {
        const auto selfPath = FEX::GetSelfPath();
        const std::string_view interpreterPath = selfPath ? std::string_view(*selfPath)
                                                          : std::string_view(paths.executable);

        const bool setupReady = FEXServerClient::SetupClient(interpreterPath);
        if (!setupReady) {
            VEXA_LOGW(env, "FEX", "FEX Server Client setup failed (continuing)", "{}");
            return {
                    Vexa::Common::Code::Ok,
                    Vexa::Common::Phase::Init,
                    "FEX Server Client setup skipped"
            };
        }
        VEXA_LOGI(env, "FEX", "FEXServerClient setup OK", Vexa::Log::AddFields({
                                                                                       Vexa::Log::F(
                                                                                               "serverFD",
                                                                                               FEXServerClient::GetServerFD())
                                                                               }).c_str());

        return {
                Vexa::Common::Code::Ok,
                Vexa::Common::Phase::Init,
                "FEX Server Client setup OK"
        };
    }

    void TrackClientFDs(JNIEnv *env, Resources
    &state) {
        if (!state.linuxSyscallHandler) return;

        const int serverFD = FEXServerClient::GetServerFD();
        if (serverFD >= 0) {
            state.linuxSyscallHandler->FM.TrackFEXFD(serverFD);
            VEXA_LOGI(env, "FEX", "Tracked FEX server FD", Vexa::Log::AddFields({
                                                                                        Vexa::Log::F(
                                                                                                "fd",
                                                                                                serverFD)
                                                                                }).c_str());
        }

        const int logFD = Vexa::Runtime::Init::GetFexFileLogSinkFd();
        if (logFD > 2) {
            state.linuxSyscallHandler->FM.TrackFEXFD(logFD);
            VEXA_LOGI(env, "FEX", "Tracked FEX log sink FD", Vexa::Log::AddFields({
                                                                                          Vexa::Log::F(
                                                                                                  "fd",
                                                                                                  logFD)
                                                                                  }).c_str());
        }
    }
}
