/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.power.stats-service.pixel"

#include "include/PowerStatsAidl.h"
#include <aidl/android/hardware/power/stats/BnPowerStats.h>

#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <inttypes.h>
#include <chrono>
#include <numeric>
#include <string>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

void PowerStats::addStateResidencyDataProvider(sp<IStateResidencyDataProvider> p) {
    if (!p) {
        return;
    }

    int32_t id = mPowerEntityInfos.size();

    for (const auto &[entityName, states] : p->getInfo()) {
        PowerEntityInfo i = {
                .powerEntityId = id++,
                .powerEntityName = entityName,
                .states = states,
        };
        mPowerEntityInfos.emplace_back(i);
        mStateResidencyDataProviders.emplace_back(p);
    }
}

ndk::ScopedAStatus PowerStats::getPowerEntityInfo(std::vector<PowerEntityInfo> *_aidl_return) {
    *_aidl_return = mPowerEntityInfos;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getStateResidency(const std::vector<int32_t> &in_powerEntityIds,
                                                 std::vector<StateResidencyResult> *_aidl_return) {
    if (mPowerEntityInfos.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    // If in_powerEntityIds is empty then return data for all supported entities
    if (in_powerEntityIds.empty()) {
        std::vector<int32_t> v(mPowerEntityInfos.size());
        std::iota(std::begin(v), std::end(v), 0);
        return getStateResidency(v, _aidl_return);
    }

    binder_status_t err = STATUS_OK;

    std::unordered_map<std::string, std::vector<StateResidency>> stateResidencies;

    for (const int32_t id : in_powerEntityIds) {
        // skip any invalid ids
        if (id < 0 || id >= mPowerEntityInfos.size()) {
            err = STATUS_BAD_VALUE;
            continue;
        }

        // Check to see if we already have data for the given id
        std::string powerEntityName = mPowerEntityInfos[id].powerEntityName;
        if (stateResidencies.find(powerEntityName) == stateResidencies.end()) {
            mStateResidencyDataProviders[id]->getStateResidencies(&stateResidencies);
        }

        // Append results if we have them
        auto stateResidency = stateResidencies.find(powerEntityName);
        if (stateResidency != stateResidencies.end()) {
            StateResidencyResult res = {
                    .powerEntityId = id,
                    .stateResidencyData = stateResidency->second,
            };
            _aidl_return->emplace_back(res);
        } else {
            // Failed to retrieve results for the given id.

            // Set error code to STATUS_FAILED_TRANSACTION but don't overwrite it
            // if there is already a higher priority error code
            err = (err == STATUS_OK) ? STATUS_FAILED_TRANSACTION : err;
        }
    }

    return ndk::ScopedAStatus::fromStatus(err);
}

void PowerStats::addEnergyConsumer(sp<IEnergyConsumer> p) {
    if (!p) {
        return;
    }

    mEnergyConsumers.emplace(p->getId(), p);
}

ndk::ScopedAStatus PowerStats::getEnergyConsumerInfo(std::vector<EnergyConsumerId> *_aidl_return) {
    for (const auto &[id, ignored] : mEnergyConsumers) {
        _aidl_return->push_back(id);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getEnergyConsumed(
        const std::vector<EnergyConsumerId> &in_energyConsumerIds,
        std::vector<EnergyConsumerResult> *_aidl_return) {
    if (mEnergyConsumers.empty()) {
        return ndk::ScopedAStatus::ok();
    }

    // If in_powerEntityIds is empty then return data for all supported energy consumers
    if (in_energyConsumerIds.empty()) {
        std::vector<EnergyConsumerId> ids;
        getEnergyConsumerInfo(&ids);
        return getEnergyConsumed(ids, _aidl_return);
    }

    binder_status_t err = STATUS_OK;

    for (const auto id : in_energyConsumerIds) {
        // skip any unavailable ids
        if (mEnergyConsumers.find(id) == mEnergyConsumers.end()) {
            err = STATUS_BAD_VALUE;
            continue;
        }

        auto res = mEnergyConsumers.at(id)->getEnergyConsumed();
        if (res) {
            _aidl_return->emplace_back(res.value());
        } else {
            // Failed to retrieve results for the given id.

            // Set error code to STATUS_FAILED_TRANSACTION but don't overwrite it
            // if there is already a higher priority error code
            err = (err == STATUS_OK) ? STATUS_FAILED_TRANSACTION : err;
        }
    }

    return ndk::ScopedAStatus::fromStatus(err);
}

void PowerStats::setEnergyMeterDataProvider(std::unique_ptr<IEnergyMeterDataProvider> p) {
    mEnergyMeterDataProvider = std::move(p);
}

ndk::ScopedAStatus PowerStats::getEnergyMeterInfo(std::vector<ChannelInfo> *_aidl_return) {
    if (!mEnergyMeterDataProvider) {
        return ndk::ScopedAStatus::ok();
    }
    return mEnergyMeterDataProvider->getEnergyMeterInfo(_aidl_return);
}

ndk::ScopedAStatus PowerStats::readEnergyMeters(const std::vector<int32_t> &in_channelIds,
                                                std::vector<EnergyMeasurement> *_aidl_return) {
    if (!mEnergyMeterDataProvider) {
        return ndk::ScopedAStatus::ok();
    }
    return mEnergyMeterDataProvider->readEnergyMeters(in_channelIds, _aidl_return);
}

void PowerStats::getEntityStateNames(
        std::unordered_map<int32_t, std::string> *entityNames,
        std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> *stateNames) {
    std::vector<PowerEntityInfo> infos;
    getPowerEntityInfo(&infos);

    for (const auto &info : infos) {
        entityNames->emplace(info.powerEntityId, info.powerEntityName);
        stateNames->emplace(info.powerEntityId, std::unordered_map<int32_t, std::string>());
        auto &entityStateNames = stateNames->at(info.powerEntityId);
        for (const auto &state : info.states) {
            entityStateNames.emplace(state.stateId, state.stateName);
        }
    }
}

void PowerStats::getChannelNames(std::unordered_map<int32_t, std::string> *channelNames) {
    std::vector<ChannelInfo> infos;
    getEnergyMeterInfo(&infos);

    for (const auto &info : infos) {
        channelNames->emplace(info.channelId, info.channelName);
    }
}

void PowerStats::dumpEnergyMeter(std::ostringstream &oss, bool delta) {
    const char *headerFormat = "  %18s   %18s\n";
    const char *dataFormat = "  %18s   %14.2f mWs\n";
    const char *headerFormatDelta = "  %18s   %18s (%14s)\n";
    const char *dataFormatDelta = "  %18s   %14.2f mWs (%14.2f)\n";

    std::unordered_map<int32_t, std::string> channelNames;
    getChannelNames(&channelNames);

    oss << "\n============= PowerStats HAL 2.0 energy meter ==============\n";

    std::vector<EnergyMeasurement> energyData;
    readEnergyMeters({}, &energyData);

    if (delta) {
        static std::vector<EnergyMeasurement> prevEnergyData;
        ::android::base::boot_clock::time_point curTime = ::android::base::boot_clock::now();
        static ::android::base::boot_clock::time_point prevTime = curTime;

        oss << "Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(curTime - prevTime).count()
            << " ms";

        oss << ::android::base::StringPrintf(headerFormatDelta, "Channel", "Cumulative Energy",
                                             "Delta   ");

        std::unordered_map<int32_t, int64_t> prevEnergyDataMap;
        for (const auto &data : prevEnergyData) {
            prevEnergyDataMap.emplace(data.channelId, data.energyUWs);
        }

        for (const auto &data : energyData) {
            auto prevEnergyDataIt = prevEnergyDataMap.find(data.channelId);
            int64_t deltaEnergy = 0;
            if (prevEnergyDataIt != prevEnergyDataMap.end()) {
                deltaEnergy = data.energyUWs - prevEnergyDataIt->second;
            }

            oss << ::android::base::StringPrintf(dataFormatDelta,
                                                 channelNames.at(data.channelId).c_str(),
                                                 static_cast<float>(data.energyUWs) / 1000.0,
                                                 static_cast<float>(deltaEnergy) / 1000.0);
        }

        prevEnergyData = energyData;
        prevTime = curTime;
    } else {
        oss << ::android::base::StringPrintf(headerFormat, "Channel", "Cumulative Energy");

        for (const auto &data : energyData) {
            oss << ::android::base::StringPrintf(dataFormat,
                                                 channelNames.at(data.channelId).c_str(),
                                                 static_cast<float>(data.energyUWs) / 1000.0);
        }
    }

    oss << "========== End of PowerStats HAL 2.0 energy meter ==========\n";
}

void PowerStats::dumpStateResidency(std::ostringstream &oss, bool delta) {
    const char *headerFormat = "  %14s   %14s   %16s   %15s   %17s\n";
    const char *dataFormat =
            "  %14s   %14s   %13" PRIu64 " ms   %15" PRIu64 "   %14" PRIu64 " ms\n";
    const char *headerFormatDelta = "  %14s   %14s   %16s (%14s)   %15s (%16s)   %17s (%14s)\n";
    const char *dataFormatDelta = "  %14s   %14s   %13" PRIu64 " ms (%14" PRId64 ")   %15" PRIu64
                                  " (%16" PRId64 ")   %14" PRIu64 " ms (%14" PRId64 ")\n";

    // Construct maps to entity and state names
    std::unordered_map<int32_t, std::string> entityNames;
    std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> stateNames;
    getEntityStateNames(&entityNames, &stateNames);

    oss << "\n============= PowerStats HAL 2.0 state residencies ==============\n";

    std::vector<StateResidencyResult> results;
    getStateResidency({}, &results);

    if (delta) {
        static std::vector<StateResidencyResult> prevResults;
        ::android::base::boot_clock::time_point curTime = ::android::base::boot_clock::now();
        static ::android::base::boot_clock::time_point prevTime = curTime;

        oss << "Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(curTime - prevTime).count()
            << " ms";

        oss << ::android::base::StringPrintf(headerFormatDelta, "Entity", "State", "Total time",
                                             "Delta   ", "Total entries", "Delta   ",
                                             "Last entry tstamp", "Delta ");

        // Process prevResults into a 2-tier lookup table for easy reference
        std::unordered_map<int32_t, std::unordered_map<int32_t, StateResidency>> prevResultsMap;
        for (const auto &prevResult : prevResults) {
            prevResultsMap.emplace(prevResult.powerEntityId,
                                   std::unordered_map<int32_t, StateResidency>());
            for (auto stateResidency : prevResult.stateResidencyData) {
                prevResultsMap.at(prevResult.powerEntityId)
                        .emplace(stateResidency.stateId, stateResidency);
            }
        }

        // Iterate over the new result data (one "result" per entity)
        for (const auto &result : results) {
            const char *entityName = entityNames.at(result.powerEntityId).c_str();

            // Look up previous result data for the same entity
            auto prevEntityResultIt = prevResultsMap.find(result.powerEntityId);

            // Iterate over individual states within the current entity's new result
            for (const auto &stateResidency : result.stateResidencyData) {
                const char *stateName =
                        stateNames.at(result.powerEntityId).at(stateResidency.stateId).c_str();

                // If a previous result was found for the same entity, see if that
                // result also contains data for the current state
                int64_t deltaTotalTime = 0;
                int64_t deltaTotalCount = 0;
                int64_t deltaTimestamp = 0;
                if (prevEntityResultIt != prevResultsMap.end()) {
                    auto prevStateResidencyIt =
                            prevEntityResultIt->second.find(stateResidency.stateId);
                    // If a previous result was found for the current entity and state, calculate
                    // the deltas and display them along with new result
                    if (prevStateResidencyIt != prevEntityResultIt->second.end()) {
                        deltaTotalTime = stateResidency.totalTimeInStateMs -
                                         prevStateResidencyIt->second.totalTimeInStateMs;
                        deltaTotalCount = stateResidency.totalStateEntryCount -
                                          prevStateResidencyIt->second.totalStateEntryCount;
                        deltaTimestamp = stateResidency.lastEntryTimestampMs -
                                         prevStateResidencyIt->second.lastEntryTimestampMs;
                    }
                }

                oss << ::android::base::StringPrintf(
                        dataFormatDelta, entityName, stateName, stateResidency.totalTimeInStateMs,
                        deltaTotalTime, stateResidency.totalStateEntryCount, deltaTotalCount,
                        stateResidency.lastEntryTimestampMs, deltaTimestamp);
            }
        }

        prevResults = results;
        prevTime = curTime;
    } else {
        oss << ::android::base::StringPrintf(headerFormat, "Entity", "State", "Total time",
                                             "Total entries", "Last entry tstamp");
        for (const auto &result : results) {
            for (const auto &stateResidency : result.stateResidencyData) {
                oss << ::android::base::StringPrintf(
                        dataFormat, entityNames.at(result.powerEntityId).c_str(),
                        stateNames.at(result.powerEntityId).at(stateResidency.stateId).c_str(),
                        stateResidency.totalTimeInStateMs, stateResidency.totalStateEntryCount,
                        stateResidency.lastEntryTimestampMs);
            }
        }
    }

    oss << "========== End of PowerStats HAL 2.0 state residencies ==========\n";
}

void PowerStats::dumpEnergyConsumer(std::ostringstream &oss, bool delta) {
    (void)delta;

    std::vector<EnergyConsumerResult> results;
    getEnergyConsumed({}, &results);

    oss << "\n============= PowerStats HAL 2.0 energy consumers ==============\n";

    for (const auto &result : results) {
        oss << ::android::base::StringPrintf("%d = %14.2f mWs\n", result.energyConsumerId,
                                             static_cast<float>(result.energyUWs) / 1000.0);
    }

    oss << "========== End of PowerStats HAL 2.0 energy consumers ==========\n";
}

binder_status_t PowerStats::dump(int fd, const char **args, uint32_t numArgs) {
    std::ostringstream oss;
    bool delta = (numArgs == 1) && (std::string(args[0]) == "delta");

    // Generate debug output for state residency
    dumpStateResidency(oss, delta);

    // Generate debug output for energy consumer
    dumpEnergyConsumer(oss, delta);

    // Generate debug output energy meter
    dumpEnergyMeter(oss, delta);

    ::android::base::WriteStringToFd(oss.str(), fd);
    fsync(fd);
    return STATUS_OK;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
