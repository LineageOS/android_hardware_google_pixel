/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include <vector>

#include "aidl/SessionRecords.h"

#define MS_TO_NS(x) (x * 1000 * 1000)
#define MS_TO_US(x) (x * 1000)

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

class SessionRecordsTest : public ::testing::Test {
  public:
    void SetUp() { mRecords = std::make_shared<SessionRecords>(kMaxNumOfRecords); }

  protected:
    std::vector<WorkDuration> fakeWorkDurations(const std::vector<int32_t> fakedTotalDurationsMs) {
        std::vector<WorkDuration> fakedWorkDurationsNs;
        for (auto &d : fakedTotalDurationsMs) {
            fakedWorkDurationsNs.emplace_back(0, MS_TO_NS(d));
        }
        return fakedWorkDurationsNs;
    }

    std::vector<WorkDuration> fakeWorkDurations(
            const std::vector<std::pair<int32_t, int32_t>> fakedReportedDurationsMs) {
        std::vector<WorkDuration> fakedWorkDurationsNs;
        for (auto &r : fakedReportedDurationsMs) {
            fakedWorkDurationsNs.emplace_back(MS_TO_NS(r.first), MS_TO_NS(r.second));
        }
        return fakedWorkDurationsNs;
    }

    static const int32_t kMaxNumOfRecords = 5;
    std::shared_ptr<SessionRecords> mRecords;
};

TEST_F(SessionRecordsTest, NoRecords) {
    ASSERT_EQ(0, mRecords->getNumOfRecords());
    ASSERT_FALSE(mRecords->getMaxDuration().has_value());
    ASSERT_FALSE(mRecords->getAvgDuration().has_value());
    ASSERT_EQ(0, mRecords->getNumOfMissedCycles());
}

TEST_F(SessionRecordsTest, addReportedDurations) {
    mRecords->addReportedDurations(fakeWorkDurations({3, 4, 3, 2}), MS_TO_NS(3));
    ASSERT_EQ(4, mRecords->getNumOfRecords());
    ASSERT_EQ(MS_TO_US(4), mRecords->getMaxDuration().value());
    ASSERT_EQ(MS_TO_US(3), mRecords->getAvgDuration().value());
    ASSERT_EQ(1, mRecords->getNumOfMissedCycles());

    // Push more records to override part of the old ones in the ring buffer
    mRecords->addReportedDurations(fakeWorkDurations({2, 1, 2}), MS_TO_NS(3));
    ASSERT_EQ(5, mRecords->getNumOfRecords());
    ASSERT_EQ(MS_TO_US(3), mRecords->getMaxDuration().value());
    ASSERT_EQ(MS_TO_US(2), mRecords->getAvgDuration().value());
    ASSERT_EQ(0, mRecords->getNumOfMissedCycles());

    // More records to override the ring buffer more rounds
    mRecords->addReportedDurations(fakeWorkDurations({10, 2, 9, 8, 4, 5, 7, 6}), MS_TO_NS(3));
    ASSERT_EQ(5, mRecords->getNumOfRecords());
    ASSERT_EQ(MS_TO_US(8), mRecords->getMaxDuration().value());
    ASSERT_EQ(MS_TO_US(6), mRecords->getAvgDuration().value());
    ASSERT_EQ(5, mRecords->getNumOfMissedCycles());
}

TEST_F(SessionRecordsTest, checkLowFrameRate) {
    ASSERT_FALSE(mRecords->isLowFrameRate(25));
    mRecords->addReportedDurations(fakeWorkDurations({{0, 8}, {10, 9}, {20, 8}, {30, 8}}),
                                   MS_TO_NS(10));
    ASSERT_EQ(4, mRecords->getNumOfRecords());
    ASSERT_FALSE(mRecords->isLowFrameRate(25));

    mRecords->addReportedDurations(fakeWorkDurations({{130, 8}, {230, 9}}), MS_TO_NS(10));
    ASSERT_EQ(5, mRecords->getNumOfRecords());
    ASSERT_FALSE(mRecords->isLowFrameRate(25));

    mRecords->addReportedDurations(fakeWorkDurations({{330, 8}, {430, 9}}), MS_TO_NS(10));
    ASSERT_EQ(5, mRecords->getNumOfRecords());
    ASSERT_TRUE(mRecords->isLowFrameRate(25));

    mRecords->addReportedDurations(fakeWorkDurations({{440, 8}, {450, 9}}), MS_TO_NS(10));
    ASSERT_EQ(5, mRecords->getNumOfRecords());
    ASSERT_FALSE(mRecords->isLowFrameRate(25));
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
