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

#define LOG_TAG "android.hardware.power.stats-service.pixel"

#include <dataproviders/PowerStatsEnergyAttribution.h>

#include <android-base/logging.h>
#include <android-base/strings.h>

#include <utility>

using android::base::Split;
using android::base::Trim;

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

bool PowerStatsEnergyAttribution::readUidTimeInState(AttributionStats *attrStats,
                                                     std::string path) {
    std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(path.c_str(), "r"), fclose);
    if (!fp) {
        PLOG(ERROR) <<  __func__ << ":Failed to open file " << path;
        return false;
    }

    char *buf = nullptr;
    size_t size = 0;
    getline(&buf, &size, fp.get());
    attrStats->uidTimeInStateNames = Split(Trim(buf), " ");
    // first element will be "uid:" and it's useless
    attrStats->uidTimeInStateNames.erase(attrStats->uidTimeInStateNames.begin());

    while(getline(&buf, &size, fp.get()) != -1) {
        std::vector<std::string> uidInfos = Split(Trim(buf), " ");
        uidInfos[0].pop_back();
        int32_t uid = std::stoi(uidInfos[0]);

        std::vector<long> uidStats;
        // first element will be uid number so skipping it
        for (auto it = ++uidInfos.begin(); it != uidInfos.end(); ++it) {
            uidStats.push_back(std::stol(*it));
        }
        attrStats->uidTimeInStats.emplace(uid, uidStats);
    }
    free(buf);

    return true;
}

AttributionStats PowerStatsEnergyAttribution::getAttributionStats(
                                                                  std::unordered_map<int32_t,
                                                                  std::string> paths) {
    AttributionStats attrStats;

    if (paths.count(UID_TIME_IN_STATE) &&
        !readUidTimeInState(&attrStats, paths.at(UID_TIME_IN_STATE))) {
            PLOG(ERROR) << ":Failed to read uid_time_in_state";
    }

    return attrStats;
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
