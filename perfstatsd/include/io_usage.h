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
#ifndef _IO_USAGE_H_
#define _IO_USAGE_H_

#include <statstype.h>
#include <sstream>
#include <string>

#include <unordered_map>

#define IO_USAGE_BUFFER_SIZE (6 * 30)
#define IO_TOP_MAX 5

namespace android {
namespace pixel {
namespace perfstatsd {

class ProcPidIoStats {
  private:
    std::chrono::system_clock::time_point mCheckTime;
    std::vector<uint32_t> mPrevPids;
    std::vector<uint32_t> mCurrPids;
    std::unordered_map<uint32_t, std::string> mUidNameMapping;
    // functions
    std::vector<uint32_t> getNewPids() {
        std::vector<uint32_t> newpids;
        // Not exists in Previous
        for (int i = 0, len = mCurrPids.size(); i < len; i++) {
            if (std::find(mPrevPids.begin(), mPrevPids.end(), mCurrPids[i]) == mPrevPids.end()) {
                newpids.push_back(mCurrPids[i]);
            }
        }
        return newpids;
    }

  public:
    void update(bool forceAll);
    bool getNameForUid(uint32_t uid, std::string *name);
};

struct user_io {
    uint32_t uid;
    uint64_t fg_read;
    uint64_t bg_read;
    uint64_t fg_write;
    uint64_t bg_write;
    uint64_t fg_fsync;
    uint64_t bg_fsync;

    user_io &operator=(const user_io &other) {
        uid = other.uid;
        fg_read = other.fg_read;
        bg_read = other.bg_read;
        fg_write = other.fg_write;
        bg_write = other.bg_write;
        fg_fsync = other.fg_fsync;
        bg_fsync = other.bg_fsync;
        return *this;
    }
    user_io operator-(const user_io &other) const {
        user_io r;
        r.uid = uid;
        r.fg_read = fg_read - other.fg_read;
        r.bg_read = bg_read - other.bg_read;
        r.fg_write = fg_write - other.fg_write;
        r.bg_write = bg_write - other.bg_write;
        r.fg_fsync = fg_fsync - other.fg_fsync;
        r.bg_fsync = bg_fsync - other.bg_fsync;
        return r;
    }
    user_io operator+(const user_io &other) const {
        user_io r;
        r.uid = uid;
        r.fg_read = fg_read + other.fg_read;
        r.bg_read = bg_read + other.bg_read;
        r.fg_write = fg_write + other.fg_write;
        r.bg_write = bg_write + other.bg_write;
        r.fg_fsync = fg_fsync + other.fg_fsync;
        r.bg_fsync = bg_fsync + other.bg_fsync;
        return r;
    }

    void reset() {
        uid = 0;
        fg_read = 0;
        bg_read = 0;
        fg_write = 0;
        bg_write = 0;
        fg_fsync = 0;
        bg_fsync = 0;
    }
};

class ScopeTimer {
  private:
    std::string mName;
    std::chrono::system_clock::time_point mStart;

  public:
    ScopeTimer();
    ScopeTimer(std::string name);
    ~ScopeTimer();
};

const uint64_t IO_USAGE_DUMP_THRESHOLD = 50L * 1000L * 1000L;  // 50MB
class IoStats {
  private:
    uint64_t mMinSizeOfTotalRead = IO_USAGE_DUMP_THRESHOLD;
    uint64_t mMinSizeOfTotalWrite = IO_USAGE_DUMP_THRESHOLD;
    std::chrono::system_clock::time_point mLast;
    std::chrono::system_clock::time_point mNow;
    std::unordered_map<uint32_t, user_io> mPrevious;
    user_io mTotal;
    user_io mWriteTop[IO_TOP_MAX];
    user_io mReadTop[IO_TOP_MAX];
    std::vector<uint32_t> mUnknownUidList;
    std::unordered_map<uint32_t, std::string> mUidNameMap;
    ProcPidIoStats mProcIoStats;
    // Functions
    std::unordered_map<uint32_t, user_io> calcIncrement(
        const std::unordered_map<uint32_t, user_io> &data);
    void updateTopWrite(user_io usage);
    void updateTopRead(user_io usage);
    void updateUnknownUidList();

  public:
    IoStats() {
        mNow = std::chrono::system_clock::now();
        mLast = mNow;
    }
    void calcAll(std::unordered_map<uint32_t, user_io> &&data);
    void setDumpThresholdSizeForRead(uint64_t size) { mMinSizeOfTotalRead = size; }
    void setDumpThresholdSizeForWrite(uint64_t size) { mMinSizeOfTotalWrite = size; }
    bool dump(std::stringstream *output);
};

class io_usage : public statstype {
  private:
    IoStats mStats;

  public:
    void refresh(void);
    void setOptions(const std::string &key, const std::string &value);
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /*  _IO_USAGE_H_ */
