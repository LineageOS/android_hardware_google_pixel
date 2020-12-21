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
bool getTypeFromString(std::string_view str, T *out) {
    auto types = hidl_enum_range<T>();
    for (const auto &type : types) {
        if (toString(type) == str) {
            *out = type;
            return true;
        }
    }
    return false;
}

float getFloatFromValue(const Json::Value &value) {
    if (value.isString()) {
        return std::stof(value.asString());
    } else {
        return value.asFloat();
    }
}

int getIntFromValue(const Json::Value &value) {
    if (value.isString()) {
        return std::stoi(value.asString());
    } else {
        return value.asInt();
    }
}

bool getFloatFromJsonValues(const Json::Value &values, ThrottlingArray *out, bool inc_check,
                            bool dec_check) {
    ThrottlingArray ret;

    if (inc_check && dec_check) {
        LOG(ERROR) << "Cannot enable inc_check and dec_check at the same time";
        return false;
    }

    if (values.size() != kThrottlingSeverityCount) {
        LOG(ERROR) << "Values size is invalid";
        return false;
    } else {
        float last = std::nanf("");
        for (Json::Value::ArrayIndex i = 0; i < kThrottlingSeverityCount; ++i) {
            ret[i] = getFloatFromValue(values[i]);
            if (inc_check && !std::isnan(last) && !std::isnan(ret[i]) && ret[i] < last) {
                LOG(FATAL) << "Invalid array[" << i << "]" << ret[i] << " min=" << last;
                return false;
            }
            if (dec_check && !std::isnan(last) && !std::isnan(ret[i]) && ret[i] > last) {
                LOG(FATAL) << "Invalid array[" << i << "]" << ret[i] << " max=" << last;
                return false;
            }
            last = std::isnan(ret[i]) ? last : ret[i];
            LOG(INFO) << "[" << i << "]: " << ret[i];
        }
    }

    *out = ret;
    return true;
}

}  // namespace

std::map<std::string, SensorInfo> ParseSensorInfo(std::string_view config_path) {
    std::string json_doc;
    std::map<std::string, SensorInfo> sensors_parsed;
    if (!android::base::ReadFileToString(config_path.data(), &json_doc)) {
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

        std::array<ThrottleType, kThrottlingSeverityCount> throttle_type;
        throttle_type.fill(NONE);
        bool support_pid = false;
        bool support_hard_limit = false;
        Json::Value values = sensors[i]["ThrottleType"];
        if (values.size()) {
            for (Json::Value::ArrayIndex j = 0; j < values.size(); ++j) {
                if (values[j].asString() == "None") {
                    LOG(INFO) << "Sensor[" << name << "]'s throttle type[" << j << "]: None";
                    continue;
                } else if (values[j].asString() == "PID") {
                    throttle_type[j] = PID;
                    support_pid = true;
                    LOG(INFO) << "Sensor[" << name << "]'s throttle type[" << j << "]: PID";
                } else if (values[j].asString() == "LIMIT") {
                    throttle_type[j] = LIMIT;
                    support_hard_limit = true;
                    LOG(INFO) << "Sensor[" << name << "]'s throttle type[" << j << "]: LIMIT";
                } else {
                    LOG(ERROR) << "cannot identify the throttling type";
                    sensors_parsed.clear();
                    return sensors_parsed;
                }
            }
        }

        bool send_cb = false;
        if (sensors[i]["Monitor"].empty() || !sensors[i]["Monitor"].isBool()) {
            LOG(INFO) << "Failed to read Sensor[" << name << "]'s Monitor, set to 'false'";
        } else if (sensors[i]["Monitor"].asBool()) {
            send_cb = true;
        }
        LOG(INFO) << "Sensor[" << name << "]'s SendCallback: " << std::boolalpha << send_cb
                  << std::noboolalpha;

        bool send_powerhint = false;
        if (sensors[i]["SendPowerHint"].empty() || !sensors[i]["SendPowerHint"].isBool()) {
            LOG(INFO) << "Failed to read Sensor[" << name << "]'s SendPowerHint, set to 'false'";
        } else if (sensors[i]["SendPowerHint"].asBool()) {
            send_powerhint = true;
        }
        LOG(INFO) << "Sensor[" << name << "]'s SendPowerHint: " << std::boolalpha << send_powerhint
                  << std::noboolalpha;

        bool is_monitor = (send_cb | send_powerhint | support_pid | support_hard_limit);
        LOG(INFO) << "Sensor[" << name << "]'s Monitor: " << is_monitor;

        std::array<float, kThrottlingSeverityCount> hot_thresholds;
        hot_thresholds.fill(NAN);
        std::array<float, kThrottlingSeverityCount> cold_thresholds;
        cold_thresholds.fill(NAN);
        std::array<float, kThrottlingSeverityCount> hot_hysteresis;
        hot_hysteresis.fill(0.0);
        std::array<float, kThrottlingSeverityCount> cold_hysteresis;
        cold_hysteresis.fill(0.0);
        std::array<std::string, kCombinationCount> linked_sensors;
        linked_sensors.fill("NAN");
        std::array<float, kCombinationCount> coefficients;
        coefficients.fill(0.0);

        std::string trigger_sensor;
        FormulaOption formula = FormulaOption::COUNT_THRESHOLD;
        bool is_virtual_sensor = false;
        if (sensors[i]["VirtualSensor"].empty() || !sensors[i]["VirtualSensor"].isBool()) {
            LOG(INFO) << "Failed to read Sensor[" << name << "]'s VirtualSensor, set to 'false'";
        } else {
            is_virtual_sensor = sensors[i]["VirtualSensor"].asBool();
        }
        values = sensors[i]["HotThreshold"];
        if (values.size() != kThrottlingSeverityCount) {
            LOG(ERROR) << "Invalid "
                       << "Sensor[" << name << "]'s HotThreshold count" << values.size();
            sensors_parsed.clear();
            return sensors_parsed;
        } else {
            float min = std::numeric_limits<float>::min();
            for (Json::Value::ArrayIndex j = 0; j < kThrottlingSeverityCount; ++j) {
                hot_thresholds[j] = getFloatFromValue(values[j]);
                if (!std::isnan(hot_thresholds[j])) {
                    if (hot_thresholds[j] < min) {
                        LOG(ERROR) << "Invalid "
                                   << "Sensor[" << name << "]'s HotThreshold[j" << j
                                   << "]: " << hot_thresholds[j] << " < " << min;
                        sensors_parsed.clear();
                        return sensors_parsed;
                    }
                    min = hot_thresholds[j];
                }
                LOG(INFO) << "Sensor[" << name << "]'s HotThreshold[" << j
                          << "]: " << hot_thresholds[j];
            }
        }

        values = sensors[i]["HotHysteresis"];
        if (values.size() != kThrottlingSeverityCount) {
            LOG(INFO) << "Cannot find valid "
                      << "Sensor[" << name << "]'s HotHysteresis, default all to 0.0";
        } else {
            for (Json::Value::ArrayIndex j = 0; j < kThrottlingSeverityCount; ++j) {
                hot_hysteresis[j] = getFloatFromValue(values[j]);
                if (std::isnan(hot_hysteresis[j])) {
                    LOG(ERROR) << "Invalid "
                               << "Sensor[" << name << "]'s HotHysteresis: " << hot_hysteresis[j];
                    sensors_parsed.clear();
                    return sensors_parsed;
                }
                LOG(INFO) << "Sensor[" << name << "]'s HotHysteresis[" << j
                          << "]: " << hot_hysteresis[j];
            }
        }

        values = sensors[i]["ColdThreshold"];
        if (values.size() != kThrottlingSeverityCount) {
            LOG(INFO) << "Cannot find valid "
                      << "Sensor[" << name << "]'s ColdThreshold, default all to NAN";
        } else {
            float max = std::numeric_limits<float>::max();
            for (Json::Value::ArrayIndex j = 0; j < kThrottlingSeverityCount; ++j) {
                cold_thresholds[j] = getFloatFromValue(values[j]);
                if (!std::isnan(cold_thresholds[j])) {
                    if (cold_thresholds[j] > max) {
                        LOG(ERROR) << "Invalid "
                                   << "Sensor[" << name << "]'s ColdThreshold[j" << j
                                   << "]: " << cold_thresholds[j] << " > " << max;
                        sensors_parsed.clear();
                        return sensors_parsed;
                    }
                    max = cold_thresholds[j];
                }
                LOG(INFO) << "Sensor[" << name << "]'s ColdThreshold[" << j
                          << "]: " << cold_thresholds[j];
            }
        }

        values = sensors[i]["ColdHysteresis"];
        if (values.size() != kThrottlingSeverityCount) {
            LOG(INFO) << "Cannot find valid "
                      << "Sensor[" << name << "]'s ColdHysteresis, default all to 0.0";
        } else {
            for (Json::Value::ArrayIndex j = 0; j < kThrottlingSeverityCount; ++j) {
                cold_hysteresis[j] = getFloatFromValue(values[j]);
                if (std::isnan(cold_hysteresis[j])) {
                    LOG(ERROR) << "Invalid "
                               << "Sensor[" << name
                               << "]'s ColdHysteresis: " << cold_hysteresis[j];
                    sensors_parsed.clear();
                    return sensors_parsed;
                }
                LOG(INFO) << "Sensor[" << name << "]'s ColdHysteresis[" << j
                          << "]: " << cold_hysteresis[j];
            }
        }

        if (is_virtual_sensor) {
            values = sensors[i]["Combination"];
            if (values.size() > kCombinationCount) {
                LOG(ERROR) << "Invalid "
                           << "Sensor[" << name << "]'s Combination count" << values.size();
                sensors_parsed.clear();
                return sensors_parsed;
            } else {
                for (Json::Value::ArrayIndex j = 0; j < kCombinationCount; ++j) {
                    if (values[j].isString()) {
                        if (values[j].asString().compare("NAN") != 0) {
                            linked_sensors[j] = values[j].asString();
                        }
                    }
                }
            }
            values = sensors[i]["Coefficient"];
            if (values.size() > kCombinationCount) {
                LOG(ERROR) << "Invalid "
                           << "Sensor[" << name << "]'s Combination count" << values.size();
                sensors_parsed.clear();
                return sensors_parsed;
            } else {
                for (Json::Value::ArrayIndex j = 0; j < kCombinationCount; ++j) {
                    if (values[j].isString()) {
                        if (values[j].asString().compare("NAN") != 0) {
                            coefficients[j] = std::stof(values[j].asString());
                        }
                    } else {
                        coefficients[j] = values[j].asFloat();
                    }
                }
            }
            trigger_sensor = sensors[i]["TriggerSensor"].asString();
            if (sensors[i]["Formula"].asString().compare("COUNT_THRESHOLD") == 0)
                formula = FormulaOption::COUNT_THRESHOLD;
            else if (sensors[i]["Formula"].asString().compare("WEIGHTED_AVG") == 0)
                formula = FormulaOption::WEIGHTED_AVG;
            else if (sensors[i]["Formula"].asString().compare("MAXIMUM") == 0)
                formula = FormulaOption::MAXIMUM;
            else
                formula = FormulaOption::MINIMUM;
        }

        float vr_threshold = NAN;
        vr_threshold = getFloatFromValue(sensors[i]["VrThreshold"]);
        LOG(INFO) << "Sensor[" << name << "]'s VrThreshold: " << vr_threshold;

        float multiplier = sensors[i]["Multiplier"].asFloat();
        LOG(INFO) << "Sensor[" << name << "]'s Multiplier: " << multiplier;

        std::chrono::milliseconds polling_delay;
        if (sensors[i]["PollingDelay"].empty()) {
            polling_delay = kUeventPollTimeoutMs;
        } else {
            polling_delay = std::chrono::milliseconds(getIntFromValue(sensors[i]["PollingDelay"]));
        }
        LOG(INFO) << "Sensor[" << name << "]'s Polling delay: " << polling_delay.count();

        std::chrono::milliseconds passive_delay;
        if (sensors[i]["PassiveDelay"].empty()) {
            passive_delay = kMinPollIntervalMs;
        } else {
            passive_delay = std::chrono::milliseconds(getIntFromValue(sensors[i]["PassiveDelay"]));
        }
        LOG(INFO) << "Sensor[" << name << "]'s Passive delay: " << passive_delay.count();

        std::array<float, kThrottlingSeverityCount> k_po;
        k_po.fill(0.0);
        std::array<float, kThrottlingSeverityCount> k_pu;
        k_pu.fill(0.0);
        std::array<float, kThrottlingSeverityCount> k_i;
        k_i.fill(0.0);
        std::array<float, kThrottlingSeverityCount> k_d;
        k_d.fill(0.0);
        std::array<float, kThrottlingSeverityCount> i_max;
        i_max.fill(NAN);
        std::array<float, kThrottlingSeverityCount> max_alloc_power;
        max_alloc_power.fill(NAN);
        std::array<float, kThrottlingSeverityCount> min_alloc_power;
        min_alloc_power.fill(NAN);
        std::array<float, kThrottlingSeverityCount> s_power;
        s_power.fill(NAN);
        std::array<float, kThrottlingSeverityCount> i_cutoff;
        i_cutoff.fill(NAN);
        std::vector<std::string> cdev_request;
        std::vector<float> cdev_weight;

        if (support_pid) {
            LOG(INFO) << "Start to parse K_Po";
            values = sensors[i]["K_Po"];
            if (!getFloatFromJsonValues(values, &k_po, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse K_Pu";
            values = sensors[i]["K_Pu"];
            if (!getFloatFromJsonValues(values, &k_pu, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse K_I";
            values = sensors[i]["K_I"];
            if (!getFloatFromJsonValues(values, &k_i, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse K_D";
            values = sensors[i]["K_D"];
            if (!getFloatFromJsonValues(values, &k_d, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse I_Max";
            values = sensors[i]["I_Max"];
            if (!getFloatFromJsonValues(values, &i_max, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse MaxAllocPower";
            values = sensors[i]["MaxAllocPower"];
            if (!getFloatFromJsonValues(values, &max_alloc_power, false, true)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse MinAllocPower";
            values = sensors[i]["MinAllocPower"];
            if (!getFloatFromJsonValues(values, &min_alloc_power, false, true)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse S_Power";
            values = sensors[i]["S_Power"];
            if (!getFloatFromJsonValues(values, &s_power, false, true)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            LOG(INFO) << "Start to parse I_Cutoff";
            values = sensors[i]["I_Cutoff"];
            if (!getFloatFromJsonValues(values, &i_cutoff, false, false)) {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            values = sensors[i]["CdevRequest"];
            if (values.size()) {
                cdev_request.reserve(values.size());
                for (Json::Value::ArrayIndex j = 0; j < values.size(); ++j) {
                    cdev_request.emplace_back(values[j].asString());
                    LOG(INFO) << "Sensor[" << name << "]'s cdev_request[" << j
                              << "]: " << cdev_request[j];
                }
            } else {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            values = sensors[i]["CdevWeight"];
            if (values.size()) {
                cdev_weight.reserve(values.size());
                for (Json::Value::ArrayIndex j = 0; j < values.size(); ++j) {
                    cdev_weight.emplace_back(getFloatFromValue(values[j]));
                    LOG(INFO) << "Sensor[" << name << "]'s cdev_weight[" << j
                              << "]: " << cdev_weight[j];
                }
            } else {
                sensors_parsed.clear();
                return sensors_parsed;
            }

            for (Json::Value::ArrayIndex j = 0; j < kThrottlingSeverityCount; ++j) {
                if (!std::isnan(s_power[j]) &&
                    (std::isnan(k_po[j]) || std::isnan(k_pu[j]) || std::isnan(k_i[j]) ||
                     std::isnan(k_d[j]) || std::isnan(i_max[j]) || std::isnan(max_alloc_power[j]) ||
                     std::isnan(min_alloc_power[j]) || std::isnan(i_cutoff[j]))) {
                    LOG(ERROR) << "Sensor[" << name << "]: Invalid PID parameters combinations";
                    sensors_parsed.clear();
                    return sensors_parsed;
                }
            }
        }

        std::map<std::string, ThrottlingArray> limit_info;
        if (support_hard_limit) {
            LOG(INFO) << "start to parse LimitInfo";
            values = sensors[i]["LimitInfo"];
            for (Json::Value::ArrayIndex j = 0; j < values.size(); ++j) {
                const std::string &cdev_name = values[j]["CdevRequest"].asString();
                Json::Value sub_values = values[j]["CdevInfo"];
                std::array<float, kThrottlingSeverityCount> state;
                if (!getFloatFromJsonValues(sub_values, &state, false, false)) {
                    sensors_parsed.clear();
                    return sensors_parsed;
                } else {
                    LOG(INFO) << "Sensor[" << name
                              << "]: Add cooling device request: " << cdev_name;
                    limit_info[cdev_name] = state;
                }
            }
        }

        std::unique_ptr<ThrottlingInfo> throttling_info(new ThrottlingInfo{
                k_po, k_pu, k_i, k_d, i_max, max_alloc_power, min_alloc_power, s_power, i_cutoff,
                throttle_type, cdev_request, cdev_weight, limit_info});

        sensors_parsed[name] = {
                .type = sensor_type,
                .hot_thresholds = hot_thresholds,
                .cold_thresholds = cold_thresholds,
                .hot_hysteresis = hot_hysteresis,
                .cold_hysteresis = cold_hysteresis,
                .vr_threshold = vr_threshold,
                .multiplier = multiplier,
                .polling_delay = polling_delay,
                .passive_delay = passive_delay,
                .linked_sensors = linked_sensors,
                .coefficients = coefficients,
                .trigger_sensor = trigger_sensor,
                .formula = formula,
                .is_virtual_sensor = is_virtual_sensor,
                .send_cb = send_cb,
                .send_powerhint = send_powerhint,
                .is_monitor = is_monitor,
                .throttling_info = std::move(throttling_info),
        };

        ++total_parsed;
    }

    LOG(INFO) << total_parsed << " Sensors parsed successfully";
    return sensors_parsed;
}

std::map<std::string, CdevInfo> ParseCoolingDevice(std::string_view config_path) {
    std::string json_doc;
    std::map<std::string, CdevInfo> cooling_devices_parsed;
    if (!android::base::ReadFileToString(config_path.data(), &json_doc)) {
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

        auto result = cooling_devices_name_parsed.insert(name.data());
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

        std::vector<float> power2state;
        Json::Value values = cooling_devices[i]["Power2State"];
        if (values.size()) {
            power2state.reserve(values.size());
            for (Json::Value::ArrayIndex j = 0; j < values.size(); ++j) {
                power2state.emplace_back(getFloatFromValue(values[j]));
                LOG(INFO) << "Cooling device[" << name << "]'s Power2State[" << j
                          << "]: " << power2state[j];
            }
        } else {
            cooling_devices_parsed.clear();
            return cooling_devices_parsed;
        }

        cooling_devices_parsed[name] = {
                .type = cooling_device_type,
                .power2state = power2state,
        };
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
