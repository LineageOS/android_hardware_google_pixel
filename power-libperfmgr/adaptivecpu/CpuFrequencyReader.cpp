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

#define LOG_TAG "powerhal-libperfmgr"

#include "CpuFrequencyReader.h"

#include <android-base/logging.h>
#include <dirent.h>
#include <inttypes.h>

#include <fstream>
#include <memory>
#include <sstream>

using std::chrono_literals::operator""ms;

constexpr std::string_view kCpuPolicyDirectory("/sys/devices/system/cpu/cpufreq");

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

bool CpuFrequencyReader::init() {
    mCpuPolicyIds = readCpuPolicyIds();
    mPreviousCpuPolicyFrequencies.clear();
    return readCpuPolicyFrequencies(&mPreviousCpuPolicyFrequencies);
}

bool CpuFrequencyReader::getRecentCpuPolicyFrequencies(
        std::vector<CpuPolicyAverageFrequency> *result) {
    std::map<uint32_t, std::map<uint64_t, std::chrono::milliseconds>> cpuPolicyFrequencies;
    if (!readCpuPolicyFrequencies(&cpuPolicyFrequencies)) {
        return false;
    }
    for (const auto &[policyId, cpuFrequencies] : cpuPolicyFrequencies) {
        const auto &previousCpuFrequencies = mPreviousCpuPolicyFrequencies.find(policyId);
        if (previousCpuFrequencies == mPreviousCpuPolicyFrequencies.end()) {
            LOG(ERROR) << "Couldn't find policy " << policyId << " in previous frequencies";
            return false;
        }
        uint64_t weightedFrequenciesSumHz = 0;
        std::chrono::milliseconds timeSum = 0ms;
        for (const auto &[frequencyHz, time] : cpuFrequencies) {
            const auto &previousCpuFrequency = previousCpuFrequencies->second.find(frequencyHz);
            if (previousCpuFrequency == previousCpuFrequencies->second.end()) {
                LOG(ERROR) << "Couldn't find frequency " << frequencyHz
                           << " in previous frequencies";
                return false;
            }
            const std::chrono::milliseconds recentTime = time - previousCpuFrequency->second;
            weightedFrequenciesSumHz += frequencyHz * recentTime.count();
            timeSum += recentTime;
        }
        const uint64_t averageFrequencyHz =
                timeSum != 0ms ? weightedFrequenciesSumHz / timeSum.count() : 0;
        result->push_back({.policyId = policyId, .averageFrequencyHz = averageFrequencyHz});
    }
    mPreviousCpuPolicyFrequencies = cpuPolicyFrequencies;
    return true;
}

std::map<uint32_t, std::map<uint64_t, std::chrono::milliseconds>>
CpuFrequencyReader::getPreviousCpuPolicyFrequencies() const {
    return mPreviousCpuPolicyFrequencies;
}

bool CpuFrequencyReader::readCpuPolicyFrequencies(
        std::map<uint32_t, std::map<uint64_t, std::chrono::milliseconds>> *result) {
    for (const uint32_t cpuPolicyId : mCpuPolicyIds) {
        std::stringstream timeInStatePath;
        timeInStatePath << "/sys/devices/system/cpu/cpufreq/policy" << cpuPolicyId
                        << "/stats/time_in_state";
        std::unique_ptr<std::istream> timeInStateFile =
                mFilesystem->readFile(timeInStatePath.str());

        std::map<uint64_t, std::chrono::milliseconds> cpuFrequencies;
        std::string timeInStateLine;
        while (std::getline(*timeInStateFile, timeInStateLine)) {
            // Time format in time_in_state is 10s of milliseconds:
            // https://www.kernel.org/doc/Documentation/cpu-freq/cpufreq-stats.txt
            uint64_t frequencyHz, time10Ms;
            if (std::sscanf(timeInStateLine.c_str(), "%" PRIu64 " %" PRIu64 "\n", &frequencyHz,
                            &time10Ms) != 2) {
                LOG(ERROR) << "Failed to parse time_in_state line: " << timeInStateLine;
                return false;
            }
            cpuFrequencies[frequencyHz] = time10Ms * 10ms;
        }
        if (cpuFrequencies.size() > 500) {
            LOG(ERROR) << "Found " << cpuFrequencies.size() << " frequencies for policy "
                       << cpuPolicyId << ", aborting";
            return false;
        }
        (*result)[cpuPolicyId] = cpuFrequencies;
    }
    return true;
}

std::vector<uint32_t> CpuFrequencyReader::readCpuPolicyIds() const {
    std::vector<uint32_t> cpuPolicyIds;
    const std::vector<std::string> entries = mFilesystem->listDirectory(kCpuPolicyDirectory.data());
    for (const auto &entry : entries) {
        uint32_t cpuPolicyId;
        if (!sscanf(entry.c_str(), "policy%d", &cpuPolicyId)) {
            continue;
        }
        cpuPolicyIds.push_back(cpuPolicyId);
    }
    return cpuPolicyIds;
}

std::vector<std::string> RealFilesystem::listDirectory(std::string path) const {
    // We can't use std::filesystem, see aosp/894015 & b/175635923.
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        LOG(ERROR) << "Failed to open directory " << path;
    }
    std::vector<std::string> entries;
    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        entries.emplace_back(entry->d_name);
    }
    return entries;
}

std::unique_ptr<std::istream> RealFilesystem::readFile(std::string path) const {
    return std::make_unique<std::ifstream>(path);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
