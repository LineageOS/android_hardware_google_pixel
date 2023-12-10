/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <battery_mitigation/BatteryMitigation.h>

#include <sstream>

#define MAX_BROWNOUT_DATA_AGE_MINUTES 5
#define ONE_SECOND_IN_US 1000000

namespace android {
namespace hardware {
namespace google {
namespace pixel {

BatteryMitigation::BatteryMitigation(const struct MitigationConfig::Config &cfg) {
        mThermalMgr = &MitigationThermalManager::getInstance();
        mThermalMgr->updateConfig(cfg);
}

bool BatteryMitigation::isMitigationLogTimeValid(std::chrono::system_clock::time_point startTime,
                                                 const char *const logFilePath,
                                                 const char *const timestampFormat,
                                                 const std::regex pattern) {
    std::string logFile;
    if (!android::base::ReadFileToString(logFilePath, &logFile)) {
        return false;
    }
    std::istringstream content(logFile);
    std::string line;
    int counter = 0;
    std::smatch pattern_match;
    while (std::getline(content, line)) {
        if (std::regex_match(line, pattern_match, pattern)) {
            std::tm triggeredTimestamp = {};
            std::istringstream ss(pattern_match.str());
            ss >> std::get_time(&triggeredTimestamp, timestampFormat);
            auto logFileTime = std::chrono::system_clock::from_time_t(mktime(&triggeredTimestamp));
            auto epoch_logFileTime = logFileTime.time_since_epoch().count() / ONE_SECOND_IN_US;

            // Convert start time to same format
            auto time_sec = std::chrono::system_clock::to_time_t(startTime);
            struct tm start_tm;
            std::stringstream oss;
            localtime_r(&time_sec, &start_tm);
            oss << std::put_time(&start_tm, timestampFormat) << std::flush;
            std::tm startTimestamp = {};
            std::istringstream st(oss.str());
            st >> std::get_time(&startTimestamp, timestampFormat);
            auto start = std::chrono::system_clock::from_time_t(mktime(&startTimestamp));
            auto epoch_startTime = start.time_since_epoch().count() / ONE_SECOND_IN_US;

            auto delta = epoch_startTime - epoch_logFileTime;
            auto delta_minutes = delta / 60;

            if ((delta_minutes < MAX_BROWNOUT_DATA_AGE_MINUTES) && (delta_minutes >= 0)) {
                return true;
            }
        }
        counter += 1;
        if (counter > 5) {
            break;
        }
    }
    return false;
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
