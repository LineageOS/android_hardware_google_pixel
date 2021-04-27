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

#pragma once

#include <string>
#include <unordered_map>

#include <android/hardware/thermal/2.0/IThermal.h>

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_enum_range;
using CoolingType_2_0 = ::android::hardware::thermal::V2_0::CoolingType;
using TemperatureType_2_0 = ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;
constexpr size_t kThrottlingSeverityCount = std::distance(
    hidl_enum_range<ThrottlingSeverity>().begin(), hidl_enum_range<ThrottlingSeverity>().end());
using ThrottlingArray = std::array<float, static_cast<size_t>(kThrottlingSeverityCount)>;
constexpr std::chrono::milliseconds kMinPollIntervalMs = std::chrono::milliseconds(2000);
constexpr std::chrono::milliseconds kUeventPollTimeoutMs = std::chrono::milliseconds(300000);

enum FormulaOption : uint32_t {
    COUNT_THRESHOLD = 0,
    WEIGHTED_AVG,
    MAXIMUM,
    MINIMUM,
};

struct VirtualSensorInfo {
    std::vector<std::string> linked_sensors;
    std::vector<float> coefficients;
    float offset;
    std::string trigger_sensor;
    FormulaOption formula;
};

// The method when the ODPM power is lower than threshold
enum ReleaseLogic : uint32_t {
    DECREASE = 0,  // DECREASE THROTTLING
    BYPASS,        // BYPASS THROTTLING
    NONE,
};

struct BindedCdevInfo {
    ThrottlingArray limit_info;
    ThrottlingArray power_thresholds;
    ReleaseLogic release_logic;
    float cdev_weight;
    int cdev_ceiling;
    int power_sample_count;
    std::chrono::milliseconds power_sample_delay;
    bool power_reversly_check;
};

struct ThrottlingInfo {
    ThrottlingArray k_po;
    ThrottlingArray k_pu;
    ThrottlingArray k_i;
    ThrottlingArray k_d;
    ThrottlingArray i_max;
    ThrottlingArray max_alloc_power;
    ThrottlingArray min_alloc_power;
    ThrottlingArray s_power;
    ThrottlingArray i_cutoff;
    std::unordered_map<std::string, BindedCdevInfo> binded_cdev_info_map;
};

struct SensorInfo {
    TemperatureType_2_0 type;
    ThrottlingArray hot_thresholds;
    ThrottlingArray cold_thresholds;
    ThrottlingArray hot_hysteresis;
    ThrottlingArray cold_hysteresis;
    float vr_threshold;
    float multiplier;
    std::chrono::milliseconds polling_delay;
    std::chrono::milliseconds passive_delay;
    bool send_cb;
    bool send_powerhint;
    bool power_tracking_enabled;
    bool is_monitor;
    std::unique_ptr<VirtualSensorInfo> virtual_sensor_info;
    std::unique_ptr<ThrottlingInfo> throttling_info;
};

struct CdevInfo {
    CoolingType_2_0 type;
    std::string read_path;
    std::string write_path;
    std::vector<float> state2power;
    std::string power_rail;
    std::chrono::milliseconds power_sample_rate;
    int power_sample_count;
    unsigned int max_state;
};

std::unordered_map<std::string, SensorInfo> ParseSensorInfo(std::string_view config_path);
std::unordered_map<std::string, CdevInfo> ParseCoolingDevice(std::string_view config_path);

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
