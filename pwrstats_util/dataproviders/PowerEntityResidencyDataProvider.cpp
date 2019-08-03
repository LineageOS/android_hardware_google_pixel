/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "pwrstats_util"

#include "PowerEntityResidencyDataProvider.h"
#include <android-base/logging.h>
#include <android/hardware/power/stats/1.0/IPowerStats.h>
using android::sp;
using android::hardware::Return;

/**
 * Power Entity State Residency data provider:
 * Provides data monitored by Power Stats HAL 1.0
 **/

int PowerEntityResidencyDataProvider::get(std::unordered_map<std::string, uint64_t>* data) {
    sp<android::hardware::power::stats::V1_0::IPowerStats> powerStatsService =
            android::hardware::power::stats::V1_0::IPowerStats::getService();
    if (powerStatsService == nullptr) {
        LOG(ERROR) << "Unable to get power.stats HAL service.";
        return 1;
    }

    std::unordered_map<uint32_t, std::string> entityNames;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> stateNames;

    Return<void> ret;
    ret = powerStatsService->getPowerEntityInfo([&entityNames](auto infos, auto /* status */) {
        for (auto const& info : infos) {
            entityNames.emplace(info.powerEntityId, info.powerEntityName);
        }
    });
    if (!ret.isOk()) {
        return 1;
    }

    ret = powerStatsService->getPowerEntityStateInfo({}, [&stateNames](auto stateSpaces,
                                                                       auto /* status */) {
        for (auto const& stateSpace : stateSpaces) {
            stateNames.emplace(stateSpace.powerEntityId,
                               std::unordered_map<uint32_t, std::string>());
            auto& entityStateNames = stateNames.at(stateSpace.powerEntityId);
            for (auto const& state : stateSpace.states) {
                entityStateNames.emplace(state.powerEntityStateId, state.powerEntityStateName);
            }
        }
    });
    if (!ret.isOk()) {
        return 1;
    }

    ret = powerStatsService->getPowerEntityStateResidencyData({}, [&entityNames, &stateNames,
                                                                   &data](auto results,
                                                                          auto /* status */) {
        for (auto const& result : results) {
            for (auto stateResidency : result.stateResidencyData) {
                std::ostringstream key;
                key << entityNames.at(result.powerEntityId) << "__"
                    << stateNames.at(result.powerEntityId).at(stateResidency.powerEntityStateId);
                data->emplace(key.str(), static_cast<uint64_t>(stateResidency.totalTimeInStateMs));
            }
        }
    });
    if (!ret.isOk()) {
        return 1;
    }

    return 0;
}
