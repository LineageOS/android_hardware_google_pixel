#pragma once

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

#include <map>

#include "IFilesystem.h"
#include "RealFilesystem.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

struct CpuLoad {
    uint32_t cpuId;
    double idleTimeFraction;

    bool operator==(const CpuLoad &other) const {
        return cpuId == other.cpuId && idleTimeFraction == other.idleTimeFraction;
    }
};

struct CpuTime {
    uint64_t idleTimeMs;
    uint64_t totalTimeMs;
};

class CpuLoadReader {
  public:
    CpuLoadReader() : mFilesystem(std::make_unique<RealFilesystem>()) {}
    CpuLoadReader(std::unique_ptr<IFilesystem> filesystem) : mFilesystem(std::move(filesystem)) {}

    // Initialize reading, must be done before calling other methods.
    //
    // Work is not done in constructor as it accesses files.
    void init();

    // Get the load of each CPU, since the last time this method was called.
    bool getRecentCpuLoads(std::vector<CpuLoad> *result);

    // Gets the last CPU times read, keyed by CPU ID. Used for dumping to bug reports.
    std::map<uint32_t, CpuTime> getPreviousCpuTimes() const;

  private:
    std::map<uint32_t, CpuTime> mPreviousCpuTimes;
    const std::unique_ptr<IFilesystem> mFilesystem;

    std::map<uint32_t, CpuTime> readCpuTimes();
    // Converts jiffies to milliseconds. Jiffies is the granularity the kernel reports times in,
    // including the timings in CPU statistics.
    static uint64_t jiffiesToMs(uint64_t jiffies);
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
