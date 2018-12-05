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
#include <hardware/google/pixelstats/1.0/IPixelStats.h>
#include <utils/StrongPointer.h>

using android::sp;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::SlowIo;
using ::hardware::google::pixelstats::V1_0::IPixelStats;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

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
    };

    SysfsCollector(const struct SysfsPaths &paths);
    void collect();

  private:
    void logAll();

    void logBatteryChargeCycles();
    void logCodecFailed();
    void logCodec1Failed();
    void logSlowIO();
    void logSpeakerImpedance();

    void reportSlowIoFromFile(const char *path, const IPixelStats::IoOperation &operation,
                              const SlowIo::IoOperation &operation_s);

    const char *const kSlowioReadCntPath;
    const char *const kSlowioWriteCntPath;
    const char *const kSlowioUnmapCntPath;
    const char *const kSlowioSyncCntPath;
    const char *const kCycleCountBinsPath;
    const char *const kImpedancePath;
    const char *const kCodecPath;
    const char *const kCodec1Path;
    sp<IPixelStats> pixelstats_;
    sp<IStats> stats_;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_SYSFSCOLLECTOR_H
