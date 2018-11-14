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

#include <android-base/logging.h>
#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace power {
namespace stats {
namespace V1_0 {
namespace implementation {

void PowerStats::setRailDataProvider(std::unique_ptr<IRailDataProvider> dataProvider) {
    mRailDataProvider = std::move(dataProvider);
}

Return<void> PowerStats::getRailInfo(getRailInfo_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->getRailInfo(_hidl_cb);
}

Return<void> PowerStats::getEnergyData(const hidl_vec<uint32_t> &railIndices,
                                       getEnergyData_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->getEnergyData(railIndices, _hidl_cb);
}

Return<void> PowerStats::streamEnergyData(uint32_t timeMs, uint32_t samplingRate,
                                          streamEnergyData_cb _hidl_cb) {
    if (!mRailDataProvider) {
        _hidl_cb({}, 0, 0, Status::NOT_SUPPORTED);
        return Void();
    }

    return mRailDataProvider->streamEnergyData(timeMs, samplingRate, _hidl_cb);
}

uint32_t PowerStats::addPowerEntity(std::string name, PowerEntityType type) {
    uint32_t id = mPowerEntityInfos.size();
    mPowerEntityInfos.push_back({id, name, type});
    return id;
}

void PowerStats::addStateResidencyDataProvider(std::shared_ptr<IStateResidencyDataProvider> p) {
    std::vector<PowerEntityStateSpace> stateSpaces = p->getStateSpaces();
    for (auto it : stateSpaces) {
        mPowerEntityStateSpaces[it.powerEntityId] = it;
        mStateResidencyDataProviders[it.powerEntityId] = p;
    }
}

Return<void> PowerStats::getPowerEntityInfo(getPowerEntityInfo_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mPowerEntityInfos.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    _hidl_cb(mPowerEntityInfos, Status::SUCCESS);
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
        for (auto i : mPowerEntityStateSpaces) {
            s.push_back(i.second);
        }
        _hidl_cb(s, Status::SUCCESS);
        return Void();
    }

    // Return state space information only for valid ids
    auto ret = Status::SUCCESS;
    for (const uint32_t i : powerEntityIds) {
        if (mPowerEntityStateSpaces.find(i) != mPowerEntityStateSpaces.end()) {
            s.push_back(mPowerEntityStateSpaces.at(i));
        } else {
            ret = Status::INVALID_INPUT;
            // TODO(117887759) should we bail here?
            // s.resize(0);
            // break;
        }
    }

    _hidl_cb(s, ret);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateResidencyData(
    const hidl_vec<uint32_t> &powerEntityIds, getPowerEntityStateResidencyData_cb _hidl_cb) {
    // If not configured, return NOT_SUPPORTED
    if (mStateResidencyDataProviders.empty() || mPowerEntityStateSpaces.empty()) {
        _hidl_cb({}, Status::NOT_SUPPORTED);
        return Void();
    }

    // If powerEntityIds is empty then return data for all supported entities
    if (powerEntityIds.size() == 0) {
        std::vector<uint32_t> ids;
        for (auto i : mPowerEntityStateSpaces) {
            ids.push_back(i.first);
        }
        return getPowerEntityStateResidencyData(ids, _hidl_cb);
    }

    std::map<uint32_t, PowerEntityStateResidencyResult> stateResidencies;
    std::vector<PowerEntityStateResidencyResult> results;

    // return results for only the given powerEntityIds
    bool invalidInput = false;
    bool filesystemError = false;
    for (auto id : powerEntityIds) {
        // skip if the given powerEntityId does not have an associated StateResidencyDataProvider
        if (mStateResidencyDataProviders.find(id) == mStateResidencyDataProviders.end()) {
            invalidInput = true;
            // TODO(117887759) should we bail here?
            // results.resize(0);
            // break;
            continue;
        }

        // get the results if we have not already done so.
        if (stateResidencies.find(id) == stateResidencies.end()) {
            if (!mStateResidencyDataProviders.at(id)->getResults(stateResidencies)) {
                filesystemError = true;
            }
        }

        // append results
        if (stateResidencies.find(id) != stateResidencies.end()) {
            results.push_back(stateResidencies.at(id));
        }
    }

    auto ret = Status::SUCCESS;
    if (filesystemError) {
        ret = Status::FILESYSTEM_ERROR;
    } else if (invalidInput) {
        ret = Status::INVALID_INPUT;
    }

    _hidl_cb(results, ret);
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
