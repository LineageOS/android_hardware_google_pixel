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

#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <hidl/HidlTransportSupport.h>

#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

constexpr char kThermalSensorsRoot[] = "/sys/devices/virtual/thermal";
constexpr char kCpuOnlineRoot[] = "/sys/devices/system/cpu";
constexpr char kCpuUsageFile[] = "/proc/stat";
constexpr char kCpuOnlineFileSuffix[] = "online";
constexpr char kCpuPresentFile[] = "/sys/devices/system/cpu/present";

namespace {
using android::base::StringPrintf;

// Pixel don't offline CPU, see std::thread::hardware_concurrency(); should work.
// However /sys/devices/system/cpu/present is preferred.
// The file is expected to contain single text line with two numbers %d-%d,
// which is a range of available cpu numbers, e.g. 0-7 would mean there
// are 8 cores number from 0 to 7.
// For Android systems this approach is safer than using cpufeatures, see bug
// b/36941727.
std::size_t getNumberOfCores() {
    std::string file;
    if (!android::base::ReadFileToString(kCpuPresentFile, &file)) {
        LOG(ERROR) << "Error reading Cpu present file: " << kCpuPresentFile;
        return 0;
    }
    std::vector<std::string> pieces = android::base::Split(file, "-");
    if (pieces.size() != 2) {
        LOG(ERROR) << "Error parsing Cpu present file content: " << file;
        return 0;
    }
    auto min_core = std::stoul(pieces[0]);
    auto max_core = std::stoul(pieces[1]);
    if (max_core < min_core) {
        LOG(ERROR) << "Error parsing Cpu present min and max: " << min_core << " - " << max_core;
        return 0;
    }
    return static_cast<std::size_t>(max_core - min_core + 1);
}
const std::size_t kMaxCpus = getNumberOfCores();

void parseCpuUsagesFileAndAssignUsages(hidl_vec<CpuUsage> *cpu_usages) {
    uint64_t cpu_num, user, nice, system, idle;
    std::string cpu_name;
    std::string data;
    if (!android::base::ReadFileToString(kCpuUsageFile, &data)) {
        LOG(ERROR) << "Error reading Cpu usage file: " << kCpuUsageFile;
        return;
    }

    std::istringstream stat_data(data);
    std::string line;
    while (std::getline(stat_data, line)) {
        if (line.find("cpu") == 0 && isdigit(line[3])) {
            // Split the string using spaces.
            std::vector<std::string> words = android::base::Split(line, " ");
            cpu_name = words[0];
            cpu_num = std::stoi(cpu_name.substr(3));

            if (cpu_num < kMaxCpus) {
                user = std::stoi(words[1]);
                nice = std::stoi(words[2]);
                system = std::stoi(words[3]);
                idle = std::stoi(words[4]);

                // Check if the CPU is online by reading the online file.
                std::string cpu_online_path = StringPrintf("%s/%s/%s", kCpuOnlineRoot,
                                                           cpu_name.c_str(), kCpuOnlineFileSuffix);
                std::string is_online;
                if (!android::base::ReadFileToString(cpu_online_path, &is_online)) {
                    LOG(ERROR) << "Could not open Cpu online file: " << cpu_online_path;
                    return;
                }
                is_online = android::base::Trim(is_online);

                (*cpu_usages)[cpu_num].name = cpu_name;
                (*cpu_usages)[cpu_num].active = user + nice + system;
                (*cpu_usages)[cpu_num].total = user + nice + system + idle;
                (*cpu_usages)[cpu_num].isOnline = (is_online == "1") ? true : false;
            } else {
                LOG(ERROR) << "Unexpected cpu number: " << words[0];
                return;
            }
        }
    }
}

}  // namespace

/*
 * Populate the sensor_name_to_file_map_ map by walking through the file tree,
 * reading the type file and assigning the temp file path to the map.  If we do
 * not succeed, abort.
 */
ThermalHelper::ThermalHelper(NotificationCallback cb)
    : thermal_watcher_(new ThermalWatcher()),
      cb_(std::move(cb)),
      cooling_device_info_map_(ParseCoolingDevice(
          "/vendor/etc/" +
          android::base::GetProperty("vendor.thermal.config", "thermal_info_config.json"))),
      sensor_info_map_(ParseSensorInfo(
          "/vendor/etc/" +
          android::base::GetProperty("vendor.thermal.config", "thermal_info_config.json"))) {
    for (auto const &name_status_pair : sensor_info_map_) {
        sensor_status_map_[name_status_pair.first] = {
            .severity = ThrottlingSeverity::NONE,
            .prev_hot_severity = ThrottlingSeverity::NONE,
            .prev_cold_severity = ThrottlingSeverity::NONE,
        };
    }

    is_initialized_ = initializeSensorMap() && initializeCoolingDevices();
    if (!is_initialized_) {
        LOG(FATAL) << "ThermalHAL could not be initialized properly.";
    }
    std::vector<std::string> paths;
    for (const auto &entry : cooling_device_info_map_) {
        std::string path = cooling_devices_.getCoolingDevicePath(entry.first);
        if (!path.empty()) {
            paths.push_back(path + "/cur_state");
        }
    }
    thermal_watcher_->registerFilesToWatch(paths);
    thermal_watcher_->registerCallback(std::bind(&ThermalHelper::thermalWatcherCallbackFunc, this,
                                                 std::placeholders::_1, std::placeholders::_2));
    // Need start watching after status map initialized
    is_initialized_ = thermal_watcher_->startWatchingDeviceFiles();
    if (!is_initialized_) {
        LOG(FATAL) << "ThermalHAL could not start watching thread properly.";
    }
}

std::vector<std::string> ThermalHelper::getCoolingDevicePaths() const {
    std::vector<std::string> paths;
    for (const auto &entry : cooling_device_info_map_) {
        std::string path = cooling_devices_.getCoolingDevicePath(entry.first);
        if (!path.empty()) {
            paths.push_back(path + "/cur_state");
        }
    }
    return paths;
}

bool ThermalHelper::readCoolingDevice(const std::string &cooling_device,
                                      CoolingDevice_2_0 *out) const {
    // Read the file.  If the file can't be read temp will be empty string.
    int data;
    std::string path;

    if (!cooling_devices_.getCoolingDeviceState(cooling_device, &data)) {
        LOG(ERROR) << "readCoolingDevice: failed to read cooling_device: " << cooling_device;
        return false;
    }

    const CoolingType &type = cooling_device_info_map_.at(cooling_device);

    out->type = type;
    out->name = cooling_device;
    out->value = data;

    return true;
}

bool ThermalHelper::readTemperature(const std::string &sensor_name, Temperature_1_0 *out) const {
    // Read the file.  If the file can't be read temp will be empty string.
    std::string temp;
    std::string path;

    if (!thermal_sensors_.readSensorFile(sensor_name, &temp, &path)) {
        LOG(ERROR) << "readTemperature: sensor not found: " << sensor_name;
        return false;
    }

    if (temp.empty() && !path.empty()) {
        LOG(ERROR) << "readTemperature: failed to open file: " << path;
        return false;
    }

    const SensorInfo &sensor_info = sensor_info_map_.at(sensor_name);
    TemperatureType_1_0 type =
        (static_cast<int>(sensor_info.type) > static_cast<int>(TemperatureType_1_0::SKIN))
            ? TemperatureType_1_0::UNKNOWN
            : static_cast<TemperatureType_1_0>(sensor_info.type);
    out->type = type;
    out->name = sensor_name;
    out->currentValue = std::stof(temp) * sensor_info.multiplier;
    out->throttlingThreshold =
        sensor_info.hot_thresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
    out->shutdownThreshold =
        sensor_info.hot_thresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)];
    out->vrThrottlingThreshold = sensor_info.vr_threshold;

    return true;
}

bool ThermalHelper::readTemperature(
    const std::string &sensor_name, Temperature_2_0 *out,
    std::pair<ThrottlingSeverity, ThrottlingSeverity> *throtting_status) const {
    // Read the file.  If the file can't be read temp will be empty string.
    std::string temp;
    std::string path;

    if (!thermal_sensors_.readSensorFile(sensor_name, &temp, &path)) {
        LOG(ERROR) << "readTemperature: sensor not found: " << sensor_name;
        return false;
    }

    if (temp.empty() && !path.empty()) {
        LOG(ERROR) << "readTemperature: failed to open file: " << path;
        return false;
    }

    const auto &sensor_info = sensor_info_map_.at(sensor_name);
    out->type = sensor_info.type;
    out->name = sensor_name;
    out->value = std::stof(temp) * sensor_info.multiplier;

    ThrottlingSeverity prev_hot_severity, prev_cold_severity;
    {
        // reader lock, readTemperature will be called in Binder call and the watcher thread.
        std::shared_lock<std::shared_mutex> _lock(sensor_status_map_mutex_);
        prev_hot_severity = sensor_status_map_.at(sensor_name).prev_hot_severity;
        prev_cold_severity = sensor_status_map_.at(sensor_name).prev_cold_severity;
    }

    std::pair<ThrottlingSeverity, ThrottlingSeverity> status = getSeverityFromThresholds(
        sensor_info.hot_thresholds, sensor_info.cold_thresholds, sensor_info.hot_hysteresis,
        sensor_info.cold_hysteresis, prev_hot_severity, prev_cold_severity, out->value);

    out->throttlingStatus = static_cast<size_t>(status.first) > static_cast<size_t>(status.second)
                                ? status.first
                                : status.second;
    if (throtting_status) {
        *throtting_status = status;
    }
    return true;
}

bool ThermalHelper::readTemperatureThreshold(const std::string &sensor_name,
                                             TemperatureThreshold *out) const {
    // Read the file.  If the file can't be read temp will be empty string.
    std::string temp;
    std::string path;

    if (!sensor_info_map_.count(sensor_name)) {
        LOG(ERROR) << __func__ << ": sensor not found: " << sensor_name;
        return false;
    }

    const auto &sensor_info = sensor_info_map_.at(sensor_name);

    out->type = sensor_info.type;
    out->name = sensor_name;
    out->hotThrottlingThresholds = sensor_info.hot_thresholds;
    out->coldThrottlingThresholds = sensor_info.cold_thresholds;
    out->vrThrottlingThreshold = sensor_info.vr_threshold;
    return true;
}

std::pair<ThrottlingSeverity, ThrottlingSeverity> ThermalHelper::getSeverityFromThresholds(
    const ThrottlingArray &hot_thresholds, const ThrottlingArray &cold_thresholds,
    const ThrottlingArray &hot_hysteresis, const ThrottlingArray &cold_hysteresis,
    ThrottlingSeverity prev_hot_severity, ThrottlingSeverity prev_cold_severity,
    float value) const {
    ThrottlingSeverity ret_hot = ThrottlingSeverity::NONE;
    ThrottlingSeverity ret_hot_hysteresis = ThrottlingSeverity::NONE;
    ThrottlingSeverity ret_cold = ThrottlingSeverity::NONE;
    ThrottlingSeverity ret_cold_hysteresis = ThrottlingSeverity::NONE;

    // Here we want to control the iteration from high to low, and hidl_enum_range doesn't support
    // a reverse iterator yet.
    for (size_t i = static_cast<size_t>(ThrottlingSeverity::SHUTDOWN);
         i > static_cast<size_t>(ThrottlingSeverity::NONE); --i) {
        if (!std::isnan(hot_thresholds[i]) && hot_thresholds[i] <= value &&
            ret_hot == ThrottlingSeverity::NONE) {
            ret_hot = static_cast<ThrottlingSeverity>(i);
        }
        if (!std::isnan(hot_thresholds[i]) && (hot_thresholds[i] - hot_hysteresis[i]) < value &&
            ret_hot_hysteresis == ThrottlingSeverity::NONE) {
            ret_hot_hysteresis = static_cast<ThrottlingSeverity>(i);
        }
        if (!std::isnan(cold_thresholds[i]) && cold_thresholds[i] >= value &&
            ret_cold == ThrottlingSeverity::NONE) {
            ret_cold = static_cast<ThrottlingSeverity>(i);
        }
        if (!std::isnan(cold_thresholds[i]) && (cold_thresholds[i] + cold_hysteresis[i]) > value &&
            ret_cold_hysteresis == ThrottlingSeverity::NONE) {
            ret_cold_hysteresis = static_cast<ThrottlingSeverity>(i);
        }
    }
    if (static_cast<size_t>(ret_hot) < static_cast<size_t>(prev_hot_severity)) {
        ret_hot = ret_hot_hysteresis;
    }
    if (static_cast<size_t>(ret_cold) < static_cast<size_t>(prev_cold_severity)) {
        ret_cold = ret_cold_hysteresis;
    }

    return std::make_pair(ret_hot, ret_cold);
}

bool ThermalHelper::initializeSensorMap() {
    for (const auto &e : sensor_info_map_) {
        std::string sensor_name = e.first;
        std::string sensor_temp_path =
            StringPrintf("%s/tz-by-name/%s/temp", kThermalSensorsRoot, sensor_name.c_str());
        if (!thermal_sensors_.addSensor(sensor_name, sensor_temp_path)) {
            LOG(ERROR) << "Could not add " << sensor_name << "to sensors map";
        }
    }
    if (sensor_info_map_.size() == thermal_sensors_.getNumSensors()) {
        return true;
    }
    return false;
}

bool ThermalHelper::initializeCoolingDevices() {
    for (const auto &cooling_device_info : cooling_device_info_map_) {
        std::string cooling_device_name = cooling_device_info.first;
        std::string cooling_device_path =
            StringPrintf("%s/cdev-by-name/%s", kThermalSensorsRoot, cooling_device_name.c_str());

        if (!cooling_devices_.addCoolingDevice(cooling_device_name, cooling_device_path)) {
            LOG(ERROR) << "Could not add " << cooling_device_name << "to cooling device map";
            continue;
        }
    }

    if (cooling_device_info_map_.size() == cooling_devices_.getNumCoolingDevices()) {
        return true;
    }
    return false;
}

bool ThermalHelper::fillTemperatures(hidl_vec<Temperature_1_0> *temperatures) const {
    temperatures->resize(sensor_info_map_.size());
    int current_index = 0;
    for (const auto &name_info_pair : sensor_info_map_) {
        Temperature_1_0 temp;

        if (readTemperature(name_info_pair.first, &temp)) {
            (*temperatures)[current_index] = temp;
        } else {
            LOG(ERROR) << __func__
                       << ": error reading temperature for sensor: " << name_info_pair.first;
            return false;
        }
        ++current_index;
    }
    return current_index > 0;
}

bool ThermalHelper::fillCurrentTemperatures(bool filterType, TemperatureType_2_0 type,
                                            hidl_vec<Temperature_2_0> *temperatures) const {
    std::vector<Temperature_2_0> ret;
    for (const auto &name_info_pair : sensor_info_map_) {
        Temperature_2_0 temp;
        if (filterType && name_info_pair.second.type != type) {
            continue;
        }
        if (readTemperature(name_info_pair.first, &temp)) {
            ret.emplace_back(std::move(temp));
        } else {
            LOG(ERROR) << __func__
                       << ": error reading temperature for sensor: " << name_info_pair.first;
            return false;
        }
    }
    *temperatures = ret;
    return ret.size() > 0;
}

bool ThermalHelper::fillTemperatureThresholds(bool filterType, TemperatureType_2_0 type,
                                              hidl_vec<TemperatureThreshold> *thresholds) const {
    std::vector<TemperatureThreshold> ret;
    for (const auto &name_info_pair : sensor_info_map_) {
        TemperatureThreshold temp;
        if (filterType && name_info_pair.second.type != type) {
            continue;
        }
        if (readTemperatureThreshold(name_info_pair.first, &temp)) {
            ret.emplace_back(std::move(temp));
        } else {
            LOG(ERROR) << __func__ << ": error reading temperature threshold for sensor: "
                       << name_info_pair.first;
            return false;
        }
    }
    *thresholds = ret;
    return ret.size() > 0;
}

bool ThermalHelper::fillCurrentCoolingDevices(bool filterType, CoolingType type,
                                              hidl_vec<CoolingDevice_2_0> *cooling_devices) const {
    std::vector<CoolingDevice_2_0> ret;
    for (const auto &name_info_pair : cooling_device_info_map_) {
        CoolingDevice_2_0 value;
        if (filterType && name_info_pair.second != type) {
            continue;
        }
        if (readCoolingDevice(name_info_pair.first, &value)) {
            ret.emplace_back(std::move(value));
        } else {
            LOG(ERROR) << __func__ << ": error reading cooling device: " << name_info_pair.first;
            return false;
        }
    }
    *cooling_devices = ret;
    return ret.size() > 0;
}

bool ThermalHelper::fillCpuUsages(hidl_vec<CpuUsage> *cpu_usages) const {
    cpu_usages->resize(kMaxCpus);
    parseCpuUsagesFileAndAssignUsages(cpu_usages);
    return true;
}

// This is called in the different thread context and will update sensor_status
void ThermalHelper::thermalWatcherCallbackFunc(const std::string &, const int) {
    std::vector<Temperature_2_0> temps;
    for (auto &name_status_pair : sensor_status_map_) {
        Temperature_2_0 temp;
        TemperatureThreshold threshold;
        SensorStatus &sensor_status = name_status_pair.second;
        const SensorInfo &sensor_info = sensor_info_map_.at(name_status_pair.first);
        // Only send notification on whitelisted sensors
        if (!sensor_info.is_monitor) {
            continue;
        }

        std::pair<ThrottlingSeverity, ThrottlingSeverity> throtting_status;
        if (!readTemperature(name_status_pair.first, &temp, &throtting_status)) {
            LOG(ERROR) << __func__
                       << ": error reading temperature for sensor: " << name_status_pair.first;
            continue;
        }
        if (!readTemperatureThreshold(name_status_pair.first, &threshold)) {
            LOG(ERROR) << __func__ << ": error reading temperature threshold for sensor: "
                       << name_status_pair.first;
            continue;
        }

        {
            // writer lock
            std::unique_lock<std::shared_mutex> _lock(sensor_status_map_mutex_);
            if (throtting_status.first != sensor_status.prev_hot_severity) {
                sensor_status.prev_hot_severity = throtting_status.first;
            }
            if (throtting_status.second != sensor_status.prev_cold_severity) {
                sensor_status.prev_cold_severity = throtting_status.second;
            }
            if (temp.throttlingStatus != sensor_status.severity) {
                temps.push_back(temp);
                sensor_status.severity = temp.throttlingStatus;
            }
        }
    }
    if (!temps.empty() && cb_) {
        cb_(temps);
    }
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
