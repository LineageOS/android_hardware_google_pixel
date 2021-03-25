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

#define LOG_TAG "pixelstats: MmMetrics"

#include <aidl/android/frameworks/stats/IStats.h>
#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <pixelstats/MmMetricsReporter.h>
#include <utils/Log.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::IStats;
using aidl::android::frameworks::stats::VendorAtom;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::base::ReadFileToString;
using android::hardware::google::pixel::PixelAtoms::PixelMmMetricsPerDay;
using android::hardware::google::pixel::PixelAtoms::PixelMmMetricsPerHour;

const std::vector<MmMetricsReporter::MmMetricsInfo> MmMetricsReporter::kMmMetricsPerHourInfo = {
        {"nr_free_pages", PixelMmMetricsPerHour::kFreePagesFieldNumber, false},
        {"nr_anon_pages", PixelMmMetricsPerHour::kAnonPagesFieldNumber, false},
        {"nr_file_pages", PixelMmMetricsPerHour::kFilePagesFieldNumber, false},
        {"nr_slab_reclaimable", PixelMmMetricsPerHour::kSlabReclaimableFieldNumber, false},
        {"nr_zspages", PixelMmMetricsPerHour::kZspagesFieldNumber, false},
        {"nr_unevictable", PixelMmMetricsPerHour::kUnevictableFieldNumber, false},
};

const std::vector<MmMetricsReporter::MmMetricsInfo> MmMetricsReporter::kMmMetricsPerDayInfo = {
        {"workingset_refault", PixelMmMetricsPerDay::kWorkingsetRefaultFieldNumber, true},
        {"workingset_refault_file", PixelMmMetricsPerDay::kWorkingsetRefaultFieldNumber, true},
        {"pswpin", PixelMmMetricsPerDay::kPswpinFieldNumber, true},
        {"pswpout", PixelMmMetricsPerDay::kPswpoutFieldNumber, true},
        {"allocstall_dma", PixelMmMetricsPerDay::kAllocstallDmaFieldNumber, true},
        {"allocstall_dma32", PixelMmMetricsPerDay::kAllocstallDma32FieldNumber, true},
        {"allocstall_normal", PixelMmMetricsPerDay::kAllocstallNormalFieldNumber, true},
        {"allocstall_movable", PixelMmMetricsPerDay::kAllocstallMovableFieldNumber, true},
        {"pgalloc_dma", PixelMmMetricsPerDay::kPgallocDmaFieldNumber, true},
        {"pgalloc_dma32", PixelMmMetricsPerDay::kPgallocDma32FieldNumber, true},
        {"pgalloc_normal", PixelMmMetricsPerDay::kPgallocNormalFieldNumber, true},
        {"pgalloc_movable", PixelMmMetricsPerDay::kPgallocMovableFieldNumber, true},
        {"pgsteal_kswapd", PixelMmMetricsPerDay::kPgstealKswapdFieldNumber, true},
        {"pgsteal_direct", PixelMmMetricsPerDay::kPgstealDirectFieldNumber, true},
        {"pgscan_kswapd", PixelMmMetricsPerDay::kPgscanKswapdFieldNumber, true},
        {"pgscan_direct", PixelMmMetricsPerDay::kPgscanDirectFieldNumber, true},
        {"oom_kill", PixelMmMetricsPerDay::kOomKillFieldNumber, true},
};

MmMetricsReporter::MmMetricsReporter()
    : kVmstatPath("/proc/vmstat"),
      kIonTotalPoolsPath("/sys/kernel/dma_heap/total_pools_kb"),
      kIonTotalPoolsPathForLegacy("/sys/kernel/ion/total_pools_kb") {}

bool MmMetricsReporter::ReadFileToUint(const char *const path, uint64_t *val) {
    std::string file_contents;

    if (!ReadFileToString(path, &file_contents)) {
        ALOGI("Unable to read %s - %s", path, strerror(errno));
        return false;
    } else {
        file_contents = android::base::Trim(file_contents);
        if (!android::base::ParseUint(file_contents, val)) {
            ALOGI("Unable to convert %s to uint - %s", path, strerror(errno));
            return false;
        }
    }
    return true;
}

/**
 * Parse the output of /proc/vmstat or the sysfs having the same output format.
 * The map containing pairs of {field_string, data} will be returned.
 */
std::map<std::string, uint64_t> MmMetricsReporter::readVmStat(const char *path) {
    std::string file_contents;
    std::map<std::string, uint64_t> vmstat_data;

    if (path == nullptr) {
        ALOGI("vmstat path is not specified");
        return vmstat_data;
    }

    if (!ReadFileToString(path, &file_contents)) {
        ALOGE("Unable to read vmstat from %s, err: %s", path, strerror(errno));
        return vmstat_data;
    }

    std::istringstream data(file_contents);
    std::string line;
    while (std::getline(data, line)) {
        std::vector<std::string> words = android::base::Split(line, " ");
        if (words.size() != 2)
            continue;

        uint64_t i;
        if (!android::base::ParseUint(words[1], &i))
            continue;

        vmstat_data[words[0]] = i;
    }
    return vmstat_data;
}

uint64_t MmMetricsReporter::getIonTotalPools() {
    uint64_t res;

    if (!ReadFileToUint(kIonTotalPoolsPathForLegacy, &res) || (res == 0)) {
        if (!ReadFileToUint(kIonTotalPoolsPath, &res)) {
            return 0;
        }
    }

    return res;
}

/**
 * fillAtomValues() is used to copy Mm metrics to values
 * metrics_info: This is a vector of MmMetricsInfo {field_string, atom_key, update_diff}
 *               field_string is used to get the data from mm_metrics.
 *               atom_key is the position where the data should be put into values.
 *               update_diff will be true if this is an accumulated data.
 *               metrics_info may have multiple entries with the same atom_key,
 *               e.g. workingset_refault and workingset_refault_file.
 * mm_metrics: This map contains pairs of {field_string, cur_value} collected
 *             from /proc/vmstat or the sysfs for the pixel specific metrics.
 *             e.g. {"nr_free_pages", 200000}
 *             Some data in mm_metrics are accumulated, e.g. pswpin.
 *             We upload the difference instead of the accumulated value
 *             when update_diff of the field is true.
 * prev_mm_metrics: The pointer to the metrics we collected last time.
 * atom_values: The atom values that will be reported later.
 */
void MmMetricsReporter::fillAtomValues(const std::vector<MmMetricsInfo> &metrics_info,
                                       const std::map<std::string, uint64_t> &mm_metrics,
                                       std::map<std::string, uint64_t> *prev_mm_metrics,
                                       std::vector<VendorAtomValue> *atom_values) {
    VendorAtomValue tmp;
    tmp.set<VendorAtomValue::longValue>(0);
    // resize atom_values to add all fields defined in metrics_info
    int max_idx = 0;
    for (auto &entry : metrics_info) {
        if (max_idx < entry.atom_key)
            max_idx = entry.atom_key;
    }
    int size = max_idx - kVendorAtomOffset + 1;
    if (atom_values->size() < size)
        atom_values->resize(size, tmp);

    for (auto &entry : metrics_info) {
        int atom_idx = entry.atom_key - kVendorAtomOffset;

        auto data = mm_metrics.find(entry.name);
        if (data == mm_metrics.end())
            continue;

        uint64_t cur_value = data->second;
        uint64_t prev_value = 0;
        if (prev_mm_metrics->size() != 0) {
            auto prev_data = prev_mm_metrics->find(entry.name);
            if (prev_data != prev_mm_metrics->end())
                prev_value = prev_data->second;
        }

        if (entry.update_diff) {
            tmp.set<VendorAtomValue::longValue>(cur_value - prev_value);
        } else {
            tmp.set<VendorAtomValue::longValue>(cur_value);
        }
        (*atom_values)[atom_idx] = tmp;
    }
    (*prev_mm_metrics) = mm_metrics;
}

void MmMetricsReporter::logPixelMmMetricsPerHour(const std::shared_ptr<IStats> &stats_client) {
    std::map<std::string, uint64_t> vmstat = readVmStat(kVmstatPath);
    if (vmstat.size() == 0)
        return;

    uint64_t ion_total_pools = getIonTotalPools();

    std::vector<VendorAtomValue> values;
    bool is_first_atom = (prev_hour_vmstat_.size() == 0) ? true : false;
    fillAtomValues(kMmMetricsPerHourInfo, vmstat, &prev_hour_vmstat_, &values);

    // resize values to add the following fields
    VendorAtomValue tmp;
    tmp.set<VendorAtomValue::longValue>(0);
    int size = PixelMmMetricsPerHour::kIonTotalPoolsFieldNumber - kVendorAtomOffset + 1;
    if (values.size() < size) {
        values.resize(size, tmp);
    }
    tmp.set<VendorAtomValue::longValue>(ion_total_pools);
    values[PixelMmMetricsPerHour::kIonTotalPoolsFieldNumber - kVendorAtomOffset] = tmp;

    // Don't report the first atom to avoid big spike in accumulated values.
    if (!is_first_atom) {
        // Send vendor atom to IStats HAL
        VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                            .atomId = PixelAtoms::Ids::PIXEL_MM_METRICS_PER_HOUR,
                            .values = std::move(values)};
        const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
        if (!ret.isOk())
            ALOGE("Unable to report PixelMmMetricsPerHour to Stats service");
    }
}

void MmMetricsReporter::logPixelMmMetricsPerDay(const std::shared_ptr<IStats> &stats_client) {
    std::map<std::string, uint64_t> vmstat = readVmStat(kVmstatPath);
    if (vmstat.size() == 0)
        return;

    std::vector<VendorAtomValue> values;
    bool is_first_atom = (prev_day_vmstat_.size() == 0) ? true : false;
    fillAtomValues(kMmMetricsPerDayInfo, vmstat, &prev_day_vmstat_, &values);

    // Don't report the first atom to avoid big spike in accumulated values.
    if (!is_first_atom) {
        // Send vendor atom to IStats HAL
        VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                            .atomId = PixelAtoms::Ids::PIXEL_MM_METRICS_PER_DAY,
                            .values = std::move(values)};
        const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
        if (!ret.isOk())
            ALOGE("Unable to report MEMORY_MANAGEMENT_INFO to Stats service");
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
