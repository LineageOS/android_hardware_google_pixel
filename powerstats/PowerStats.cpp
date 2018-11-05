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

#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace power {
namespace stats {
namespace V1_0 {
namespace implementation {

PowerStats::PowerStats() = default;

void PowerStats::setRailDataProvider(std::unique_ptr<IRailDataProvider> r) {
    mRailDataProvider = std::move(r);
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
    _hidl_cb({}, Status::NOT_SUPPORTED);
    return Void();
}

Return<void> PowerStats::getPowerEntityStateInfo(const hidl_vec<uint32_t> &powerEntityIds,
                                                 getPowerEntityStateInfo_cb _hidl_cb) {
    (void)powerEntityIds;
    _hidl_cb({}, Status::NOT_SUPPORTED);
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
