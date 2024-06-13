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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYTTFREPORTER_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYTTFREPORTER_H

#include <aidl/android/frameworks/stats/IStats.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;

/**
 * A class to upload battery capacity metrics
 */
class BatteryTTFReporter {
  public:
    BatteryTTFReporter();
    void checkAndReportStats(const std::shared_ptr<IStats> &stats_client);

  private:
    void reportBatteryTTFStatsEvent(const std::shared_ptr<IStats> &stats_client, const char *line);
    bool reportBatteryTTFStats(const std::shared_ptr<IStats> &stats_client);

    int64_t report_time_ = 0;
    int64_t getTimeSecs();

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;

    const std::string kBatteryTTFPath = "/sys/class/power_supply/battery/ttf_stats";
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_BATTERYTTFREPORTER_H
