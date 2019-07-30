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

#include "RailEnergyDataProvider.h"
#include <android-base/logging.h>
#include <android/hardware/power/stats/1.0/IPowerStats.h>

using android::sp;
using android::hardware::Return;
using android::hardware::power::stats::V1_0::IPowerStats;
using android::hardware::power::stats::V1_0::Status;

int RailEnergyDataProvider::get(std::unordered_map<std::string, uint64_t>* data) {
    // example using the power stats HAL
    sp<IPowerStats> powerStatsService =
            android::hardware::power::stats::V1_0::IPowerStats::getService();
    if (powerStatsService == nullptr) {
        LOG(ERROR) << "Unable to get power.stats HAL service.";
        return 1;
    }

    std::unordered_map<uint32_t, std::string> railNames;

    Return<void> ret;
    Status retStatus = Status::SUCCESS;
    ret = powerStatsService->getRailInfo([&railNames, &retStatus](auto railInfos, auto status) {
        retStatus = status;
        if (status != Status::SUCCESS) {
            return;
        }

        for (auto const& info : railInfos) {
            railNames.emplace(info.index,
                              std::string(info.subsysName) + "__" + std::string(info.railName));
        }
    });
    if (retStatus == Status::NOT_SUPPORTED) {
        LOG(WARNING) << "rail energy stats not supported";
        return 0;
    }
    if (!ret.isOk() || retStatus != Status::SUCCESS) {
        LOG(ERROR) << "no rail information available";
        return 1;
    }

    bool resultSuccess = true;
    ret = powerStatsService->getEnergyData(
            {}, [&data, &railNames, &resultSuccess](auto energyData, auto status) {
                if (status != Status::SUCCESS) {
                    LOG(ERROR) << "Error getting rail energy";
                    resultSuccess = false;
                    return;
                }
                for (auto const& energyDatum : energyData) {
                    auto railName = railNames.find(energyDatum.index);
                    if (railName == railNames.end()) {
                        LOG(ERROR) << "Missing one or more rail names";
                        resultSuccess = false;
                        return;
                    }
                    data->emplace(railName->second, energyDatum.energy);
                }
            });
    if (!ret.isOk() || !resultSuccess) {
        LOG(ERROR) << "Failed to get rail energy stats";
        return 1;
    }

    return 0;
}
