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

#pragma once

#include <PowerStatsAidl.h>

#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace powerstats {

class IioRailEnergyDataProvider : public PowerStats::IRailEnergyDataProvider {
  public:
    IioRailEnergyDataProvider(const std::string &deviceName);

    // Methods from PowerStats::IRailEnergyDataProvider
    ndk::ScopedAStatus getRailEnergy(const std::vector<int32_t> &in_railIds,
                                     std::vector<EnergyMeasurement> *_aidl_return) override;
    ndk::ScopedAStatus getRailInfo(std::vector<ChannelInfo> *_aidl_return) override;

  private:
    void findIioPowerMonitorNodes();
    void parsePowerRails();
    int parseIioEnergyNode(std::string path);

    std::mutex mLock;
    std::vector<std::string> mDevicePaths;
    std::unordered_map<std::string, int32_t> mRailIds;  // key: name, value: id
    std::vector<ChannelInfo> mRailInfos;
    std::vector<EnergyMeasurement> mReading;

    const std::string kDeviceName;
    const std::string kDeviceType = "iio:device";
    const std::string kIioRootDir = "/sys/bus/iio/devices/";
    const std::string kNameNode = "/name";
    const std::string kSamplingRateNode = "/sampling_rate";
    const std::string kEnabledRailsNode = "/enabled_rails";
    const std::string kEnergyValueNode = "/energy_value";
};

}  // namespace powerstats
}  // namespace hardware
}  // namespace android
}  // namespace aidl
