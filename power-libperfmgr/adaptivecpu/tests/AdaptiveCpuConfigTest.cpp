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
using std::chrono_literals::operator""min;

TEST(AdaptiveCpuConfigTest, valid) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "25");
    android::base::SetProperty("debug.adaptivecpu.hint_timeout_ms", "500");
    android::base::SetProperty("debug.adaptivecpu.random_throttle_decision_percent", "25");
    android::base::SetProperty("debug.adaptivecpu.enabled_hint_timeout_ms", "1000");
    const AdaptiveCpuConfig config{
            .iterationSleepDuration = 25ms,
            .hintTimeout = 500ms,
            .randomThrottleDecisionProbability = 0.25,
            .enabledHintTimeout = 1000ms,
    };
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties(), config);
}

TEST(AdaptiveCpuConfigTest, defaultConfig) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "");
    android::base::SetProperty("debug.adaptivecpu.hint_timeout_ms", "");
    android::base::SetProperty("debug.adaptivecpu.random_throttle_decision_percent", "");
    android::base::SetProperty("debug.adaptivecpu.enabled_hint_timeout_ms", "");
    const AdaptiveCpuConfig config{
            .iterationSleepDuration = 1000ms,
            .hintTimeout = 2000ms,
            .randomThrottleDecisionProbability = 0,
            .enabledHintTimeout = 120min,
    };
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties(), config);
}

TEST(AdaptiveCpuConfigTest, iterationSleepDuration_belowMin) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "2");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties().iterationSleepDuration, 20ms);
}

TEST(AdaptiveCpuConfigTest, iterationSleepDuration_negative) {
    android::base::SetProperty("debug.adaptivecpu.iteration_sleep_duration_ms", "-100");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties().iterationSleepDuration, 1000ms);
}

TEST(AdaptiveCpuConfigTest, randomThrottleDecisionProbability_float) {
    android::base::SetProperty("debug.adaptivecpu.random_throttle_decision_percent", "0.5");
    ASSERT_EQ(AdaptiveCpuConfig::ReadFromSystemProperties().randomThrottleDecisionProbability, 0);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
