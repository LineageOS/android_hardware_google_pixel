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

#include <android-base/logging.h>
#include <gtest/gtest.h>

#include <random>
#include <set>

#include "adaptivecpu/TreeReader.h"
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

TEST(TreeReaderTest, TreeReader_DeserializeProtoTree_CompareEqual) {
    // Construct proto tree with the total of 5 nodes (2 split nodes).
    proto::ModelTree protoTree;

    proto::Node *node = protoTree.add_nodes();
    proto::SplitNode *splitNode = node->mutable_split_node();
    splitNode->set_feature(proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1);
    splitNode->set_value_index(2);
    splitNode->set_threshold(12.345);

    node = protoTree.add_nodes();
    splitNode = node->mutable_split_node();
    splitNode->set_feature(proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4);
    splitNode->set_value_index(2);
    splitNode->set_threshold(45.678);

    node = protoTree.add_nodes();
    proto::LeafNode *leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::THROTTLE_70);

    node = protoTree.add_nodes();
    leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::NO_THROTTLE);

    node = protoTree.add_nodes();
    leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::NO_THROTTLE);

    // Add feature statistics - 15 means and std devs.
    for (int i = 0; i < 15; i++) {
        proto::FeatureStatistic *stat = protoTree.add_feature_means();
        stat->set_feature(proto::Feature(i));
        stat->set_statistic(10.0);
    }
    for (int i = 0; i < 15; i++) {
        proto::FeatureStatistic *stat = protoTree.add_feature_stds();
        stat->set_feature(proto::Feature(i));
        stat->set_statistic(1.0);
    }

    // Construct equivalent cpp tree. (And denormalize thresholds.)
    std::unique_ptr<TreeNode> l2 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);
    std::unique_ptr<TreeNode> r2 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> l1 = std::make_unique<SplitNode>(
            std::move(l2), std::move(r2), 0.55678, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4, 2);
    std::unique_ptr<TreeNode> r1 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);

    std::unique_ptr<TreeNode> root = std::make_unique<SplitNode>(
            std::move(l1), std::move(r1), 0.22345, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1, 2);

    std::unique_ptr<ModelTree> model = std::make_unique<ModelTree>(std::move(root));

    // Call function for deserializing proto tree.
    std::unique_ptr<ModelTree> deserializedTree;
    TreeReader::DeserializeProtoTreeToMemory(protoTree, &deserializedTree);
    // Compare.
    ASSERT_EQ(*model, *deserializedTree);
}

TEST(TreeReaderTest, TreeReader_DeserializeProtoTree_CompareDifferent) {
    // Construct proto tree with the total of 5 nodes (2 split nodes).
    proto::ModelTree protoTree;

    proto::Node *node = protoTree.add_nodes();
    proto::SplitNode *splitNode = node->mutable_split_node();
    splitNode->set_feature(proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1);
    splitNode->set_value_index(2);
    splitNode->set_threshold(12.345);

    node = protoTree.add_nodes();
    splitNode = node->mutable_split_node();
    splitNode->set_feature(proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4);
    splitNode->set_value_index(2);
    splitNode->set_threshold(45.678);

    node = protoTree.add_nodes();
    proto::LeafNode *leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::THROTTLE_70);

    node = protoTree.add_nodes();
    leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::NO_THROTTLE);

    node = protoTree.add_nodes();
    leafNode = node->mutable_leaf_node();
    leafNode->set_decision(proto::ThrottleDecision::NO_THROTTLE);

    // Add feature statistics - 15 means and std devs.
    for (int i = 0; i < 15; i++) {
        proto::FeatureStatistic *stat = protoTree.add_feature_means();
        stat->set_feature(proto::Feature(i));
        stat->set_statistic(10.0);
    }
    for (int i = 0; i < 15; i++) {
        proto::FeatureStatistic *stat = protoTree.add_feature_stds();
        stat->set_feature(proto::Feature(i));
        stat->set_statistic(1.0);
    }

    // Construct a cpp tree with different decisions.
    std::unique_ptr<TreeNode> l2 = std::make_unique<LeafNode>(proto::ThrottleDecision::NO_THROTTLE);
    std::unique_ptr<TreeNode> r2 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);

    std::unique_ptr<TreeNode> l1 = std::make_unique<SplitNode>(
            std::move(l2), std::move(r2), 0.55678, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_4, 2);
    std::unique_ptr<TreeNode> r1 = std::make_unique<LeafNode>(proto::ThrottleDecision::THROTTLE_70);

    std::unique_ptr<TreeNode> root = std::make_unique<SplitNode>(
            std::move(l1), std::move(r1), 0.22345, proto::Feature::CPU_CORE_IDLE_TIME_PERCENT_1, 2);

    std::unique_ptr<ModelTree> model = std::make_unique<ModelTree>(std::move(root));

    // Call function for deserializing proto tree.
    std::unique_ptr<ModelTree> deserializedTree;
    TreeReader::DeserializeProtoTreeToMemory(protoTree, &deserializedTree);
    // Compare.
    ASSERT_FALSE(*model == *deserializedTree);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
