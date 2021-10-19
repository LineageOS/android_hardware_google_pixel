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

#include <android-base/file.h>
#include <android-base/logging.h>

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
using std::chrono_literals::operator""ns;

// The standard target duration, based on 60 FPS. Durations submitted with different targets are
// normalized against this target. For example, a duration that was at 80% of its target will be
// scaled to 0.8 * kNormalTargetDuration.
static const std::chrono::nanoseconds kNormalTargetDuration = 16666666ns;

// All durations shorter than this are ignored.
static const std::chrono::nanoseconds kMinDuration = 0ns;

// All durations longer than this are ignored.
static const std::chrono::nanoseconds kMaxDuration = 600 * kNormalTargetDuration;

// Whether to block until work durations are available before running the model. We currently block
// in order to avoid spinning needlessly and wasting energy. In the final version, the iteration
// should be controlled by the model itself, so we may return an empty vector instead.
// TODO(b/188770301) Once the gating logic is implemented, don't block indefinitely.
static const bool kBlockForWorkDurations = true;

// The sleep duration for each iteration. Currently set to a large value as a safeguard measure, so
// we don't spin needlessly and waste energy. To experiment with the model, set to a smaller value,
// e.g. around 25ms.
// TODO(b/188770301) Once the gating logic is implemented, reduce the sleep duration.
static const std::chrono::milliseconds kIterationSleepDuration = 1000ms;

// We pass the previous N ModelInputs to the model, including the most recent ModelInput.
constexpr uint32_t kNumHistoricalModelInputs = 3;

AdaptiveCpu::AdaptiveCpu(std::shared_ptr<HintManager> hintManager)
    : mHintManager(hintManager), mIsEnabled(false), mIsInitialized(false) {}

bool AdaptiveCpu::IsEnabled() const {
    return mIsEnabled;
}

void AdaptiveCpu::HintReceived(bool enable) {
    LOG(INFO) << "AdaptiveCpu received hint: enable=" << enable;
    if (enable) {
        StartThread();
    } else {
        SuspendThread();
    }
}

void AdaptiveCpu::StartThread() {
    std::lock_guard lock(mThreadCreationMutex);
    LOG(INFO) << "Starting AdaptiveCpu thread";
    mIsEnabled = true;
    if (!mLoopThread.joinable()) {
        mLoopThread = std::thread([&]() {
            LOG(INFO) << "Started AdaptiveCpu thread successfully";
            RunMainLoop();
        });
    }
}

void AdaptiveCpu::SuspendThread() {
    LOG(INFO) << "Stopping AdaptiveCpu thread";
    // This stops the thread from receiving work durations in ReportWorkDurations, which means the
    // thread blocks indefinitely.
    mIsEnabled = false;
}

void AdaptiveCpu::ReportWorkDurations(const std::vector<WorkDuration> &workDurations,
                                      std::chrono::nanoseconds targetDuration) {
    if (!mIsEnabled) {
        return;
    }
    LOG(VERBOSE) << "AdaptiveCpu received " << workDurations.size()
                 << " work durations with target " << targetDuration.count() << "ns";
    {
        std::unique_lock<std::mutex> lock(mWorkDurationsMutex);
        mWorkDurationBatches.emplace_back(workDurations, targetDuration);
    }
    mWorkDurationsAvailableCondition.notify_one();
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

std::vector<WorkDurationBatch> AdaptiveCpu::TakeWorkDurations() {
    std::unique_lock<std::mutex> lock(mWorkDurationsMutex);

    if (kBlockForWorkDurations) {
        mWorkDurationsAvailableCondition.wait(lock, [&] { return !mWorkDurationBatches.empty(); });
    }

    std::vector<WorkDurationBatch> result;
    mWorkDurationBatches.swap(result);
    return result;
}

void AdaptiveCpu::RunMainLoop() {
    if (!mIsInitialized) {
        if (!mCpuFrequencyReader.init()) {
            LOG(INFO) << "Failed to initialize CPU frequency reading";
            mIsEnabled = false;
            return;
        }
        mCpuLoadReader.init();
        mIsInitialized = true;
    }

    std::deque<ModelInput> historicalModelInputs;
    ThrottleDecision previousThrottleDecision = ThrottleDecision::NO_THROTTLE;
    while (true) {
        std::vector<WorkDurationBatch> workDurationBatches = TakeWorkDurations();

        if (workDurationBatches.empty()) {
            continue;
        }

        std::chrono::nanoseconds durationsSum;
        int durationsCount = 0;
        for (const WorkDurationBatch &batch : workDurationBatches) {
            for (const WorkDuration workDuration : batch.workDurations) {
                std::chrono::nanoseconds duration(workDuration.durationNanos);

                if (duration < kMinDuration || duration > kMaxDuration) {
                    continue;
                }
                // Normalise the duration and add it to the total.
                // kMaxDuration * kStandardTarget.count() fits comfortably within int64_t.
                durationsSum +=
                        duration * kNormalTargetDuration.count() / batch.targetDuration.count();
                ++durationsCount;
            }
        }

        std::chrono::nanoseconds averageDuration = durationsSum / durationsCount;

        LOG(VERBOSE) << "AdaptiveCPU processing durations: count=" << durationsCount
                     << " average=" << averageDuration.count() << "ns";

        std::vector<CpuPolicyAverageFrequency> cpuPolicyFrequencies;
        if (!mCpuFrequencyReader.getRecentCpuPolicyFrequencies(&cpuPolicyFrequencies)) {
            break;
        }
        LOG(VERBOSE) << "Got CPU frequencies: " << cpuPolicyFrequencies.size();
        for (const auto &cpuPolicyFrequency : cpuPolicyFrequencies) {
            LOG(VERBOSE) << "policy=" << cpuPolicyFrequency.policyId
                         << ", freq=" << cpuPolicyFrequency.averageFrequencyHz;
        }

        std::vector<CpuLoad> cpuLoads;
        if (!mCpuLoadReader.getRecentCpuLoads(&cpuLoads)) {
            break;
        }
        LOG(VERBOSE) << "Got CPU loads: " << cpuLoads.size();
        for (const auto &cpuLoad : cpuLoads) {
            LOG(VERBOSE) << "cpu=" << cpuLoad.cpuId << ", idle=" << cpuLoad.idleTimeFraction;
        }

        ModelInput modelInput;
        if (!modelInput.Init(cpuPolicyFrequencies, cpuLoads, averageDuration, durationsCount,
                             previousThrottleDecision)) {
            break;
        }
        historicalModelInputs.push_back(modelInput);
        if (historicalModelInputs.size() > kNumHistoricalModelInputs) {
            historicalModelInputs.pop_front();
        }

        const ThrottleDecision throttleDecision = RunModel(historicalModelInputs);
        LOG(VERBOSE) << "Model decision: " << static_cast<uint32_t>(throttleDecision);
        previousThrottleDecision = throttleDecision;

        // TODO(b/188770301): Pass the model output to HintManager.

        // TODO(b/187691504): Add a check configuration properties and exit the loop if necessary.
        std::this_thread::sleep_for(kIterationSleepDuration);
    }
    // If the loop exits due to an error, disable Adaptive CPU so no work is done on e.g.
    // TakeWorkDurations.
    mIsEnabled = false;
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
