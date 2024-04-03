/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_VENDORATOMINTVALUEUTIL_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_VENDORATOMINTVALUEUTIL_H

using aidl::android::frameworks::stats::VendorAtomValue;

constexpr int intValue = static_cast<int>(VendorAtomValue::intValue);
constexpr int longValue = static_cast<int>(VendorAtomValue::longValue);
constexpr int floatValue = static_cast<int>(VendorAtomValue::floatValue);
constexpr int stringValue = static_cast<int>(VendorAtomValue::stringValue);

static int64_t getVendorAtomIntValue(const VendorAtomValue &v) {
    switch (v.getTag()) {
        case VendorAtomValue::intValue:
            return v.get<VendorAtomValue::intValue>();
            break;
        case VendorAtomValue::longValue:
            return v.get<VendorAtomValue::longValue>();
            break;
        default:
            return -1;
    }
}

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_ATOMVALUEUTIL_H
