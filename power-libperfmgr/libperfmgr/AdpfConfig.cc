/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "libperfmgr"

#include "perfmgr/AdpfConfig.h"

#include <android-base/logging.h>
#include <android-base/parsedouble.h>
#include <android-base/properties.h>

#include <string>

namespace android {
namespace perfmgr {

int64_t AdpfConfig::getPidIInitDivI() {
    return (mPidI == 0) ? 0 : static_cast<int64_t>(mPidIInit / mPidI);
}
int64_t AdpfConfig::getPidIHighDivI() {
    return (mPidI == 0) ? 0 : static_cast<int64_t>(mPidIHigh / mPidI);
}
int64_t AdpfConfig::getPidILowDivI() {
    return (mPidI == 0) ? 0 : static_cast<int64_t>(mPidILow / mPidI);
}

}  // namespace perfmgr
}  // namespace android
