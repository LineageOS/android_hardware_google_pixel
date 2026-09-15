/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android_hardware_usb_flags.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace usb_flags = android::hardware::usb::flags;

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool isPixel8Series() {
    std::string device = to_lower(android::base::GetProperty("ro.product.device", ""));
    return device == "shiba" || device == "husky" || device == "akita";
}

bool isPixel10Series() {
    std::string device = to_lower(android::base::GetProperty("ro.product.device", ""));
    return device == "frankel" || device == "blazer" || device == "mustang" || device == "rango";
}

int main() {
    bool aoa_userspace_enabled;

    if (isPixel10Series()) {
        aoa_userspace_enabled = usb_flags::enable_uaoa_p10();
    } else if (isPixel8Series()) {
        aoa_userspace_enabled = usb_flags::enable_uaoa_p8();
    } else {
        aoa_userspace_enabled = usb_flags::enable_uaoa_p6();
    }

    if (!android::base::SetProperty("ro.vendor.usb.userspace.aoa.enabled",
                                    aoa_userspace_enabled ? "true" : "false")) {
        LOG(FATAL) << "Failed to set property ro.vendor.usb.userspace.aoa.enabled"
                   << aoa_userspace_enabled;
    }

    return 0;
}
