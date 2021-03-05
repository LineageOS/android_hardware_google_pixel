/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H

#include <android/frameworks/stats/1.0/IStats.h>
#include <utils/StrongPointer.h>

#include "BatteryEEPROMReporter.h"

namespace aidl {
namespace android {
namespace frameworks {
namespace stats {

class IStats;
class VendorAtomValue;

}  // namespace stats
}  // namespace frameworks
}  // namespace android
}  // namespace aidl

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::sp;
using android::frameworks::stats::V1_0::SlowIo;

class SysfsCollector {
  public:
    struct SysfsPaths {
        const char *const SlowioReadCntPath;
        const char *const SlowioWriteCntPath;
        const char *const SlowioUnmapCntPath;
        const char *const SlowioSyncCntPath;
        const char *const CycleCountBinsPath;
        const char *const ImpedancePath;
        const char *const CodecPath;
        const char *const Codec1Path;
        const char *const SpeechDspPath;
        const char *const BatteryCapacityCC;
        const char *const BatteryCapacityVFSOC;
        const char *const UFSLifetimeA;
        const char *const UFSLifetimeB;
        const char *const UFSLifetimeC;
        const char *const UFSHostResetPath;
        const char *const F2fsStatsPath;
        const char *const UserdataBlockProp;
        const char *const ZramMmStatPath;
        const char *const ZramBdStatPath;
        const char *const EEPROMPath;
    };

    SysfsCollector(const struct SysfsPaths &paths);
    void collect();

  private:
    struct MmMetricsInfo {
        std::string name;
        int atom_key;
        bool update_diff;
    };

    static const std::vector<MmMetricsInfo> kMmMetricsPerHourInfo;
    static const std::vector<MmMetricsInfo> kMmMetricsPerDayInfo;

    bool ReadFileToInt(const std::string &path, int *val);
    bool ReadFileToInt(const char *path, int *val);
    bool ReadFileToUint(const char *const path, uint64_t *val);
    void logPerDay();
    void logPerHour();

    void logBatteryChargeCycles();
    void logCodecFailed();
    void logCodec1Failed();
    void logSlowIO();
    void logSpeakerImpedance(const std::shared_ptr<IStats> &stats_client);
    void logSpeechDspStat();
    void logBatteryCapacity(const std::shared_ptr<IStats> &stats_client);
    void logUFSLifetime(const std::shared_ptr<IStats> &stats_client);
    void logUFSErrorStats(const std::shared_ptr<IStats> &stats_client);
    void logF2fsStats(const std::shared_ptr<IStats> &stats_client);
    void logF2fsCompressionInfo(const std::shared_ptr<IStats> &stats_client);
    void logZramStats(const std::shared_ptr<IStats> &stats_client);
    void logBootStats(const std::shared_ptr<IStats> &stats_client);
    void logBatteryEEPROM();

    void reportSlowIoFromFile(const char *path, const SlowIo::IoOperation &operation_s);
    void reportZramMmStat(const std::shared_ptr<IStats> &stats_client);
    void reportZramBdStat(const std::shared_ptr<IStats> &stats_client);

    std::map<std::string, uint64_t> readVmStat(const char *path);
    uint64_t getIonTotalPools();
    void fillAtomValues(const std::vector<MmMetricsInfo> &metrics_info,
                        const std::map<std::string, uint64_t> &mm_metrics,
                        std::map<std::string, uint64_t> *prev_mm_metrics,
                        std::vector<VendorAtomValue> *atom_values);
    void logPixelMmMetricsPerHour(const std::shared_ptr<IStats> &stats_client);
    void logPixelMmMetricsPerDay(const std::shared_ptr<IStats> &stats_client);

    const char *const kSlowioReadCntPath;
    const char *const kSlowioWriteCntPath;
    const char *const kSlowioUnmapCntPath;
    const char *const kSlowioSyncCntPath;
    const char *const kCycleCountBinsPath;
    const char *const kImpedancePath;
    const char *const kCodecPath;
    const char *const kCodec1Path;
    const char *const kSpeechDspPath;
    const char *const kBatteryCapacityCC;
    const char *const kBatteryCapacityVFSOC;
    const char *const kUFSLifetimeA;
    const char *const kUFSLifetimeB;
    const char *const kUFSLifetimeC;
    const char *const kUFSHostResetPath;
    const char *const kF2fsStatsPath;
    const char *const kZramMmStatPath;
    const char *const kZramBdStatPath;
    const char *const kEEPROMPath;
    const char *const kVmstatPath;
    const char *const kIonTotalPoolsPath;
    const char *const kIonTotalPoolsPathForLegacy;
    sp<android::frameworks::stats::V1_0::IStats> stats_;

    BatteryEEPROMReporter battery_EEPROM_reporter_;

    std::map<std::string, uint64_t> prev_hour_vmstat_;
    std::map<std::string, uint64_t> prev_day_vmstat_;

    // Proto messages are 1-indexed and VendorAtom field numbers start at 2, so
    // store everything in the values array at the index of the field number
    // -2.
    const int kVendorAtomOffset = 2;

    bool log_once_reported = false;
    int64_t prev_huge_pages_since_boot_ = -1;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H
