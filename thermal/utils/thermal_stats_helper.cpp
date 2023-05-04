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

#include "thermal_stats_helper.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

#include <algorithm>
#include <string_view>

namespace aidl {
namespace android {
namespace hardware {
namespace thermal {
namespace implementation {

using aidl::android::frameworks::stats::VendorAtom;
namespace PixelAtoms = ::android::hardware::google::pixel::PixelAtoms;

namespace {
static std::shared_ptr<IStats> stats_client = nullptr;
std::shared_ptr<IStats> getStatsService() {
    static std::once_flag statsServiceFlag;
    std::call_once(statsServiceFlag, []() {
        const std::string instance = std::string() + IStats::descriptor + "/default";
        bool isStatsDeclared = AServiceManager_isDeclared(instance.c_str());
        if (!isStatsDeclared) {
            LOG(ERROR) << "Stats service is not registered.";
            return;
        }
        stats_client = IStats::fromBinder(
                ndk::SpAIBinder(AServiceManager_waitForService(instance.c_str())));
    });
    return stats_client;
}

template <typename T>
static bool isRecordStats(const std::shared_ptr<StatsInfo<T>> &stats_info) {
    return stats_info != nullptr && stats_info->record_stats;
}
}  // namespace

bool ThermalStatsHelper::initializeStats(
        const std::unordered_map<std::string, SensorInfo> &sensor_info_map_,
        const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_) {
    bool is_initialized_ =
            initializeSensorTempStats(sensor_info_map_) &&
            initializeSensorCdevRequestStats(sensor_info_map_, cooling_device_info_map_);
    if (is_initialized_) {
        last_total_stats_report_time = boot_clock::now();
        LOG(INFO) << "Thermal Stats Initialized Successfully";
    }
    return is_initialized_;
}

bool ThermalStatsHelper::initializeSensorTempStats(
        const std::unordered_map<std::string, SensorInfo> &sensor_info_map_) {
    std::unique_lock<std::shared_mutex> _lock(sensor_temp_stats_map_mutex_);
    for (const auto &name_info_pair : sensor_info_map_) {
        auto &sensor_info = name_info_pair.second;
        if (isRecordStats(sensor_info.stats_info)) {
            const size_t time_in_state_size =
                    sensor_info.stats_info->stats_threshold.empty()
                            ? kThrottlingSeverityCount  // if default, use severity as bucket
                            : (sensor_info.stats_info->stats_threshold.size() + 1);
            sensor_temp_stats_map_[name_info_pair.first] = StatsRecord(time_in_state_size);
            LOG(INFO) << "Thermal Sensor stats initialized for sensor: " << name_info_pair.first;
        }
    }
    return true;
}

bool ThermalStatsHelper::initializeSensorCdevRequestStats(
        const std::unordered_map<std::string, SensorInfo> &sensor_info_map_,
        const std::unordered_map<std::string, CdevInfo> &cooling_device_info_map_) {
    std::unique_lock<std::shared_mutex> _lock(sensor_cdev_request_stats_map_mutex_);
    for (const auto &sensor_info_pair : sensor_info_map_) {
        for (const auto &binded_cdev_info_pair :
             sensor_info_pair.second.throttling_info->binded_cdev_info_map) {
            if (!isRecordStats(binded_cdev_info_pair.second.stats_info)) {
                continue;
            }
            const auto &max_state =
                    cooling_device_info_map_.at(binded_cdev_info_pair.first).max_state;
            const auto &stats_threshold = binded_cdev_info_pair.second.stats_info->stats_threshold;
            size_t time_in_state_size;
            // if default, use each state as bucket
            if (stats_threshold.empty()) {
                time_in_state_size = max_state + 1;  // +1 for bucket to include last state
            } else {
                // check last threshold value(>=number of buckets) is less than
                // max_state
                if (stats_threshold.back() >= max_state) {
                    LOG(ERROR) << "Invalid bindedCdev stats threshold: " << stats_threshold.back()
                               << " >= " << max_state;
                    return false;
                }
                time_in_state_size =
                        stats_threshold.size() +
                        1;  // +1 for bucket to include values greater than last threshold
            }
            sensor_cdev_request_stats_map_[sensor_info_pair.first][binded_cdev_info_pair.first] =
                    StatsRecord(time_in_state_size);
            LOG(INFO) << "Thermal BindedCdev stats initialized for sensor: "
                      << sensor_info_pair.first << " " << binded_cdev_info_pair.first;
        }
    }
    return true;
}

void ThermalStatsHelper::updateStatsRecord(StatsRecord *stats_record, int new_state) {
    const auto now = boot_clock::now();
    const auto cur_state_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - stats_record->cur_state_start_time);
    LOG(VERBOSE) << "Adding duration " << cur_state_duration.count()
                 << " for cur_state: " << stats_record->cur_state << " with value: "
                 << stats_record->time_in_state_ms[stats_record->cur_state].count();
    // Update last record end time
    stats_record->time_in_state_ms[stats_record->cur_state] += cur_state_duration;
    stats_record->cur_state_start_time = now;
    stats_record->cur_state = new_state;
}

void ThermalStatsHelper::updateSensorTempStats(std::string_view sensor,
                                               const std::shared_ptr<StatsInfo<float>> &stats_info,
                                               const Temperature &t) {
    if (!isRecordStats(stats_info)) {
        return;
    }
    int new_value;
    if (stats_info->stats_threshold.empty()) {
        new_value = static_cast<int>(t.throttlingStatus);
    } else {
        const auto &thresholds = stats_info->stats_threshold;
        auto threshold_idx = std::lower_bound(thresholds.begin(), thresholds.end(), t.value);
        new_value = (threshold_idx - thresholds.begin());
    }
    std::unique_lock<std::shared_mutex> _lock(sensor_temp_stats_map_mutex_);
    StatsRecord &stats_record = sensor_temp_stats_map_.at(sensor.data());
    LOG(VERBOSE) << "Updating sensor stats for sensor: " << sensor.data()
                 << " with new value: " << new_value;
    updateStatsRecord(&stats_record, new_value);
}

void ThermalStatsHelper::updateSensorCdevRequestStats(
        std::string_view trigger_sensor, std::string_view cdev,
        const std::shared_ptr<StatsInfo<int>> &stats_info, int new_value) {
    if (!isRecordStats(stats_info)) {
        return;
    }
    if (!stats_info->stats_threshold.empty()) {
        const auto &thresholds = stats_info->stats_threshold;
        auto threshold_idx = std::lower_bound(thresholds.begin(), thresholds.end(), new_value);
        new_value = (threshold_idx - thresholds.begin());
    }
    std::unique_lock<std::shared_mutex> _lock(sensor_cdev_request_stats_map_mutex_);
    StatsRecord &stats_record =
            sensor_cdev_request_stats_map_.at(trigger_sensor.data()).at(cdev.data());
    LOG(VERBOSE) << "Updating bindedCdev stats for trigger_sensor: " << trigger_sensor.data()
                 << " , cooling_device: " << cdev.data() << " with new value: " << new_value;
    updateStatsRecord(&stats_record, new_value);
}

int ThermalStatsHelper::reportStats() {
    const auto curTime = boot_clock::now();
    const auto since_last_total_stats_update_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(curTime -
                                                                  last_total_stats_report_time);
    LOG(VERBOSE) << "Duration from last total stats update is: "
                 << since_last_total_stats_update_ms.count();
    if (since_last_total_stats_update_ms < kUpdateIntervalMs) {
        LOG(VERBOSE) << "Time elapsed since last update less than " << kUpdateIntervalMs.count();
        return 0;
    }

    const std::shared_ptr<IStats> stats_client = getStatsService();
    if (!stats_client) {
        LOG(ERROR) << "Unable to get AIDL Stats service";
        return -1;
    }
    int count_failed_reporting =
            reportSensorTempStats(stats_client) + reportSensorCdevRequestStats(stats_client);
    last_total_stats_report_time = curTime;
    return count_failed_reporting;
}

int ThermalStatsHelper::reportSensorTempStats(const std::shared_ptr<IStats> &stats_client) {
    int count_failed_reporting = 0;
    std::unique_lock<std::shared_mutex> _lock(sensor_temp_stats_map_mutex_);
    for (auto &sensor_temp_stats_pair : sensor_temp_stats_map_) {
        LOG(VERBOSE) << "Reporting sensor stats for " << sensor_temp_stats_pair.first;
        // Load values array
        std::vector<VendorAtomValue> values(1);
        values[0].set<VendorAtomValue::stringValue>(sensor_temp_stats_pair.first);
        if (!reportThermalStats(stats_client, PixelAtoms::Atom::kVendorTempResidencyStats, values,
                                &sensor_temp_stats_pair.second)) {
            LOG(ERROR) << "Unable to report VendorTempResidencyStats to Stats service for "
                          "sensor: "
                       << sensor_temp_stats_pair.first;
            count_failed_reporting++;
        }
    }
    return count_failed_reporting;
}

int ThermalStatsHelper::reportSensorCdevRequestStats(const std::shared_ptr<IStats> &stats_client) {
    int count_failed_reporting = 0;
    std::unique_lock<std::shared_mutex> _lock(sensor_cdev_request_stats_map_mutex_);
    for (auto &sensor_binded_cdev_stats_pair : sensor_cdev_request_stats_map_) {
        for (auto &cdev_stats_pair : sensor_binded_cdev_stats_pair.second) {
            LOG(VERBOSE) << "Reporting bindedCdev stats for sensor: "
                         << sensor_binded_cdev_stats_pair.first
                         << " cooling_device: " << cdev_stats_pair.first;
            // Load values array
            std::vector<VendorAtomValue> values(2);
            values[0].set<VendorAtomValue::stringValue>(sensor_binded_cdev_stats_pair.first);
            values[1].set<VendorAtomValue::stringValue>(cdev_stats_pair.first);
            if (!reportThermalStats(stats_client, PixelAtoms::Atom::kVendorSensorCoolingDeviceStats,
                                    values, &cdev_stats_pair.second)) {
                LOG(ERROR) << "Unable to report VendorSensorCoolingDeviceStats to Stats "
                              "service for sensor: "
                           << sensor_binded_cdev_stats_pair.first
                           << " cooling_device: " << cdev_stats_pair.first;
                count_failed_reporting++;
            }
        }
    }
    return count_failed_reporting;
}

bool ThermalStatsHelper::reportThermalStats(const std::shared_ptr<IStats> &stats_client,
                                            const int32_t &atom_id,
                                            std::vector<VendorAtomValue> values,
                                            StatsRecord *stats_record) {
    // maintain a copy in case reporting fails
    StatsRecord thermal_stats_before_reporting = *stats_record;
    std::vector<int64_t> time_in_state_ms = processStatsRecordForReporting(stats_record);
    const auto since_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats_record->cur_state_start_time - stats_record->last_stats_report_time);
    VendorAtomValue tmp;
    tmp.set<VendorAtomValue::longValue>(since_last_update_ms.count());
    values.push_back(tmp);
    for (auto &time_in_state : time_in_state_ms) {
        tmp.set<VendorAtomValue::longValue>(time_in_state);
        values.push_back(tmp);
    }

    LOG(VERBOSE) << "Reporting thermal stats for atom_id " << atom_id;
    // Send vendor atom to IStats HAL
    VendorAtom event = {.reverseDomainName = "", .atomId = atom_id, .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk()) {
        LOG(ERROR) << "Unable to report to Stats service for atom " << atom_id;
        *stats_record = restoreStatsRecordOnFailure(std::move(thermal_stats_before_reporting));
        return false;
    } else {
        // Update last time of stats reporting
        stats_record->last_stats_report_time = boot_clock::now();
    }
    return true;
}

std::vector<int64_t> ThermalStatsHelper::processStatsRecordForReporting(StatsRecord *stats_record) {
    // update the last unclosed entry and start new record with same state
    updateStatsRecord(stats_record, stats_record->cur_state);
    std::vector<std::chrono::milliseconds> &time_in_state_ms = stats_record->time_in_state_ms;
    // convert std::chrono::milliseconds time_in_state to int64_t vector for reporting
    std::vector<int64_t> stats_residency(time_in_state_ms.size());
    std::transform(time_in_state_ms.begin(), time_in_state_ms.end(), stats_residency.begin(),
                   [](std::chrono::milliseconds time_ms) { return time_ms.count(); });
    // clear previous stats
    std::fill(time_in_state_ms.begin(), time_in_state_ms.end(), std::chrono::milliseconds::zero());
    return stats_residency;
}

StatsRecord ThermalStatsHelper::restoreStatsRecordOnFailure(
        StatsRecord &&stats_record_before_failure) {
    stats_record_before_failure.report_fail_count += 1;
    // If consecutive count of failure is high, reset stat to avoid overflow
    if (stats_record_before_failure.report_fail_count >= kMaxStatsReportingFailCount) {
        return StatsRecord(stats_record_before_failure.time_in_state_ms.size(),
                           stats_record_before_failure.cur_state);
    } else {
        return stats_record_before_failure;
    }
}

std::unordered_map<std::string, StatsRecord> ThermalStatsHelper::GetSensorTempStatsSnapshot() {
    auto sensor_stats_snapshot = sensor_temp_stats_map_;
    for (auto &sensor_temp_stats_pair : sensor_stats_snapshot) {
        auto &temp_stats = sensor_temp_stats_pair.second;
        // update the last unclosed entry
        updateStatsRecord(&temp_stats, temp_stats.cur_state);
    }
    return sensor_stats_snapshot;
}

std::unordered_map<std::string, std::unordered_map<std::string, StatsRecord>>
ThermalStatsHelper::GetSensorCoolingDeviceRequestStatsSnapshot() {
    auto binded_cdev_stats_snapshot = sensor_cdev_request_stats_map_;
    for (auto &sensor_binded_cdev_stats_pair : binded_cdev_stats_snapshot) {
        for (auto &cdev_stats_pair : sensor_binded_cdev_stats_pair.second) {
            auto &request_stats = cdev_stats_pair.second;
            // update the last unclosed entry
            updateStatsRecord(&request_stats, request_stats.cur_state);
        }
    }
    return binded_cdev_stats_snapshot;
}

}  // namespace implementation
}  // namespace thermal
}  // namespace hardware
}  // namespace android
}  // namespace aidl
