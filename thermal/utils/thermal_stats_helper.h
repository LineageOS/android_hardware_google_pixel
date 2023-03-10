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

#pragma once

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/chrono_utils.h>

#include <chrono>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "utils/thermal_info.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::base::boot_clock;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using android::hardware::thermal::V2_0::ThrottlingSeverity;

constexpr int kMaxStatsReportingFailCount = 3;

struct ThermalStats {
    int prev_state; /* temperature / cdev state at prev_update_time */
    std::vector<std::chrono::milliseconds> time_in_state_ms; /* stats array */
    boot_clock::time_point prev_update_time;
    boot_clock::time_point last_stats_report_time = boot_clock::time_point::min();
    int report_fail_count = 0; /* Number of times failed to report stats */

    ThermalStats() = default;
    explicit ThermalStats(const size_t &time_in_state_size, int state = 0) : prev_state(state) {
        prev_update_time = last_stats_report_time = boot_clock::now();
        report_fail_count = 0;
        time_in_state_ms = std::vector<std::chrono::milliseconds>(
                time_in_state_size, std::chrono::milliseconds::zero());
    }
    ThermalStats(const ThermalStats &) = default;
    ThermalStats(ThermalStats &&) = default;
    ThermalStats &operator=(const ThermalStats &) = default;
    ThermalStats &operator=(ThermalStats &&) = default;
    ~ThermalStats() = default;
};

class ThermalStatsHelper {
  public:
    ThermalStatsHelper() = default;
    ~ThermalStatsHelper() = default;
    // Disallow copy and assign
    ThermalStatsHelper(const ThermalStatsHelper &) = delete;
    void operator=(const ThermalStatsHelper &) = delete;

    bool initializeStats(const std::unordered_map<std::string, SensorInfo> &sensors_parsed,
                         const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_);
    /*
     * Function to report all the stats by calling all specific stats reporting function.
     * Returns:
     *   0, if time_elapsed < kUpdateIntervalMs or if no failure in reporting
     *  -1, if failed to get AIDL stats services
     *  >0, count represents the number of stats failed to report.
     */
    int reportStats();
    void updateSensorStats(std::string_view sensor,
                           const std::shared_ptr<StatsInfo<float>> &stats_info,
                           const Temperature_2_0 &t);
    void updateBindedCdevStats(std::string_view trigger_sensor, std::string_view cdev,
                               const std::shared_ptr<StatsInfo<int>> &stats_info, int new_state);
    // Get a snapshot of Thermal Stats Sensor Map till that point in time
    std::unordered_map<std::string, ThermalStats> GetSensorThermalStatsSnapshot();
    // Get a snapshot of Thermal Stats Sensor Map till that point in time
    std::unordered_map<std::string, std::unordered_map<std::string, ThermalStats>>
    GetBindedCdevThermalStatsSnapshot();

  private:
    static constexpr std::chrono::milliseconds kUpdateIntervalMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(24h);
    boot_clock::time_point last_total_stats_report_time = boot_clock::time_point::min();
    mutable std::shared_mutex thermal_stats_sensor_temp_map_mutex_;
    // Temperature stats for each sensor being watched
    std::unordered_map<std::string, ThermalStats> thermal_stats_sensor_temp_map_;
    mutable std::shared_mutex thermal_stats_sensor_binded_cdev_state_map_mutex_;
    // Cdev request stat for each cdev binded to a sensor (sensor -> cdev -> ThermalStats)
    std::unordered_map<std::string, std::unordered_map<std::string, ThermalStats>>
            thermal_stats_sensor_binded_cdev_state_map_;

    bool initializeSensorStats(const std::unordered_map<std::string, SensorInfo> &sensor_info_map_);
    bool initializeBindedCdevStats(
            const std::unordered_map<std::string, SensorInfo> &sensor_info_map_,
            const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_);
    int reportSensorStats(const std::shared_ptr<IStats> &stats_client);
    int reportBindedCdevStats(const std::shared_ptr<IStats> &stats_client);
    bool reportThermalStats(const std::shared_ptr<IStats> &stats_client, const int32_t &atom_id,
                            std::vector<VendorAtomValue> values, ThermalStats *thermal_stats);
    void updateStats(ThermalStats *thermal_stats, int new_state);
    void closePrevStateStat(ThermalStats *thermal_stats);
    std::vector<int64_t> processStatsBeforeReporting(ThermalStats *thermal_stats);
    ThermalStats restoreThermalStatOnFailure(ThermalStats &&thermal_stats_before_failure);
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
