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

#include "aidl/ChannelManager.h"
#include "mocks/MockPowerHintSession.h"
#include "mocks/MockPowerSessionManager.h"

namespace aidl::google::hardware::power::impl::pixel {

using namespace std::chrono_literals;
using namespace testing;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;
using android::hardware::power::ChannelMessage;

using SessionMessageQueue = AidlMessageQueue<ChannelMessage, SynchronizedReadWrite>;
using FlagMessageQueue = AidlMessageQueue<int8_t, SynchronizedReadWrite>;

class ChannelManagerTest : public Test {
  public:
    virtual void SetUp() {
        mChannelManager = ChannelManager<
                ChannelGroup<testing::NiceMock<mock::pixel::MockPowerSessionManager>,
                             testing::NiceMock<mock::pixel::MockPowerHintSession>>>::getInstance();
    }

  protected:
    ChannelManager<ChannelGroup<testing::NiceMock<mock::pixel::MockPowerSessionManager>,
                                testing::NiceMock<mock::pixel::MockPowerHintSession>>>
            *mChannelManager;
};

TEST_F(ChannelManagerTest, testGetChannelConfig) {
    int kUid = 3000;
    int kTgid = 4000;
    ChannelConfig config;
    auto out = mChannelManager->getChannelConfig(kUid, kTgid, &config);
    ASSERT_EQ(out, true);
}

TEST_F(ChannelManagerTest, testCloseChannel) {
    int kUid = 3000;
    int kTgid = 4000;
    ChannelConfig config;
    mChannelManager->getChannelConfig(kUid, kTgid, &config);
    bool success = mChannelManager->closeChannel(kUid, kTgid);
    ASSERT_EQ(success, true);
}

TEST_F(ChannelManagerTest, testManyChannelsSpawnMoreGroups) {
    int kUid = 3000;
    int kTgid = 4000;
    int kChannelsToSpawn = 40;
    ChannelConfig config;
    // Spawn first one separately to make sure the group is created
    mChannelManager->getChannelConfig(kUid, kTgid, &config);
    ASSERT_EQ(mChannelManager->getChannelCount(), 1);
    ASSERT_EQ(mChannelManager->getGroupCount(), 1);
    for (int i = 1; i < kChannelsToSpawn; ++i) {
        mChannelManager->getChannelConfig(kUid + i, kTgid + i, &config);
    }
    ASSERT_GT(mChannelManager->getGroupCount(), 1);
    ASSERT_EQ(mChannelManager->getChannelCount(), kChannelsToSpawn);
}

TEST_F(ChannelManagerTest, testNewChannelsReplaceOldChannels) {
    int kUid = 3000;
    int kTgid = 4000;
    int kChannelsToSpawn = 40;
    ChannelConfig config;
    // Spawn first one separately to make sure the group isn't destroyed later
    mChannelManager->getChannelConfig(kUid, kTgid, &config);
    for (int i = 1; i < kChannelsToSpawn; ++i) {
        mChannelManager->getChannelConfig(kUid + i, kTgid + i, &config);
        mChannelManager->closeChannel(kUid + i, kTgid + i);
    }
    ASSERT_EQ(mChannelManager->getGroupCount(), 1);
    ASSERT_EQ(mChannelManager->getChannelCount(), 1);
}

TEST_F(ChannelManagerTest, testGroupsCloseOnLastChannelDies) {
    int kUid = 3000;
    int kTgid = 4000;
    int kChannelsToSpawn = 40;
    ChannelConfig config;
    for (int i = 0; i < kChannelsToSpawn; ++i) {
        mChannelManager->getChannelConfig(kUid + i, kTgid + i, &config);
    }
    ASSERT_GT(mChannelManager->getGroupCount(), 0);
    ASSERT_EQ(mChannelManager->getChannelCount(), 40);
    for (int i = 0; i < kChannelsToSpawn; ++i) {
        mChannelManager->closeChannel(kUid + i, kTgid + i);
    }
    ASSERT_EQ(mChannelManager->getGroupCount(), 0);
    ASSERT_EQ(mChannelManager->getChannelCount(), 0);
}

}  // namespace aidl::google::hardware::power::impl::pixel
