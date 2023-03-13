/*
 * Copyright 2023 The Android Open Source Project
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

#pragma once

#include <chrono>
#include <memory>
#include <ostream>
#include <unordered_map>

#include "AdpfTypes.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

// --------------------------------------------------------
// Hold a min and max for acceptable uclamp values
struct UclampRange {
    int uclampMin{kUclampMin};
    int uclampMax{kUclampMax};
};

// --------------------------------------------------------
// Hold a min max range of acceptable votes, is active status, time duration
// info, and helper methods for consistent use
class VoteRange {
  public:
    VoteRange() {}

    VoteRange(bool active, int uclampMin, int uclampMax,
              std::chrono::steady_clock::time_point startTime, std::chrono::nanoseconds durationNs)
        : mActive(active),
          mUclampRange({std::min(uclampMin, uclampMax), std::max(uclampMin, uclampMax)}),
          mStartTime(startTime),
          mDurationNs(durationNs) {}

    // Returns true if this vote range is active, false if it is not active
    bool active() const { return mActive; }

    // Returns the utilization clamp minimum
    int uclampMin() const { return mUclampRange.uclampMin; }

    // Returns the utilization clamp maximum
    int uclampMax() const { return mUclampRange.uclampMax; }

    // Returns the start time of this vote range
    std::chrono::steady_clock::time_point startTime() const { return mStartTime; }

    // Returns the duration in nanoseconds of the vote range
    std::chrono::nanoseconds durationNs() const { return mDurationNs; }

    // Set the is active flag to bool param
    void setActive(bool active) { mActive = active; }

    // Update the vote duration
    void updateDuration(std::chrono::nanoseconds durationNs) { mDurationNs = durationNs; }

    // Return true if time point parameter in range of startTime to startTime+duration
    inline bool isTimeInRange(std::chrono::steady_clock::time_point t) const {
        return mActive && ((mStartTime <= t) && ((mStartTime + mDurationNs) >= t));
    }

    // Factory method to make a vote range
    static VoteRange makeMinRange(int uclampMin, std::chrono::steady_clock::time_point startTime,
                                  std::chrono::nanoseconds durationNs);

  private:
    bool mActive{true};
    UclampRange mUclampRange;
    std::chrono::steady_clock::time_point mStartTime{};
    std::chrono::nanoseconds mDurationNs{};
};

// Helper for logging
std::ostream &operator<<(std::ostream &o, const VoteRange &vr);

// --------------------------------------------------------
// Thread safe collection of votes that can be used to get
// a clamped range
class Votes {
  public:
    Votes();

    // Add a vote and associate with vote id, overwrites existing vote
    void add(int voteId, const VoteRange &v);

    // Update the duration of a vote given a vote id
    void updateDuration(int voteId, std::chrono::nanoseconds durationNs);

    // Given input UclampRange, and a time point now, increase the min and
    // decrease max if this VoteRange is in range, return UclampRange with
    // the largest min and the smallest max
    void getUclampRange(UclampRange *uclampRange, std::chrono::steady_clock::time_point t) const;

    // Return true if any vote has timed out, otherwise return false
    bool anyTimedOut(std::chrono::steady_clock::time_point t) const;

    // Return true if all votes have timed out, otherwise return false
    bool allTimedOut(std::chrono::steady_clock::time_point t) const;

    // Remove vote based on vote vote id, return true if remove was successful,
    // false if remove failed for example no vote with that id exists
    bool remove(int voteId);

    // Turn on/off vote
    bool setUseVote(int voteId, bool active);

    // Return number of votes
    size_t size() const;

    bool voteIsActive(int voteId);

    std::chrono::steady_clock::time_point voteTimeout(int voteId);

  private:
    std::unordered_map<int, VoteRange> mVotes;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
