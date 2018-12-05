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
#ifndef __DEVICE_FILE_WATCHER_H__
#define __DEVICE_FILE_WATCHER_H__

#include <chrono>
#include <condition_variable>
#include <future>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <android-base/unique_fd.h>
#include <utils/Looper.h>
#include <utils/Thread.h>

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using android::base::unique_fd;
using WatcherCallback = std::function<void(const std::string &path, const int fd)>;

// A helper class for polling thermal files for changes.
// TODO: change to use uevent (b/119189816)
class ThermalWatcher : public ::android::Thread {
  public:
    ThermalWatcher(const WatcherCallback &cb)
        : Thread(false), cb_(cb), looper_(new Looper(true)) {}
    ~ThermalWatcher() = default;

    // Disallow copy and assign.
    ThermalWatcher(const ThermalWatcher &) = delete;
    void operator=(const ThermalWatcher &) = delete;

    // Start the thread and return true if it succeeds.
    bool startWatchingDeviceFiles();
    // Give the file watcher a list of files to start watching. This helper
    // class will by default wait for modifications to the file with a looper.
    // This should be called before starting watcher thread.
    void registerFilesToWatch(const std::vector<std::string> &files_to_watch);
    // Wake up the looper thus the worker thread, immediately. This can be called
    // in any thread.
    void wake();

  private:
    // The work done by the watcher thread. This will use inotify to check for
    // modifications to the files to watch. If any modification is seen this
    // will callback the registered function with the new data read from the
    // modified file.
    bool threadLoop() override;

    // Maps watcher filer descriptor to watched file path.
    std::unordered_map<int, std::string> watch_to_file_path_map_;
    std::vector<android::base::unique_fd> fds_;

    // The callback function. Called whenever a modification is seen.  The
    // function passed in should expect a pair of strings in the form
    // (path, data). Where path is the path of the file that saw a modification
    // and data was the modification. Callback will return a time for next polling.
    const WatcherCallback cb_;

    sp<Looper> looper_;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif  // __DEVICE_FILE_WATCHER_H__
