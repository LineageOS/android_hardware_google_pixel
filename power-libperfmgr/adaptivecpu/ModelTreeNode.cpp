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
            other.mThreshold == mThreshold && mLeft->Equal(*other.mLeft) &&
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

proto::ThrottleDecision SplitNode::EvaluateSubtree(const std::deque<ModelInput> &model_inputs
                                                   __attribute__((unused))) const {
    LOG(ERROR) << "TODO: implement function.";

    return proto::ThrottleDecision::NO_THROTTLE;
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
