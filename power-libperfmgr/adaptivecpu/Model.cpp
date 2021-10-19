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

#include "Model.h"

#include <android-base/logging.h>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

bool ModelInput::Init(const std::vector<CpuPolicyAverageFrequency> &cpuPolicyAverageFrequencies,
                      const std::vector<CpuLoad> &cpuLoads,
                      std::chrono::nanoseconds averageFrameTime, uint16_t numRenderedFrames,
                      ThrottleDecision previousThrottleDecision) {
    if (cpuPolicyAverageFrequencies.size() != cpuPolicyAverageFrequencyHz.size()) {
        LOG(ERROR) << "Received incorrect amount of CPU policy frequencies, expected "
                   << cpuPolicyAverageFrequencyHz.size() << ", received "
                   << cpuPolicyAverageFrequencies.size();
        return false;
    }
    int32_t previousPolicyId = -1;
    for (uint32_t i = 0; i < cpuPolicyAverageFrequencies.size(); i++) {
        if (previousPolicyId >= static_cast<int32_t>(cpuPolicyAverageFrequencies[i].policyId)) {
            LOG(ERROR) << "CPU frequencies weren't sorted by policy ID, found " << previousPolicyId
                       << " " << cpuPolicyAverageFrequencies[i].policyId;
            return false;
        }
        previousPolicyId = cpuPolicyAverageFrequencies[i].policyId;
        cpuPolicyAverageFrequencyHz[i] = cpuPolicyAverageFrequencies[i].averageFrequencyHz;
    }

    if (cpuLoads.size() != cpuCoreIdleTimesPercentage.size()) {
        LOG(ERROR) << "Received incorrect amount of CPU loads, expected "
                   << cpuCoreIdleTimesPercentage.size() << ", received " << cpuLoads.size();
        return false;
    }
    for (const auto &cpuLoad : cpuLoads) {
        if (cpuLoad.cpuId >= cpuCoreIdleTimesPercentage.size()) {
            LOG(ERROR) << "Unrecognized CPU ID found when building ModelInput: " << cpuLoad.cpuId;
            return false;
        }
        cpuCoreIdleTimesPercentage[cpuLoad.cpuId] = cpuLoad.idleTimeFraction;
    }

    this->averageFrameTime = averageFrameTime;
    this->numRenderedFrames = numRenderedFrames;
    this->previousThrottleDecision = previousThrottleDecision;
    return true;
}

ThrottleDecision RunModel(const std::deque<ModelInput> &modelInputs) {
#include "models/model.inc"
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
