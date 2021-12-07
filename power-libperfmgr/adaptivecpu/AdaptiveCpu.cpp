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
#include <sys/resource.h>
#include <utils/Trace.h>

#include <chrono>
#include <deque>
#include <numeric>

#include "Model.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using std::chrono_literals::operator""ms;

// The sleep duration for each iteration. Currently set to a large value as a safeguard measure, so
// we don't spin needlessly and waste energy. To experiment with the model, set to a smaller value,
// e.g. around 25ms.
// TODO(b/188770301) Once the gating logic is implemented, reduce the sleep duration.
static const std::chrono::milliseconds kIterationSleepDuration = 1000ms;

// Timeout applied to hints. If Adaptive CPU doesn't receive any frames in this time, CPU throttling
// hints are cancelled.
static const std::chrono::milliseconds kHintTimeout = 2000ms;

// We pass the previous N ModelInputs to the model, including the most recent ModelInput.
constexpr uint32_t kNumHistoricalModelInputs = 3;

AdaptiveCpu::AdaptiveCpu(std::shared_ptr<HintManager> hintManager)
    : mHintManager(hintManager), mIsEnabled(false), mIsInitialized(false) {}

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
    mWorkDurationProcessor.ReportWorkDurations(workDurations, targetDuration);
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

    std::deque<ModelInput> historicalModelInputs;
    ThrottleDecision previousThrottleDecision = ThrottleDecision::NO_THROTTLE;
    while (true) {
        ATRACE_NAME("loop");
        WaitForEnabledAndWorkDurations();

        ATRACE_BEGIN("compute");

        if (!mIsInitialized) {
            if (!mCpuFrequencyReader.init()) {
                mIsEnabled = false;
                continue;
            }
            mCpuLoadReader.init();
            mIsInitialized = true;
        }

        const WorkDurationFeatures workDurationFeatures = mWorkDurationProcessor.GetFeatures();
        LOG(VERBOSE) << "Got work durations: count=" << workDurationFeatures.numDurations
                     << ", average=" << workDurationFeatures.averageDuration.count() << "ns";
        if (workDurationFeatures.numDurations == 0) {
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

        std::vector<CpuLoad> cpuLoads;
        if (!mCpuLoadReader.getRecentCpuLoads(&cpuLoads)) {
            mIsEnabled = false;
            continue;
        }
        LOG(VERBOSE) << "Got CPU loads: " << cpuLoads.size();
        for (const auto &cpuLoad : cpuLoads) {
            LOG(VERBOSE) << "cpu=" << cpuLoad.cpuId << ", idle=" << cpuLoad.idleTimeFraction;
        }

        ModelInput modelInput;
        if (!modelInput.Init(cpuPolicyFrequencies, cpuLoads, workDurationFeatures,
                             previousThrottleDecision)) {
            mIsEnabled = false;
            continue;
        }
        modelInput.LogToAtrace();
        historicalModelInputs.push_back(modelInput);
        if (historicalModelInputs.size() > kNumHistoricalModelInputs) {
            historicalModelInputs.pop_front();
        }

        const ThrottleDecision throttleDecision = RunModel(historicalModelInputs);
        LOG(VERBOSE) << "Model decision: " << static_cast<uint32_t>(throttleDecision);
        ATRACE_INT("AdaptiveCpu_throttleDecision", static_cast<uint32_t>(throttleDecision));

        if (throttleDecision != previousThrottleDecision) {
            ATRACE_NAME("sendHints");
            for (const auto &hintName : kThrottleDecisionToHintNames.at(throttleDecision)) {
                mHintManager->DoHint(hintName, kHintTimeout);
            }
            for (const auto &hintName : kThrottleDecisionToHintNames.at(previousThrottleDecision)) {
                mHintManager->EndHint(hintName);
            }
            previousThrottleDecision = throttleDecision;
        }

        ATRACE_END();  // compute
        {
            ATRACE_NAME("sleep");
            std::this_thread::sleep_for(kIterationSleepDuration);
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
    result << "CPU loads:\n";
    const auto previousCpuTimes = mCpuLoadReader.getPreviousCpuTimes();
    for (const auto &[cpuId, cpuTime] : previousCpuTimes) {
        result << "- CPU=" << cpuId << ", idleTime=" << cpuTime.idleTimeMs
               << "ms, totalTime=" << cpuTime.totalTimeMs << "ms\n";
    }
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
