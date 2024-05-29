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

#define LOG_TAG "pixelstats: BatteryFGReporter"

#include <log/log.h>
#include <time.h>
#include <utils/Timers.h>
#include <cinttypes>
#include <cmath>

#include <android-base/file.h>
#include <pixelstats/BatteryFGReporter.h>
#include <pixelstats/StatsHelper.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::VendorAtom;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::base::ReadFileToString;
using android::hardware::google::pixel::PixelAtoms::BatteryEEPROM;
using android::hardware::google::pixel::PixelAtoms::FuelGaugeAbnormality;


BatteryFGReporter::BatteryFGReporter() {}

int64_t BatteryFGReporter::getTimeSecs() {
    return nanoseconds_to_seconds(systemTime(SYSTEM_TIME_BOOTTIME));
}

void BatteryFGReporter::setAtomFieldValue(std::vector<VendorAtomValue> *values, int offset,
                                          int content) {
    std::vector<VendorAtomValue> &val = *values;
    if (offset - kVendorAtomOffset < val.size())
        val[offset - kVendorAtomOffset].set<VendorAtomValue::intValue>(content);
}

void BatteryFGReporter::reportAbnormalEvent(const std::shared_ptr<IStats> &stats_client,
                                            struct BatteryFGAbnormalData data) {
    // Load values array
    std::vector<VendorAtomValue> values(35);
    uint32_t duration = 0;

    /* save time when trigger, calculate duration when clear */
    if (data.state == 1 && ab_trigger_time_[data.event] == 0) {
        ab_trigger_time_[data.event] = getTimeSecs();
    } else {
        duration = getTimeSecs() - ab_trigger_time_[data.event];
        ab_trigger_time_[data.event] = 0;
    }

    ALOGD("reportEvent: event=%d,state=%d,cycles=%04X,vcel=%04X,avgv=%04X,curr=%04X,avgc=%04X,"
          "timerh=%04X,temp=%04X,repcap=%04X,mixcap=%04X,fcrep=%04X,fcnom=%04X,qresd=%04X,"
          "avcap=%04X,vfremcap=%04X,repsoc=%04X,vfsoc=%04X,msoc=%04X,vfocv=%04X,dpacc=%04X,"
          "dqacc=%04X,qh=%04X,qh0=%04X,vfsoc0=%04X,qrtable20=%04X,qrtable30=%04X,status=%04X,"
          "fstat=%04X,rcomp0=%04X,tempco=%04X,duration=%u",
          data.event, data.state, data.cycles, data.vcel, data.avgv, data.curr, data.avgc,
          data.timerh, data.temp, data.repcap, data.mixcap, data.fcrep, data.fcnom, data.qresd,
          data.avcap, data.vfremcap, data.repsoc, data.vfsoc, data.msoc, data.vfocv, data.dpacc,
          data.dqacc, data.qh, data.qh0, data.vfsoc0, data.qrtable20, data.qrtable30, data.status,
          data.fstat, data.rcomp0, data.tempco, duration);

    setAtomFieldValue(&values, FuelGaugeAbnormality::kEventFieldNumber, data.event);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kEventStateFieldNumber, data.state);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kDurationSecsFieldNumber, duration);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress1FieldNumber, data.cycles);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData1FieldNumber, data.vcel);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress2FieldNumber, data.avgv);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData2FieldNumber, data.curr);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress3FieldNumber, data.avgc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData3FieldNumber, data.timerh);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress4FieldNumber, data.temp);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData4FieldNumber, data.repcap);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress5FieldNumber, data.mixcap);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData5FieldNumber, data.fcrep);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress6FieldNumber, data.fcnom);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData6FieldNumber, data.qresd);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress7FieldNumber, data.avcap);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData7FieldNumber, data.vfremcap);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress8FieldNumber, data.repsoc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData8FieldNumber, data.vfsoc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress9FieldNumber, data.msoc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData9FieldNumber, data.vfocv);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress10FieldNumber, data.dpacc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData10FieldNumber, data.dqacc);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress11FieldNumber, data.qh);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData11FieldNumber, data.qh0);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress12FieldNumber, data.vfsoc0);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData12FieldNumber, data.qrtable20);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress13FieldNumber, data.qrtable30);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData13FieldNumber, data.status);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress14FieldNumber, data.fstat);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData14FieldNumber, data.rcomp0);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress15FieldNumber, data.tempco);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData15FieldNumber, 0);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterAddress16FieldNumber, 0);
    setAtomFieldValue(&values, FuelGaugeAbnormality::kFgRegisterData16FieldNumber, 0);

    VendorAtom event = {.reverseDomainName = "",
                        .atomId = PixelAtoms::Atom::kFuelGaugeAbnormality,
                        .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report FuelGaugeAbnormality to Stats service");
}

void BatteryFGReporter::reportEvent(const std::shared_ptr<IStats> &stats_client,
                                    const struct BatteryFGLearningParam &params) {
    // upload atom
    const std::vector<int> eeprom_history_fields = {
            BatteryEEPROM::kCycleCntFieldNumber,  BatteryEEPROM::kFullCapFieldNumber,
            BatteryEEPROM::kEsrFieldNumber,       BatteryEEPROM::kRslowFieldNumber,
            BatteryEEPROM::kSohFieldNumber,       BatteryEEPROM::kBattTempFieldNumber,
            BatteryEEPROM::kCutoffSocFieldNumber, BatteryEEPROM::kCcSocFieldNumber,
            BatteryEEPROM::kSysSocFieldNumber,    BatteryEEPROM::kMsocFieldNumber,
            BatteryEEPROM::kBattSocFieldNumber,   BatteryEEPROM::kReserveFieldNumber,
            BatteryEEPROM::kMaxTempFieldNumber,   BatteryEEPROM::kMinTempFieldNumber,
            BatteryEEPROM::kMaxVbattFieldNumber,  BatteryEEPROM::kMinVbattFieldNumber,
            BatteryEEPROM::kMaxIbattFieldNumber,  BatteryEEPROM::kMinIbattFieldNumber,
            BatteryEEPROM::kChecksumFieldNumber,  BatteryEEPROM::kTempcoFieldNumber,
            BatteryEEPROM::kRcomp0FieldNumber,    BatteryEEPROM::kTimerHFieldNumber,
            BatteryEEPROM::kFullRepFieldNumber};

    switch(params.type) {
      case EvtFWUpdate:
        ALOGD("reportEvent: firmware update try: %u, success: %u, fail: %u",
              params.fcnom, params.dpacc, params.dqacc);
              break;
      default:
        ALOGD("unknown event type %04x", params.type);
        break;
    }

    std::vector<VendorAtomValue> values(eeprom_history_fields.size());
    VendorAtomValue val;

    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kCycleCntFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.fcnom);
    values[BatteryEEPROM::kFullCapFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.dpacc);
    values[BatteryEEPROM::kEsrFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.dqacc);
    values[BatteryEEPROM::kRslowFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kSohFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kBattTempFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kCutoffSocFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kCcSocFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kSysSocFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kMsocFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kBattSocFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kReserveFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kMaxTempFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kMinTempFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.fcrep);
    values[BatteryEEPROM::kMaxVbattFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.msoc);
    values[BatteryEEPROM::kMinVbattFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.vfsoc);
    values[BatteryEEPROM::kMaxIbattFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.fstat);
    values[BatteryEEPROM::kMinIbattFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>((uint16_t)params.type);
    values[BatteryEEPROM::kChecksumFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.tempco);
    values[BatteryEEPROM::kTempcoFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.rcomp0);
    values[BatteryEEPROM::kRcomp0FieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(0);
    values[BatteryEEPROM::kTimerHFieldNumber - kVendorAtomOffset] = val;
    val.set<VendorAtomValue::intValue>(params.repsoc);
    values[BatteryEEPROM::kFullRepFieldNumber - kVendorAtomOffset] = val;

    VendorAtom event = {.reverseDomainName = "",
                        .atomId = PixelAtoms::Atom::kBatteryEeprom,
                        .values = std::move(values)};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report BatteryEEPROM to Stats service");
}

void BatteryFGReporter::checkAndReportFwUpdate(const std::shared_ptr<IStats> &stats_client,
                                               const std::string &path) {
    struct BatteryFGLearningParam params;
    std::string file_contents;
    int16_t num;

    if (path.empty())
        return;

    if (!ReadFileToString(path, &file_contents)) {
        ALOGE("Unable to read FirmwareUpdate path: %s - %s", path.c_str(), strerror(errno));
        return;
    }

    /* FU: Firmware Update */
    params.type = EvtFWUpdate;
    num = sscanf(file_contents.c_str(), "%" SCNu16 " %" SCNu16 " %" SCNu16,
                 &params.fcnom, &params.dpacc, &params.dqacc);
    if (num != kNumFwUpdateFields) {
        ALOGE("Couldn't process FirmwareUpdate history path. num=%d\n", num);
        return;
    }

    /* No event to report */
    if (params.fcnom == 0 )
        return;

    /* Reporting data only when can clear */
    if (::android::base::WriteStringToFile("0", path.c_str()))
        reportEvent(stats_client, params);
    else
        ALOGE("Couldn't clear %s - %s", path.c_str(), strerror(errno));
}

void BatteryFGReporter::checkAndReportFGAbnormality(const std::shared_ptr<IStats> &stats_client,
                                                    const std::vector<std::string> &paths) {
    std::string path;
    struct timespec boot_time;
    std::vector<std::vector<uint16_t>> events;

    if (paths.empty())
        return;

    for (int i = 0; i < paths.size(); i++) {
        if (fileExists(paths[i])) {
            path = paths[i];
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &boot_time);
    readLogbuffer(path, kNumAbnormalEventFields, EvtFGAbnormalEvent, FormatNoAddr, last_ab_check_, events);
    for (int seq = 0; seq < events.size(); seq++) {
        if (events[seq].size() == kNumAbnormalEventFields) {
            struct BatteryFGAbnormalData data;
            uint16_t *pdata = (uint16_t *)&data;
            for (int i = 0; i < kNumAbnormalEventFields; i++)
                *pdata++ = events[seq][i];
            reportAbnormalEvent(stats_client, data);
        } else {
            ALOGE("Not support %zu fields for FG abnormal event", events[seq].size());
        }
    }

    last_ab_check_ = (unsigned int)boot_time.tv_sec;
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
