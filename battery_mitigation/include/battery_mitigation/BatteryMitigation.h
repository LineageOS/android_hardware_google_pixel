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

#include <errno.h>
#include <unistd.h>
#include <thread>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <utils/RefBase.h>

#include "MitigationThermalManager.h"
#include "uapi/brownout_stats.h"

#define DUMP_TIMES               12
#define EPOLL_MAXEVENTS          5
#define BUF_SIZE                 16
#define FVP_STATS_SIZE           4096
#define UP_DOWN_LINK_SIZE        512
#define MIN_SUPPORTED_PLATFORM   4
#define STAT_NAME_SIZE           48
#define STATS_MAX_SIZE           64

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using ::android::sp;

struct numericStat {
    char name[STAT_NAME_SIZE];
    int value;
};

struct BrownoutStatsExtend {
    struct brownout_stats brownoutStats;
    char fvpStats[FVP_STATS_SIZE];
    char pcieModem[UP_DOWN_LINK_SIZE];
    char pcieWifi[UP_DOWN_LINK_SIZE];
    struct numericStat numericStats[STATS_MAX_SIZE];
    timeval eventReceivedTime;
    timeval dumpTime;
    unsigned int eventIdx;
};

class BatteryMitigation : public RefBase {
  public:
    BatteryMitigation(const struct MitigationConfig::Config &cfg,
                      const struct MitigationConfig::EventThreadConfig &eventThreadCfg);
    ~BatteryMitigation();
    bool isMitigationLogTimeValid(std::chrono::system_clock::time_point startTime,
                                  const char *const logFilePath, const char *const timestampFormat,
                                  const std::regex pattern);

    void startBrownoutEventThread();
    void stopBrownoutEventThread();
  private:
    MitigationThermalManager *mThermalMgr;
    struct MitigationConfig::EventThreadConfig cfg;
    std::vector<numericSysfs> totalNumericSysfsStatPaths;
    int brownoutStatsFd;
    int triggeredIdxFd;
    int triggeredIdxEpollFd;
    int wakeupEventFd;
    char *storingAddr;
    std::atomic_bool threadStop{false};
    std::thread brownoutEventThread;

    void BrownoutEventThread();
    void initTotalNumericSysfsPaths();
    int initThisMeal();
    int initFd();
    void tearDownBrownoutEventThread();
    int readNumericStats(struct BrownoutStatsExtend*);
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
