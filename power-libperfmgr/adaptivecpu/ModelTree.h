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

#include "ModelTreeNode.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

// TODO(jasnamarin@): Revisit MAX_TREE_DEPTH, change to a larger number if needed.
constexpr uint32_t MAX_TREE_DEPTH = 128;
constexpr uint64_t MAX_NUM_NODES = 1000000;

class ModelTree {
  public:
    ModelTree(std::unique_ptr<TreeNode> root) : mRoot(std::move(root)) {}
    proto::ThrottleDecision RunModel(const std::deque<ModelInput> &model_inputs);
    const std::unique_ptr<TreeNode> &GetModel() const;
    bool operator==(const ModelTree &other) const;

  private:
    const std::unique_ptr<TreeNode> mRoot;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
