/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "pixelstats-uevent"

#include <pixelstats/UeventListener.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <android/frameworks/stats/1.0/IStats.h>
#include <cutils/uevent.h>
#include <log/log.h>
#include <utils/StrongPointer.h>

#include <unistd.h>
#include <thread>

using android::sp;
using android::base::ReadFileToString;
using android::frameworks::stats::V1_0::HardwareFailed;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::UsbPortOverheatEvent;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

constexpr int32_t UEVENT_MSG_LEN = 2048;  // it's 2048 in all other users.

bool UeventListener::ReadFileToInt(const std::string &path, int *val) {
    return ReadFileToInt(path.c_str(), val);
}

bool UeventListener::ReadFileToInt(const char *const path, int *val) {
    std::string file_contents;

    if (!ReadFileToString(path, &file_contents)) {
        ALOGE("Unable to read %s - %s", path, strerror(errno));
        return false;
    } else if (sscanf(file_contents.c_str(), "%d", val) != 1) {
        ALOGE("Unable to convert %s to int - %s", path, strerror(errno));
        return false;
    }
    return true;
}

void UeventListener::ReportMicBrokenOrDegraded(const int mic, const bool isbroken) {
    sp<IStats> stats_client = IStats::tryGetService();

    if (stats_client) {
        HardwareFailed failure = {.hardwareType = HardwareFailed::HardwareType::MICROPHONE,
                                  .hardwareLocation = mic,
                                  .errorCode = isbroken
                                                   ? HardwareFailed::HardwareErrorCode::COMPLETE
                                                   : HardwareFailed::HardwareErrorCode::DEGRADE};
        Return<void> ret = stats_client->reportHardwareFailed(failure);
        if (!ret.isOk())
            ALOGE("Unable to report physical drop to Stats service");
    } else
        ALOGE("Unable to connect to Stats service");
}

void UeventListener::ReportMicStatusUevents(const char *devpath, const char *mic_status) {
    if (!devpath || !mic_status)
        return;
    if (!strcmp(devpath, ("DEVPATH=" + kAudioUevent).c_str())) {
        std::vector<std::string> value = android::base::Split(mic_status, "=");
        bool isbroken;

        if (value.size() == 2) {
            if (!value[0].compare("MIC_BREAK_STATUS"))
                isbroken = true;
            else if (!value[0].compare("MIC_DEGRADE_STATUS"))
                isbroken = false;
            else
                return;

            if (!value[1].compare("true"))
                ReportMicBrokenOrDegraded(0, isbroken);
            else {
                int mic_status = atoi(value[1].c_str());

                if (mic_status > 0 && mic_status <= 7) {
                    for (int mic_bit = 0; mic_bit < 3; mic_bit++)
                        if (mic_status & (0x1 << mic_bit))
                            ReportMicBrokenOrDegraded(mic_bit, isbroken);
                } else if (mic_status == 0) {
                    // mic is ok
                    return;
                } else {
                    // should not enter here
                    ALOGE("invalid mic status");
                    return;
                }
            }
        }
    }
}

void UeventListener::ReportUsbPortOverheatEvent(const char *driver) {
    UsbPortOverheatEvent event = {};
    std::string file_contents;

    if (!driver || strcmp(driver, "DRIVER=google,overheat_mitigation")) {
        return;
    }

    ReadFileToInt((kUsbPortOverheatPath + "/plug_temp"), &event.plugTemperatureDeciC);
    ReadFileToInt((kUsbPortOverheatPath + "/max_temp"), &event.maxTemperatureDeciC);
    ReadFileToInt((kUsbPortOverheatPath + "/trip_time"), &event.timeToOverheat);
    ReadFileToInt((kUsbPortOverheatPath + "/hysteresis_time"), &event.timeToHysteresis);
    ReadFileToInt((kUsbPortOverheatPath + "/cleared_time"), &event.timeToInactive);

    sp<IStats> stats_client = IStats::tryGetService();

    if (stats_client) {
        stats_client->reportUsbPortOverheatEvent(event);
    }
}

bool UeventListener::ProcessUevent() {
    char msg[UEVENT_MSG_LEN + 2];
    char *cp;
    const char *action, *power_supply_typec_mode, *driver, *product;
    const char *mic_break_status, *mic_degrade_status;
    const char *devpath;
    int n;

    if (uevent_fd_ < 0) {
        uevent_fd_ = uevent_open_socket(64 * 1024, true);
        if (uevent_fd_ < 0) {
            ALOGE("uevent_init: uevent_open_socket failed\n");
            return false;
        }
    }

    n = uevent_kernel_multicast_recv(uevent_fd_, msg, UEVENT_MSG_LEN);
    if (n <= 0 || n >= UEVENT_MSG_LEN)
        return false;

    // Ensure double-null termination of msg.
    msg[n] = '\0';
    msg[n + 1] = '\0';

    action = power_supply_typec_mode = driver = product = NULL;
    mic_break_status = mic_degrade_status = devpath = NULL;

    /**
     * msg is a sequence of null-terminated strings.
     * Iterate through and record positions of string/value pairs of interest.
     * Double null indicates end of the message. (enforced above).
     */
    cp = msg;
    while (*cp) {
        if (!strncmp(cp, "ACTION=", strlen("ACTION="))) {
            action = cp;
        } else if (!strncmp(cp, "POWER_SUPPLY_TYPEC_MODE=", strlen("POWER_SUPPLY_TYPEC_MODE="))) {
            power_supply_typec_mode = cp;
        } else if (!strncmp(cp, "DRIVER=", strlen("DRIVER="))) {
            driver = cp;
        } else if (!strncmp(cp, "PRODUCT=", strlen("PRODUCT="))) {
            product = cp;
        } else if (!strncmp(cp, "MIC_BREAK_STATUS=", strlen("MIC_BREAK_STATUS="))) {
            mic_break_status = cp;
        } else if (!strncmp(cp, "MIC_DEGRADE_STATUS=", strlen("MIC_DEGRADE_STATUS="))) {
            mic_degrade_status = cp;
        } else if (!strncmp(cp, "DEVPATH=", strlen("DEVPATH="))) {
            devpath = cp;
        }

        /* advance to after the next \0 */
        while (*cp++) {
        }
    }

    /* Process the strings recorded. */
    ReportMicStatusUevents(devpath, mic_break_status);
    ReportMicStatusUevents(devpath, mic_degrade_status);
    ReportUsbPortOverheatEvent(driver);

    return true;
}

UeventListener::UeventListener(const std::string audio_uevent, const std::string overheat_path)
    : kAudioUevent(audio_uevent),
      kUsbPortOverheatPath(overheat_path),
      uevent_fd_(-1) {}

/* Thread function to continuously monitor uevents.
 * Exit after kMaxConsecutiveErrors to prevent spinning. */
void UeventListener::ListenForever() {
    constexpr int kMaxConsecutiveErrors = 10;
    int consecutive_errors = 0;
    while (1) {
        if (ProcessUevent()) {
            consecutive_errors = 0;
        } else {
            if (++consecutive_errors >= kMaxConsecutiveErrors) {
                ALOGE("Too many ProcessUevent errors; exiting UeventListener.");
                return;
            }
        }
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
