/*
 * Copyright (C) 2023 The Android Open Source Project
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

#define LOG_TAG "pixelstats: DisplayStats"

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <pixelstats/DisplayStatsReporter.h>
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
using android::hardware::google::pixel::PixelAtoms::DisplayPanelErrorStats;

DisplayStatsReporter::DisplayStatsReporter() {}

bool DisplayStatsReporter::readDisplayPanelErrorCount(const std::string &path, int64_t *val) {
    std::string file_contents;

    if (path.empty()) {
        return false;
    }

    if (!ReadFileToString(path.c_str(), &file_contents)) {
        if (errno != ENOENT) {
            ALOGD("readDisplayPanelErrorCount Unable to read %s - %s", path.c_str(),
                  strerror(errno));
        }
        return false;
    } else {
        file_contents = android::base::Trim(file_contents);
        if (!android::base::ParseInt(file_contents, val)) {
            return false;
        }
    }

    return true;
}

bool DisplayStatsReporter::captureDisplayPanelErrorStats(
        const std::vector<std::string> &display_stats_paths,
        struct DisplayPanelErrorStats *pcur_data) {
    bool report_stats = false;
    std::string path;

    if (display_stats_paths.size() < kNumOfDisplayPanelErrorStats) {
        ALOGE("Number of display stats paths (%zu) is less than expected (%d)",
              display_stats_paths.size(), kNumOfDisplayPanelErrorStats);
        return false;
    }

    int64_t index = PixelAtoms::DisplayPanelErrorStats::kPrimaryErrorCountTeFieldNumber;
    index = index - kVendorAtomOffset;
    path = display_stats_paths[index];

    // Read primary panel error stats.
    if (!readDisplayPanelErrorCount(path, &(pcur_data->primary_error_count_te))) {
        pcur_data->primary_error_count_te = prev_data_.primary_error_count_te;
    } else {
        report_stats |= (pcur_data->primary_error_count_te > prev_data_.primary_error_count_te);
    }

    index = PixelAtoms::DisplayPanelErrorStats::kPrimaryErrorCountUnknownFieldNumber;
    index = index - kVendorAtomOffset;
    path = display_stats_paths[index];
    if (!readDisplayPanelErrorCount(path, &(pcur_data->primary_error_count_unknown))) {
        pcur_data->primary_error_count_unknown = prev_data_.primary_error_count_unknown;
    } else {
        report_stats |=
                (pcur_data->primary_error_count_unknown > prev_data_.primary_error_count_unknown);
    }

    // Read secondary panel error stats.
    index = PixelAtoms::DisplayPanelErrorStats::kSecondaryErrorCountTeFieldNumber;
    index = index - kVendorAtomOffset;
    path = display_stats_paths[index];
    if (!readDisplayPanelErrorCount(path, &(pcur_data->secondary_error_count_te))) {
        pcur_data->secondary_error_count_te = prev_data_.secondary_error_count_te;
    } else {
        report_stats |= (pcur_data->secondary_error_count_te > prev_data_.secondary_error_count_te);
    }

    index = PixelAtoms::DisplayPanelErrorStats::kSecondaryErrorCountUnknownFieldNumber;
    index = index - kVendorAtomOffset;
    path = display_stats_paths[index];
    if (!readDisplayPanelErrorCount(path, &(pcur_data->secondary_error_count_unknown))) {
        pcur_data->secondary_error_count_unknown = prev_data_.secondary_error_count_unknown;
    } else {
        report_stats |= (pcur_data->secondary_error_count_unknown >
                         prev_data_.secondary_error_count_unknown);
    }

    return report_stats;
}

void DisplayStatsReporter::logDisplayPanelErrorStats(
        const std::shared_ptr<IStats> &stats_client,
        const std::vector<std::string> &display_stats_paths) {
    struct DisplayPanelErrorStats cur_data = prev_data_;

    if (!captureDisplayPanelErrorStats(display_stats_paths, &cur_data)) {
        prev_data_ = cur_data;
        return;
    }

    VendorAtomValue tmp;
    int64_t max_error_count = static_cast<int64_t>(INT32_MAX);
    int error_count;
    std::vector<VendorAtomValue> values(kNumOfDisplayPanelErrorStats);

    error_count = std::min<int64_t>(
            cur_data.primary_error_count_te - prev_data_.primary_error_count_te, max_error_count);
    tmp.set<VendorAtomValue::intValue>(error_count);
    int64_t index = PixelAtoms::DisplayPanelErrorStats::kPrimaryErrorCountTeFieldNumber;
    index = index - kVendorAtomOffset;
    values[index] = tmp;

    error_count = std::min<int64_t>(
            cur_data.primary_error_count_unknown - prev_data_.primary_error_count_unknown,
            max_error_count);
    tmp.set<VendorAtomValue::intValue>(error_count);
    index = PixelAtoms::DisplayPanelErrorStats::kPrimaryErrorCountUnknownFieldNumber;
    index = index - kVendorAtomOffset;
    values[index] = tmp;

    prev_data_ = cur_data;

    ALOGD("Report updated display panel metrics to stats service");
    // Send vendor atom to IStats HAL
    VendorAtom event = {.reverseDomainName = "",
                        .atomId = PixelAtoms::Atom::kDisplayPanelErrorStats,
                        .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report display Display Panel stats to Stats service");
}

void DisplayStatsReporter::logDisplayStats(const std::shared_ptr<IStats> &stats_client,
                                           const std::vector<std::string> &display_stats_paths) {
    logDisplayPanelErrorStats(stats_client, display_stats_paths);
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
