/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <cstddef>
#include <mutex>
#include <string>

#pragma once

namespace thermal {
namespace vtestimator {

// Current version only supports single input/output tensors
constexpr int kNumInputTensors = 1;
constexpr int kNumOutputTensors = 1;

typedef void *(*tflitewrapper_create)(int num_input_tensors, int num_output_tensors);
typedef bool (*tflitewrapper_init)(void *handle, const char *model_path);
typedef bool (*tflitewrapper_invoke)(void *handle, float *input_samples, int num_input_samples,
                                     float *output_samples, int num_output_samples);
typedef void (*tflitewrapper_destroy)(void *handle);
typedef bool (*tflitewrapper_get_input_config_size)(void *handle, int *config_size);
typedef bool (*tflitewrapper_get_input_config)(void *handle, char *config_buffer,
                                               int config_buffer_size);

struct TFLiteWrapperMethods {
    tflitewrapper_create create;
    tflitewrapper_init init;
    tflitewrapper_invoke invoke;
    tflitewrapper_destroy destroy;
    tflitewrapper_get_input_config_size get_input_config_size;
    tflitewrapper_get_input_config get_input_config;
    mutable std::mutex mutex;
};

struct InputRangeInfo {
    float max_threshold = std::numeric_limits<float>::max();
    float min_threshold = std::numeric_limits<float>::min();
};

struct VtEstimatorCommonData {
    VtEstimatorCommonData(size_t num_input_sensors) {
        num_linked_sensors = num_input_sensors;
        prev_samples_order = 1;
        is_initialized = false;
        use_prev_samples = false;
        cur_sample_count = 0;
    }

    std::vector<float> offset_thresholds;
    std::vector<float> offset_values;

    size_t num_linked_sensors;
    size_t prev_samples_order;
    size_t cur_sample_count;
    bool use_prev_samples;
    bool is_initialized;
};

struct VtEstimatorTFLiteData {
    VtEstimatorTFLiteData() {
        scratch_buffer = nullptr;
        input_buffer = nullptr;
        input_buffer_size = 0;
        output_label_count = 1;
        num_hot_spots = 1;
        output_buffer = nullptr;
        output_buffer_size = 1;

        tflite_wrapper = nullptr;
        tflite_methods.create = nullptr;
        tflite_methods.init = nullptr;
        tflite_methods.get_input_config_size = nullptr;
        tflite_methods.get_input_config = nullptr;
        tflite_methods.invoke = nullptr;
        tflite_methods.destroy = nullptr;
    }

    void *tflite_wrapper;
    float *scratch_buffer;
    float *input_buffer;
    size_t input_buffer_size;
    size_t num_hot_spots;
    size_t output_label_count;
    float *output_buffer;
    size_t output_buffer_size;
    std::string model_path;
    TFLiteWrapperMethods tflite_methods;
    std::vector<InputRangeInfo> input_range;

    ~VtEstimatorTFLiteData() {
        if (tflite_wrapper && tflite_methods.destroy) {
            tflite_methods.destroy(tflite_wrapper);
        }

        if (scratch_buffer) {
            delete scratch_buffer;
        }

        if (input_buffer) {
            delete input_buffer;
        }

        if (output_buffer) {
            delete output_buffer;
        }
    }
};

struct VtEstimatorLinearModelData {
    VtEstimatorLinearModelData() {}

    ~VtEstimatorLinearModelData() {}

    std::vector<std::vector<float>> input_samples;
    std::vector<std::vector<float>> coefficients;
    mutable std::mutex mutex;
};

}  // namespace vtestimator
}  // namespace thermal
