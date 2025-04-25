/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.touch-service.pixel"

#include "GloveMode.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::vendor::lineage::touch::GloveMode;

int main() {
    binder_status_t status = STATUS_OK;

    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<GloveMode> htpr = ndk::SharedRefBase::make<GloveMode>();

    const std::string instanceHtpr = std::string(GloveMode::descriptor) + "/default";
    status = AServiceManager_addService(htpr->asBinder().get(), instanceHtpr.c_str());
    if (status != STATUS_OK) {
        LOG(WARNING) << "Can't register IGloveMode/default";
    }

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
