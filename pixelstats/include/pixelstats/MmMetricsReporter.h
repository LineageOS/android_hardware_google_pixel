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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_MMMETRICSREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_MMMETRICSREPORTER_H

#include <map>
#include <string>

#include <aidl/android/frameworks/stats/IStats.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtomValue;

/**
 * A class to upload Pixel MM health metrics
 */
class MmMetricsReporter {
  public:
    MmMetricsReporter();
    void logPixelMmMetricsPerHour(const std::shared_ptr<IStats> &stats_client);
    void logPixelMmMetricsPerDay(const std::shared_ptr<IStats> &stats_client);

  private:
    struct MmMetricsInfo {
        std::string name;
        int atom_key;
        bool update_diff;
    };

    static const std::vector<MmMetricsInfo> kMmMetricsPerHourInfo;
    static const std::vector<MmMetricsInfo> kMmMetricsPerDayInfo;

    bool ReadFileToUint(const char *const path, uint64_t *val);
    std::map<std::string, uint64_t> readVmStat(const char *path);
    uint64_t getIonTotalPools();
    void fillAtomValues(const std::vector<MmMetricsInfo> &metrics_info,
                        const std::map<std::string, uint64_t> &mm_metrics,
                        std::map<std::string, uint64_t> *prev_mm_metrics,
                        std::vector<VendorAtomValue> *atom_values);

    const char *const kVmstatPath;
    const char *const kIonTotalPoolsPath;
    const char *const kIonTotalPoolsPathForLegacy;
    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;

    std::map<std::string, uint64_t> prev_hour_vmstat_;
    std::map<std::string, uint64_t> prev_day_vmstat_;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYCAPACITYREPORTER_H
