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
#include <aidl/android/hardware/thermal/Temperature.h>
#include <android-base/chrono_utils.h>

#include <chrono>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "thermal_info.h"

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtomValue;
using ::android::base::boot_clock;

constexpr int kMaxStatsReportingFailCount = 3;

struct StatsRecord {
    int cur_state; /* temperature / cdev state at current time */
    boot_clock::time_point cur_state_start_time;
    boot_clock::time_point last_stats_report_time = boot_clock::time_point::min();
    std::vector<std::chrono::milliseconds> time_in_state_ms; /* stats array */
    int report_fail_count = 0; /* Number of times failed to report stats */
    explicit StatsRecord(const size_t &time_in_state_size, int state = 0)
        : cur_state(state),
          cur_state_start_time(boot_clock::now()),
          last_stats_report_time(boot_clock::now()),
          report_fail_count(0) {
        time_in_state_ms = std::vector<std::chrono::milliseconds>(
                time_in_state_size, std::chrono::milliseconds::zero());
    }
    StatsRecord() = default;
    StatsRecord(const StatsRecord &) = default;
    StatsRecord &operator=(const StatsRecord &) = default;
    StatsRecord(StatsRecord &&) = default;
    StatsRecord &operator=(StatsRecord &&) = default;
    ~StatsRecord() = default;
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
    void updateSensorTempStats(std::string_view sensor,
                               const std::shared_ptr<StatsInfo<float>> &stats_info,
                               const Temperature &t);
    void updateSensorCdevRequestStats(std::string_view trigger_sensor, std::string_view cdev,
                                      const std::shared_ptr<StatsInfo<int>> &stats_info,
                                      int new_state);
    // Get a snapshot of Thermal Stats Sensor Map till that point in time
    std::unordered_map<std::string, StatsRecord> GetSensorTempStatsSnapshot();
    // Get a snapshot of Thermal Stats Sensor Map till that point in time
    std::unordered_map<std::string, std::unordered_map<std::string, StatsRecord>>
    GetSensorCoolingDeviceRequestStatsSnapshot();

  private:
    static constexpr std::chrono::milliseconds kUpdateIntervalMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(24h);
    boot_clock::time_point last_total_stats_report_time = boot_clock::time_point::min();
    mutable std::shared_mutex sensor_temp_stats_map_mutex_;
    // Temperature stats for each sensor being watched
    std::unordered_map<std::string, StatsRecord> sensor_temp_stats_map_;
    mutable std::shared_mutex sensor_cdev_request_stats_map_mutex_;
    // userVote request stat for the sensor to the corresponding cdev (sensor -> cdev ->
    // StatsRecord)
    std::unordered_map<std::string, std::unordered_map<std::string, StatsRecord>>
            sensor_cdev_request_stats_map_;

    bool initializeSensorTempStats(
            const std::unordered_map<std::string, SensorInfo> &sensor_info_map_);
    bool initializeSensorCdevRequestStats(
            const std::unordered_map<std::string, SensorInfo> &sensor_info_map_,
            const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_);
    void updateStatsRecord(StatsRecord *stats_record, int new_state);
    int reportSensorTempStats(const std::shared_ptr<IStats> &stats_client);
    int reportSensorCdevRequestStats(const std::shared_ptr<IStats> &stats_client);
    bool reportThermalStats(const std::shared_ptr<IStats> &stats_client, const int32_t &atom_id,
                            std::vector<VendorAtomValue> values, StatsRecord *stats_record);
    std::vector<int64_t> processStatsRecordForReporting(StatsRecord *stats_record);
    StatsRecord restoreStatsRecordOnFailure(StatsRecord &&stats_record_before_failure);
};

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
