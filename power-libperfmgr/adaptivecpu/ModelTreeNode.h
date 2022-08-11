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

#pragma once

#include <tree.pb.h>

#include <deque>
#include <vector>

#include "Model.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

class SplitNode;
class LeafNode;

// Abstract parent class for different types of decision tree nodes.
class TreeNode {
  public:
    virtual ~TreeNode() = 0;
    // Recursive method used to determine if two trees are equal.
    virtual bool Equal(const TreeNode &other) const = 0;
    // Because of limited support for dynamic type checking in C++,
    // we define different comparison functions for different node types
    // and call them from the more general Equal(TreeNode) function.
    virtual bool EqualToLeaf(const LeafNode &other) const = 0;
    virtual bool EqualToSplit(const SplitNode &other) const = 0;
    virtual proto::ThrottleDecision EvaluateSubtree(
            const std::deque<ModelInput> &model_inputs) const = 0;
};

// Internal tree node used to make a decision to go left or right
// based on the value of its feature compared to its threshold.
class SplitNode : public TreeNode {
  public:
    virtual ~SplitNode() {}
    SplitNode(std::unique_ptr<TreeNode> left, std::unique_ptr<TreeNode> right, float threshold,
              proto::Feature feature, int valueIndex)
        : mLeft(std::move(left)),
          mRight(std::move(right)),
          mThreshold(threshold),
          mFeature(feature),
          mValueIndex(valueIndex) {}

    bool Equal(const TreeNode &other) const override;
    bool EqualToLeaf(const LeafNode &other) const override;
    bool EqualToSplit(const SplitNode &other) const override;
    proto::ThrottleDecision EvaluateSubtree(
            const std::deque<ModelInput> &model_inputs) const override;

  private:
    const std::unique_ptr<TreeNode> mLeft;
    const std::unique_ptr<TreeNode> mRight;

    const float mThreshold;
    const proto::Feature mFeature;
    const int mValueIndex;
};

// Leaf node only contains final throttle decision.
class LeafNode : public TreeNode {
  public:
    virtual ~LeafNode() {}
    LeafNode(proto::ThrottleDecision d) : mDecision(d) {}

    bool Equal(const TreeNode &other) const override;
    bool EqualToLeaf(const LeafNode &other) const override;
    bool EqualToSplit(const SplitNode &other) const override;
    proto::ThrottleDecision EvaluateSubtree(
            const std::deque<ModelInput> &model_inputs) const override;

  private:
    proto::ThrottleDecision mDecision;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
