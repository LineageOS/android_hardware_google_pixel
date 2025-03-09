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

#include "thermal_predictions_helper.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>

#include <algorithm>
#include <numeric>
#include <string_view>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

bool ThermalPredictionsHelper::registerPredictorSensor(std::string_view sensor_name,
                                                       int sample_duration, int num_out_samples) {
    if (sample_duration <= 0 || num_out_samples <= 0) {
        LOG(ERROR) << "Invalid sample_duration: " << sample_duration
                   << " or num_out_samples: " << num_out_samples << " for sensor: " << sensor_name;
        return false;
    }

    if (predictor_sensors_.count(sensor_name.data())) {
        LOG(ERROR) << "sensor_name " << sensor_name << " is already registered as predictor";
        return false;
    }

    predictor_sensors_[sensor_name.data()] = PredictorSensorInfo(
            {std::string(sensor_name), sample_duration, num_out_samples,
             std::vector<PredictionSample>(num_out_samples, PredictionSample(num_out_samples)), 0});
    return true;
}

bool ThermalPredictionsHelper::registerPredictedSensor(std::string_view sensor_name,
                                                       std::string_view linked_sensor,
                                                       int duration) {
    if (duration < 0) {
        LOG(ERROR) << "Invalid duration: " << duration << " for sensor: " << sensor_name;
        return false;
    }

    if (predicted_sensors_.count(sensor_name.data())) {
        LOG(ERROR) << "sensor_name " << sensor_name << " is already registered as predicted sensor";
        return false;
    }

    if (predictor_sensors_.count(linked_sensor.data()) == 0) {
        LOG(ERROR) << "linked_sensor_name " << linked_sensor << " is not registered as predictor";
        return false;
    }

    PredictorSensorInfo &predictor_sensor_info = predictor_sensors_[linked_sensor.data()];
    const int max_prediction_duration =
            (predictor_sensor_info.num_out_samples - 1) * predictor_sensor_info.sample_duration;

    if (duration > max_prediction_duration) {
        LOG(ERROR) << "Predicted sensor " << sensor_name
                   << " duration is greater than max prediction duration of predictor "
                   << linked_sensor << " which is " << max_prediction_duration;
        return false;
    }

    // round up to nearest lower index
    const int prediction_index = duration / predictor_sensor_info.sample_duration;
    if (duration % predictor_sensor_info.sample_duration != 0) {
        LOG(INFO) << "Predicted sensor " << sensor_name << " duration " << duration
                  << " is not a multiple of " << linked_sensor << " sample duration "
                  << predictor_sensor_info.sample_duration << " and hence updated to "
                  << prediction_index * predictor_sensor_info.sample_duration;
    }

    predicted_sensors_[sensor_name.data()] = PredictedSensorInfo(
            {std::string(sensor_name), std::string(linked_sensor), duration, prediction_index});
    return true;
}

bool ThermalPredictionsHelper::updateSensor(std::string_view sensor_name,
                                            std::vector<float> &values) {
    std::unique_lock<std::shared_mutex> _lock(sensor_predictions_mutex_);
    const auto sensor_itr = predictor_sensors_.find(sensor_name.data());
    if (sensor_itr == predictor_sensors_.end()) {
        LOG(ERROR) << "sensor_name " << sensor_name << " is not registered as predictor";
        return false;
    }

    PredictorSensorInfo &predictor_sensor_info = predictor_sensors_[sensor_name.data()];
    if (values.size() != static_cast<size_t>(predictor_sensor_info.num_out_samples)) {
        LOG(ERROR) << "Invalid number of values: " << values.size()
                   << " for sensor: " << sensor_name
                   << ", expected: " << predictor_sensor_info.num_out_samples;
        return false;
    }

    predictor_sensor_info.samples[predictor_sensor_info.cur_index].timestamp = boot_clock::now();
    predictor_sensor_info.samples[predictor_sensor_info.cur_index].values = values;
    predictor_sensor_info.cur_index++;
    predictor_sensor_info.cur_index %= predictor_sensor_info.num_out_samples;

    return true;
}

SensorReadStatus ThermalPredictionsHelper::readSensor(std::string_view sensor_name, float *temp) {
    std::shared_lock<std::shared_mutex> _lock(sensor_predictions_mutex_);
    const auto sensor_itr = predicted_sensors_.find(sensor_name.data());
    if (sensor_itr == predicted_sensors_.end()) {
        LOG(ERROR) << "sensor_name " << sensor_name << " is not registered as predicted sensor";
        return SensorReadStatus::ERROR;
    }

    PredictedSensorInfo &predicted_sensor_info = predicted_sensors_[sensor_name.data()];
    const int prediction_index = predicted_sensor_info.prediction_index;

    const auto linked_sensor_itr = predictor_sensors_.find(predicted_sensor_info.linked_sensor);
    if (linked_sensor_itr == predictor_sensors_.end()) {
        LOG(ERROR) << "linked_sensor_name " << predicted_sensor_info.linked_sensor
                   << " is not registered as predictor for sensor" << sensor_name;
        return SensorReadStatus::ERROR;
    }

    PredictorSensorInfo predictor_sensor_info = linked_sensor_itr->second;
    boot_clock::time_point now = boot_clock::now();
    const auto min_time_elapsed_ms = predicted_sensor_info.duration - kToleranceIntervalMs;
    const auto max_time_elapsed_ms = predicted_sensor_info.duration + kToleranceIntervalMs;
    int loop_count = 0;
    do {
        int index = predictor_sensor_info.cur_index - loop_count - 1;
        if (index < 0) {
            index += predictor_sensor_info.num_out_samples;
        }

        const auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - predictor_sensor_info.samples[index].timestamp);
        if (time_elapsed.count() <= max_time_elapsed_ms &&
            time_elapsed.count() >= min_time_elapsed_ms) {
            *temp = predictor_sensor_info.samples[index].values[prediction_index];
            return SensorReadStatus::OKAY;
        }

        loop_count++;
    } while (loop_count < predictor_sensor_info.num_out_samples);

    LOG(INFO) << "sensor_name: " << sensor_name << " no valid prediction samples found";
    return SensorReadStatus::UNDER_COLLECTING;
}

bool ThermalPredictionsHelper::initializePredictionSensors(
        const std::unordered_map<std::string, SensorInfo> &sensor_info_map) {
    std::unique_lock<std::shared_mutex> _lock(sensor_predictions_mutex_);

    for (auto it = sensor_info_map.begin(); it != sensor_info_map.end(); ++it) {
        const std::string_view sensor_name = it->first;
        const SensorInfo &sensor_info = it->second;

        if (!sensor_info.predictor_info || !sensor_info.virtual_sensor_info ||
            (!sensor_info.predictor_info->supports_predictions)) {
            continue;
        }

        if (!registerPredictorSensor(sensor_name,
                                     sensor_info.predictor_info->prediction_sample_interval,
                                     sensor_info.predictor_info->num_prediction_samples)) {
            LOG(ERROR) << "Failed to register predictor sensor: " << sensor_name;
            return false;
        }
    }

    for (auto it = sensor_info_map.begin(); it != sensor_info_map.end(); ++it) {
        const std::string_view sensor_name = it->first;
        const SensorInfo &sensor_info = it->second;

        if (!sensor_info.predictor_info || !sensor_info.virtual_sensor_info ||
            (sensor_info.virtual_sensor_info->formula != FormulaOption::PREVIOUSLY_PREDICTED)) {
            continue;
        }

        if (sensor_info.virtual_sensor_info->linked_sensors.size() != 1) {
            LOG(ERROR) << "Invalid number of linked sensors: "
                       << sensor_info.virtual_sensor_info->linked_sensors.size()
                       << " for sensor: " << sensor_name;
            return false;
        }

        if (!registerPredictedSensor(sensor_name,
                                     sensor_info.virtual_sensor_info->linked_sensors[0],
                                     sensor_info.predictor_info->prediction_duration)) {
            LOG(ERROR) << "Failed to register predicted sensor: " << sensor_name;
            return false;
        }
    }

    return true;
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
