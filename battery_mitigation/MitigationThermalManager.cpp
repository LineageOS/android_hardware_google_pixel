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
#define LOG_TAG "mitigation-logger"

#include <aidl/android/hardware/thermal/IThermal.h>
#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <battery_mitigation/MitigationThermalManager.h>
#include <errno.h>
#include <pixelthermalwrapper/ThermalHidlWrapper.h>
#include <sys/time.h>
#include <utils/Log.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#define NUM_OF_SAMPLES     20
#define CAPTURE_INTERVAL_S 2     /* 2 seconds between new capture */

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using android::hardware::thermal::V1_0::ThermalStatus;
using android::hardware::thermal::V1_0::ThermalStatusCode;

MitigationThermalManager &MitigationThermalManager::getInstance() {
    static MitigationThermalManager mitigationThermalManager;
    return mitigationThermalManager;
}

void MitigationThermalManager::remove() {
    const std::lock_guard<std::mutex> lock(thermal_hal_mutex_);
    if (!thermal) {
        return;
    }
    if (callback) {
        ThermalStatus returnStatus;
        auto ret = thermal->unregisterThermalChangedCallback(
                callback, [&returnStatus](ThermalStatus status) { returnStatus = status; });
        if (!ret.isOk() || returnStatus.code != ThermalStatusCode::SUCCESS) {
            LOG(ERROR) << "Failed to release thermal callback!";
        }
    }
    if (hidlDeathRecipient) {
        auto ret = thermal->unlinkToDeath(hidlDeathRecipient);
        if (!ret.isOk()) {
            LOG(ERROR) << "Failed to release thermal death notification!";
        }
    }
}

MitigationThermalManager::MitigationThermalManager() {
    if (!ABinderProcess_isThreadPoolStarted()) {
        std::thread([]() {
            ABinderProcess_joinThreadPool();
            LOG(ERROR) << "SHOULD NOT EXIT";
        }).detach();
        LOG(ERROR) << "The user of MitigationThermalManager did not start a threadpool";
    }
    if (!connectThermalHal()) {
        remove();
    }
}

MitigationThermalManager::~MitigationThermalManager() {
    remove();
}

void MitigationThermalManager::updateConfig(const struct MitigationConfig::Config &cfg) {
    kLogFilePath = std::string(cfg.LogFilePath);
    kSystemPath = cfg.SystemPath;
    kSystemName = cfg.SystemName;
    kFilteredZones = cfg.FilteredZones;
    kTimestampFormat = cfg.TimestampFormat;
}

bool MitigationThermalManager::connectThermalHal() {
    const std::string thermal_instance_name =
            std::string(::aidl::android::hardware::thermal::IThermal::descriptor) + "/default";
    std::shared_ptr<aidl::android::hardware::thermal::IThermal> thermal_aidl_service;
    const std::lock_guard<std::mutex> lock(thermal_hal_mutex_);
    if (AServiceManager_isDeclared(thermal_instance_name.c_str())) {
        thermal_aidl_service = ::aidl::android::hardware::thermal::IThermal::fromBinder(
                ndk::SpAIBinder(AServiceManager_waitForService(thermal_instance_name.c_str())));
        if (thermal_aidl_service) {
            thermal = sp<::aidl::android::hardware::thermal::ThermalHidlWrapper>::make(
                    thermal_aidl_service);
        } else {
            LOG(ERROR) << "Unable to connect Thermal AIDL service, trying HIDL...";
        }
    } else {
        LOG(WARNING) << "Thermal AIDL service is not declared, trying HIDL...";
    }

    if (!thermal) {
        thermal = IThermal::getService();
    }

    if (thermal) {
        lastCapturedTime = ::android::base::boot_clock::now();
        if (thermal_aidl_service) {
            registerCallback(thermal_aidl_service->asBinder().get());
        } else {
            registerCallback(nullptr);
        }
        return true;
    } else {
        LOG(ERROR) << "Cannot get IThermal service!";
    }
    return false;
}

bool MitigationThermalManager::isMitigationTemperature(const Temperature &temperature) {
    if (std::find(kFilteredZones.begin(), kFilteredZones.end(), temperature.name) !=
            kFilteredZones.end()) {
        return true;
    }
    return false;
}

void MitigationThermalManager::thermalCb(const Temperature &temperature) {
    if ((temperature.throttlingStatus == ThrottlingSeverity::NONE) ||
          (!isMitigationTemperature(temperature))) {
        return;
    }
    auto currentTime = ::android::base::boot_clock::now();
    auto delta =
            std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastCapturedTime);
    if (delta.count() < CAPTURE_INTERVAL_S) {
        /* Do not log if delta is within 2 seconds */
        return;
    }
    int flag = O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_APPEND | O_TRUNC;
    android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(kLogFilePath.c_str(), flag, 0644)));
    lastCapturedTime = currentTime;
    std::stringstream oss;
    oss << temperature.name << " triggered at " << temperature.value << std::endl << std::flush;
    android::base::WriteStringToFd(oss.str(), fd);
    fsync(fd);

    for (int i = 0; i < NUM_OF_SAMPLES; i++) {
        auto now = std::chrono::system_clock::now();
        auto time_sec = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) -
                std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
        struct tm now_tm;
        localtime_r(&time_sec, &now_tm);
        oss << std::put_time(&now_tm, kTimestampFormat.c_str()) << "." << std::setw(3)
            << std::setfill('0') << ms.count() << std::endl
            << std::flush;
        android::base::WriteStringToFd(oss.str(), fd);
        fsync(fd);
        oss.str("");
        /* log System info */
        for (int j = 0; j < kSystemName.size(); j++) {
            std::string value;
            bool result = android::base::ReadFileToString(kSystemPath[j], &value);
            if (!result) {
                LOG(ERROR) << "Could not read: " << kSystemName[j];
            }
            android::base::WriteStringToFd(kSystemName[j] + ":" + value, fd);
        }
    }
    fsync(fd);
}

void MitigationThermalManager::registerCallback(AIBinder *thermal_aidl_binder) {
    if (!thermal) {
        LOG(ERROR) << "Cannot register thermal callback!";
        return;
    }
    ThermalStatus returnStatus;
    // Create thermal callback recipient object.
    if (callback == nullptr) {
        callback = new MitigationThermalManager::ThermalCallback(
                [this](const Temperature &temperature) {
                    std::lock_guard api_lock(thermal_callback_mutex_);
                    thermalCb(temperature);
                });
    }
    // Register thermal callback to thermal hal to cover all.  Cannot register twice.
    auto ret_callback = thermal->registerThermalChangedCallback(
            callback, false, TemperatureType::UNKNOWN,
            [&returnStatus](ThermalStatus status) { return returnStatus = status; });
    if (!ret_callback.isOk() || returnStatus.code != ThermalStatusCode::SUCCESS) {
        LOG(ERROR) << "Failed to register thermal callback!";
    }

    // Register thermal death notification to thermal hal.
    if (thermal_aidl_binder) {
        // Create AIDL thermal death recipient object.
        if (aidlDeathRecipient.get() == nullptr) {
            aidlDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(
                    AIBinder_DeathRecipient_new(onThermalAidlBinderDied));
        }
        auto linked = AIBinder_linkToDeath(thermal_aidl_binder, aidlDeathRecipient.get(), this);
        if (linked != STATUS_OK) {
            LOG(ERROR) << "Failed to register AIDL thermal death notification!";
        }
    } else {
        // Create HIDL thermal death recipient object.
        if (hidlDeathRecipient == nullptr) {
            hidlDeathRecipient = new MitigationThermalManager::ThermalDeathRecipient();
        }
        auto retLink = thermal->linkToDeath(hidlDeathRecipient, 0);
        if (!retLink.isOk()) {
            LOG(ERROR) << "Failed to register HIDL thermal death notification!";
        }
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
