/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

enum class MiscWriterActions : int32_t {
  kSetDarkThemeFlag = 0,
  kClearDarkThemeFlag,
  kSetSotaFlag,
  kClearSotaFlag,
  kSetEnablePkvmFlag,
  kSetDisablePkvmFlag,
  kSetWristOrientationFlag,
  kClearWristOrientationFlag,
  kWriteTimeFormat,
  kWriteTimeOffset,
  kSetMaxRamSize,
  kClearMaxRamSize,
  kWriteTimeRtcOffset,
  kWriteTimeMinRtc,
  kSetSotaConfig,
  kWriteDstTransition,
  kWriteDstOffset,
  kSetDisplayMode,
  kClearDisplayMode,

  kUnset = -1,
};

class MiscWriter {
  public:
    // sync with bootloader's abl bootloader_message.h
    typedef struct bootloader_message_vendor {
        char theme[32];
        char sota[32];
        char pkvm[32];
        char wrist_orientation[32];
        char timeformat[32];
        char timeoffset[32];
        char max_ram_size[32];
        char sota_client_state[40];
        char timertcoffset[32];
        char timeminrtc[32];
        char faceauth_eval[32];
        char sota_schedule_shipmode[32];
        char dsttransition[32];
        char dstoffset[32];
        char user_preferred_resolution[32];
    } __attribute__((__packed__)) bootloader_message_vendor_t;

    static constexpr uint32_t kThemeFlagOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, theme);
    static constexpr char kDarkThemeFlag[] = "theme-dark";
    static constexpr uint32_t kSotaFlagOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, sota);
    static constexpr char kSotaFlag[] = "enable-sota";
    static constexpr uint32_t kPkvmFlagOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, pkvm);
    static constexpr char kEnablePkvmFlag[] = "enable-pkvm";
    static constexpr char kDisablePkvmFlag[] = "disable-pkvm";
    static constexpr uint32_t kWristOrientationFlagOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, wrist_orientation);
    static constexpr char kWristOrientationFlag[] = "wrist-orientation=";
    static constexpr uint32_t kTimeFormatValOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, timeformat);
    static constexpr char kTimeFormat[] = "timeformat=";
    static constexpr uint32_t kTimeOffsetValOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, timeoffset);
    static constexpr char kTimeOffset[] = "timeoffset=";
    static constexpr uint32_t kMaxRamSizeOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, max_ram_size);
    static constexpr char kMaxRamSize[] = "max-ram-size=";
    static constexpr uint32_t kSotaStateOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, sota_client_state);
    static constexpr uint32_t kRTimeRtcOffsetValOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, timertcoffset);
    static constexpr char kTimeRtcOffset[] = "timertcoffset=";
    static constexpr uint32_t kRTimeMinRtcValOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, timeminrtc);
    static constexpr char kTimeMinRtc[] = "timeminrtc=";
    static constexpr uint32_t kFaceauthEvalValOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, faceauth_eval);
    static constexpr uint32_t kSotaScheduleShipmodeOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, sota_schedule_shipmode);
    static constexpr uint32_t kDstTransitionOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, dsttransition);
    static constexpr char kDstTransition[] = "dst-transition=";
    static constexpr uint32_t kDstOffsetOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, dstoffset);
    static constexpr char kDstOffset[] = "dst-offset=";
    static constexpr uint32_t kDisplayModeOffsetInVendorSpace =
            offsetof(bootloader_message_vendor_t, user_preferred_resolution);
    static constexpr char kDisplayModePrefix[] = "mode=";

    // Minimum and maximum valid value for max-ram-size
    static constexpr int32_t kRamSizeDefault = -1;
    static constexpr uint32_t kRamSizeMin = 2048;
    static constexpr uint32_t kRamSizeMax = 65536;

    // Minimum and maximum time zone are -12 and 14 hours from GMT
    static constexpr int32_t kMinTimeOffset = -12 * 60 * 60 * 1000;
    static constexpr int32_t kMaxTimeOffset = 14 * 60 * 60 * 1000;

    // Maximum display mode string length
    static constexpr size_t kDisplayModeMaxSize = 32 - sizeof(kDisplayModePrefix);

    // Returns true of |size| bytes data starting from |offset| is fully inside the vendor space.
    static bool OffsetAndSizeInVendorSpace(size_t offset, size_t size);
    // Writes the given data to the vendor space in /misc partition, at the given offset. Note that
    // offset is in relative to the start of the vendor space.
    static bool WriteMiscPartitionVendorSpace(const void *data, size_t size, size_t offset,
                                              std::string *err);

    explicit MiscWriter(const MiscWriterActions &action) : action_(action) {}
    explicit MiscWriter(const MiscWriterActions &action, const char data)
        : action_(action), chardata_(data) {}
    explicit MiscWriter(const MiscWriterActions &action, std::string data)
        : action_(action), stringdata_(data) {}

    // Performs the stored MiscWriterActions. If |override_offset| is set, writes to the input
    // offset in the vendor space of /misc instead of the default offset.
    bool PerformAction(std::optional<size_t> override_offset = std::nullopt);

  private:
    MiscWriterActions action_{MiscWriterActions::kUnset};
    char chardata_{'0'};
    std::string stringdata_;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
