/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.power.stats-service.pixel"

#include <dataproviders/IioEnergyMeterDataProvider.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

void IioEnergyMeterDataProvider::findIioEnergyMeterNodes() {
    struct dirent *ent;
    DIR *iioDir = opendir(kIioRootDir.c_str());
    if (!iioDir) {
        PLOG(ERROR) << "Error opening directory" << kIioRootDir;
        return;
    }

    // Find any iio:devices that match the given deviceName
    while (ent = readdir(iioDir), ent) {
        std::string devTypeDir = ent->d_name;
        if (devTypeDir.find(kDeviceType) != std::string::npos) {
            std::string devicePath = kIioRootDir + devTypeDir;
            std::string deviceName;
            if (!::android::base::ReadFileToString(devicePath + kNameNode, &deviceName)) {
                LOG(WARNING) << "Failed to read device name from " << devicePath;
            } else if (deviceName.find(kDeviceName) != std::string::npos) {
                mDevicePaths.push_back(devicePath);
            }
        }
    }

    closedir(iioDir);
    return;
}

void IioEnergyMeterDataProvider::parseEnabledRails() {
    std::string data;
    int32_t id = 0;
    for (const auto &path : mDevicePaths) {
        // Get sampling rate
        unsigned long samplingRate;
        if (!::android::base::ReadFileToString(path + kSamplingRateNode, &data) ||
            (samplingRate = std::stoul(data)) == 0) {
            LOG(ERROR) << "Error reading sampling rate from " << path;
            continue;
        }

        // Get list of enabled rails
        if (!::android::base::ReadFileToString(path + kEnabledRailsNode, &data)) {
            LOG(ERROR) << "Error reading enabled rails from " << path;
            continue;
        }

        // Build RailInfos from list of enabled rails
        std::istringstream railNames(data);
        std::string line;
        while (std::getline(railNames, line)) {
            std::vector<std::string> words = ::android::base::Split(line, ":");
            if (words.size() == 2) {
                mChannelInfos.push_back({.channelId = id, .channelName = words[0]});
                mChannelIds.emplace(words[0], id);
                id++;
            } else {
                LOG(WARNING) << "Unexpected enabled rail format in " << path;
            }
        }
    }
}

IioEnergyMeterDataProvider::IioEnergyMeterDataProvider(const std::string &deviceName)
    : kDeviceName(std::move(deviceName)) {
    findIioEnergyMeterNodes();
    parseEnabledRails();
    mReading.resize(mChannelInfos.size());
}

int IioEnergyMeterDataProvider::parseEnergyValue(std::string path) {
    int ret = 0;
    std::string data;
    if (!::android::base::ReadFileToString(path + kEnergyValueNode, &data)) {
        LOG(ERROR) << "Error reading energy value in " << path;
        return -1;
    }

    std::istringstream energyData(data);
    std::string line;
    uint64_t timestamp = 0;
    bool timestampRead = false;
    while (std::getline(energyData, line)) {
        std::vector<std::string> words = ::android::base::Split(line, ",");
        if (timestampRead == false) {
            if (words.size() == 1) {
                timestamp = std::stoull(words[0]);
                if (timestamp == 0 || timestamp == ULLONG_MAX) {
                    LOG(WARNING) << "Potentially wrong timestamp: " << timestamp;
                }
                timestampRead = true;
            }
        } else if (words.size() == 2) {
            std::string railName = words[0];
            if (mChannelIds.count(railName) != 0) {
                size_t id = mChannelIds[railName];
                mReading[id].channelId = id;
                mReading[id].timestampMs = timestamp;
                mReading[id].energyUWs = std::stoull(words[1]);
                if (mReading[id].energyUWs == ULLONG_MAX) {
                    LOG(WARNING) << "Potentially wrong energy value: " << mReading[id].energyUWs;
                }
            }
        } else {
            LOG(WARNING) << "Unexpected energy value format in " << path;
            ret = -1;
            break;
        }
    }
    return ret;
}

ndk::ScopedAStatus IioEnergyMeterDataProvider::readEnergyMeters(
        const std::vector<int32_t> &in_channelIds, std::vector<EnergyMeasurement> *_aidl_return) {
    std::scoped_lock lock(mLock);
    binder_status_t ret = STATUS_OK;
    for (const auto &devicePath : mDevicePaths) {
        if (parseEnergyValue(devicePath) < 0) {
            LOG(ERROR) << "Error in parsing " << devicePath;
            return ndk::ScopedAStatus::fromStatus(STATUS_FAILED_TRANSACTION);
        }
    }

    if (in_channelIds.empty()) {
        *_aidl_return = mReading;
    } else {
        _aidl_return->reserve(in_channelIds.size());
        for (const auto &channelId : in_channelIds) {
            if (channelId < mChannelInfos.size()) {
                _aidl_return->emplace_back(mReading[channelId]);
            } else {
                ret = STATUS_BAD_VALUE;
            }
        }
    }
    return ndk::ScopedAStatus::fromStatus(ret);
}

ndk::ScopedAStatus IioEnergyMeterDataProvider::getEnergyMeterInfo(
        std::vector<ChannelInfo> *_aidl_return) {
    std::scoped_lock lk(mLock);
    *_aidl_return = mChannelInfos;
    return ndk::ScopedAStatus::ok();
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
