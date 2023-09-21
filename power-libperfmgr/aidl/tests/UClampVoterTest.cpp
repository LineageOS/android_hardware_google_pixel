/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <gtest/gtest.h>

#include "aidl/UClampVoter.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using std::literals::chrono_literals::operator""s;
using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""ns;

TEST(VoteRange, active) {
    auto tNow = std::chrono::steady_clock::now();
    VoteRange vr(true, 101, 202, tNow, 200ms);
    EXPECT_TRUE(vr.active());
}

TEST(VoteRange, inActive) {
    auto tNow = std::chrono::steady_clock::now();
    VoteRange vr(false, 101, 202, tNow, 200ms);
    EXPECT_FALSE(vr.active());
}

TEST(VoteRange, makeMinRange) {
    auto tNow = std::chrono::steady_clock::now();
    auto vr = VoteRange::makeMinRange(123, tNow, 210ms);
    EXPECT_EQ(123, vr.uclampMin());
    EXPECT_EQ(tNow, vr.startTime());
    EXPECT_EQ(210ms, vr.durationNs());
}

TEST(VoteRange, isTimeInRange) {
    const auto tNow = std::chrono::steady_clock::now();
    auto voteRange = VoteRange::makeMinRange(234, tNow, 250ms);
    EXPECT_EQ(234, voteRange.uclampMin());
    EXPECT_FALSE(voteRange.isTimeInRange(tNow - 1ns));
    EXPECT_TRUE(voteRange.isTimeInRange(tNow));
    EXPECT_TRUE(voteRange.isTimeInRange(tNow + 1ns));
    EXPECT_TRUE(voteRange.isTimeInRange(tNow + 250ms));
    EXPECT_FALSE(voteRange.isTimeInRange(tNow + 250ms + 1ns));
}

TEST(VoteRange, isTimeInRangeInActive) {
    const auto tNow = std::chrono::steady_clock::now();
    auto voteRange = VoteRange::makeMinRange(345, tNow, 250ms);
    voteRange.setActive(false);
    EXPECT_FALSE(voteRange.active());
    // Still reports 345 as the min even if inactive
    EXPECT_EQ(345, voteRange.uclampMin());
    EXPECT_FALSE(voteRange.isTimeInRange(tNow));
    EXPECT_FALSE(voteRange.isTimeInRange(tNow + 1ns));
    EXPECT_FALSE(voteRange.isTimeInRange(tNow + 250ms));
    EXPECT_FALSE(voteRange.isTimeInRange(tNow + 250ms + 1ns));
}

TEST(VoteRange, getUclampRange) {
    const auto tNow = std::chrono::steady_clock::now();
    const auto tNext = tNow + 1s;
    const auto tEnd1 = tNow + 4000000001ns;
    const auto tPrev = tNow - 1s;

    const auto voteFirst = VoteRange::makeMinRange(11, tNow, 4000000000ns);
    EXPECT_FALSE(voteFirst.isTimeInRange(tPrev));
    EXPECT_TRUE(voteFirst.isTimeInRange(tNext));
    EXPECT_FALSE(voteFirst.isTimeInRange(tEnd1));

    Votes votes;
    votes.add(1, voteFirst);
    UclampRange ur;

    votes.getUclampRange(&ur, tNext);
    EXPECT_EQ(11, ur.uclampMin);
    EXPECT_EQ(kUclampMax, ur.uclampMax);
}

TEST(UclampVoter, simple) {
    const auto tNow = std::chrono::steady_clock::now();

    auto votes = std::make_shared<Votes>();
    EXPECT_EQ(0, votes->size());

    votes->add(1, VoteRange::makeMinRange(11, tNow, 4s));
    EXPECT_EQ(1, votes->size());

    votes->add(2, VoteRange::makeMinRange(22, tNow, 1s));
    EXPECT_EQ(2, votes->size());

    UclampRange ur1;
    votes->getUclampRange(&ur1, tNow);
    EXPECT_EQ(22, ur1.uclampMin);
    EXPECT_EQ(kUclampMax, ur1.uclampMax);

    UclampRange ur2;
    votes->getUclampRange(&ur2, tNow + 2s);
    EXPECT_EQ(11, ur2.uclampMin);
    EXPECT_EQ(kUclampMax, ur2.uclampMax);

    UclampRange ur3;
    votes->getUclampRange(&ur3, tNow + 5s);
    EXPECT_EQ(0, ur3.uclampMin);
    EXPECT_EQ(1024, ur3.uclampMax);

    EXPECT_FALSE(votes->allTimedOut(tNow + 200ns));
    EXPECT_TRUE(votes->allTimedOut(tNow + 5s));

    EXPECT_TRUE(votes->setUseVote(2, false));
    EXPECT_TRUE(votes->anyTimedOut(tNow + 5s));
    EXPECT_TRUE(votes->setUseVote(2, true));

    EXPECT_FALSE(votes->anyTimedOut(tNow + 200ns));
}

TEST(UclampVoter, overwrite) {
    const auto tNow = std::chrono::steady_clock::now();

    auto votes = std::make_shared<Votes>();
    EXPECT_EQ(0, votes->size());

    votes->add(1, VoteRange::makeMinRange(11, tNow, 4s));
    EXPECT_EQ(1, votes->size());

    votes->add(2, VoteRange::makeMinRange(22, tNow, 2s));
    EXPECT_EQ(2, votes->size());

    UclampRange ucr1;
    votes->getUclampRange(&ucr1, tNow + 1s);
    EXPECT_EQ(22, ucr1.uclampMin);

    votes->add(1, VoteRange::makeMinRange(32, tNow, 5s));
    UclampRange ucr2;
    votes->getUclampRange(&ucr2, tNow + 1s);
    EXPECT_EQ(32, ucr2.uclampMin);
}

TEST(UclampVoter, updateDuration) {
    const auto tNow = std::chrono::steady_clock::now();

    auto votes = std::make_shared<Votes>();
    EXPECT_EQ(0, votes->size());

    votes->add(1, VoteRange::makeMinRange(11, tNow, 4s));
    votes->add(2, VoteRange::makeMinRange(22, tNow, 2s));
    EXPECT_EQ(2, votes->size());

    EXPECT_TRUE(votes->allTimedOut(tNow + 7s));
    votes->updateDuration(1, 8s);
    EXPECT_FALSE(votes->allTimedOut(tNow + 7s));
    votes->updateDuration(5, 10s);
    EXPECT_TRUE(votes->allTimedOut(tNow + 9s));
}

TEST(UclampVoter, loadVoteTest) {
    const int defaultVoteId = 1;
    const int loadVoteId = 2;
    const int uclampMinDefault = 50;
    const int uclampMinInit = 162;
    const int uclampMinHigh = 450;
    const auto tNow = std::chrono::steady_clock::now();
    UclampRange ucr;
    auto votes = std::make_shared<Votes>();

    // Default: min = 50 (original)
    votes->add(defaultVoteId, VoteRange::makeMinRange(uclampMinDefault, tNow, 400ms));
    votes->getUclampRange(&ucr, tNow + 100ms);
    EXPECT_EQ(uclampMinDefault, ucr.uclampMin);

    // Default: min = UclampMinInit
    votes->add(defaultVoteId, VoteRange::makeMinRange(uclampMinInit, tNow, 400ns));
    // Load: min = uclampMinHigh
    votes->add(loadVoteId, VoteRange::makeMinRange(uclampMinHigh, tNow, 250ns));

    // Check that load is enabled
    ucr.uclampMin = 0;
    votes->getUclampRange(&ucr, tNow + 100ns);
    EXPECT_EQ(uclampMinHigh, ucr.uclampMin);

    // Timeout or restore after 1st frame
    // Expect to get 162.
    ucr.uclampMin = 0;
    votes->getUclampRange(&ucr, tNow + 350ns);
    EXPECT_EQ(uclampMinInit, ucr.uclampMin);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
