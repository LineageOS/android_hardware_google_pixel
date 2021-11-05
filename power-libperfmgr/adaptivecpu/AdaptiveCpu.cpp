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
#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)

#include "AdaptiveCpu.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <sys/resource.h>
#include <utils/Trace.h>

#include <chrono>
#include <deque>
#include <numeric>

#include "CpuLoadReaderSysDevices.h"
#include "Model.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

// We pass the previous N ModelInputs to the model, including the most recent ModelInput.
constexpr uint32_t kNumHistoricalModelInputs = 3;

// The sleep duration for each iteration.
// N.B.: The model will typically be trained with this value set to 25ms. We set it to 1s as a
// safety measure, but best performance will be seen at 25ms.
constexpr std::string_view kIterationSleepDurationProperty(
        "debug.adaptivecpu.iteration_sleep_duration_ms");
static const std::chrono::milliseconds kIterationSleepDurationDefault = 1000ms;
static const std::chrono::milliseconds kIterationSleepDurationMin = 20ms;

// TODO(b/207662659): Add config for changing between different reader types.
AdaptiveCpu::AdaptiveCpu(std::shared_ptr<HintManager> hintManager)
    : mCpuLoadReader(std::make_unique<CpuLoadReaderSysDevices>()), mHintManager(hintManager) {}

bool AdaptiveCpu::IsEnabled() const {
    return mIsEnabled;
}

void AdaptiveCpu::HintReceived(bool enable) {
    ATRACE_CALL();
    LOG(INFO) << "AdaptiveCpu received hint: enable=" << enable;
    if (enable) {
        StartThread();
    } else {
        SuspendThread();
    }
}

void AdaptiveCpu::StartThread() {
    ATRACE_CALL();
    std::lock_guard lock(mThreadCreationMutex);
    LOG(INFO) << "Starting AdaptiveCpu thread";
    mIsEnabled = true;
    mShouldReloadConfig = true;
    if (!mLoopThread.joinable()) {
        mLoopThread = std::thread([&]() {
            pthread_setname_np(pthread_self(), "AdaptiveCpu");
            // Parent threads may have higher priorities, so we reset to the default.
            int ret = setpriority(PRIO_PROCESS, 0, 0);
            if (ret != 0) {
                PLOG(ERROR) << "setpriority on AdaptiveCpu thread failed: " << ret;
            }
            LOG(INFO) << "Started AdaptiveCpu thread successfully";
            RunMainLoop();
            LOG(ERROR) << "AdaptiveCpu thread ended, this should never happen!";
        });
    }
}

void AdaptiveCpu::SuspendThread() {
    ATRACE_CALL();
    LOG(INFO) << "Stopping AdaptiveCpu thread";
    // This stops the thread from receiving work durations in ReportWorkDurations, which means the
    // thread blocks indefinitely.
    mIsEnabled = false;
}

void AdaptiveCpu::ReportWorkDurations(const std::vector<WorkDuration> &workDurations,
                                      std::chrono::nanoseconds targetDuration) {
    ATRACE_CALL();
    if (!mIsEnabled) {
        return;
    }
    if (!mWorkDurationProcessor.ReportWorkDurations(workDurations, targetDuration)) {
        mIsEnabled = false;
        return;
    }
    mWorkDurationsAvailableCondition.notify_one();
}

void AdaptiveCpu::WaitForEnabledAndWorkDurations() {
    ATRACE_CALL();
    std::unique_lock<std::mutex> lock(mWaitMutex);
    // TODO(b/188770301) Once the gating logic is implemented, don't block indefinitely.
    mWorkDurationsAvailableCondition.wait(
            lock, [&] { return mIsEnabled && mWorkDurationProcessor.HasWorkDurations(); });
}

void AdaptiveCpu::RunMainLoop() {
    ATRACE_CALL();

    std::chrono::milliseconds iterationSleepDuration = kIterationSleepDurationDefault;

    std::deque<ModelInput> historicalModelInputs;
    ThrottleDecision previousThrottleDecision = ThrottleDecision::NO_THROTTLE;
    while (true) {
        ATRACE_NAME("loop");
        WaitForEnabledAndWorkDurations();

        if (mShouldReloadConfig) {
            iterationSleepDuration =
                    std::chrono::milliseconds(::android::base::GetUintProperty<uint32_t>(
                            kIterationSleepDurationProperty.data(),
                            kIterationSleepDurationDefault.count()));
            iterationSleepDuration = std::max(iterationSleepDuration, kIterationSleepDurationMin);
            LOG(VERBOSE) << "Read property iterationSleepDuration="
                         << iterationSleepDuration.count() << "ms";
            mShouldReloadConfig = false;
        }

        ATRACE_BEGIN("compute");
        mAdaptiveCpuStats.RegisterStartRun();

        if (!mIsInitialized) {
            if (!mCpuFrequencyReader.init()) {
                mIsEnabled = false;
                continue;
            }
            if (!mCpuLoadReader->Init()) {
                mIsEnabled = false;
                continue;
            }
            mIsInitialized = true;
        }

        ModelInput modelInput;
        modelInput.previousThrottleDecision = previousThrottleDecision;

        modelInput.workDurationFeatures = mWorkDurationProcessor.GetFeatures();
        LOG(VERBOSE) << "Got work durations: count=" << modelInput.workDurationFeatures.numDurations
                     << ", average=" << modelInput.workDurationFeatures.averageDuration.count()
                     << "ns";
        if (modelInput.workDurationFeatures.numDurations == 0) {
            continue;
        }

        std::vector<CpuPolicyAverageFrequency> cpuPolicyFrequencies;
        if (!mCpuFrequencyReader.getRecentCpuPolicyFrequencies(&cpuPolicyFrequencies)) {
            mIsEnabled = false;
            continue;
        }
        LOG(VERBOSE) << "Got CPU frequencies: " << cpuPolicyFrequencies.size();
        for (const auto &cpuPolicyFrequency : cpuPolicyFrequencies) {
            LOG(VERBOSE) << "policy=" << cpuPolicyFrequency.policyId
                         << ", freq=" << cpuPolicyFrequency.averageFrequencyHz;
        }
        // TODO(mishaw): Move SetCpuFrequencies logic to CpuFrequencyReader.
        if (!modelInput.SetCpuFreqiencies(cpuPolicyFrequencies)) {
            mIsEnabled = false;
            continue;
        }

        if (!mCpuLoadReader->GetRecentCpuLoads(&modelInput.cpuCoreIdleTimesPercentage)) {
            mIsEnabled = false;
            continue;
        }

        modelInput.LogToAtrace();
        historicalModelInputs.push_back(modelInput);
        if (historicalModelInputs.size() > kNumHistoricalModelInputs) {
            historicalModelInputs.pop_front();
        }

        const ThrottleDecision throttleDecision = mModel.Run(historicalModelInputs);
        LOG(VERBOSE) << "Model decision: " << static_cast<uint32_t>(throttleDecision);
        ATRACE_INT("AdaptiveCpu_throttleDecision", static_cast<uint32_t>(throttleDecision));

        if (throttleDecision != previousThrottleDecision) {
            ATRACE_NAME("sendHints");
            for (const auto &hintName : kThrottleDecisionToHintNames.at(throttleDecision)) {
                mHintManager->DoHint(hintName, HINT_TIMEOUT);
            }
            for (const auto &hintName : kThrottleDecisionToHintNames.at(previousThrottleDecision)) {
                mHintManager->EndHint(hintName);
            }
            previousThrottleDecision = throttleDecision;
        }

        mAdaptiveCpuStats.RegisterSuccessfulRun(previousThrottleDecision, throttleDecision,
                                                modelInput.workDurationFeatures);
        ATRACE_END();  // compute
        {
            ATRACE_NAME("sleep");
            std::this_thread::sleep_for(iterationSleepDuration);
        }
    }
}

void AdaptiveCpu::DumpToFd(int fd) const {
    std::stringstream result;
    result << "========== Begin Adaptive CPU stats ==========\n";
    result << "Enabled: " << mIsEnabled << "\n";
    result << "CPU frequencies per policy:\n";
    const auto previousCpuPolicyFrequencies = mCpuFrequencyReader.getPreviousCpuPolicyFrequencies();
    for (const auto &[policyId, cpuFrequencies] : previousCpuPolicyFrequencies) {
        result << "- Policy=" << policyId << "\n";
        for (const auto &[frequencyHz, time] : cpuFrequencies) {
            result << "  - frequency=" << frequencyHz << "Hz, time=" << time.count() << "ms\n";
        }
    }
    mCpuLoadReader->DumpToStream(result);
    mAdaptiveCpuStats.DumpToStream(result);
    result << "==========  End Adaptive CPU stats  ==========\n";
    if (!::android::base::WriteStringToFd(result.str(), fd)) {
        PLOG(ERROR) << "Failed to dump state to fd";
    }
}

const std::unordered_map<ThrottleDecision, std::vector<std::string>>
        AdaptiveCpu::kThrottleDecisionToHintNames = {
                {ThrottleDecision::NO_THROTTLE, {}},
                {ThrottleDecision::THROTTLE_60,
                 {"LOW_POWER_LITTLE_CLUSTER_60", "LOW_POWER_MID_CLUSTER_60", "LOW_POWER_CPU_60"}},
                {ThrottleDecision::THROTTLE_70,
                 {"LOW_POWER_LITTLE_CLUSTER_70", "LOW_POWER_MID_CLUSTER_70", "LOW_POWER_CPU_70"}},
                {ThrottleDecision::THROTTLE_80,
                 {"LOW_POWER_LITTLE_CLUSTER_80", "LOW_POWER_MID_CLUSTER_80", "LOW_POWER_CPU_80"}},
                {ThrottleDecision::THROTTLE_90,
                 {"LOW_POWER_LITTLE_CLUSTER_90", "LOW_POWER_MID_CLUSTER_90", "LOW_POWER_CPU_90"}}};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
