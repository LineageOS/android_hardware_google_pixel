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

#define LOG_TAG "bootcontrolhal"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <bootloader_message/bootloader_message.h>

#include "BootControlShared.h"

namespace android {
namespace hardware {
namespace boot {
namespace V1_1 {
namespace implementation {

using android::base::unique_fd;
using android::hardware::boot::V1_1::MergeStatus;

constexpr off_t kBootloaderControlOffset = offsetof(bootloader_message_ab, slot_suffix);

static uint32_t CRC32(const uint8_t *buf, size_t size) {
    static uint32_t crc_table[256];

    // Compute the CRC-32 table only once.
    if (!crc_table[1]) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (uint32_t j = 0; j < 8; ++j) {
                uint32_t mask = -(crc & 1);
                crc = (crc >> 1) ^ (0xEDB88320 & mask);
            }
            crc_table[i] = crc;
        }
    }

    uint32_t ret = -1;
    for (size_t i = 0; i < size; ++i) {
        ret = (ret >> 8) ^ crc_table[(ret ^ buf[i]) & 0xFF];
    }

    return ~ret;
}

uint32_t ComputeChecksum(const bootloader_control *boot_ctrl) {
    return CRC32(reinterpret_cast<const uint8_t *>(boot_ctrl),
                 offsetof(bootloader_control, crc32_le));
}

static bool LoadUpdateState(const std::string &misc_device, bootloader_control *buffer) {
    unique_fd fd(open(misc_device.c_str(), O_RDONLY));
    if (fd < 0) {
        PLOG(ERROR) << "failed to open " << misc_device;
        return false;
    }
    if (lseek(fd, kBootloaderControlOffset, SEEK_SET) != kBootloaderControlOffset) {
        PLOG(ERROR) << "failed to lseek " << misc_device;
        return false;
    }
    if (!android::base::ReadFully(fd, buffer, sizeof(bootloader_control))) {
        PLOG(ERROR) << "failed to read " << misc_device;
        return false;
    }
    return true;
}

static bool SaveUpdateState(const std::string &misc_device, bootloader_control *buffer) {
    unique_fd fd(open(misc_device.c_str(), O_WRONLY | O_SYNC));
    if (fd < 0) {
        PLOG(ERROR) << "failed to open " << misc_device;
        return false;
    }
    if (lseek(fd, kBootloaderControlOffset, SEEK_SET) != kBootloaderControlOffset) {
        PLOG(ERROR) << "failed to lseek " << misc_device;
        return false;
    }

    buffer->crc32_le = ComputeChecksum(buffer);
    if (!android::base::WriteFully(fd, buffer, sizeof(bootloader_control))) {
        PLOG(ERROR) << "failed to write " << misc_device;
        return false;
    }
    return true;
}

BootControlShared::BootControlShared() {
    std::string err;
    misc_device_ = get_bootloader_message_blk_device(&err);
    if (misc_device_.empty()) {
        LOG(FATAL) << "Unable to locate misc device: " << err;
    }

    bootloader_control control;
    if (!LoadUpdateState(misc_device_, &control)) {
        LOG(FATAL) << "Unable to read update state from misc partition";
    }
    uint32_t computed_crc32 = ComputeChecksum(&control);

    bool initialize = false;
    if (computed_crc32 != control.crc32_le) {
        LOG(WARNING) << "Invalid boot control found, expected CRC32 0x" << std::hex
                     << computed_crc32 << " but found 0x" << std::hex << control.crc32_le;
        initialize = true;
    } else if (control.magic != BOOT_CTRL_MAGIC) {
        LOG(WARNING) << "Invalid boot control magic, " << std::hex << control.magic;
        initialize = true;
    }
    if (initialize) {
        LOG(WARNING) << "Re-initializing misc.";

        // We only use the |merge_status| field of this structure.
        memset(&control, 0, sizeof(control));
        control.magic = BOOT_CTRL_MAGIC;
        control.version = BOOT_CTRL_VERSION;
        control.merge_status = static_cast<uint8_t>(MergeStatus::NONE);
        SaveUpdateState(misc_device_, &control);
    }
}

Return<bool> BootControlShared::setSnapshotMergeStatus(MergeStatus status) {
    bootloader_control control;
    if (!LoadUpdateState(misc_device_, &control)) {
        return false;
    }
    control.merge_status = static_cast<uint8_t>(status);
    return SaveUpdateState(misc_device_, &control);
}

Return<MergeStatus> BootControlShared::getSnapshotMergeStatus() {
    bootloader_control control;
    if (!LoadUpdateState(misc_device_, &control)) {
        return MergeStatus::UNKNOWN;
    }
    return static_cast<MergeStatus>(control.merge_status);
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace boot
}  // namespace hardware
}  // namespace android
