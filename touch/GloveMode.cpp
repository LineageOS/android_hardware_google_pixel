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
    GetBoolProperty(kTouchSensitivityProp, false);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GloveMode::setEnabled(bool enable) {
    SetProperty(kTouchSensitivityProp, enabled ? "1" : "0");

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
