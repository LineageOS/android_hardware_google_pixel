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

#define LOG_TAG "powerhal-libperfmgr"
#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)

#include "AdaptiveCpuConfig.h"

#include <android-base/properties.h>
#include <inttypes.h>
#include <utils/Trace.h>

#include <string_view>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using std::chrono_literals::operator""ms;

constexpr std::string_view kIterationSleepDurationProperty(
        "debug.adaptivecpu.iteration_sleep_duration_ms");
static const std::chrono::milliseconds kIterationSleepDurationMin = 20ms;

const AdaptiveCpuConfig AdaptiveCpuConfig::DEFAULT{
        // N.B.: The model will typically be trained with this value set to 25ms. We set it to 1s as
        // a safety measure, but best performance will be seen at 25ms.
        .iterationSleepDuration = 1000ms,
};

AdaptiveCpuConfig AdaptiveCpuConfig::ReadFromSystemProperties() {
    ATRACE_CALL();
    std::chrono::milliseconds iterationSleepDuration = std::chrono::milliseconds(
            ::android::base::GetUintProperty<uint32_t>(kIterationSleepDurationProperty.data(),
                                                       DEFAULT.iterationSleepDuration.count()));
    iterationSleepDuration = std::max(iterationSleepDuration, kIterationSleepDurationMin);
    return {
            .iterationSleepDuration = iterationSleepDuration,
    };
}

bool AdaptiveCpuConfig::operator==(const AdaptiveCpuConfig &other) const {
    return iterationSleepDuration == other.iterationSleepDuration;
}

std::ostream &operator<<(std::ostream &stream, const AdaptiveCpuConfig &config) {
    stream << "AdaptiveCpuConfig(";
    stream << "iterationSleepDuration=" << config.iterationSleepDuration.count() << "ms";
    stream << ")";
    return stream;
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
