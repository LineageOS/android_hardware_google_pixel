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
#include <set>

#include "adaptivecpu/ModelTreeNode.h"
#include "mocks.h"

using std::chrono_literals::operator""ns;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::UnorderedElementsAre;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

TEST(ModelTreeTest, ModelTree_RunModel_Throttle70) {
    // Define a small model.
    std::unique_ptr<TreeNode> l2 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);
    std::unique_ptr<TreeNode> r2 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> l1 = std::make_unique<SplitNode>(
            std::move(l2), std::move(r2), 0.55678, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4, 2);
    std::unique_ptr<TreeNode> r1 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> root = std::make_unique<SplitNode>(
            std::move(l1), std::move(r1), 0.22345, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1, 2);

    // Most values can be random, only hardcoding inputs for idle times % used in the mock model.
    std::default_random_engine generator;
    std::uniform_real_distribution<double> frequencyDistribution(0, 1e6);
    std::uniform_int_distribution<uint32_t> frameTimeDistribution(1, 100);
    std::uniform_int_distribution<uint16_t> numRenderedFramesDistribution(1, 20);
    std::uniform_int_distribution<uint32_t> throttleDecisionDistribution(0, 3);

    const auto randomModelInput = [&]() {
        return ModelInput{
                .cpuPolicyAverageFrequencyHz = {frequencyDistribution(generator),
                                                frequencyDistribution(generator),
                                                frequencyDistribution(generator)},
                .cpuCoreIdleTimesPercentage = {0.1, 0.1, 0.1, 0.5, 0.5, 0.5, 0.5, 0.5},
                .workDurationFeatures =
                        {.averageDuration =
                                 std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .maxDuration = std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .numMissedDeadlines = numRenderedFramesDistribution(generator),
                         .numDurations = numRenderedFramesDistribution(generator)},
                .previousThrottleDecision =
                        static_cast<ThrottleDecision>(throttleDecisionDistribution(generator)),
        };
    };

    proto::ThrottleDecision decision;
    for (int i = 0; i < 10; i++) {
        std::deque<ModelInput> modelInputs{randomModelInput(), randomModelInput(),
                                           randomModelInput()};
        decision = root->EvaluateSubtree(modelInputs);
        ASSERT_EQ(decision, proto::ThrottleDecision::THROTTLE_70);
    }
}

TEST(ModelTreeTest, ModelTree_RunModel_NoThrottle) {
    // Define a small model.
    std::unique_ptr<TreeNode> l2 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);
    std::unique_ptr<TreeNode> r2 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);

    std::unique_ptr<TreeNode> l1 = std::make_unique<SplitNode>(
            std::move(l2), std::move(r2), 0.55678, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4, 2);
    std::unique_ptr<TreeNode> r1 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);

    std::unique_ptr<TreeNode> root = std::make_unique<SplitNode>(
            std::move(l1), std::move(r1), 0.22345, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1, 2);

    // Most values can be random, only hardcoding inputs for idle times % used in the mock model.
    std::default_random_engine generator;
    std::uniform_real_distribution<double> frequencyDistribution(0, 1e6);
    std::uniform_int_distribution<uint32_t> frameTimeDistribution(1, 100);
    std::uniform_int_distribution<uint16_t> numRenderedFramesDistribution(1, 20);
    std::uniform_int_distribution<uint32_t> throttleDecisionDistribution(0, 3);

    const auto randomModelInput = [&]() {
        return ModelInput{
                .cpuPolicyAverageFrequencyHz = {frequencyDistribution(generator),
                                                frequencyDistribution(generator),
                                                frequencyDistribution(generator)},
                .cpuCoreIdleTimesPercentage = {0.1, 0.1, 0.1, 0.5, 0.5, 0.5, 0.5, 0.5},
                .workDurationFeatures =
                        {.averageDuration =
                                 std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .maxDuration = std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .numMissedDeadlines = numRenderedFramesDistribution(generator),
                         .numDurations = numRenderedFramesDistribution(generator)},
                .previousThrottleDecision =
                        static_cast<ThrottleDecision>(throttleDecisionDistribution(generator)),
        };
    };

    proto::ThrottleDecision decision;
    for (int i = 0; i < 10; i++) {
        std::deque<ModelInput> modelInputs{randomModelInput(), randomModelInput(),
                                           randomModelInput()};
        decision = root->EvaluateSubtree(modelInputs);
        ASSERT_EQ(decision, proto::ThrottleDecision::NO_THROTTLE);
    }
}

TEST(ModelTreeTest, ModelTree_RandomInputs) {
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
                .workDurationFeatures =
                        {.averageDuration =
                                 std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .maxDuration = std::chrono::nanoseconds(frameTimeDistribution(generator)),
                         .numMissedDeadlines = numRenderedFramesDistribution(generator),
                         .numDurations = numRenderedFramesDistribution(generator)},
                .previousThrottleDecision =
                        static_cast<ThrottleDecision>(throttleDecisionDistribution(generator)),
        };
    };

    std::unique_ptr<TreeNode> l2 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);
    std::unique_ptr<TreeNode> r2 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> l1 = std::make_unique<SplitNode>(
            std::move(l2), std::move(r2), 0.55678, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4, 2);
    std::unique_ptr<TreeNode> r1 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> root = std::make_unique<SplitNode>(
            std::move(l1), std::move(r1), 0.22345, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1, 2);

    for (int i = 0; i < 10; i++) {
        std::deque<ModelInput> modelInputs{randomModelInput(), randomModelInput(),
                                           randomModelInput()};
        root->EvaluateSubtree(modelInputs);
    }
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
