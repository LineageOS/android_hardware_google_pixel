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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "TestHelper.h"
#include "aidl/PowerHintSession.h"
#include "aidl/PowerSessionManager.h"
#include "mocks/MockHintManager.h"
#include "perfmgr/AdpfConfig.h"

using namespace testing;

namespace aidl::google::hardware::power::impl::pixel {

using TestingPowerSessionManager = PowerSessionManager<NiceMock<mock::pixel::MockHintManager>>;

class PowerSessionManagerTest : public Test {
  public:
    void SetUp() {
        mTestConfig = std::make_shared<::android::perfmgr::AdpfConfig>(makeMockConfig());
        mMockHintManager = NiceMock<mock::pixel::MockHintManager>::GetInstance();
        ON_CALL(*mMockHintManager, GetAdpfProfile()).WillByDefault(Return(mTestConfig));

        mPowerSessionManager =
                PowerSessionManager<NiceMock<mock::pixel::MockHintManager>>::getInstance();
    }

    void TearDown() { Mock::VerifyAndClear(mMockHintManager); }

    int mTgid = 10000;
    int mUid = 1001;
    std::vector<int> mTids = {10000};

    // Make an actual hint session that is associated with this PowerSessionManager, but still has
    // HintManager mocked
    std::shared_ptr<
            PowerHintSession<NiceMock<mock::pixel::MockHintManager>, TestingPowerSessionManager>>
    makeHintSession() {
        std::vector<int> tids = {10000};
        return ndk::SharedRefBase::make<PowerHintSession<NiceMock<mock::pixel::MockHintManager>,
                                                         TestingPowerSessionManager>>(
                10000, 1001, tids, 1, SessionTag::OTHER);
    }

  protected:
    std::shared_ptr<::android::perfmgr::AdpfConfig> mTestConfig;
    NiceMock<mock::pixel::MockHintManager> *mMockHintManager;
    PowerSessionManager<NiceMock<mock::pixel::MockHintManager>> *mPowerSessionManager;
};

TEST_F(PowerSessionManagerTest, ensureSessionTrackerWorks) {
    auto session = makeHintSession();
    SessionConfig config;
    session->getSessionConfig(&config);

    // Insert the session into the tracker
    mPowerSessionManager->registerSession(session, config.id);

    // Ensure they are the exact same session
    auto trackedSession = std::static_pointer_cast<
            PowerHintSession<NiceMock<mock::pixel::MockHintManager>, TestingPowerSessionManager>>(
            mPowerSessionManager->getSession(config.id));
    ASSERT_EQ(trackedSession.get(), session.get());

    // Remove the session
    mPowerSessionManager->unregisterSession(config.id);

    // Ensure it is gone
    trackedSession = std::static_pointer_cast<
            PowerHintSession<NiceMock<mock::pixel::MockHintManager>, TestingPowerSessionManager>>(
            mPowerSessionManager->getSession(config.id));
    ASSERT_EQ(trackedSession.get(), nullptr);
}

TEST_F(PowerSessionManagerTest, ensureSessionDeregistersOnDeath) {
    SessionConfig config;
    {
        auto temporaryHintSession = makeHintSession();
        temporaryHintSession->getSessionConfig(&config);

        // Insert the session into the tracker
        mPowerSessionManager->registerSession(temporaryHintSession, config.id);

        // Ensure it is there
        auto trackedSession =
                std::static_pointer_cast<PowerHintSession<NiceMock<mock::pixel::MockHintManager>,
                                                          TestingPowerSessionManager>>(
                        mPowerSessionManager->getSession(config.id));
        ASSERT_NE(trackedSession, nullptr);

        // Kill the session
    }

    // Ensure it is gone
    auto trackedSession = std::static_pointer_cast<
            PowerHintSession<NiceMock<mock::pixel::MockHintManager>, TestingPowerSessionManager>>(
            mPowerSessionManager->getSession(config.id));
    ASSERT_EQ(trackedSession.get(), nullptr);
}

}  // namespace aidl::google::hardware::power::impl::pixel
