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

#define LOG_TAG "powerhal-adaptivecpu"

#include "ModelTreeNode.h"

#include <android-base/logging.h>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

TreeNode::~TreeNode() {}

bool SplitNode::Equal(const TreeNode &other) const {
    return other.EqualToSplit(*this);
}

bool SplitNode::EqualToSplit(const SplitNode &other) const {
    return (other.mFeature == mFeature && other.mValueIndex == mValueIndex &&
            std::abs(other.mThreshold - mThreshold) <= 0.00001 && mLeft->Equal(*other.mLeft) &&
            mRight->Equal(*other.mRight));
}

bool SplitNode::EqualToLeaf(const LeafNode &other __attribute__((unused))) const {
    return false;
}

bool LeafNode::Equal(const TreeNode &other) const {
    return other.EqualToLeaf(*this);
}

bool LeafNode::EqualToSplit(const SplitNode &other __attribute__((unused))) const {
    return false;
}

bool LeafNode::EqualToLeaf(const LeafNode &other) const {
    return other.mDecision == mDecision;
}

proto::ThrottleDecision SplitNode::EvaluateSubtree(
        const std::deque<ModelInput> &model_inputs) const {
    // Node's mValueIndex determines index in model_inputs deque.
    ModelInput modelInput = model_inputs[mValueIndex];
    // Find the feature value corresponding to this split node in modelInput.
    float featureValue;
    switch (mFeature) {
        case proto::Feature::CPU_POLICY_AVG_FREQ_0:
            featureValue = modelInput.cpuPolicyAverageFrequencyHz[0];
            break;
        case proto::Feature::CPU_POLICY_AVG_FREQ_1:
            featureValue = modelInput.cpuPolicyAverageFrequencyHz[1];
            break;
        case proto::Feature::CPU_POLICY_AVG_FREQ_2:
            featureValue = modelInput.cpuPolicyAverageFrequencyHz[2];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_0:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[0];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[1];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_2:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[2];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_3:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[3];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[4];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_5:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[5];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_6:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[6];
            break;
        case proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_7:
            featureValue = modelInput.cpuCoreIdleTimesPercentage[7];
            break;
        case proto::Feature::AVG_DURATION:
            featureValue = modelInput.workDurationFeatures.averageDuration.count();
            break;
        case proto::Feature::MAX_DURATION:
            featureValue = modelInput.workDurationFeatures.maxDuration.count();
            break;
        case proto::Feature::NUM_DURATIONS:
            featureValue = modelInput.workDurationFeatures.numDurations;
            break;
        case proto::Feature::NUM_MISSED_DEADLINES:
            featureValue = modelInput.workDurationFeatures.numMissedDeadlines;
            break;
        default:
            return proto::ThrottleDecision::NO_THROTTLE;
    }

    if (featureValue <= mThreshold) {
        return mLeft->EvaluateSubtree(model_inputs);
    } else {
        return mRight->EvaluateSubtree(model_inputs);
    }
}

proto::ThrottleDecision LeafNode::EvaluateSubtree(const std::deque<ModelInput> &model_inputs
                                                  __attribute__((unused))) const {
    return mDecision;
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
