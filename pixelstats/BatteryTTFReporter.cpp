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

#define LOG_TAG "pixelstats: BatteryTTFReporter"

#include <android-base/file.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <log/log.h>
#include <pixelstats/BatteryTTFReporter.h>
#include <pixelstats/StatsHelper.h>
#include <time.h>
#include <utils/Timers.h>

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
using android::hardware::google::pixel::PixelAtoms::BatteryTimeToFullStatsReported;

const int SECONDS_PER_MONTH = 60 * 60 * 24 * 30;

BatteryTTFReporter::BatteryTTFReporter() {}

int64_t BatteryTTFReporter::getTimeSecs(void) {
    return nanoseconds_to_seconds(systemTime(SYSTEM_TIME_BOOTTIME));
}

bool BatteryTTFReporter::reportBatteryTTFStats(const std::shared_ptr<IStats> &stats_client) {
    std::string path = kBatteryTTFPath;
    std::string file_contents, line;
    std::istringstream ss;

    if (!ReadFileToString(path.c_str(), &file_contents)) {
        ALOGD("Unsupported path %s - %s", path.c_str(), strerror(errno));
        return false;
    }

    ss.str(file_contents);

    while (std::getline(ss, line)) {
        reportBatteryTTFStatsEvent(stats_client, line.c_str());
    }

    return true;
}

void BatteryTTFReporter::reportBatteryTTFStatsEvent(
        const std::shared_ptr<IStats> &stats_client, const char *line) {
    int ttf_stats_stats_fields[] = {
        BatteryTimeToFullStatsReported::kTtfTypeFieldNumber,
        BatteryTimeToFullStatsReported::kTtfRangeFieldNumber,
        BatteryTimeToFullStatsReported::kSoc0FieldNumber,
        BatteryTimeToFullStatsReported::kSoc1FieldNumber,
        BatteryTimeToFullStatsReported::kSoc2FieldNumber,
        BatteryTimeToFullStatsReported::kSoc3FieldNumber,
        BatteryTimeToFullStatsReported::kSoc4FieldNumber,
        BatteryTimeToFullStatsReported::kSoc5FieldNumber,
        BatteryTimeToFullStatsReported::kSoc6FieldNumber,
        BatteryTimeToFullStatsReported::kSoc7FieldNumber,
        BatteryTimeToFullStatsReported::kSoc8FieldNumber,
        BatteryTimeToFullStatsReported::kSoc9FieldNumber,
    };

    const int32_t fields_size = std::size(ttf_stats_stats_fields);
    const int32_t soc_start = 2; /* after type and range */
    int32_t size, range, type, i = 0, soc[fields_size - soc_start] = { 0 };
    std::vector<VendorAtomValue> values(fields_size);
    VendorAtomValue val;
    char ttf_type;

    size = sscanf(line, "%c%d:\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
                  &ttf_type, &range, &soc[0], &soc[1], &soc[2], &soc[3],
                  &soc[4], &soc[5], &soc[6], &soc[7], &soc[8], &soc[9]);

    if (size != fields_size)
        return;

    if (ttf_type == 'T') /* Elap Time */
        type = 0;
    else if (ttf_type == 'C') /* Charge Counter */
        type = 1;
    else
        return; /* Unknown */

    ALOGD("BatteryTTFStats: processed %s", line);
    val.set<VendorAtomValue::intValue>(type);
    values[ttf_stats_stats_fields[0] - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(range);
    values[ttf_stats_stats_fields[1] - kVendorAtomOffset] = val;
    for (i = soc_start; i < fields_size; i++) {
        val.set<VendorAtomValue::intValue>(soc[i - soc_start]);
        values[ttf_stats_stats_fields[i] - kVendorAtomOffset] = val;
    }

    VendorAtom event = {.reverseDomainName = "",
                        .atomId = PixelAtoms::Atom::kBatteryTimeToFullStatsReported,
                        .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report BatteryTTFStats to Stats service");
}

void BatteryTTFReporter::checkAndReportStats(const std::shared_ptr<IStats> &stats_client) {
    int64_t now = getTimeSecs();
    if ((report_time_ != 0) && (now - report_time_ < SECONDS_PER_MONTH)) {
        ALOGD("Do not upload yet. now: %" PRId64 ", pre: %" PRId64, now, report_time_);
        return;
    }

    bool successStats = reportBatteryTTFStats(stats_client);

    if (successStats) {
        report_time_ = now;
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
