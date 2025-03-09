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

#define LOG_TAG "pixelstats: WaterEvent"

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <pixelstats/WaterEventReporter.h>
#include <utils/Log.h>

#include <cinttypes>


namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtom;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::base::ReadFileToString;

static const char * const WATER_EVENT_DRIVER_STR = "DRIVER=h2omg";

WaterEventReporter::WaterEventReporter() {};

static bool fileExists(const std::string &path) {
    struct stat sb;

    return stat(path.c_str(), &sb) == 0;
}

static bool readFileToInt(const char *const path, int *val) {
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

static inline bool readFileToInt(const std::string &path, int *val) {
    return readFileToInt(path.c_str(), val);
}

void WaterEventReporter::logEvent(const std::shared_ptr<IStats> &stats_client,
                                  PixelAtoms::WaterEventReported::EventPoint event_point,
                                  const std::string_view sysfs_root)
{
   const std::string sysfs_path(sysfs_root);
   static int count = 0;

    if (!fileExists(sysfs_path)) {
        ALOGE("WaterEvent path is not valid %s", sysfs_path.c_str());
        return;
    }

    std::vector<VendorAtomValue> values(kNumOfWaterEventAtoms, 0);

    // Is this during boot or as a result of an event
    values[PixelAtoms::WaterEventReported::kCollectionEventFieldNumber - kVendorAtomOffset] = event_point;

    // Most important, what is the state of the fuse
    std::string fuse_state_str;
    if (ReadFileToString(sysfs_path + "/fuse/status", &fuse_state_str)) {
        if (!fuse_state_str.compare(0, 4, "open")) {
            values[PixelAtoms::WaterEventReported::kFuseStateFieldNumber - kVendorAtomOffset] =
                    PixelAtoms::WaterEventReported::FuseState::WaterEventReported_FuseState_BLOWN;
        } else if (!fuse_state_str.compare(0, 5, "short")) {
            values[PixelAtoms::WaterEventReported::kFuseStateFieldNumber - kVendorAtomOffset] =
                    PixelAtoms::WaterEventReported::FuseState::WaterEventReported_FuseState_INTACT;
        } else {
             values[PixelAtoms::WaterEventReported::kFuseStateFieldNumber - kVendorAtomOffset] =
                     PixelAtoms::WaterEventReported::FuseState::WaterEventReported_FuseState_FUSE_STATE_UNKNOWN;
        }
    }

    // Is the fuse enabled
    int fuse_enable;
    if (readFileToInt(sysfs_path + "/fuse/enable", &fuse_enable))
        values[PixelAtoms::WaterEventReported::kFuseEnabledFieldNumber - kVendorAtomOffset] =
                fuse_enable ? PixelAtoms::WaterEventReported::CircuitState::WaterEventReported_CircuitState_CIRCUIT_ENABLED :
                              PixelAtoms::WaterEventReported::CircuitState::WaterEventReported_CircuitState_CIRCUIT_DISABLED;

    // Is system fault enabled
    int fault_enable;
    if (readFileToInt(sysfs_path + "/fault/enable", &fault_enable))
        values[PixelAtoms::WaterEventReported::kFaultEnabledFieldNumber - kVendorAtomOffset] =
                fault_enable ? PixelAtoms::WaterEventReported::CircuitState::WaterEventReported_CircuitState_CIRCUIT_ENABLED :
                              PixelAtoms::WaterEventReported::CircuitState::WaterEventReported_CircuitState_CIRCUIT_DISABLED;

    std::tuple<std::string, int, int> sensors[] = {
        {"reference", PixelAtoms::WaterEventReported::kReferenceStateFieldNumber, PixelAtoms::WaterEventReported::kReferenceThresholdFieldNumber},
        {"sensor0", PixelAtoms::WaterEventReported::kSensor0StateFieldNumber, PixelAtoms::WaterEventReported::kSensor0ThresholdFieldNumber},
        {"sensor1", PixelAtoms::WaterEventReported::kSensor1StateFieldNumber, PixelAtoms::WaterEventReported::kSensor1ThresholdFieldNumber},
        {"sensor2", PixelAtoms::WaterEventReported::kSensor1StateFieldNumber, PixelAtoms::WaterEventReported::kSensor1ThresholdFieldNumber}
    };

    //   Get the sensor states (including reference) from either the boot_value (if this is during
    //   startup), or the latched_value if this is the result of a uevent
    for (auto e : sensors) {
        std::string sensor_path = std::get<0>(e);
        int sensor_state_field_number = std::get<1>(e);
        int threshold_field_number = std::get<2>(e);

        std::string sensor_state_path = sysfs_path + "/" + sensor_path;
        sensor_state_path += (event_point == PixelAtoms::WaterEventReported::EventPoint::WaterEventReported_EventPoint_BOOT) ? "/boot_value" : "/latched_value";

        std::string sensor_state_str;
        if (!ReadFileToString(sensor_state_path, &sensor_state_str)) {
            continue;
        }

        if (!sensor_state_str.compare(0, 3, "dry")) {
             values[sensor_state_field_number - kVendorAtomOffset] =
                     PixelAtoms::WaterEventReported::SensorState::WaterEventReported_SensorState_DRY;
        } else if (sensor_state_str.compare(0, 3, "wet")) {
             values[sensor_state_field_number- kVendorAtomOffset] =
                PixelAtoms::WaterEventReported::SensorState::WaterEventReported_SensorState_WET;
        } else if (!sensor_state_str.compare(0, 3, "invl")) {
            values[sensor_state_field_number - kVendorAtomOffset] =
                PixelAtoms::WaterEventReported::SensorState::WaterEventReported_SensorState_INVALID;
        } else if (!sensor_state_str.compare(0, 3, "dis")) {
                values[sensor_state_field_number - kVendorAtomOffset] =
                        PixelAtoms::WaterEventReported::SensorState::WaterEventReported_SensorState_DISABLED;
        } else {
                values[sensor_state_field_number - kVendorAtomOffset] =
                    PixelAtoms::WaterEventReported::SensorState::WaterEventReported_SensorState_SENSOR_STATE_UNKNOWN;
            continue;
        }

        // report the threshold
        std::string threshold_path = sysfs_path + "/" + sensor_path + "/threshold";
        int sensor_threshold;
        if (readFileToInt(threshold_path, &sensor_threshold)) {
            values[PixelAtoms::WaterEventReported::kReferenceThresholdFieldNumber - kVendorAtomOffset] = sensor_threshold;
        }
    }

    VendorAtom event = {.reverseDomainName = "",
                        .atomId = PixelAtoms::Atom::kWaterEventReported,
                        .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report Water event.");
}

void WaterEventReporter::logUevent(const std::shared_ptr<IStats> &stats_client,
                                   const std::string_view uevent_devpath)
{
    ALOGI("Reporting Water event");
    std::string dpath(uevent_devpath);

    std::vector<std::string> value = android::base::Split(dpath, "=");
    if (value.size() != 2) {
        ALOGE("Error report Water event split failed");
        return;
    }

    std::string sysfs_path("/sys");
    sysfs_path += value[1];

    PixelAtoms::WaterEventReported::EventPoint event_point =
        PixelAtoms::WaterEventReported::EventPoint::WaterEventReported_EventPoint_IRQ;
    logEvent(stats_client, event_point, sysfs_path);
}

bool WaterEventReporter::ueventDriverMatch(const char * const driver) {
    return !strncmp(driver, WATER_EVENT_DRIVER_STR, strlen(WATER_EVENT_DRIVER_STR));
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
