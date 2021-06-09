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
#include <thread>
#include <vector>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::aidl::android::hardware::power::WorkDuration;
using ::android::perfmgr::HintManager;

struct WorkDurationBatch {
    WorkDurationBatch(const std::vector<WorkDuration> &workDurations,
                      std::chrono::nanoseconds targetDuration)
        : workDurations(workDurations), targetDuration(targetDuration) {}
    std::vector<WorkDuration> workDurations;
    std::chrono::nanoseconds targetDuration;
};

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
    // The main loop of Adaptive CPU
    void RunMainLoop();

    // Atomically clears the available work durations from mWorkDurations and returns them.
    std::vector<WorkDurationBatch> TakeWorkDurations();

    std::shared_ptr<HintManager> mHintManager;

    // The thread in which work durations are processed.
    std::thread mLoopThread;

    // For guarding access to mWorkDurations.
    std::mutex mWorkDurationsMutex;

    // A condition variable that will be notified when new work durations arrive.
    std::condition_variable mWorkDurationsAvailableCondition;

    // The work durations waiting to be processed, ordered from least recent to most recent.
    std::vector<WorkDurationBatch> mWorkDurationBatches;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
