/*
 * Copyright 2024 The Android Open Source Project
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

#include "TestHelper.h"

namespace aidl::google::hardware::power::impl::pixel {

::android::perfmgr::AdpfConfig makeMockConfig() {
    return ::android::perfmgr::AdpfConfig("REFRESH_60FPS", /* Name */
                                          true,            /* PID_On */
                                          2.0,             /* PID_Po */
                                          1.0,             /* PID_Pu */
                                          0.0,             /* PID_I */
                                          200,             /* PID_I_Init */
                                          512,             /* PID_I_High */
                                          -30,             /* PID_I_Low */
                                          500.0,           /* PID_Do */
                                          0.0,             /* PID_Du */
                                          true,            /* UclampMin_On */
                                          162,             /* UclampMin_Init */
                                          480,             /* UclampMin_High */
                                          2,               /* UclampMin_Low */
                                          1,               /* SamplingWindow_P */
                                          0,               /* SamplingWindow_I */
                                          1,               /* SamplingWindow_D */
                                          166666660,       /* ReportingRateLimitNs */
                                          1.0,             /* TargetTimeFactor */
                                          15.0,            /* StaleTimeFactor */
                                          true,            /* GpuBoost */
                                          25000,           /* GpuCapacityBoostMax */
                                          0,               /* GpuCapacityLoadUpHeadroom */
                                          true,            /* HeuristicBoost_On */
                                          8,               /* HBoostOnMissedCycles */
                                          4.0,             /* HBoostOffMaxAvgRatio */
                                          5,               /* HBoostOffMissedCycles */
                                          0.5,             /* HBoostPidPuFactor */
                                          722,             /* HBoostUclampMin */
                                          1.2,             /* JankCheckTimeFactor */
                                          25,              /* LowFrameRateThreshold */
                                          300,             /* MaxRecordsNum */
                                          480,             /* UclampMin_LoadUp */
                                          480,             /* UclampMin_LoadReset */
                                          500,             /* UclampMax_EfficientBase */
                                          200);            /* UclampMax_EfficientOffset */
}
}  // namespace aidl::google::hardware::power::impl::pixel
