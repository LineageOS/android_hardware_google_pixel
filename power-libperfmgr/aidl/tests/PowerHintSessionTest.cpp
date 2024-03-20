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

#include <aidl/android/hardware/power/SessionTag.h>
#include <android-base/file.h>
#include <gtest/gtest.h>
#include <sys/syscall.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// define private as public to expose the private members for test.
#define private public
#include "aidl/PowerHintSession.h"
#include "aidl/PowerSessionManager.h"

#define gettid() syscall(SYS_gettid)

using std::literals::chrono_literals::operator""ms;
using std::literals::chrono_literals::operator""ns;
using std::literals::chrono_literals::operator""s;
using android::base::ReadFileToString;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

class PowerHintSessionTest : public ::testing::Test {
  public:
    void SetUp() {
        // create a list of threads
        for (int i = 0; i < numOfThreads; i++) {
            threadIsAlive.emplace_back(true);
            threadList.emplace_back(std::thread([this, threadInd = i]() {
                ALOGI("Test thread %d is running.", (int32_t)gettid());
                {
                    std::lock_guard<std::mutex> lock(m);
                    threadIds[threadInd] = gettid();
                }
                while (threadIsAlive[threadInd]) {
                    std::this_thread::sleep_for(50ms);
                }
                ALOGI("Test thread %d is closed.", (int32_t)gettid());
            }));
        }
        std::this_thread::sleep_for(50ms);

        // create two hint sessions
        for (int i = 0; i < numOfThreads; i++) {
            if (i <= numOfThreads / 2) {
                session1Threads.emplace_back(threadIds[i]);
            }

            if (i >= numOfThreads / 2) {
                session2Threads.emplace_back(threadIds[i]);
            }
        }

        sess1 = ndk::SharedRefBase::make<PowerHintSession>(1, 1, session1Threads, 1000000,
                                                           SessionTag::OTHER);
        sess2 = ndk::SharedRefBase::make<PowerHintSession>(2, 2, session2Threads, 1000000,
                                                           SessionTag::OTHER);
    }

    void TearDown() {
        for (int i = 0; i < numOfThreads; i++) {
            if (threadIsAlive[i]) {
                threadIsAlive[i] = false;
                threadList[i].join();
            }
        }
        threadList.clear();
        threadIds.clear();
        threadIsAlive.clear();
        session1Threads.clear();
        session2Threads.clear();
    }

  protected:
    static const int numOfThreads = 3;
    std::vector<std::thread> threadList;
    std::unordered_map<int, int32_t> threadIds;
    std::vector<bool> threadIsAlive;
    std::mutex m;
    std::vector<int32_t> session1Threads;
    std::vector<int32_t> session2Threads;
    std::shared_ptr<PowerHintSession> sess1;
    std::shared_ptr<PowerHintSession> sess2;

    // close the i-th thread in thread list.
    void closeThread(int i) {
        if (i < 0 || i >= numOfThreads)
            return;
        if (threadIsAlive[i]) {
            threadIsAlive[i] = false;
            threadList[i].join();
        }
    }

    // Reads the session active flag from a sched dump for a pid. Returns error status and
    // stores result in isActive.
    bool ReadThreadADPFTag(pid_t pid, bool *isActive) {
        std::string pidStr = std::to_string(pid);
        std::string schedDump;
        *isActive = false;

        // Store the SchedDump into a string.
        if (!ReadFileToString("/proc/vendor_sched/dump_task", &schedDump)) {
            std::cerr << "Error: Could not read /proc/vendor_sched/dump_task." << std::endl;
            return false;
        }

        // Find our pid entry start from the sched dump.
        // We use rfind since the dump is ordered by PID and we made a new thread recently.
        size_t pid_position = schedDump.rfind(pidStr);
        if (pid_position == std::string::npos) {
            std::cerr << "Error: pid not found in sched dump." << std::endl;
            return false;
        }

        // Find the end boundary of our sched dump entry.
        size_t entry_end_position = schedDump.find_first_of("\n", pid_position);
        if (entry_end_position == std::string::npos) {
            std::cerr << "Error: could not find end of sched dump entry." << std::endl;
            return false;
        }

        // Extract our sched dump entry.
        std::string threadEntry = schedDump.substr(pid_position, entry_end_position - pid_position);
        if (threadEntry.size() < 3) {
            std::cerr << "Error: sched dump entry invalid." << std::endl;
            return false;
        }

        // We do reverse array access since the first entries have variable length.
        char powerSessionActiveFlag = threadEntry[threadEntry.size() - 3];
        if (powerSessionActiveFlag == '1') {
            *isActive = true;
        }

        // At this point, we have found a valid entry with SessionAllowed == bool, so we return
        // success status.
        return true;
    }
};

TEST_F(PowerHintSessionTest, removeDeadThread) {
    ALOGI("Running dead thread test for hint sessions.");
    auto sessManager = sess1->mPSManager;
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions.size());

    // The sessions' thread list doesn't change after thread died until the uclamp
    // min update is triggered.
    int deadThreadInd = numOfThreads / 2;
    auto deadThreadID = threadIds[deadThreadInd];
    closeThread(deadThreadInd);
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks,
              session1Threads);
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess2->mSessionId].linkedTasks,
              session2Threads);
    ASSERT_EQ(sessManager->mSessionTaskMap.mTasks[deadThreadID].size(), 2);

    // Trigger an update of uclamp min.
    auto tNow = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
    WorkDuration wDur(tNow, 1100000);
    sess1->reportActualWorkDuration(std::vector<WorkDuration>{wDur});
    ASSERT_EQ(sessManager->mSessionTaskMap.mTasks[deadThreadID].size(), 1);
    sess2->reportActualWorkDuration(std::vector<WorkDuration>{wDur});
    ASSERT_EQ(sessManager->mSessionTaskMap.mTasks.count(deadThreadID), 0);
    std::erase(session1Threads, deadThreadID);
    std::erase(session2Threads, deadThreadID);
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks,
              session1Threads);
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess2->mSessionId].linkedTasks,
              session2Threads);

    // Close all the threads in session 1.
    for (int i = 0; i <= numOfThreads / 2; i++) {
        closeThread(i);
    }
    tNow = std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::high_resolution_clock::now().time_since_epoch())
                   .count();
    sess1->reportActualWorkDuration(std::vector<WorkDuration>{wDur});
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions.size());  // Session still alive
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks.size(), 0);
}

TEST_F(PowerHintSessionTest, setThreads) {
    auto sessManager = sess1->mPSManager;
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions.size());

    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks,
              session1Threads);

    std::vector<int32_t> newSess1Threads;
    for (auto tid : threadIds) {
        newSess1Threads.emplace_back(tid.second);
    }
    sess1->setThreads(newSess1Threads);
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks,
              newSess1Threads);

    sess1->close();
    sess2->close();
}

TEST_F(PowerHintSessionTest, pauseResumeSession) {
    auto sessManager = sess1->mPSManager;
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions.size());
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks.size());

    sess1->pause();
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions.size());
    ASSERT_EQ(0, sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks.size());

    sess1->resume();
    ASSERT_EQ(sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks,
              session1Threads);
    ASSERT_EQ(session1Threads, sess1->mDescriptor->thread_ids);
    ASSERT_EQ(SessionTag::OTHER, sess1->mDescriptor->tag);

    sess1->close();
    sess2->close();
}

TEST_F(PowerHintSessionTest, checkPauseResumeTag) {
    auto sessManager = sess1->mPSManager;
    bool isActive;

    // Check we actually start with two PIDs.
    ASSERT_EQ(2, sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks.size());
    pid_t threadOnePid = sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks[0];
    pid_t threadTwoPid = sessManager->mSessionTaskMap.mSessions[sess1->mSessionId].linkedTasks[1];

    // Start the powerhint session and check the powerhint tags are on.
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(ReadThreadADPFTag(threadOnePid, &isActive));
    ASSERT_TRUE(isActive);
    ASSERT_TRUE(ReadThreadADPFTag(threadTwoPid, &isActive));
    ASSERT_TRUE(isActive);

    // Pause session 1, the powerhint session tag for thread 1 should be off.
    // But, thread two should still have tag on since it is part of session 2.
    sess1->pause();
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(ReadThreadADPFTag(threadOnePid, &isActive));
    ASSERT_TRUE(!isActive);
    ASSERT_TRUE(ReadThreadADPFTag(threadTwoPid, &isActive));
    ASSERT_TRUE(isActive);

    // Resume the powerhint session and check the powerhint sessions are allowed.
    sess1->resume();
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(ReadThreadADPFTag(threadOnePid, &isActive));
    ASSERT_TRUE(isActive);
    ASSERT_TRUE(ReadThreadADPFTag(threadTwoPid, &isActive));
    ASSERT_TRUE(isActive);

    sess1->close();
    sess2->close();
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
