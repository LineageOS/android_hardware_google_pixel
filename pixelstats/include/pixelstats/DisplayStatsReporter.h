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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DISPLAYSTATSREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DISPLAYSTATSREPORTER_H

#include <aidl/android/frameworks/stats/IStats.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

#include <string>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtomValue;

/**
 * A class to upload Pixel Display Stats metrics
 */
class DisplayStatsReporter {
  public:
    DisplayStatsReporter();
    void logDisplayStats(const std::shared_ptr<IStats> &stats_client,
                         const std::vector<std::string> &display_stats_paths);

  private:
    struct DisplayPanelErrorStats {
        int64_t primary_error_count_te;
        int64_t primary_error_count_unknown;
        int64_t secondary_error_count_te;
        int64_t secondary_error_count_unknown;
    };

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;
    const int kNumOfDisplayPanelErrorStats = 4;
    struct DisplayPanelErrorStats prev_data_;

    void logDisplayPanelErrorStats(const std::shared_ptr<IStats> &stats_client,
                                   const std::vector<std::string> &display_stats_paths);
    bool captureDisplayPanelErrorStats(const std::vector<std::string> &display_stats_paths,
                                       struct DisplayPanelErrorStats *cur_data);
    bool readDisplayPanelErrorCount(const std::string &path, int64_t *val);
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DISPLAYSTATSREPORTER_H
