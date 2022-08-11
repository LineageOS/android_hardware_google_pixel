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

#include "TreeReader.h"

#include <android-base/logging.h>

#include <fstream>
#include <string>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

// Proto tree contains an array of nodes serialized in pre-order. To deserialize this array
// recursively, we need to know which node is being processed in each function call - for this we
// use nodeIndex.
bool TreeReader::DeserializeRecursive(const proto::ModelTree &protoTree,
                                      std::unique_ptr<TreeNode> *next, int *nodeIndex,
                                      int currentTreeDepth) {
    // Pre-increment operator sets the expression after incrementing the value
    proto::Node currNode = protoTree.nodes(++(*nodeIndex));
    if (currentTreeDepth > MAX_TREE_DEPTH) {
        LOG(ERROR) << "Tree depth exceedes " << currentTreeDepth << " nodes!";
        return false;
    }

    if (currNode.has_split_node()) {
        // The next node in the array is the left child of the current node. (pre-order)
        std::unique_ptr<TreeNode> left;
        if (!TreeReader::DeserializeRecursive(protoTree, &left, nodeIndex, currentTreeDepth + 1)) {
            return false;
        }
        // When the whole left sub-tree is deserialized, repeat the same for the right.
        std::unique_ptr<TreeNode> right;
        if (!TreeReader::DeserializeRecursive(protoTree, &right, nodeIndex, currentTreeDepth + 1)) {
            return false;
        }

        *next = std::make_unique<SplitNode>(
                std::move(left), std::move(right), currNode.split_node().threshold(),
                currNode.split_node().feature(), currNode.split_node().value_index());
        return true;
    } else {
        *next = std::make_unique<LeafNode>(currNode.leaf_node().decision());
        return true;
    }
}

bool TreeReader::DeserializeProtoTreeToMemory(const proto::ModelTree &protoTree,
                                              std::unique_ptr<ModelTree> *model) {
    // Traverse proto tree in pre-order, create an in-memory tree.
    int nodeIndex = -1;
    int currentTreeDepth = 0;
    std::unique_ptr<TreeNode> root;
    if (!TreeReader::DeserializeRecursive(protoTree, &root, &nodeIndex, currentTreeDepth)) {
        LOG(ERROR) << "Model not successfully deserialized!";
        return false;
    }
    // Read feature statistics.
    std::map<proto::Feature, float> means;
    for (auto i = 0; i < protoTree.feature_means().size(); i++) {
        means.insert(
                {protoTree.feature_means(i).feature(), protoTree.feature_means(i).statistic()});
    }
    std::map<proto::Feature, float> stds;
    for (auto i = 0; i < protoTree.feature_stds().size(); i++) {
        stds.insert({protoTree.feature_stds(i).feature(), protoTree.feature_stds(i).statistic()});
    }
    // Save the new tree and feature stats into the in-memory ModelTree object.
    *model = std::make_unique<ModelTree>(std::move(root), means, stds);

    return true;
}

bool TreeReader::ReadProtoTreeFromString(const std::string &content, proto::ModelTree *protoTree) {
    if (!protoTree->ParseFromString(content)) {
        LOG(ERROR) << "Failed to parse serialized tree from string!";
        return false;
    }

    return true;
}

bool TreeReader::ReadProtoTreeFromFile(std::string_view filePath, proto::ModelTree *protoTree) {
    std::ifstream binary_file(filePath.data(), std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(binary_file)),
                        std::istreambuf_iterator<char>());
    if (!TreeReader::ReadProtoTreeFromString(content, protoTree)) {
        LOG(ERROR) << "Failed to parse serialized tree from file " << filePath;
        return false;
    }

    return true;
}

bool TreeReader::DeserializeTreeFromFile(std::string_view filePath,
                                         std::unique_ptr<ModelTree> *model) {
    proto::ModelTree protoTree;
    if (!TreeReader::ReadProtoTreeFromFile(filePath, &protoTree)) {
        return false;
    }
    if (!TreeReader::DeserializeProtoTreeToMemory(protoTree, model)) {
        return false;
    }

    return true;
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
