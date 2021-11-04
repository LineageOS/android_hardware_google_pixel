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

#define LOG_TAG "powerhal-libperfmgr"

#include "RealFilesystem.h"

#include <android-base/logging.h>
#include <dirent.h>

#include <fstream>
#include <istream>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

std::vector<std::string> RealFilesystem::listDirectory(const std::string &path) const {
    // We can't use std::filesystem, see aosp/894015 & b/175635923.
    auto dir = std::unique_ptr<DIR, decltype(&closedir)>{opendir(path.c_str()), closedir};
    if (!dir) {
        LOG(ERROR) << "Failed to open directory " << path;
    }
    std::vector<std::string> entries;
    dirent *entry;
    while ((entry = readdir(&*dir)) != nullptr) {
        entries.emplace_back(entry->d_name);
    }
    return entries;
}

std::unique_ptr<std::istream> RealFilesystem::readFileStream(const std::string &path) const {
    return std::make_unique<std::ifstream>(path);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
