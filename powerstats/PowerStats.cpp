/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "libpixelpowerstats"

#include <log/log.h>
#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace power {
namespace stats {
namespace V1_0 {
namespace implementation {

PowerStats::PowerStats() = default;

void PowerStats::setRailDataProvider(std::unique_ptr<IRailDataProvider> dataProvider) {
    mRailDataProvider = std::move(dataProvider);
}

void PowerStats::setPowerEntityConfig(const std::vector<PowerEntityConfig> &configs) {
    for (uint32_t i = 0; i < configs.size(); ++i) {
        auto &entityConfig = configs[i];

        // Inserting each PowerEntityInfo into mPowerEntityInfos
        mPowerEntityInfos.push_back({i, entityConfig.name, entityConfig.type});

        if (!entityConfig.states.empty()) {
            // Inserting each PowerEntityStateSpace into mPowerEntityStateSpaces
            mPowerEntityStateSpaces[i] = {i, {}};

            // Inserting each PowerEntityStateInfo into its corresponding PowerEntityStateSpace
            mPowerEntityStateSpaces[i].states.resize(entityConfig.states.size());
            for (uint32_t j = 0; j < entityConfig.states.size(); ++j) {
                mPowerEntityStateSpaces[i].states[j] = {j, entityConfig.states[j]};
            }
        }
    }
}

Return<void> PowerStats::getRailInfo(getRailInfo_cb _hidl_cb) {
    if (mRailDataProvider) {
        return mRailDataProvider->getRailInfo(_hidl_cb);
    } else {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }
}

Return<void> PowerStats::getEnergyData(const hidl_vec<uint32_t> &railIndices,
                                       getEnergyData_cb _hidl_cb) {
    if (mRailDataProvider) {
        return mRailDataProvider->getEnergyData(railIndices, _hidl_cb);
    } else {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }
}

Return<void> PowerStats::streamEnergyData(uint32_t timeMs, uint32_t samplingRate,
                                          streamEnergyData_cb _hidl_cb) {
    if (mRailDataProvider) {
        return mRailDataProvider->streamEnergyData(timeMs, samplingRate, _hidl_cb);
    } else {
        _hidl_cb({}, 0, 0, Status::NOT_SUPPORTED);
        return Void();
    }
}

Return<void> PowerStats::getPowerEntityInfo(getPowerEntityInfo_cb _hidl_cb) {
    _hidl_cb(mPowerEntityInfos,
             mPowerEntityInfos.empty() ? Status::NOT_SUPPORTED : Status::SUCCESS);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateInfo(const hidl_vec<uint32_t> &powerEntityIds,
                                                 getPowerEntityStateInfo_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mPowerEntityStateSpaces.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    std::vector<PowerEntityStateSpace> s;

    // If powerEntityIds is empty then return state space info for all entities
    if (powerEntityIds.size() == 0) {
        s.reserve(mPowerEntityStateSpaces.size());
        for (auto i : mPowerEntityStateSpaces) {
            s.push_back(i.second);
        }
        _hidl_cb(s, Status::SUCCESS);
        return Void();
    }

    // Return state space information only for valid ids
    for (const uint32_t i : powerEntityIds) {
        if (mPowerEntityStateSpaces.count(i) != 0) {
            s.push_back(mPowerEntityStateSpaces[i]);
        }
    }

    _hidl_cb(s, s.empty() ? Status::INVALID_INPUT : Status::SUCCESS);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateResidencyData(
    const hidl_vec<uint32_t> &powerEntityIds, getPowerEntityStateResidencyData_cb _hidl_cb) {
    (void)powerEntityIds;
    _hidl_cb({}, Status::NOT_SUPPORTED);
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
