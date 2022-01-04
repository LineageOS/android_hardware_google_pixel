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

#include <android-base/properties.h>
#include <gtest/gtest.h>

#include "adaptivecpu/AdaptiveCpuConfig.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using std::chrono_literals::operator""ms;

TEST(AdaptiveCpuConfigTest, valid) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "25");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties(),
              AdaptiveCpuConfig{.iterationSleepDuration = 25ms});
}

TEST(AdaptiveCpuConfigTest, defaultConfig) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties(),
              AdaptiveCpuConfig{.iterationSleepDuration = 1000ms});
}

TEST(AdaptiveCpuConfigTest, iterationSleepDuration_belowMin) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "2");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties().iterationSleepDuration, 20ms);
}

TEST(AdaptiveCpuConfigTest, iterationSleepDuration_negative) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "-100");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties().iterationSleepDuration, 1000ms);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
