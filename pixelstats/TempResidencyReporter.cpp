/*
 * Copyright (C) 2021 The Android Open Source Project
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

#define LOG_TAG "pixelstats: TempResidencyStats"

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <pixelstats/TempResidencyReporter.h>
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
using android::base::WriteStringToFile;
using android::hardware::google::pixel::PixelAtoms::ThermalDfsStats;

/**
 * Parse file_contents and read residency stats into stats.
 */
bool parse_file_contents(std::string file_contents,
                         std::map<std::string, std::vector<int64_t>> *stats) {
    const char *data = file_contents.c_str();
    int data_len = file_contents.length();
    char sensor_name[32];
    int offset = 0;
    int bytes_read;

    while (sscanf(data + offset, "THERMAL ZONE: %31s\n%n", sensor_name, &bytes_read) == 1) {
        int64_t temp_res_value;
        int num_stats_buckets;
        int index = 0;
        offset += bytes_read;
        if (offset >= data_len)
            return false;

        std::string sensor_name_str = sensor_name;

        if (!sscanf(data + offset, "NUM_TEMP_RESIDENCY_BUCKETS: %d\n%n", &num_stats_buckets,
                    &bytes_read))
            return false;
        offset += bytes_read;
        if (offset >= data_len)
            return false;
        while (index < num_stats_buckets) {
            if (sscanf(data + offset, "-inf - %*d ====> %" PRId64 "ms\n%n", &temp_res_value,
                       &bytes_read) != 1 &&
                sscanf(data + offset, "%*d - %*d ====> %" PRId64 "ms\n%n", &temp_res_value,
                       &bytes_read) != 1 &&
                sscanf(data + offset, "%*d - inf ====> %" PRId64 "ms\n\n%n", &temp_res_value,
                       &bytes_read) != 1)
                return false;

            (*stats)[sensor_name_str].push_back(temp_res_value);
            index++;

            offset += bytes_read;
            if ((offset >= data_len) && (index < num_stats_buckets))
                return false;
        }
    }
    return true;
}

/**
 * Logs the Temperature residency stats for every thermal zone.
 */
void TempResidencyReporter::logTempResidencyStats(
        const std::shared_ptr<IStats> &stats_client, std::string_view temperature_residency_path,
        std::string_view temperature_residency_reset_path) {
    if (temperature_residency_path.empty() || temperature_residency_reset_path.empty()) {
        ALOGV("TempResidency Stats/Reset path not specified");
        return;
    }
    std::string file_contents;
    if (!ReadFileToString(temperature_residency_path.data(), &file_contents)) {
        ALOGE("Unable to read TempResidencyStatsPath");
        return;
    }
    std::map<std::string, std::vector<int64_t>> stats_map;
    if (!parse_file_contents(file_contents, &stats_map)) {
        ALOGE("Fail to parse TempResidencyStatsPath");
        return;
    }
    if (!stats_map.size())
        return;
    ::android::base::boot_clock::time_point curTime = ::android::base::boot_clock::now();
    int64_t since_last_update_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(curTime - prevTime).count();

    // Reset the stats for next day collection and if failed return without reporting to report the
    // combined residency next day.
    if (!WriteStringToFile(std::to_string(1), temperature_residency_reset_path.data())) {
        ALOGE("Failed to reset TempResidencyStats");
        return;
    }

    auto stats_map_iterator = stats_map.begin();
    VendorAtomValue tmp_atom_value;

    // Iterate through stats_map by sensor_name
    while (stats_map_iterator != stats_map.end()) {
        std::vector<VendorAtomValue> values;
        std::string sensor_name_str = stats_map_iterator->first;
        std::vector<int64_t> residency_stats = stats_map_iterator->second;
        tmp_atom_value.set<VendorAtomValue::stringValue>(sensor_name_str);
        values.push_back(tmp_atom_value);
        tmp_atom_value.set<VendorAtomValue::longValue>(since_last_update_ms);
        values.push_back(tmp_atom_value);

        if (residency_stats.size() > kMaxBucketLen) {
            stats_map_iterator++;
            continue;
        }
        // Iterate over every temperature residency buckets
        for (int index = 0; index < residency_stats.size(); index++) {
            tmp_atom_value.set<VendorAtomValue::longValue>(residency_stats[index]);
            values.push_back(tmp_atom_value);
        }
        //  Send vendor atom to IStats HAL
        VendorAtom event = {.reverseDomainName = "",
                            .atomId = PixelAtoms::Atom::kVendorTempResidencyStats,
                            .values = std::move(values)};
        ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
        if (!ret.isOk())
            ALOGE("Unable to report VendorTempResidencyStats to Stats service");

        stats_map_iterator++;
    }
    prevTime = curTime;
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
