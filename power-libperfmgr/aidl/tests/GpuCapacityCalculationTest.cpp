/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <aidl/android/hardware/power/WorkDuration.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "aidl/GpuCalculationHelpers.h"

using aidl::android::hardware::power::WorkDuration;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""ns;
using std::literals::chrono_literals::operator""h;
using testing::DoubleEq;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

template <typename R, typename D>
inline int64_t to_int_nanoseconds(std::chrono::duration<R, D> duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

TEST(GpuCapacityCalculation, gpu_total_time_attribution) {
    EXPECT_THAT(gpu_time_attribution(12ms, 0ms), DoubleEq(0));
    EXPECT_THAT(gpu_time_attribution(12ms, 8ms), DoubleEq(2.0 / 3.0));
    EXPECT_THAT(gpu_time_attribution(12ms, 6ms), DoubleEq(0.5));
    EXPECT_THAT(gpu_time_attribution(12ms, 12ms), DoubleEq(1.0));
    EXPECT_THAT(gpu_time_attribution(12ms, 12ms), DoubleEq(1.0));
}

TEST(GpuCapacityCalculation, total_time_of_zero_reports_zero_capacity) {
    EXPECT_THAT(gpu_time_attribution(0ms, 8ms), DoubleEq(0.0));
    EXPECT_THAT(gpu_time_attribution(0ms, 0ms), DoubleEq(0.0));
}

TEST(GpuCapacityCalculation, no_overrun_frame) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(12ms),
            .cpuDurationNanos = to_int_nanoseconds(8ms),
            .gpuDurationNanos = to_int_nanoseconds(8ms),
    };
    std::chrono::nanoseconds total_target =
            std::chrono::duration_cast<std::chrono::nanoseconds>(15ms);

    EXPECT_EQ(calculate_capacity(observation, total_target, 1000_hz), Cycles(0));
}

TEST(GpuCapacityCalculation, basic_50_50_frame) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(12ms),
            .cpuDurationNanos = to_int_nanoseconds(8ms),
            .gpuDurationNanos = to_int_nanoseconds(8ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 10ms, 1000000_hz), Cycles(1333));
}

TEST(GpuCapacityCalculation, frame_10_90) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(1ms),
            .gpuDurationNanos = to_int_nanoseconds(9ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100000_hz), Cycles(90));
}

TEST(GpuCapacityCalculation, frame_0_100) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(0ms),
            .gpuDurationNanos = to_int_nanoseconds(10ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100000_hz), Cycles(100));
}

TEST(GpuCapacityCalculation, frame_40_60_parallel) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(6ms),
            .gpuDurationNanos = to_int_nanoseconds(8ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100000000_hz), Cycles(80000));
}

TEST(GpuCapacityCalculation, frame_100_0) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(10ms),
            .gpuDurationNanos = to_int_nanoseconds(0ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100000_hz), Cycles(0));
}

TEST(GpuCapacityCalculation, frame_100_100) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(10ms),
            .gpuDurationNanos = to_int_nanoseconds(10ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100000_hz), Cycles(100));
}

TEST(GpuCapacityCalculation, report_underaccounts_total_50_50) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(12ms),
            .cpuDurationNanos = to_int_nanoseconds(4ms),
            .gpuDurationNanos = to_int_nanoseconds(4ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 10ms, 1000_hz), Cycles(1));
}

TEST(GpuCapacityCalculation, report_underaccounts_total_90_10) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(20ms),
            .cpuDurationNanos = to_int_nanoseconds(9ms),
            .gpuDurationNanos = to_int_nanoseconds(1ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 10ms, 100000_hz), Cycles(100));
}

TEST(GpuCapacityCalculation, frame_very_long_report) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10h),
            .cpuDurationNanos = to_int_nanoseconds(1h),
            .gpuDurationNanos = to_int_nanoseconds(9h),
    };
    EXPECT_EQ(calculate_capacity(observation, 9h, 10_hz),
              Cycles(9 * std::chrono::duration_cast<std::chrono::seconds>(1h).count()));
}

TEST(GpuCapacityCalculation, frame_nonsense_frequency) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(1ms),
            .gpuDurationNanos = to_int_nanoseconds(9ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, Frequency(-10)), Cycles(0));
    EXPECT_EQ(calculate_capacity(observation, 9ms, Frequency(0)), Cycles(0));
}

TEST(GpuCapacityCalculation, frame_nonsense_report) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(1ms),
            .gpuDurationNanos = to_int_nanoseconds(1ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 9ms, 100_hz), Cycles(0));
}

TEST(GpuCapacityCalculation, frame_nonsense_target) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(10ms),
            .cpuDurationNanos = to_int_nanoseconds(1ms),
            .gpuDurationNanos = to_int_nanoseconds(1ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 0ms, 100_hz), Cycles(0));
    EXPECT_EQ(calculate_capacity(observation, -1ms, 100_hz), Cycles(0));
}

TEST(GpuCapacityCalculation, frame_nonsense_subtarget_cpu) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(20ms),
            .cpuDurationNanos = to_int_nanoseconds(40ms),
            .gpuDurationNanos = to_int_nanoseconds(20ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 10ms, 100000_hz), Cycles(0));
}

TEST(GpuCapacityCalculation, frame_nonsense_subtarget_gpu) {
    WorkDuration observation{
            .durationNanos = to_int_nanoseconds(20ms),
            .cpuDurationNanos = to_int_nanoseconds(20ms),
            .gpuDurationNanos = to_int_nanoseconds(40ms),
    };
    EXPECT_EQ(calculate_capacity(observation, 10ms, 100000_hz), Cycles(0));
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
