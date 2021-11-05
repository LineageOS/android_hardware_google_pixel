/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "adaptivecpu/CpuLoadReader.h"
#include "mocks.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

std::ostream &operator<<(std::ostream &stream, const CpuLoad &cpuLoad) {
    return stream << "CpuLoad(" << cpuLoad.cpuId << ", " << cpuLoad.idleTimeFraction << ")";
}

TEST(CpuLoadReaderTest, getRecentCpuLoads) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, readFileStream("/proc/stat"))
            .Times(2)
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 100 0 0 50 0 0 0 0 0 0\n";
                ss << "cpu2 200 0 0 50 0 0 0 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            })
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 200 0 0 150 0 0 0 0 0 0\n";
                ss << "cpu2 500 0 0 150 0 0 0 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            });

    CpuLoadReader reader(std::move(filesystem));
    reader.init();

    std::vector<CpuLoad> actualCpuLoads;
    ASSERT_TRUE(reader.getRecentCpuLoads(&actualCpuLoads));
    std::vector<CpuLoad> expectedCpuLoads(
            {{.cpuId = 1, .idleTimeFraction = 0.5}, {.cpuId = 2, .idleTimeFraction = 0.25}});
    ASSERT_EQ(actualCpuLoads, expectedCpuLoads);
}

TEST(CpuLoadReaderTest, getRecentCpuLoads_failsWithMissingValues) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, readFileStream("/proc/stat"))
            .Times(2)
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 100 0 0 50 0 0 0\n";
                ss << "cpu2 200 0 0 50 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            })
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 200 0 0 150 0 0 0\n";
                ss << "cpu2 500 0 0 150 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            });

    CpuLoadReader reader(std::move(filesystem));
    reader.init();
    std::vector<CpuLoad> actualCpuLoads;
    ASSERT_FALSE(reader.getRecentCpuLoads(&actualCpuLoads));
}

TEST(CpuLoadReaderTest, getRecentCpuLoads_failsWithEmptyFile) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, readFileStream("/proc/stat"))
            .Times(2)
            .WillOnce([]() { return std::make_unique<std::istringstream>(""); })
            .WillOnce([]() { return std::make_unique<std::istringstream>(""); });

    CpuLoadReader reader(std::move(filesystem));
    reader.init();
    std::vector<CpuLoad> actualCpuLoads;
    ASSERT_FALSE(reader.getRecentCpuLoads(&actualCpuLoads));
}

TEST(CpuLoadReaderTest, getRecentCpuLoads_failsWithDifferentCpus) {
    std::unique_ptr<MockFilesystem> filesystem = std::make_unique<MockFilesystem>();
    EXPECT_CALL(*filesystem, readFileStream("/proc/stat"))
            .Times(2)
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 100 0 0 50 0 0 0 0 0 0\n";
                ss << "cpu2 200 0 0 50 0 0 0 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            })
            .WillOnce([]() {
                std::stringstream ss;
                ss << "bad line\n";
                ss << "cpu1 200 0 0 150 0 0 0 0 0 0\n";
                ss << "cpu3 500 0 0 150 0 0 0 0 0 0\n";
                return std::make_unique<std::istringstream>(ss.str());
            });

    CpuLoadReader reader(std::move(filesystem));
    reader.init();
    std::vector<CpuLoad> actualCpuLoads;
    ASSERT_FALSE(reader.getRecentCpuLoads(&actualCpuLoads));
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
