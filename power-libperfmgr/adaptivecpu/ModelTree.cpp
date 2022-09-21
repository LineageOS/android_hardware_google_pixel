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

#include "ModelTree.h"

#include <android-base/logging.h>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

proto::ThrottleDecision ModelTree::RunModel(const std::deque<ModelInput> &model_inputs
                                            __attribute__((unused))) {
    return mRoot->EvaluateSubtree(model_inputs);
}

const std::unique_ptr<TreeNode> &ModelTree::GetModel() const {
    return mRoot;
}

bool ModelTree::operator==(const ModelTree &other) const {
    return mRoot->Equal(*other.mRoot);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
