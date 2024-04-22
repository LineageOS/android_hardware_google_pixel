/*
 * Copyright 2023 The Android Open Source Project
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

#include <cstdint>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

enum class AdpfErrorCode : int32_t { ERR_OK = 0, ERR_BAD_STATE = -1, ERR_BAD_ARG = -2 };

enum class AdpfVoteType : int32_t {
    CPU_VOTE_DEFAULT = 0,
    CPU_LOAD_UP,
    CPU_LOAD_RESET,
    CPU_LOAD_RESUME,
    VOTE_POWER_EFFICIENCY,
    GPU_LOAD_UP,
    GPU_LOAD_DOWN,
    GPU_LOAD_RESET,
    GPU_CAPACITY,
    VOTE_TYPE_SIZE
};

constexpr const char *AdpfVoteTypeToStr(AdpfVoteType voteType) {
    switch (voteType) {
        case AdpfVoteType::CPU_VOTE_DEFAULT:
            return "CPU_VOTE_DEFAULT";
        case AdpfVoteType::CPU_LOAD_UP:
            return "CPU_LOAD_UP";
        case AdpfVoteType::CPU_LOAD_RESET:
            return "CPU_LOAD_RESET";
        case AdpfVoteType::CPU_LOAD_RESUME:
            return "CPU_LOAD_RESUME";
        case AdpfVoteType::VOTE_POWER_EFFICIENCY:
            return "VOTE_POWER_EFFICIENCY";
        case AdpfVoteType::GPU_LOAD_UP:
            return "GPU_LOAD_UP";
        case AdpfVoteType::GPU_LOAD_DOWN:
            return "GPU_LOAD_DOWN";
        case AdpfVoteType::GPU_LOAD_RESET:
            return "GPU_LOAD_RESET";
        case AdpfVoteType::GPU_CAPACITY:
            return "GPU_CAPACITY";
        default:
            return "INVALID_VOTE";
    }
}

constexpr int kUclampMin{0};
constexpr int kUclampMax{1024};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
