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

#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_DEBUGGING_H

#include <android/hardware/power/1.1/IPower.h>

namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

using PlatSleepStateType = android::hardware::power::V1_0::PowerStatePlatformSleepState;
using SubsystemType = android::hardware::power::V1_1::PowerStateSubsystem;

// Dump a vector of PowerHAL 1.0 PowerStatePlatformSleepState instances (e.g.
// from the PowerHAL getPlatformLowPowerStats() interface) to a file descriptor
bool DumpPowerHal1_0PlatStatsToFd(android::hardware::power::V1_0::Status halResult,
    const android::hardware::hidl_vec<PlatSleepStateType>& platStates,
    int fd);

// Dump a vector of PowerHAL 1.1 PowerStateSubsystem instances (e.g. from the
// PowerHAL getSubsystemLowPowerStats() interface) to a file descriptor
bool DumpPowerHal1_1SubsysStatsToFd(android::hardware::power::V1_0::Status halResult,
    const android::hardware::hidl_vec<SubsystemType>& subsystems,
    int fd);

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware

#endif // HARDWARE_GOOGLE_PIXEL_POWERSTATS_DEBUGGING_H

