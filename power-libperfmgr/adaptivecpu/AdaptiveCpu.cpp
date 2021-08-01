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

#define LOG_TAG "powerhal-libperfmgr"

#include "AdaptiveCpu.h"

#include <android-base/logging.h>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

AdaptiveCpu::AdaptiveCpu(std::shared_ptr<HintManager> hintManager) : mHintManager(hintManager) {}

void AdaptiveCpu::StartInBackground() {
    // TODO(b/188770301) Start the main loop thead.
    LOG(INFO) << "AdaptiveCpu starting.";
}

void AdaptiveCpu::ReportWorkDurations(const std::vector<WorkDuration> &workDurations,
                                      std::chrono::nanoseconds targetDuration) {
    // TODO(b/188770301) Enqueue the work duration for processing.
    LOG(VERBOSE) << "AdaptiveCpu received " << workDurations.size()
                 << " work durations with target " << targetDuration.count() << "ns";
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
