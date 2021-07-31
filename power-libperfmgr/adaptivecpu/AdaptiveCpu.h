/*
 * Copyright (C) 2021 The Android Open Source Project
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

#pragma once

#include <aidl/android/hardware/power/WorkDuration.h>
#include <perfmgr/HintManager.h>
#include <chrono>
#include <vector>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::aidl::android::hardware::power::WorkDuration;
using ::android::perfmgr::HintManager;

// Applies CPU frequency hints infered by an ML model based on the recent CPU statistics and work
// durations.
// This class's public members are not synchronised and should not be used from multiple threads,
// with the exception of ReportWorkDuration, which can be called from an arbitrary thread.
class AdaptiveCpu {
  public:
    AdaptiveCpu(std::shared_ptr<HintManager> hintManager);

    // Starts Adaptive CPU in a background thread.
    void StartInBackground();

    // Reports work durations for processing. This method returns immediately as work durations are
    // processed asynchonuously.
    void ReportWorkDurations(const std::vector<WorkDuration> &workDurations,
                             std::chrono::nanoseconds targetDuration);

  private:
    std::shared_ptr<HintManager> mHintManager;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
