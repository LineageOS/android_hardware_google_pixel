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

#include <string>

#include "ModelTree.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

class TreeReader {
  public:
    // Reading directly from a proto message for now (public for tests).
    static bool DeserializeProtoTreeToMemory(const proto::ModelTree &protoTree,
                                             std::unique_ptr<ModelTree> *model);
    // Helper function to read temporary out.bin file which contains the message.
    static bool ReadProtoTreeFromFile(std::string_view filePath, proto::ModelTree *protoTree);
    static bool DeserializeTreeFromFile(std::string_view filePath,
                                        std::unique_ptr<ModelTree> *model);

  private:
    // Helper function that holds deserialization logic.
    static bool DeserializeRecursive(const proto::ModelTree &protoTree,
                                     const std::map<proto::Feature, float> &means,
                                     const std::map<proto::Feature, float> &stds,
                                     std::unique_ptr<TreeNode> *next, int *nodeIndex,
                                     int currentTreeDepth);
    static bool ReadProtoTreeFromString(const std::string &content, proto::ModelTree *protoTree);
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
