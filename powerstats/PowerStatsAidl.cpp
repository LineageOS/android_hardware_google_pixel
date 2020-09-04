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

#define LOG_TAG "android.hardware.powerstats-service.pixel"

#include <numeric>

#include "include/PowerStatsAidl.h"
#include <aidl/android/hardware/powerstats/BnPowerStats.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

namespace aidl {
namespace android {
namespace hardware {
namespace powerstats {

void PowerStats::addStateResidencyDataProvider(sp<IStateResidencyDataProvider> p) {
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

ndk::ScopedAStatus PowerStats::getEnergyData(const std::vector<int32_t> &in_railIndices,
                                             std::vector<EnergyData> *_aidl_return) {
    (void)in_railIndices;
    (void)_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getPowerEntityInfo(std::vector<PowerEntityInfo> *_aidl_return) {
    *_aidl_return = mPowerEntityInfos;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerStats::getPowerEntityStateResidencyData(
        const std::vector<int32_t> &in_powerEntityIds,
        std::vector<PowerEntityStateResidencyResult> *_aidl_return) {
    // If powerEntityIds is empty then return data for all supported entities
    if (in_powerEntityIds.empty()) {
        std::vector<int32_t> v(mPowerEntityInfos.size());
        std::iota(std::begin(v), std::end(v), 0);
        return getPowerEntityStateResidencyData(v, _aidl_return);
    }

    binder_status_t err = STATUS_OK;

    std::unordered_map<std::string, std::vector<PowerEntityStateResidencyData>> stateResidencies;

    for (const int32_t id : in_powerEntityIds) {
        // skip any invalid ids
        if (id < 0 || id >= mPowerEntityInfos.size()) {
            err = STATUS_BAD_VALUE;
            continue;
        }

        // Check to see if we already have data for the given id
        std::string powerEntityName = mPowerEntityInfos[id].powerEntityName;
        if (stateResidencies.find(powerEntityName) == stateResidencies.end()) {
            mStateResidencyDataProviders[id]->getResults(&stateResidencies);
        }

        // Append results if we have them
        auto stateResidency = stateResidencies.find(powerEntityName);
        if (stateResidency != stateResidencies.end()) {
            PowerEntityStateResidencyResult res = {
                    .powerEntityId = id,
                    .stateResidencyData = stateResidency->second,
            };
            _aidl_return->emplace_back(res);
        } else {
            // We failed to retrieve results for the given id.

            // Set error code to STATUS_FAILED_TRANSACTION but don't overwrite it
            // if there is already a higher priority error code
            err = (err == STATUS_OK) ? STATUS_FAILED_TRANSACTION : err;
        }
    }

    return ndk::ScopedAStatus::fromStatus(err);
}

ndk::ScopedAStatus PowerStats::getRailInfo(std::vector<RailInfo> *_aidl_return) {
    (void)_aidl_return;
    return ndk::ScopedAStatus::ok();
}

binder_status_t PowerStats::dump(int fd, const char **, uint32_t) {
    std::ostringstream dumpStats;

    std::vector<PowerEntityInfo> infos;
    getPowerEntityInfo(&infos);

    dumpStats << ::android::base::StringPrintf("infos:\n");
    for (auto i : infos) {
        for (auto j : i.states) {
            dumpStats << ::android::base::StringPrintf(
                    "%s, %d, %s, %d\n", i.powerEntityName.c_str(), i.powerEntityId,
                    j.powerEntityStateName.c_str(), j.powerEntityStateId);
        }
    }

    dumpStats << ::android::base::StringPrintf("residencies:\n");
    std::vector<PowerEntityStateResidencyResult> results;
    getPowerEntityStateResidencyData({}, &results);
    for (auto i : results) {
        for (auto j : i.stateResidencyData) {
            dumpStats << ::android::base::StringPrintf(
                    "%d, %d, %ld, %ld, %ld\n", i.powerEntityId, j.powerEntityStateId,
                    j.totalTimeInStateMs, j.totalStateEntryCount, j.lastEntryTimestampMs);
        }
    }

    ::android::base::WriteStringToFd(dumpStats.str(), fd);
    fsync(fd);
    return STATUS_OK;
}

}  // namespace powerstats
}  // namespace hardware
}  // namespace android
}  // namespace aidl
