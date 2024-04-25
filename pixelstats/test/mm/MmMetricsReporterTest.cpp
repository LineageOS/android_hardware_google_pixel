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
#include <pixelstats/MmMetricsReporter.h>

#include "MmMetricsGoldenAtomFieldTypes.h"
#include "MmMetricsGoldenResults.h"
#include "MockMmMetricsReporter.h"
#include "VendorAtomIntValueUtil.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using mm_metrics_atom_field_test_golden_results::PixelMmMetricsPerDay_field_types;
using mm_metrics_atom_field_test_golden_results::PixelMmMetricsPerHour_field_types;
using mm_metrics_reporter_test_golden_result::PixelMmMetricsPerDay_golden;
using mm_metrics_reporter_test_golden_result::PixelMmMetricsPerHour_golden;

const char *data_base_path = "/data/local/tmp/test/pixelstats_mm_test/data";

TEST(MmMetricsReporterTest, MmMetricsPerHourAtomFieldOffsetTypeTest) {
    int i = -1;
    uint64_t golden_result;
    int field_type;
    std::vector<VendorAtomValue> values;
    MockMmMetricsReporter mreport;
    const std::string data_path0 = std::string(data_base_path) + "/test_data_0";
    const std::string data_path1 = std::string(data_base_path) + "/test_data_1";

    // Assert failure means the test case itself has a bug.
    ASSERT_EQ(ARRAY_SIZE(PixelMmMetricsPerHour_golden),
              ARRAY_SIZE(PixelMmMetricsPerHour_field_types));

    /**
     * In test code we use setBasePath() to read different data sets for simulating
     * different timing reads of a sysfs node.
     */

    /**
     * aggregatePixelMmMetricsPer5Min() aggregates PSI into max, min, and avg.
     * For the regular code, it will be called 12 times per hour (i.e. once per 5min)
     * For test code we do 6 times: enough for testing.
     * e.g. here average  = (3 x data0 + 3 x data1) / 6 == avg of data 0, 1
     * The following sequence simulate regular code obtaining sysfs nodes into
     * values[] array (i.e. atom), ready to be sent to the server
     */
    mreport.setBasePath(data_path0);
    mreport.aggregatePixelMmMetricsPer5Min();
    mreport.aggregatePixelMmMetricsPer5Min();
    mreport.aggregatePixelMmMetricsPer5Min();
    mreport.setBasePath(data_path1);
    mreport.aggregatePixelMmMetricsPer5Min();
    mreport.aggregatePixelMmMetricsPer5Min();
    mreport.aggregatePixelMmMetricsPer5Min();

    // other fields from data set #0
    mreport.setBasePath(data_path0);
    values = mreport.genPixelMmMetricsPerHour();

    // Validate the atom: compare with golden results
    EXPECT_EQ(values.size(), ARRAY_SIZE(PixelMmMetricsPerHour_field_types));
    for (auto const &v : values) {
        i++;
        golden_result = PixelMmMetricsPerHour_golden[i];
        field_type = PixelMmMetricsPerHour_field_types[i];
        if (golden_result == -1)
            continue;  // no need to test (e.g. deprecated field)

        EXPECT_EQ(static_cast<int>(v.getTag()), field_type) << "type mismatch at offset " << i;
        EXPECT_EQ(getVendorAtomIntValue(v), golden_result) << "value mismatch at offset " << i;
    }
}

TEST(MmMetricsReporterTest, MmMetricsPerDayAtomFieldOffsetTypeTest) {
    int i = -1;
    uint64_t golden_result;
    int field_type;
    std::vector<VendorAtomValue> values;
    MockMmMetricsReporter mreport;
    const std::string data_path0 = std::string(data_base_path) + "/test_data_0";
    const std::string data_path1 = std::string(data_base_path) + "/test_data_1";

    // Assert failure means the test case itself has a bug.
    ASSERT_EQ(ARRAY_SIZE(PixelMmMetricsPerDay_golden),
              ARRAY_SIZE(PixelMmMetricsPerDay_field_types));

    mreport.setBasePath(data_path0);
    values = mreport.genPixelMmMetricsPerDay();

    // PixelMmMetricsPerDay calculatd the difference of consecutive readings.
    // So, it will not send values[] at the 1st read. (i.e. empty for the 1st read)
    EXPECT_EQ(values.size(), 0);
    values.clear();

    mreport.setBasePath(data_path1);
    values = mreport.genPixelMmMetricsPerDay();

    // Per Day metrics (diffs) should be calculated, values[] will be non-empty now.
    // number of data should be the same as the number of fields in the atom.
    EXPECT_EQ(values.size(), ARRAY_SIZE(PixelMmMetricsPerDay_field_types));
    for (auto const &v : values) {
        i++;
        EXPECT_LT(i, ARRAY_SIZE(PixelMmMetricsPerDay_field_types));

        golden_result = PixelMmMetricsPerDay_golden[i];
        field_type = PixelMmMetricsPerDay_field_types[i];
        if (golden_result == -1)
            continue;  // no need to test (e.g. deprecated field)

        EXPECT_EQ(static_cast<int>(v.getTag()), field_type) << "type mismatch at offset " << i;
        EXPECT_EQ(getVendorAtomIntValue(v), golden_result) << "value mismatch at offset " << i;
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
