/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pixelpowerstats/Debugging.h>

#include <android-base/file.h>
#include <android-base/stringprintf.h>

#include <inttypes.h>

namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

using android::hardware::power::V1_0::PowerStatePlatformSleepState;
using android::hardware::power::V1_0::PowerStateVoter;
using android::hardware::power::V1_1::PowerStateSubsystem;
using android::hardware::power::V1_1::PowerStateSubsystemSleepState;
using android::hardware::power::V1_0::Status;
using android::hardware::hidl_vec;

namespace {  // Local utility functions

// Utility function for displaying data from getPlatformLowPowerStats()
bool WritePlatformStatsToFd(const hidl_vec<PowerStatePlatformSleepState>& platStates, int fd) {
    static const char *headerFormat = "  %14s   %16s   %16s   %15s\n";
    static const char *dataFormat = "  %14s   %16s   %13" PRIu64 " ms   %15" PRIu64 "\n";

    if (platStates.size() == 0) {
        return android::base::WriteStringToFd("  No data available!\n", fd);
    }

    std::ostringstream result;
    result << android::base::StringPrintf(headerFormat,
                                          "Platform State",
                                          "State Voter",
                                          "Total time",
                                          "Total entries/votes");

    for (const PowerStatePlatformSleepState& platState : platStates) {
        result << android::base::StringPrintf(dataFormat,
                                              platState.name.c_str(),
                                              "",
                                              platState.residencyInMsecSinceBoot,
                                              platState.totalTransitions);

        if (platState.voters.size() == 0) {
            result << android::base::StringPrintf(headerFormat,
                                                  "",
                                                  "No voter data",
                                                  "",
                                                  "");
            continue;
        }

        for (const PowerStateVoter voter : platState.voters) {
            result << android::base::StringPrintf(dataFormat,
                                                  "",
                                                  voter.name.c_str(),
                                                  voter.totalTimeInMsecVotedForSinceBoot,
                                                  voter.totalNumberOfTimesVotedSinceBoot);
        }
    }

    return android::base::WriteStringToFd(result.str(), fd);
}

// Utility function for displaying data from getSubsystemLowPowerStats()
bool WriteSubsystemStatsToFd(const hidl_vec<PowerStateSubsystem>& subsystems, int fd) {
    static const char *headerFormat = "  %14s   %14s   %16s   %15s   %16s\n";
    static const char *dataFormat = "  %14s   %14s   %13" PRIu64 " ms   %15" PRIu64 \
                                    "   %13" PRIu64 " ms\n";

    if (subsystems.size() == 0) {
        return android::base::WriteStringToFd("  No data available!\n", fd);
    }

    std::ostringstream result;
    result << android::base::StringPrintf(headerFormat,
                                          "Subsystem",
                                          "State",
                                          "Total time",
                                          "Total entries",
                                          "Last entry timestamp");

    for (const PowerStateSubsystem& subsystem : subsystems) {
        if (subsystem.states.size() == 0) {
            result << android::base::StringPrintf(headerFormat,
                                                  subsystem.name.c_str(),
                                                  "No state data available",
                                                  "",
                                                  "",
                                                  "");
            continue;
        }

        for (const PowerStateSubsystemSleepState state : subsystem.states) {
            result << android::base::StringPrintf(dataFormat,
                                                  subsystem.name.c_str(),
                                                  state.name.c_str(),
                                                  state.residencyInMsecSinceBoot,
                                                  state.totalTransitions,
                                                  state.lastEntryTimestampMs);
        }
    }

    return android::base::WriteStringToFd(result.str(), fd);
}

} // namespace (anonymous: local utility functions)


// Externally-visible function for displaying the results of getPlatformLowPowerStats()
bool DumpPowerHal1_0PlatStatsToFd(Status halResult,
                                  const hidl_vec<PowerStatePlatformSleepState>& platStates,
                                  int fd) {
    static const char *header = "\n========== PowerHAL 1.0 platform low power stats ==========\n";
    static const char *footer = "========== End of platform low power stats ==========\n";
    bool result;

    result = android::base::WriteStringToFd(header, fd);
    if (!result) {
        return false;
    }

    if (halResult == Status::SUCCESS) {
        result = WritePlatformStatsToFd(platStates, fd);
    } else {
        std::string msg = android::base::StringPrintf("Error getting platform stats: %s\n",
                                                      toString(halResult).c_str());
        result = android::base::WriteStringToFd(msg, fd);
    }

    android::base::WriteStringToFd(footer, fd);

    return result;
}

// Externally-visible function for displaying the results of getSubsystemLowPowerStats()
bool DumpPowerHal1_1SubsysStatsToFd(Status halResult,
                                    const hidl_vec<PowerStateSubsystem>& subsystems,
                                    int fd) {
    static const char *header = "\n========== PowerHAL 1.1 subsystem low power stats ==========\n";
    static const char *footer = "========== End of subsystem low power stats ==========\n";
    bool result;

    result = android::base::WriteStringToFd(header, fd);
    if (!result) {
        return result;
    }

    if (halResult == Status::SUCCESS) {
        result = WriteSubsystemStatsToFd(subsystems, fd);
    } else {
        std::string msg = android::base::StringPrintf("Error getting subsystem stats: %s\n",
                                                      toString(halResult).c_str());
        result = android::base::WriteStringToFd(msg, fd);
    }

    android::base::WriteStringToFd(footer, fd);

    return result;
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware

