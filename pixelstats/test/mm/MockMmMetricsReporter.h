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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MOCKMMMETRICS_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MOCKMMMETRICS_H

#include <android-base/stringprintf.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "pixelstats/MmMetricsReporter.h"

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using android::hardware::google::pixel::PixelAtoms::VendorSlowIo;

/**
 * mock version of MmMetricsReporter class
 * Testing on the mock version only
 */
class MockMmMetricsReporter : public MmMetricsReporter {
  public:
    MockMmMetricsReporter() : MmMetricsReporter() {}
    virtual ~MockMmMetricsReporter() {}
    void setBasePath(const std::string &path) { base_path_ = path; }

  private:
    /**
     * This is the base path of the following map (see getSysfsPath() below).
     *
     * The test code can modify this path (by setBasePath()) for redirecting
     * the sysfs read to a set test data files.  Since one sysfs node could
     * be read multiple times (e.g. create and the diff), the test
     * code can use this base_path_ to select which set of test data files
     * to read.
     */
    std::string base_path_;

    /**
     * map (redirect) the sysfs node read path to the test data file
     * for test data injection.
     */
    const std::map<const std::string, const char *> mock_path_map = {
            {"/sys/kernel/pixel_stat/mm/compaction/mm_compaction_duration", "compaction_duration"},
            {"/sys/kernel/pixel_stat/mm/vmscan/direct_reclaim/native/latency_stat",
             "direct_reclaim_native_latency_stat"},
            {"/sys/kernel/pixel_stat/mm/vmscan/direct_reclaim/other/latency_stat",
             "direct_reclaim_other_latency_stat"},
            {"/sys/kernel/pixel_stat/mm/vmscan/direct_reclaim/top/latency_stat",
             "direct_reclaim_top_latency_stat"},
            {"/sys/kernel/pixel_stat/mm/vmscan/direct_reclaim/visible/latency_stat",
             "direct_reclaim_visible_latency_stat"},
            {"/sys/kernel/dma_heap/total_pools_kb", "dma_heap_total_pools"},
            {"/sys/kernel/pixel_stat/gpu/mem/total_page_count", "gpu_pages"},
            {"/sys/kernel/ion/total_pools_kb", "ion_total_pools"},
            {"/sys/kernel/pixel_stat/mm/vmstat", "pixel_vmstat"},
            {"/proc/meminfo", "proc_meminfo"},
            {"/proc/stat", "proc_stat"},
            {"/proc/vmstat", "proc_vmstat"},
            {"/proc/pressure/cpu", "psi_cpu"},
            {"/proc/pressure/io", "psi_io"},
            {"/proc/pressure/memory", "psi_memory"},
            {"kswapd0", "kswapd0_stat"},
            {"kcompactd0", "kcompactd0_stat"},
    };

    virtual std::string getSysfsPath(const std::string &path) {
        return base_path_ + "/" + mock_path_map.at(path);
    }

    virtual std::string getProcessStatPath(const std::string &name, int *prev_pid) {
        (void)(prev_pid);  // unused parameter
        return getSysfsPath(name);
    }
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MOCKMMMETRICS_H
