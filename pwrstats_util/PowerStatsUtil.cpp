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

#include "PowerStatsUtil.h"

#include <android-base/logging.h>
#include <android-base/parsedouble.h>
#include <android/hardware/power/stats/1.0/IPowerStats.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>

using android::sp;
using android::hardware::Return;

/**
 * C-state residency data provider:
 * Provides C-state residency information for each of the CPUs and L3 cache.
 **/
class CstateDataProvider : public IPwrStatsUtilDataProvider {
  public:
    CstateDataProvider() = default;

    int get(std::unordered_map<std::string, uint64_t>& data) override {
        // example accessing a sysfs node
        std::ifstream file("/sys/kernel/debug/lpm_stats/stats");

        std::smatch matches;
        const std::regex searchExpr("\\[(.*?)\\] (.*?):");
        std::string line;
        const std::string searchStr = "total success time:";

        while (std::getline(file, line)) {
            if (std::regex_search(line, matches, searchExpr)) {
                std::ostringstream key;
                key << matches[1] << "__" << matches[2];

                while (std::getline(file, line)) {
                    size_t pos = line.find(searchStr);
                    if (pos != std::string::npos) {
                        float val;
                        if (android::base::ParseFloat(line.substr(pos + searchStr.size()), &val)) {
                            data.emplace(key.str(), static_cast<uint64_t>(val * 1000));
                        } else {
                            LOG(ERROR) << __func__ << ": failed to parse c-state data";
                        }
                        break;
                    }
                }
            }
        }

        return 0;
    }
};

/**
 * Power Stats HAL data provider:
 * Provides data monitored by Power Stats HAL 1.0
 **/
class PwrStatsHalDataProvider : public IPwrStatsUtilDataProvider {
  public:
    PwrStatsHalDataProvider() = default;

    int get(std::unordered_map<std::string, uint64_t>& data) override {
        // example using the power stats HAL
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

        ret = powerStatsService->getPowerEntityStateResidencyData(
                {}, [&entityNames, &stateNames, &data](auto results, auto /* status */) {
                    for (auto const& result : results) {
                        for (auto stateResidency : result.stateResidencyData) {
                            std::ostringstream key;
                            key << entityNames.at(result.powerEntityId) << "__"
                                << stateNames.at(result.powerEntityId)
                                            .at(stateResidency.powerEntityStateId);
                            data.emplace(key.str(),
                                         static_cast<uint64_t>(stateResidency.totalTimeInStateMs));
                        }
                    }
                });
        if (!ret.isOk()) {
            return 1;
        }

        return 0;
    }
};

PowerStatsUtil::PowerStatsUtil() {
    mDataProviders.emplace_back(std::make_unique<CstateDataProvider>());
    mDataProviders.emplace_back(std::make_unique<PwrStatsHalDataProvider>());
}

int PowerStatsUtil::getData(std::unordered_map<std::string, uint64_t>& data) {
    data.clear();
    for (auto&& provider : mDataProviders) {
        int ret = provider->get(data);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}
