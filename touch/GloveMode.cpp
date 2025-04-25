/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GloveMode.h"

#include <android-base/properties.h>

using ::android::base::GetBoolProperty;
using ::android::base::SetProperty;

namespace {

constexpr const char* kTouchSensitivityProp = "persist.vendor.touch_sensitivity_mode";

}  // anonymous namespace

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

ndk::ScopedAStatus GloveMode::getEnabled(bool* _aidl_return) {
    bool enabled = GetBoolProperty(kTouchSensitivityProp, false);

    *_aidl_return = enabled;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GloveMode::setEnabled(bool enable) {
    SetProperty(kTouchSensitivityProp, enable ? "1" : "0");

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
