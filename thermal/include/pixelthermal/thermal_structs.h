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

#ifndef __THERMAL_STRUCTS_PARSER_H__
#define __THERMAL_STRUCTS_PARSER_H__

#include <android/hardware/thermal/1.0/IThermal.h>

#include <cmath>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace thermal {

using ::android::hardware::thermal::V1_0::TemperatureType;

struct SensorInfo {
    TemperatureType type;
    bool is_override;
    float throttling;
    float shutdown;
    float multiplier;
};

struct ThrottlingThresholds {
    ThrottlingThresholds() : cpu(NAN), gpu(NAN), ss(NAN), battery(NAN) {}
    float cpu;
    float gpu;
    float ss;
    float battery;
};

}  // namespace thermal
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif
