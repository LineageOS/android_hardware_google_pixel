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

#include <aidl/android/hardware/powerstats/BnPowerStats.h>

#include <utils/RefBase.h>

#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace powerstats {

using ::android::sp;

class PowerStats : public BnPowerStats {
  public:
    class IStateResidencyDataProvider : public virtual ::android::RefBase {
      public:
        virtual ~IStateResidencyDataProvider() = default;
        virtual bool getResults(
                std::unordered_map<std::string, std::vector<StateResidency>> *results) = 0;
        virtual std::unordered_map<std::string, std::vector<StateInfo>> getInfo() = 0;
    };

    class IRailEnergyDataProvider {
      public:
        virtual ~IRailEnergyDataProvider() = default;
        virtual ndk::ScopedAStatus getRailEnergy(const std::vector<int32_t> &in_railIds,
                                                 std::vector<EnergyMeasurement> *_aidl_return) = 0;
        virtual ndk::ScopedAStatus getRailInfo(std::vector<ChannelInfo> *_aidl_return) = 0;
    };

    PowerStats() = default;
    void setRailDataProvider(std::unique_ptr<IRailEnergyDataProvider> p);
    void addStateResidencyDataProvider(sp<IStateResidencyDataProvider> p);

    // Methods from aidl::android::hardware::powerstats::IPowerStats
    ndk::ScopedAStatus getPowerEntityInfo(std::vector<PowerEntityInfo> *_aidl_return) override;
    ndk::ScopedAStatus getPowerEntityStateResidency(
            const std::vector<int32_t> &in_powerEntityIds,
            std::vector<StateResidencyResult> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumerInfo(std::vector<EnergyConsumerId> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyConsumed(const std::vector<EnergyConsumerId> &in_energyConsumerIds,
                                         std::vector<EnergyConsumerResult> *_aidl_return) override;
    ndk::ScopedAStatus getEnergyMeterInfo(std::vector<ChannelInfo> *_aidl_return) override;
    ndk::ScopedAStatus readEnergyMeters(const std::vector<int32_t> &in_channelIds,
                                        std::vector<EnergyMeasurement> *_aidl_return) override;
    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

  private:
    void getEntityStateMaps(
            std::unordered_map<int32_t, std::string> *entityNames,
            std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> *stateNames);
    void getRailEnergyMaps(std::unordered_map<int32_t, std::string> *railNames);
    void dumpStateResidency(std::ostringstream &oss, bool delta);
    void dumpStateResidencyDelta(std::ostringstream &oss,
                                 const std::vector<StateResidencyResult> &results);
    void dumpStateResidencyOneShot(std::ostringstream &oss,
                                   const std::vector<StateResidencyResult> &results);
    void dumpRailEnergy(std::ostringstream &oss, bool delta);

    std::vector<sp<IStateResidencyDataProvider>> mStateResidencyDataProviders;
    std::vector<PowerEntityInfo> mPowerEntityInfos;

    std::unique_ptr<IRailEnergyDataProvider> mRailEnergyDataProvider;
};

}  // namespace powerstats
}  // namespace hardware
}  // namespace android
}  // namespace aidl
