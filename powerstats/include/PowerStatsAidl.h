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

#include <utils/LightRefBase.h>
#include <utils/StrongPointer.h>

#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace powerstats {

using ::android::LightRefBase;
using ::android::sp;

class PowerStats : public BnPowerStats {
  public:
    class IStateResidencyDataProvider : public LightRefBase<IStateResidencyDataProvider> {
      public:
        virtual ~IStateResidencyDataProvider() = default;
        virtual bool getResults(
                std::unordered_map<std::string, std::vector<PowerEntityStateResidencyData>>
                        *results) = 0;
        virtual std::unordered_map<std::string, std::vector<PowerEntityStateInfo>> getInfo() = 0;
    };

    PowerStats() = default;
    void addStateResidencyDataProvider(sp<IStateResidencyDataProvider> p);

    // Methods from aidl::android::hardware::powerstats::IPowerStats
    ndk::ScopedAStatus getEnergyData(const std::vector<int32_t> &in_railIndices,
                                     std::vector<EnergyData> *_aidl_return) override;
    ndk::ScopedAStatus getPowerEntityInfo(std::vector<PowerEntityInfo> *_aidl_return) override;
    ndk::ScopedAStatus getPowerEntityStateResidencyData(
            const std::vector<int32_t> &in_powerEntityIds,
            std::vector<PowerEntityStateResidencyResult> *_aidl_return) override;
    ndk::ScopedAStatus getRailInfo(std::vector<RailInfo> *_aidl_return) override;
    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

  private:
    void getEntityStateMaps(
            std::unordered_map<int32_t, std::string> *entityNames,
            std::unordered_map<int32_t, std::unordered_map<int32_t, std::string>> *stateNames);
    void dumpStateResidency(std::ostringstream &oss, bool delta);
    void dumpStateResidencyDelta(std::ostringstream &oss,
                                 const std::vector<PowerEntityStateResidencyResult> &results);
    void dumpStateResidencyOneShot(std::ostringstream &oss,
                                   const std::vector<PowerEntityStateResidencyResult> &results);
    void dumpRailEnergy(std::ostringstream &oss, bool delta);

    std::vector<sp<IStateResidencyDataProvider>> mStateResidencyDataProviders;
    std::vector<PowerEntityInfo> mPowerEntityInfos;
};

}  // namespace powerstats
}  // namespace hardware
}  // namespace android
}  // namespace aidl
