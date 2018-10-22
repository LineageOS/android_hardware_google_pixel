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
#include <dirent.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <chrono>
#include <fstream>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

#include "thermal_watcher.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using std::chrono_literals::operator""ms;

void ThermalWatcher::registerFilesToWatch(const std::vector<std::string> files_to_watch) {
    int flags = O_RDONLY | O_CLOEXEC | O_BINARY;

    for (const auto &path : files_to_watch) {
        std::lock_guard<std::mutex> _lock(watcher_mutex_);

        android::base::unique_fd fd(TEMP_FAILURE_RETRY(open(path.c_str(), flags)));
        if (fd == -1) {
            PLOG(ERROR) << "failed to watch: " << path;
            continue;
        }
        watch_to_file_path_map_.emplace(fd.get(), path);
        fds_.emplace_back(std::move(fd));
        looper_->addFd(fd.get(), 0, Looper::EVENT_INPUT, nullptr, nullptr);
    }
}

bool ThermalWatcher::startWatchingDeviceFiles() {
    if (cb_) {
        auto ret = this->run("FileWatcherThread", PRIORITY_HIGHEST);
        if (ret != NO_ERROR) {
            LOG(ERROR) << "ThermalWatcherThread start fail";
            return false;
        } else {
            LOG(INFO) << "ThermalWatcherThread started";
            return true;
        }
    }
    return false;
}

void ThermalWatcher::registerCallback(WatcherCallback cb) {
    std::lock_guard<std::mutex> _lock(watcher_mutex_);
    cb_ = cb;
}

void ThermalWatcher::wake() {
    looper_->wake();
}

bool ThermalWatcher::threadLoop() {
    LOG(VERBOSE) << "ThermalWatcher polling...";
    // Max poll timeout 2s
    static constexpr int kMinPollIntervalMs = 2000;
    int fd = looper_->pollOnce(kMinPollIntervalMs);
    std::string path;
    if (fd > 0) {
        path = watch_to_file_path_map_.at(fd);
    }
    {
        std::lock_guard<std::mutex> _lock(watcher_mutex_);
        cb_(path, fd);
    }
    return true;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
