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

#include <gtest/gtest.h>

#include <random>

#include "adaptivecpu/Model.h"
#include "mocks.h"

using testing::_;
using testing::ByMove;
using testing::Return;
using std::chrono_literals::operator""ns;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

TEST(ModelTest, ModelInput_Create) {
    const ModelInput expected{
            .cpuPolicyAverageFrequencyHz = {100, 101, 102},
            .cpuCoreIdleTimesPercentage = {0.0, 0.1, 0.0, 0.3, 0.0, 0.0, 0.0, 0.7},
            .averageFrameTime = 16ns,
            .numRenderedFrames = 100,
            .previousThrottleDecision = ThrottleDecision::THROTTLE_70,
    };
    ModelInput actual;
    ASSERT_TRUE(actual.Init(
            {
                    {.policyId = 0, .averageFrequencyHz = 100},
                    {.policyId = 4, .averageFrequencyHz = 101},
                    {.policyId = 6, .averageFrequencyHz = 102},
            },
            {
                    {.cpuId = 3, .idleTimeFraction = 0.3},
                    {.cpuId = 0, .idleTimeFraction = 0.0},
                    {.cpuId = 1, .idleTimeFraction = 0.1},
                    {.cpuId = 7, .idleTimeFraction = 0.7},
                    {.cpuId = 2, .idleTimeFraction = 0.0},
                    {.cpuId = 4, .idleTimeFraction = 0.0},
                    {.cpuId = 6, .idleTimeFraction = 0.0},
                    {.cpuId = 5, .idleTimeFraction = 0.0},
            },
            16ns, 100, ThrottleDecision::THROTTLE_70));
    ASSERT_EQ(actual, expected);
}

TEST(ModelTest, ModelInput_Create_failsWithOutOfOrderFrquencies) {
    ASSERT_FALSE(ModelInput().Init(
            {
                    {.policyId = 0, .averageFrequencyHz = 100},
                    {.policyId = 6, .averageFrequencyHz = 102},
                    {.policyId = 4, .averageFrequencyHz = 101},
            },
            {
                    {.cpuId = 3, .idleTimeFraction = 0.3},
                    {.cpuId = 0, .idleTimeFraction = 0.0},
                    {.cpuId = 1, .idleTimeFraction = 0.1},
                    {.cpuId = 7, .idleTimeFraction = 0.7},
                    {.cpuId = 2, .idleTimeFraction = 0.0},
                    {.cpuId = 4, .idleTimeFraction = 0.0},
                    {.cpuId = 6, .idleTimeFraction = 0.0},
                    {.cpuId = 5, .idleTimeFraction = 0.0},
            },
            16ns, 100, ThrottleDecision::THROTTLE_70));
}

TEST(ModelTest, RunModel_randomInputs) {
    std::default_random_engine generator;
    std::uniform_real_distribution<double> frequencyDistribution(0, 1e6);
    std::uniform_real_distribution<double> idleTimesDistribution(0, 1);
    std::uniform_int_distribution<uint32_t> frameTimeDistribution(1, 100);
    std::uniform_int_distribution<uint16_t> numRenderedFramesDistribution(1, 20);
    std::uniform_int_distribution<uint32_t> throttleDecisionDistribution(0, 3);

    const auto randomModelInput = [&]() {
        return ModelInput{
                .cpuPolicyAverageFrequencyHz = {frequencyDistribution(generator),
                                                frequencyDistribution(generator),
                                                frequencyDistribution(generator)},
                .cpuCoreIdleTimesPercentage =
                        {idleTimesDistribution(generator), idleTimesDistribution(generator),
                         idleTimesDistribution(generator), idleTimesDistribution(generator),
                         idleTimesDistribution(generator), idleTimesDistribution(generator),
                         idleTimesDistribution(generator), idleTimesDistribution(generator)},
                .averageFrameTime = std::chrono::nanoseconds(frameTimeDistribution(generator)),
                .numRenderedFrames = numRenderedFramesDistribution(generator),
                .previousThrottleDecision =
                        static_cast<ThrottleDecision>(throttleDecisionDistribution(generator)),
        };
    };

    for (int i = 0; i < 10; i++) {
        std::deque<ModelInput> modelInputs{randomModelInput(), randomModelInput(),
                                           randomModelInput()};
        RunModel(modelInputs);
    }
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
