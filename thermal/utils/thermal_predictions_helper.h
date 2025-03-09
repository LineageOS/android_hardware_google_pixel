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

#pragma once

#include <aidl/android/hardware/thermal/Temperature.h>
#include <android-base/chrono_utils.h>

#include <chrono>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "thermal_info.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using ::android::base::boot_clock;
constexpr int kToleranceIntervalMs = 1000;

struct PredictionSample {
    PredictionSample(int num_out_samples) {
        timestamp = boot_clock::time_point::min();
        values = std::vector<float>(num_out_samples, NAN);
    }
    boot_clock::time_point timestamp;
    std::vector<float> values;
};

struct PredictorSensorInfo {
    std::string sensor_name;
    int sample_duration;
    int num_out_samples;
    std::vector<PredictionSample> samples;
    int cur_index;
};

struct PredictedSensorInfo {
    std::string sensor_name;
    std::string linked_sensor;
    int duration;
    int prediction_index;
};

class ThermalPredictionsHelper {
  public:
    ThermalPredictionsHelper() = default;
    ~ThermalPredictionsHelper() = default;
    // Disallow copy and assign
    ThermalPredictionsHelper(const ThermalPredictionsHelper &) = delete;
    void operator=(const ThermalPredictionsHelper &) = delete;

    bool initializePredictionSensors(
            const std::unordered_map<std::string, SensorInfo> &sensor_info_map);
    bool updateSensor(std::string_view sensor_name, std::vector<float> &values);
    SensorReadStatus readSensor(std::string_view sensor_name, float *temp);

  private:
    std::unordered_map<std::string, PredictorSensorInfo> predictor_sensors_;
    std::unordered_map<std::string, PredictedSensorInfo> predicted_sensors_;
    mutable std::shared_mutex sensor_predictions_mutex_;

    bool registerPredictedSensor(std::string_view sensor_name, std::string_view linked_sensor,
                                 int duration);
    bool registerPredictorSensor(std::string_view sensor_name, int sample_duration,
                                 int num_out_samples);
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
