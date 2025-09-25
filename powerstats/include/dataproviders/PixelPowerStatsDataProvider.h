/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <aidl/android/vendor/powerstats/BnPixelPowerStatsCallback.h>
#include <aidl/android/vendor/powerstats/BnPixelPowerStatsProvider.h>
#include <android/binder_manager.h>

using ::aidl::android::vendor::powerstats::BnPixelPowerStatsProvider;
using ::aidl::android::vendor::powerstats::IPixelPowerStatsCallback;

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

class PixelPowerStatsDataProvider : public PowerStats::IStateResidencyDataProvider {
  public:
    PixelPowerStatsDataProvider();
    ~PixelPowerStatsDataProvider() = default;
    void addEntity(std::string name, std::vector<State> states);
    void start();

    // Methods from PowerStats::IStateResidencyDataProvider
    bool getStateResidencies(
            std::unordered_map<std::string, std::vector<StateResidency>> *residencies) override;
    std::unordered_map<std::string, std::vector<State>> getInfo() override;

  private:
    class ProviderService : public BnPixelPowerStatsProvider {
      public:
        ProviderService(PixelPowerStatsDataProvider *enclosed) : mEnclosed(enclosed) {}
        // Methods from BnPixelPowerStatsProvider
        ::ndk::ScopedAStatus registerCallback(
                const std::string &in_entityName,
                const std::shared_ptr<IPixelPowerStatsCallback> &in_cb) override {
            return mEnclosed->registerCallback(in_entityName, in_cb);
        }

        ::ndk::ScopedAStatus registerCallbackByStates(
                const std::string &in_entityName,
                const std::shared_ptr<IPixelPowerStatsCallback> &in_cb,
                const std::vector<State> &in_states) override {
            return mEnclosed->registerCallbackByStates(in_entityName, in_cb, in_states);
        }

        ::ndk::ScopedAStatus unregisterCallback(
                const std::shared_ptr<IPixelPowerStatsCallback> &in_cb) override {
            return mEnclosed->unregisterCallback(in_cb);
        }

      private:
        PixelPowerStatsDataProvider *mEnclosed;
    };

    struct Entry {
        Entry(std::string name, std::vector<State> states)
            : mName(name), mStates(states), mCallback(nullptr) {}
        std::string mName;
        std::vector<State> mStates;
        std::shared_ptr<IPixelPowerStatsCallback> mCallback;
    };

    void registerStatesUpdateCallback(
            std::function<void(const std::string &, const std::vector<State> &)>
                    statesUpdateCallback) override;

    ::ndk::ScopedAStatus registerCallback(const std::string &in_entityName,
                                          const std::shared_ptr<IPixelPowerStatsCallback> &in_cb);

    ::ndk::ScopedAStatus registerCallbackByStates(
            const std::string &in_entityName,
            const std::shared_ptr<IPixelPowerStatsCallback> &in_cb,
            const std::vector<State> &in_states);

    ::ndk::ScopedAStatus unregisterCallback(const std::shared_ptr<IPixelPowerStatsCallback> &in_cb);

    ::ndk::ScopedAStatus getStateResidenciesTimed(const Entry &entry,
                                                  std::vector<StateResidency> *residency);

    const std::string kInstance = "power.stats-vendor";
    std::mutex mLock;
    std::shared_ptr<ProviderService> mProviderService;
    std::vector<Entry> mEntries;
    std::function<void(const std::string &, const std::vector<State> &)> mStatesUpdateCallback;
};

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
