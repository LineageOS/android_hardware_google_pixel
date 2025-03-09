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

#include <aidl/android/hardware/power/BnPower.h>
#include <fmq/AidlMessageQueue.h>
#include <fmq/EventFlag.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>

#include "aidl/AdpfTypes.h"
#include "aidl/ChannelGroup.h"
#include "aidl/ChannelManager.h"
#include "aidl/android/hardware/power/ChannelMessage.h"
#include "aidl/android/hardware/power/WorkDurationFixedV1.h"
#include "android/binder_auto_utils.h"
#include "gmock/gmock.h"
#include "mocks/MockPowerHintSession.h"
#include "mocks/MockPowerSessionManager.h"

namespace aidl::google::hardware::power::impl::pixel {

using namespace std::chrono_literals;
using namespace testing;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;
using ::android::hardware::EventFlag;
using android::hardware::power::ChannelMessage;

using SessionMessageQueue = AidlMessageQueue<ChannelMessage, SynchronizedReadWrite>;
using FlagMessageQueue = AidlMessageQueue<int8_t, SynchronizedReadWrite>;

constexpr int kChannelId = 1;

class ChannelGroupTest : public Test {
  public:
    virtual void SetUp() {
        mManager = NiceMock<mock::pixel::MockPowerSessionManager>::getInstance();
        Mock::VerifyAndClear(mManager);
    }

  protected:
    NiceMock<mock::pixel::MockPowerSessionManager> *mManager;
    ChannelGroup<NiceMock<mock::pixel::MockPowerSessionManager>,
                 NiceMock<mock::pixel::MockPowerHintSession>>
            mChannelGroup{1};
};

class FMQTest : public ChannelGroupTest {
  public:
    void SetUp() override {
        ChannelGroupTest::SetUp();
        std::optional<FlagQueueDesc> flagDesc;
        mChannelGroup.getFlagDesc(&flagDesc);
        mBackendChannel = mChannelGroup.createChannel(mTestTgid, mTestUid);
        ChannelQueueDesc channelDesc;
        mBackendChannel->getDesc(&channelDesc);
        mChannel = std::make_shared<SessionMessageQueue>(channelDesc, true);
        mReadFlag = mBackendChannel->getReadBitmask();
        mWriteFlag = mBackendChannel->getWriteBitmask();
        ASSERT_TRUE(mChannel->isValid());

        if (flagDesc.has_value()) {
            mFlagChannel = std::make_shared<FlagMessageQueue>(*flagDesc, true);
            ASSERT_EQ(EventFlag::createEventFlag(mFlagChannel->getEventFlagWord(), &mEventFlag),
                      ::android::OK);
        } else {
            ASSERT_EQ(EventFlag::createEventFlag(mChannel->getEventFlagWord(), &mEventFlag),
                      ::android::OK);
        }

        ASSERT_NE(mEventFlag, nullptr);

        mMockPowerHintSession = std::make_shared<NiceMock<mock::pixel::MockPowerHintSession>>();
        ON_CALL(*mMockPowerHintSession, getSessionConfig)
                .WillByDefault(DoAll(SetArgPointee<0>(SessionConfig{.id = mSessionId}),
                                     Return(ByMove(ndk::ScopedAStatus::ok()))));

        ON_CALL(*mManager, getSession(Eq(mSessionId))).WillByDefault(Return(mMockPowerHintSession));
    }
    void TearDown() {}

  protected:
    int mTestTgid = 123;
    int mTestUid = 234;
    int mSessionId = 4;
    uint32_t mReadFlag;
    uint32_t mWriteFlag;
    std::shared_ptr<IPowerHintSession> mSession;
    std::shared_ptr<SessionMessageQueue> mChannel;
    std::shared_ptr<FlagMessageQueue> mFlagChannel;
    std::shared_ptr<SessionChannel> mBackendChannel;
    ::android::hardware::EventFlag *mEventFlag;
    // SessionConfig mSessionConfig{.id=mSessionId};
    std::shared_ptr<NiceMock<mock::pixel::MockPowerHintSession>> mMockPowerHintSession;
};

bool WorkDurationsAreSame(WorkDuration a, WorkDuration b) {
    return a.timeStampNanos == b.timeStampNanos && a.cpuDurationNanos,
           b.cpuDurationNanos && a.gpuDurationNanos == b.gpuDurationNanos &&
                   a.workPeriodStartTimestampNanos == b.workPeriodStartTimestampNanos &&
                   a.durationNanos == b.durationNanos;
}

TEST_F(ChannelGroupTest, testCreateAndDestroyChannelGroup) {}

TEST_F(ChannelGroupTest, testCreateChannel) {
    int tgid = 234;
    int uid = 123;
    int count1 = mChannelGroup.getChannelCount();
    auto out = mChannelGroup.createChannel(tgid, uid);

    EXPECT_EQ(mChannelGroup.getChannelCount(), count1 + 1);
    EXPECT_EQ(out->getUid(), uid);
    EXPECT_EQ(out->getTgid(), tgid);
}

TEST_F(ChannelGroupTest, testGetChannel) {
    int tgid = 234;
    int uid = 123;
    int count1 = mChannelGroup.getChannelCount();
    auto out1 = mChannelGroup.createChannel(tgid, uid);
    auto out2 = mChannelGroup.getChannel(
            ChannelManager<>::ChannelMapValue{.value = out1->getId()}.offset);

    EXPECT_EQ(mChannelGroup.getChannelCount(), count1 + 1);
    EXPECT_EQ(out1->getId(), out2->getId());
    EXPECT_EQ(out1->getTgid(), out2->getTgid());
    EXPECT_EQ(out1->getId(), out2->getId());
}

TEST_F(ChannelGroupTest, testRemoveChannel) {
    int tgid = 234;
    int uid = 123;
    int count1 = mChannelGroup.getChannelCount();
    auto out1 = mChannelGroup.createChannel(tgid, uid);

    EXPECT_EQ(mChannelGroup.getChannelCount(), count1 + 1);

    mChannelGroup.removeChannel(ChannelManager<>::ChannelMapValue{.value = out1->getId()}.offset);

    EXPECT_EQ(mChannelGroup.getChannelCount(), count1);
}

TEST_F(ChannelGroupTest, testGetFlagDesc) {
    std::optional<FlagQueueDesc> desc;
    mChannelGroup.getFlagDesc(&desc);

    EXPECT_EQ(desc.has_value(), true);
}

TEST_F(FMQTest, testSendingHint) {
    std::promise<SessionHint> sentHint;
    EXPECT_CALL(*mMockPowerHintSession, sendHint).Times(1).WillOnce([&](SessionHint hint) {
        sentHint.set_value(hint);
        return ndk::ScopedAStatus::ok();
    });

    ChannelMessage in{.timeStampNanos = 1L,
                      .sessionID = mSessionId,
                      .data = ChannelMessage::ChannelMessageContents::make<
                              ChannelMessage::ChannelMessageContents::Tag::hint>(
                              SessionHint::GPU_LOAD_RESET)};
    mChannel->writeBlocking(&in, 1, mReadFlag, mWriteFlag, 0, mEventFlag);

    auto future = sentHint.get_future();
    auto status = future.wait_for(1s);
    EXPECT_EQ(status, std::future_status::ready);
    SessionHint out = future.get();

    EXPECT_EQ(out, SessionHint::GPU_LOAD_RESET);
}

ChannelMessage fromWorkDuration(WorkDuration in, int32_t sessionId) {
    return ChannelMessage{
            .timeStampNanos = in.timeStampNanos,
            .sessionID = sessionId,
            .data = ChannelMessage::ChannelMessageContents::make<
                    ChannelMessage::ChannelMessageContents::Tag::workDuration>(WorkDurationFixedV1{
                    .durationNanos = in.durationNanos,
                    .workPeriodStartTimestampNanos = in.workPeriodStartTimestampNanos,
                    .cpuDurationNanos = in.cpuDurationNanos,
                    .gpuDurationNanos = in.gpuDurationNanos})};
}

TEST_F(FMQTest, testSendingReportActualMessage) {
    std::promise<WorkDuration> reportedDuration;
    EXPECT_CALL(*mMockPowerHintSession, reportActualWorkDuration)
            .Times(1)
            .WillOnce([&](const std::vector<WorkDuration> &actualDurations) {
                reportedDuration.set_value(actualDurations[0]);
                return ndk::ScopedAStatus::ok();
            });

    WorkDuration expected{.timeStampNanos = 1L,
                          .durationNanos = 5L,
                          .workPeriodStartTimestampNanos = 3L,
                          .cpuDurationNanos = 4L,
                          .gpuDurationNanos = 5L};

    ChannelMessage in = fromWorkDuration(expected, mSessionId);

    mChannel->writeBlocking(&in, 1, mReadFlag, mWriteFlag, 0, mEventFlag);

    auto future = reportedDuration.get_future();
    auto status = future.wait_for(1s);
    EXPECT_EQ(status, std::future_status::ready);
    WorkDuration out = future.get();

    EXPECT_EQ(WorkDurationsAreSame(expected, out), true);
}

TEST_F(FMQTest, testSendingManyReportActualMessages) {
    std::promise<std::vector<WorkDuration>> reportedDurations;
    EXPECT_CALL(*mMockPowerHintSession, reportActualWorkDuration)
            .Times(1)
            .WillOnce([&](const std::vector<WorkDuration> &actualDurations) {
                reportedDurations.set_value(actualDurations);
                return ndk::ScopedAStatus::ok();
            });

    WorkDuration expectedBase{.timeStampNanos = 10L,
                              .durationNanos = 50L,
                              .workPeriodStartTimestampNanos = 30L,
                              .cpuDurationNanos = 40L,
                              .gpuDurationNanos = 50L};

    std::vector<WorkDuration> in;
    std::vector<ChannelMessage> messagesIn;
    for (int i = 0; i < 20; i++) {
        in.emplace_back(expectedBase.timeStampNanos + i, expectedBase.durationNanos + i,
                        expectedBase.workPeriodStartTimestampNanos + i,
                        expectedBase.cpuDurationNanos + i, expectedBase.gpuDurationNanos + i);
        messagesIn.emplace_back(fromWorkDuration(in[i], mSessionId));
    }

    mChannel->writeBlocking(messagesIn.data(), 20, mReadFlag, mWriteFlag, 0, mEventFlag);

    auto future = reportedDurations.get_future();
    auto status = future.wait_for(1s);
    EXPECT_EQ(status, std::future_status::ready);
    std::vector<WorkDuration> out = future.get();
    EXPECT_EQ(out.size(), 20);

    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(WorkDurationsAreSame(in[i], out[i]), true);
    }
}

}  // namespace aidl::google::hardware::power::impl::pixel
