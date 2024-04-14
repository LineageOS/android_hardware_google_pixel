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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MMMETRICSGOLDENATOMFIELDTYPES_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MMMETRICSGOLDENATOMFIELDTYPES_H

#include "VendorAtomIntValueUtil.h"

namespace android {
namespace hardware {
namespace google {
namespace pixel {

namespace mm_metrics_atom_field_test_golden_results {
const int PixelMmMetricsPerHour_field_types[]{
        longValue,  // optional int64 free_pages = 2;
        longValue,  // optional int64 anon_pages = 3;
        longValue,  // optional int64 file_pages = 4;
        longValue,  // optional int64 slab_reclaimable = 5;
        longValue,  // optional int64 zspages = 6;
        longValue,  // optional int64 unevictable = 7;
        longValue,  // optional int64 ion_total_pools = 8;
        longValue,  // optional int64 gpu_memory = 9;
        longValue,  // optional int64 slab_unreclaimable = 10;
        longValue,  // optional int64 psi_cpu_some_total = 11;
        longValue,  // optional int64 psi_io_full_total = 12;
        longValue,  // optional int64 psi_io_some_total = 13;
        longValue,  // optional int64 psi_mem_full_total = 14;
        longValue,  // optional int64 psi_mem_some_total = 15;
        intValue,   // optional int32 psi_cpu_some_avg10_min = 16;
        intValue,   // optional int32 psi_cpu_some_avg10_max = 17;
        intValue,   // optional int32 psi_cpu_some_avg10_avg = 18;
        intValue,   // optional int32 psi_cpu_some_avg60_min = 19;
        intValue,   // optional int32 psi_cpu_some_avg60_max = 20;
        intValue,   // optional int32 psi_cpu_some_avg60_avg = 21;
        intValue,   // optional int32 psi_cpu_some_avg300_min = 22;
        intValue,   // optional int32 psi_cpu_some_avg300_max = 23;
        intValue,   // optional int32 psi_cpu_some_avg300_avg = 24;
        intValue,   // optional int32 psi_io_full_avg10_min = 25;
        intValue,   // optional int32 psi_io_full_avg10_max = 26;
        intValue,   // optional int32 psi_io_full_avg10_avg = 27;
        intValue,   // optional int32 psi_io_full_avg60_min = 28;
        intValue,   // optional int32 psi_io_full_avg60_max = 29;
        intValue,   // optional int32 psi_io_full_avg60_avg = 30;
        intValue,   // optional int32 psi_io_full_avg300_min = 31;
        intValue,   // optional int32 psi_io_full_avg300_max = 32;
        intValue,   // optional int32 psi_io_full_avg300_avg = 33;
        intValue,   // optional int32 psi_io_some_avg10_min = 34;
        intValue,   // optional int32 psi_io_some_avg10_max = 35;
        intValue,   // optional int32 psi_io_some_avg10_avg = 36;
        intValue,   // optional int32 psi_io_some_avg60_min = 37;
        intValue,   // optional int32 psi_io_some_avg60_max = 38;
        intValue,   // optional int32 psi_io_some_avg60_avg = 39;
        intValue,   // optional int32 psi_io_some_avg300_min = 40;
        intValue,   // optional int32 psi_io_some_avg300_max = 41;
        intValue,   // optional int32 psi_io_some_avg300_avg = 42;
        intValue,   // optional int32 psi_mem_full_avg10_min = 43;
        intValue,   // optional int32 psi_mem_full_avg10_max = 44;
        intValue,   // optional int32 psi_mem_full_avg10_avg = 45;
        intValue,   // optional int32 psi_mem_full_avg60_min = 46;
        intValue,   // optional int32 psi_mem_full_avg60_max = 47;
        intValue,   // optional int32 psi_mem_full_avg60_avg = 48;
        intValue,   // optional int32 psi_mem_full_avg300_min = 49;
        intValue,   // optional int32 psi_mem_full_avg300_max = 50;
        intValue,   // optional int32 psi_mem_full_avg300_avg = 51;
        intValue,   // optional int32 psi_mem_some_avg10_min = 52;
        intValue,   // optional int32 psi_mem_some_avg10_max = 53;
        intValue,   // optional int32 psi_mem_some_avg10_avg = 54;
        intValue,   // optional int32 psi_mem_some_avg60_min = 55;
        intValue,   // optional int32 psi_mem_some_avg60_max = 56;
        intValue,   // optional int32 psi_mem_some_avg60_avg = 57;
        intValue,   // optional int32 psi_mem_some_avg300_min = 58;
        intValue,   // optional int32 psi_mem_some_avg300_max = 59;
        intValue,   // optional int32 psi_mem_some_avg300_avg = 60;
        intValue,   // optional int32 version = 61 [deprecated = true];
        longValue,  // optional int64 shmem_pages = 62;
        longValue,  // optional int64 page_table_pages = 63;
        longValue,  // optional int64 dmabuf_kb = 64;
};

const int PixelMmMetricsPerDay_field_types[]{
        longValue,  // optional int64 workingset_refault = 2;  /* refault_file */
        longValue,  // optional int64 pswpin = 3;
        longValue,  // optional int64 pswpout = 4;
        longValue,  // optional int64 allocstall_dma = 5;
        longValue,  // optional int64 allocstall_dma32 = 6;
        longValue,  // optional int64 allocstall_normal = 7;
        longValue,  // optional int64 allocstall_movable = 8;
        longValue,  // optional int64 pgalloc_dma = 9;
        longValue,  // optional int64 pgalloc_dma32 = 10;
        longValue,  // optional int64 pgalloc_normal = 11;
        longValue,  // optional int64 pgalloc_movable = 12;
        longValue,  // optional int64 pgsteal_kswapd = 13;
        longValue,  // optional int64 pgsteal_direct = 14;
        longValue,  // optional int64 pgscan_kswapd = 15;
        longValue,  // optional int64 pgscan_direct = 16;
        longValue,  // optional int64 oom_kill = 17;
        longValue,  // optional int64 pgalloc_high = 18;
        longValue,  // optional int64 pgcache_hit = 19;
        longValue,  // optional int64 pgcache_miss = 20;
        longValue,  // optional int64 kswapd_stime_clks = 21;
        longValue,  // optional int64 kcompactd_stime_clks = 22;
        longValue,  // optional int64 direct_reclaim_native_latency_total_time = 23;
        longValue,  // optional int64 direct_reclaim_native_latency0 = 24;
        longValue,  // optional int64 direct_reclaim_native_latency1 = 25;
        longValue,  // optional int64 direct_reclaim_native_latency2 = 26;
        longValue,  // optional int64 direct_reclaim_native_latency3 = 27;
        longValue,  // optional int64 direct_reclaim_visible_latency_total_time = 28;
        longValue,  // optional int64 direct_reclaim_visible_latency0 = 29;
        longValue,  // optional int64 direct_reclaim_visible_latency1 = 30;
        longValue,  // optional int64 direct_reclaim_visible_latency2 = 31;
        longValue,  // optional int64 direct_reclaim_visible_latency3 = 32;
        longValue,  // optional int64 direct_reclaim_top_latency_total_time = 33;
        longValue,  // optional int64 direct_reclaim_top_latency0 = 34;
        longValue,  // optional int64 direct_reclaim_top_latency1 = 35;
        longValue,  // optional int64 direct_reclaim_top_latency2 = 36;
        longValue,  // optional int64 direct_reclaim_top_latency3 = 37;
        longValue,  // optional int64 direct_reclaim_other_latency_total_time = 38;
        longValue,  // optional int64 direct_reclaim_other_latency0 = 39;
        longValue,  // optional int64 direct_reclaim_other_latency1 = 40;
        longValue,  // optional int64 direct_reclaim_other_latency2 = 41;
        longValue,  // optional int64 direct_reclaim_other_latency3 = 42;
        longValue,  // optional int64 compaction_total_time = 43;
        longValue,  // optional int64 compaction_ev_count0 = 44;
        longValue,  // optional int64 compaction_ev_count1 = 45;
        longValue,  // optional int64 compaction_ev_count2 = 46;
        longValue,  // optional int64 compaction_ev_count3 = 47;
        longValue,  // optional int64 compaction_ev_count4 = 48;
        longValue,  // optional int64 workingset_refault_anon = 49;
        longValue,  // optional int64 workingset_refault_file = 50;
        longValue,  // optional int64 compact_success = 51;
        longValue,  // optional int64 compact_fail = 52;
        longValue,  // optional int64 kswapd_low_wmark_hq = 53;
        longValue,  // optional int64 kswapd_high_wmark_hq = 54;
        longValue,  // optional int64 thp_file_alloc = 55;
        longValue,  // optional int64 thp_zero_page_alloc = 56;
        longValue,  // optional int64 thp_split_page = 57;
        longValue,  // optional int64 thp_migration_split = 58;
        longValue,  // optional int64 thp_deferred_split_page = 59;
        longValue,  // optional int64 version = 60 [deprecated = true];
        longValue,  // optional int64 cpu_total_time_cs = 61;
        longValue,  // optional int64 cpu_idle_time_cs = 62;
        longValue,  // optional int64 cpu_io_wait_time_cs = 63;
        longValue,  // optional int64 kswapd_pageout_run = 64;
};
}  // namespace mm_metrics_atom_field_test_golden_results

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_TEST_MMMETRICSGOLDENATOMFIELDTYPES_H
