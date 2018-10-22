/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __THERMAL_HELPER_H__
#define __THERMAL_HELPER_H__

#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <android/hardware/thermal/2.0/IThermal.h>

#include "utils/config_parser.h"
#include "utils/cooling_devices.h"
#include "utils/sensors.h"
#include "utils/thermal_watcher.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermal;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using TemperatureType_1_0 = ::android::hardware::thermal::V1_0::TemperatureType;
using TemperatureType_2_0 = ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;
using ::android::hardware::thermal::V2_0::ThrottlingSeverityCount;

using NotificationCallback = std::function<void(const std::vector<Temperature_2_0> &temps)>;
using NotificationTime = std::chrono::time_point<std::chrono::steady_clock>;

struct SensorStatus {
    ThrottlingSeverity severity;
    NotificationTime last_notify_time;
};

class ThermalHelper {
  public:
    ThermalHelper(NotificationCallback cb);
    ~ThermalHelper() = default;

    bool fillTemperatures(hidl_vec<Temperature_1_0> *temperatures) const;
    bool fillCurrentTemperatures(bool filterType, TemperatureType_2_0 type,
                                 hidl_vec<Temperature_2_0> *temperatures) const;
    bool fillTemperatureThresholds(bool filterType, TemperatureType_2_0 type,
                                   hidl_vec<TemperatureThreshold> *thresholds) const;
    bool fillCurrentCoolingDevices(bool filterType, CoolingType type,
                                   hidl_vec<CoolingDevice_2_0> *coolingdevices) const;
    bool fillCpuUsages(hidl_vec<CpuUsage> *cpu_usages) const;

    // Dissallow copy and assign.
    ThermalHelper(const ThermalHelper &) = delete;
    void operator=(const ThermalHelper &) = delete;

    bool isInitializedOk() const { return is_initialized_; }

    // Returns a vector of all cooling devices that has been found on the
    // device.
    std::vector<std::string> getCoolingDevicePaths() const;

    // Read the temperature of a single sensor.
    bool readTemperature(const std::string &sensor_name, Temperature_1_0 *out) const;
    bool readTemperature(const std::string &sensor_name, Temperature_2_0 *out) const;
    bool readTemperatureThreshold(const std::string &sensor_name, TemperatureThreshold *out) const;
    // Read the value of a single cooling device.
    bool readCoolingDevice(const std::string &cooling_device, CoolingDevice_2_0 *out) const;

  private:
    bool initializeSensorMap();
    bool initializeCoolingDevices();
    ThrottlingSeverity getSeverityFromThresholds(
        std::array<float, static_cast<size_t>(ThrottlingSeverityCount::NUM_THROTTLING_LEVELS)>
            hot_thresholds,
        std::array<float, static_cast<size_t>(ThrottlingSeverityCount::NUM_THROTTLING_LEVELS)>
            cold_thresholds,
        float value) const;

    sp<ThermalWatcher> thermal_watcher_;
    Sensors thermal_sensors_;
    CoolingDevices cooling_devices_;
    bool is_initialized_;
    NotificationCallback cb_;
    const std::map<std::string, CoolingType> cooling_device_info_map_;
    const std::map<std::string, SensorInfo> sensor_info_map_;

    // used in thermalWatcherCallbackFunc only
    std::map<std::string, SensorStatus> thermal_watcher_sensor_status_;
    void thermalWatcherCallbackFunc(const std::string &, const int);
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif  // __THERMAL_HELPER_H__
