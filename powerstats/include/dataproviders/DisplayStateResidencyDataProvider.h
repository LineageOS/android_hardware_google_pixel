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

#include <utils/Looper.h>
#include <utils/Thread.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace aidl {
namespace android {
namespace hardware {
namespace powerstats {

class DisplayStateResidencyDataProvider : public PowerStats::IStateResidencyDataProvider,
                                          public ::android::Thread {
  public:
    // name = powerEntityName to be associated with this data provider
    // path = path to the display state file descriptor
    // state = list of states to be tracked
    DisplayStateResidencyDataProvider(std::string name, std::string path,
                                      std::vector<std::string> states);
    ~DisplayStateResidencyDataProvider();

    // Methods from PowerStats::IStateResidencyDataProvider
    bool getResults(std::unordered_map<std::string, std::vector<PowerEntityStateResidencyData>>
                            *results) override;
    std::unordered_map<std::string, std::vector<PowerEntityStateInfo>> getInfo() override;

  private:
    // Method associated with ::android::Thread. Poll for display state changes
    bool threadLoop() override;
    // Main function to update the stats when display state change is detected
    void updateStats();

    // File descriptor of display state
    int mFd;
    // Path to display state file descriptor
    const std::string mPath;
    // Power Entity name associated with this data provider
    const std::string mName;
    // List of states to track indexed by mCurState
    std::vector<std::string> mStates;
    // Lock to protect concurrent read/write to mResidencies and mCurState
    std::mutex mLock;
    // Accumulated display state stats indexed by mCurState
    std::vector<PowerEntityStateResidencyData> mResidencies;
    // Index of current state
    int mCurState;
    // Looper to facilitate polling of display state file desciptor
    sp<::android::Looper> mLooper;
};

}  // namespace powerstats
}  // namespace hardware
}  // namespace android
}  // namespace aidl
