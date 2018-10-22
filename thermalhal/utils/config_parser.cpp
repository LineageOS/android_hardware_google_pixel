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
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <cmath>
#include <set>

#include <json/reader.h>
#include <json/value.h>

#include "config_parser.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_enum_range;
using ::android::hardware::thermal::V2_0::toString;
using TemperatureType_2_0 = ::android::hardware::thermal::V2_0::TemperatureType;

namespace {

template <typename T>
// Return false when failed parsing
bool getTypeFromString(const std::string &str, T *out) {
    auto types = hidl_enum_range<T>();
    for (auto type : types) {
        if (toString(type) == str) {
            *out = type;
            return true;
        }
    }
    return false;
}
}  // namespace

std::map<std::string, SensorInfo> ParseSensorInfo(const std::string &config_path) {
    std::string json_doc;
    std::map<std::string, SensorInfo> sensors_parsed;
    if (!android::base::ReadFileToString(config_path, &json_doc)) {
        LOG(ERROR) << "Failed to read JSON config from " << config_path;
        return sensors_parsed;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(json_doc, root)) {
        LOG(ERROR) << "Failed to parse JSON config";
        return sensors_parsed;
    }

    Json::Value sensors = root["Sensors"];
    std::size_t total_parsed = 0;
    std::set<std::string> sensors_name_parsed;

    for (Json::Value::ArrayIndex i = 0; i < sensors.size(); ++i) {
        const std::string &name = sensors[i]["Name"].asString();
        LOG(INFO) << "Sensor[" << i << "]'s Name: " << name;
        if (name.empty()) {
            LOG(ERROR) << "Failed to read "
                       << "Sensor[" << i << "]'s Name";
            sensors_parsed.clear();
            return sensors_parsed;
        }

        auto result = sensors_name_parsed.insert(name);
        if (!result.second) {
            LOG(ERROR) << "Duplicate Sensor[" << i << "]'s Name";
            sensors_parsed.clear();
            return sensors_parsed;
        }

        std::string sensor_type_str = sensors[i]["Type"].asString();
        LOG(INFO) << "Sensor[" << name << "]'s Type: " << sensor_type_str;
        TemperatureType_2_0 sensor_type;

        if (!getTypeFromString(sensor_type_str, &sensor_type)) {
            LOG(ERROR) << "Invalid "
                       << "Sensor[" << name << "]'s Type: " << sensor_type_str;
            sensors_parsed.clear();
            return sensors_parsed;
        }

        const size_t count = static_cast<size_t>(ThrottlingSeverityCount::NUM_THROTTLING_LEVELS);
        std::array<float, count> hot_thresholds = {NAN};
        std::array<float, count> cold_thresholds = {NAN};

        Json::Value hot_values = sensors[i]["HotThreshold"];
        Json::Value cold_values = sensors[i]["ColdThreshold"];
        if (hot_values.size() != count) {
            LOG(ERROR) << "Invalid "
                       << "Sensor[" << name << "]'s HotThreshold count" << hot_values.size();
            sensors_parsed.clear();
            return sensors_parsed;
        }
        if (cold_values.size() != count) {
            LOG(ERROR) << "Invalid "
                       << "Sensor[" << name << "]'s HotThreshold count" << cold_values.size();
            sensors_parsed.clear();
            return sensors_parsed;
        }
        for (Json::Value::ArrayIndex j = 0; j < count; ++j) {
            if (hot_values[j].isString()) {
                hot_thresholds[j] = std::stof(hot_values[j].asString());
            } else {
                hot_thresholds[j] = hot_values[j].asFloat();
            }
            if (cold_values[j].isString()) {
                cold_thresholds[j] = std::stof(cold_values[j].asString());
            } else {
                cold_thresholds[j] = cold_values[j].asFloat();
            }
            LOG(INFO) << "Sensor[" << name << "]'s HotThreshold[" << j
                      << "]: " << hot_thresholds[j];
            LOG(INFO) << "Sensor[" << name << "]'s ColdThreshold[" << j
                      << "]: " << cold_thresholds[j];
        }
        float vr_threshold = NAN;
        if (sensors[i]["VrThreshold"].isString()) {
            vr_threshold = std::stof(sensors[i]["VrThreshold"].asString());
        } else {
            vr_threshold = sensors[i]["VrThreshold"].asFloat();
        }
        LOG(INFO) << "Sensor[" << name << "]'s VrThreshold: " << vr_threshold;

        float multiplier = sensors[i]["Multiplier"].asFloat();
        LOG(INFO) << "Sensor[" << name << "]'s Multiplier: " << multiplier;

        sensors_parsed[name] = {
            .type = sensor_type,
            .hot_thresholds = hot_thresholds,
            .cold_thresholds = cold_thresholds,
            .vr_threshold = vr_threshold,
            .multiplier = multiplier,
        };
        ++total_parsed;
    }

    LOG(INFO) << total_parsed << " Sensors parsed successfully";
    return sensors_parsed;
}

std::map<std::string, CoolingType> ParseCoolingDevice(const std::string &config_path) {
    std::string json_doc;
    std::map<std::string, CoolingType> cooling_devices_parsed;
    if (!android::base::ReadFileToString(config_path, &json_doc)) {
        LOG(ERROR) << "Failed to read JSON config from " << config_path;
        return cooling_devices_parsed;
    }

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(json_doc, root)) {
        LOG(ERROR) << "Failed to parse JSON config";
        return cooling_devices_parsed;
    }

    Json::Value cooling_devices = root["CoolingDevices"];
    std::size_t total_parsed = 0;
    std::set<std::string> cooling_devices_name_parsed;

    for (Json::Value::ArrayIndex i = 0; i < cooling_devices.size(); ++i) {
        const std::string &name = cooling_devices[i]["Name"].asString();
        LOG(INFO) << "CoolingDevice[" << i << "]'s Name: " << name;
        if (name.empty()) {
            LOG(ERROR) << "Failed to read "
                       << "CoolingDevice[" << i << "]'s Name";
            cooling_devices_parsed.clear();
            return cooling_devices_parsed;
        }

        auto result = cooling_devices_name_parsed.insert(name);
        if (!result.second) {
            LOG(ERROR) << "Duplicate CoolingDevice[" << i << "]'s Name";
            cooling_devices_parsed.clear();
            return cooling_devices_parsed;
        }

        std::string cooling_device_type_str = cooling_devices[i]["Type"].asString();
        LOG(INFO) << "CoolingDevice[" << name << "]'s Type: " << cooling_device_type_str;
        CoolingType cooling_device_type;

        if (!getTypeFromString(cooling_device_type_str, &cooling_device_type)) {
            LOG(ERROR) << "Invalid "
                       << "CoolingDevice[" << name << "]'s Type: " << cooling_device_type_str;
            cooling_devices_parsed.clear();
            return cooling_devices_parsed;
        }

        cooling_devices_parsed[name] = cooling_device_type;

        ++total_parsed;
    }

    LOG(INFO) << total_parsed << " CoolingDevices parsed successfully";
    return cooling_devices_parsed;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
