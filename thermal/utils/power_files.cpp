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
#include <dirent.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include "power_files.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

constexpr std::string_view kDeviceType("iio:device");
constexpr std::string_view kIioRootDir("/sys/bus/iio/devices");
constexpr std::string_view kEnergyValueNode("energy_value");

using android::base::ReadFileToString;
using android::base::StringPrintf;

void PowerFiles::setPowerDataToDefault(std::string_view sensor_name) {
    if (!throttling_release_map_.count(sensor_name.data())) {
        return;
    }

    auto &cdev_release_map = throttling_release_map_.at(sensor_name.data());
    PowerSample power_sample = {};

    for (auto &cdev_release_pair : cdev_release_map) {
        auto power_history_size = cdev_release_pair.second.power_history.size();
        for (size_t i = 0; i < power_history_size; ++i) {
            cdev_release_pair.second.power_history.pop();
            cdev_release_pair.second.power_history.emplace(power_sample);
        }
        cdev_release_pair.second.release_step = 0;
    }
}

unsigned int PowerFiles::getReleaseStep(std::string_view sensor_name, std::string_view power_rail) {
    unsigned int release_step = 0;

    if (throttling_release_map_.count(sensor_name.data()) &&
        throttling_release_map_[sensor_name.data()].count(power_rail.data())) {
        release_step = throttling_release_map_[sensor_name.data()][power_rail.data()].release_step;
    }

    return release_step;
}

bool PowerFiles::registerPowerRailsToWatch(std::string_view sensor_name,
                                           std::string_view power_rail,
                                           const BindedCdevInfo &binded_cdev_info) {
    std::queue<PowerSample> power_history;
    PowerSample power_sample = {
            .energy_counter = 0,
            .duration = 0,
    };

    if (!energy_info_map_.size() && !updateEnergyValues()) {
        LOG(ERROR) << "Faield to update energy info";
        return false;
    }

    for (int i = 0; i < binded_cdev_info.power_sample_count; i++) {
        if (energy_info_map_.count(power_rail.data())) {
            power_history.emplace(power_sample);
        }
    }

    if (energy_info_map_.count(power_rail.data())) {
        throttling_release_map_[sensor_name.data()][power_rail.data()] = {
                .power_history = power_history,
                .release_step = 0,
                .time_remaining = binded_cdev_info.power_sample_delay,
        };
    } else {
        return false;
    }

    return true;
}

bool PowerFiles::findEnergySourceToWatch(void) {
    std::string devicePath;

    if (energy_path_set_.size()) {
        return true;
    }

    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(kIioRootDir.data()), closedir);
    if (!dir) {
        PLOG(ERROR) << "Error opening directory" << kIioRootDir;
        return false;
    }

    // Find any iio:devices that support energy_value
    while (struct dirent *ent = readdir(dir.get())) {
        std::string devTypeDir = ent->d_name;
        if (devTypeDir.find(kDeviceType) != std::string::npos) {
            devicePath = StringPrintf("%s/%s", kIioRootDir.data(), devTypeDir.data());
            std::string deviceEnergyContent;

            if (!ReadFileToString(StringPrintf("%s/%s", devicePath.data(), kEnergyValueNode.data()),
                                  &deviceEnergyContent)) {
            } else if (deviceEnergyContent.size()) {
                energy_path_set_.emplace(
                        StringPrintf("%s/%s", devicePath.data(), kEnergyValueNode.data()));
            }
        }
    }

    if (!energy_path_set_.size()) {
        return false;
    }

    return true;
}

void PowerFiles::clearEnergyInfoMap(void) {
    energy_info_map_.clear();
}

bool PowerFiles::updateEnergyValues(void) {
    std::string deviceEnergyContent;
    std::string deviceEnergyContents;
    std::string line;

    for (const auto &path : energy_path_set_) {
        if (!android::base::ReadFileToString(path, &deviceEnergyContent)) {
            LOG(ERROR) << "Failed to read energy content from " << path;
            return false;
        } else {
            deviceEnergyContents.append(deviceEnergyContent);
        }
    }

    std::istringstream energyData(deviceEnergyContents);

    clearEnergyInfoMap();
    while (std::getline(energyData, line)) {
        /* Read rail energy */
        uint64_t energy_counter = 0;
        uint64_t duration = 0;

        /* Format example: CH3(T=358356)[S2M_VDD_CPUCL2], 761330 */
        auto start_pos = line.find("T=");
        auto end_pos = line.find(')');
        if (start_pos != std::string::npos) {
            duration =
                    strtoul(line.substr(start_pos + 2, end_pos - start_pos - 2).c_str(), NULL, 10);
        } else {
            continue;
        }

        start_pos = line.find(")[");
        end_pos = line.find(']');
        std::string railName;
        if (start_pos != std::string::npos) {
            railName = line.substr(start_pos + 2, end_pos - start_pos - 2);
        } else {
            continue;
        }

        start_pos = line.find("],");
        if (start_pos != std::string::npos) {
            energy_counter = strtoul(line.substr(start_pos + 2).c_str(), NULL, 10);
        } else {
            continue;
        }

        energy_info_map_[railName] = {
                .energy_counter = energy_counter,
                .duration = duration,
        };
    }

    return true;
}

void PowerFiles::throttlingReleaseUpdate(std::string_view sensor_name,
                                         const ThrottlingSeverity severity,
                                         const std::chrono::milliseconds time_elapsed_ms,
                                         const BindedCdevInfo &binded_cdev_info,
                                         std::string_view power_rail) {
    if (!throttling_release_map_.count(sensor_name.data()) ||
        !throttling_release_map_[sensor_name.data()].count(power_rail.data())) {
        return;
    }

    auto &cdev_release_status = throttling_release_map_[sensor_name.data()].at(power_rail.data());

    if (time_elapsed_ms > cdev_release_status.time_remaining) {
        cdev_release_status.time_remaining = binded_cdev_info.power_sample_delay;
    } else {
        cdev_release_status.time_remaining = cdev_release_status.time_remaining - time_elapsed_ms;
        LOG(VERBOSE) << "Power rail " << power_rail
                     << " : timeout remaining = " << cdev_release_status.time_remaining.count();
        return;
    }

    if (!energy_info_map_.size() && !updateEnergyValues()) {
        LOG(ERROR) << "Failed to update energy values";
        cdev_release_status.release_step = 0;
        return;
    }

    // Cannot find the power energy value, so we do not release the throttling
    if (!energy_info_map_.count(power_rail.data())) {
        LOG(ERROR) << "Cannot find the power energy value";
        cdev_release_status.release_step = 0;
        return;
    }

    const auto last_sample = cdev_release_status.power_history.front();
    const auto curr_sample = energy_info_map_.at(power_rail.data());

    const auto duration = curr_sample.duration - last_sample.duration;
    const auto deltaEnergy = curr_sample.energy_counter - last_sample.energy_counter;
    const auto avg_power = deltaEnergy / duration;

    cdev_release_status.power_history.pop();
    cdev_release_status.power_history.push(curr_sample);

    bool is_over_budget = true;
    if (!last_sample.duration) {
        LOG(VERBOSE) << "Power rail " << power_rail.data() << ": the last energy timestamp is zero";
    } else if (duration <= 0 || deltaEnergy < 0) {
        LOG(ERROR) << "Power rail " << power_rail.data() << " is invalid: duration = " << duration
                   << ", deltaEnergy = " << deltaEnergy;
    } else {
        if (!binded_cdev_info.power_reversly_check) {
            if (avg_power < binded_cdev_info.power_thresholds[static_cast<int>(severity)]) {
                is_over_budget = false;
            }
        } else {
            if (avg_power > binded_cdev_info.power_thresholds[static_cast<int>(severity)]) {
                is_over_budget = false;
            }
        }
        LOG(VERBOSE) << "Power rail " << power_rail << ": power threshold = "
                     << binded_cdev_info.power_thresholds[static_cast<int>(severity)]
                     << ", avg power = " << avg_power << ", duration = " << duration
                     << ", deltaEnergy = " << deltaEnergy;
    }

    switch (binded_cdev_info.release_logic) {
        case ReleaseLogic::DECREASE:
            if (!is_over_budget) {
                if (cdev_release_status.release_step < std::numeric_limits<int>::max()) {
                    cdev_release_status.release_step++;
                }
            } else {
                cdev_release_status.release_step = 0;
            }
            break;
        case ReleaseLogic::BYPASS:
            cdev_release_status.release_step = is_over_budget ? 0 : std::numeric_limits<int>::max();
            break;
        case ReleaseLogic::NONE:
        default:
            break;
    }
    return;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
