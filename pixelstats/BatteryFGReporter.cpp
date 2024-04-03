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
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {

using aidl::android::frameworks::stats::VendorAtom;
using aidl::android::frameworks::stats::VendorAtomValue;
using android::base::ReadFileToString;
using android::hardware::google::pixel::PixelAtoms::BatteryEEPROM;


BatteryFGReporter::BatteryFGReporter() {}

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
                                                    const std::string &path) {
    std::string file_contents;
    unsigned int abnl;

    if (path.empty())
        return;

    if (!ReadFileToString(path, &file_contents)) {
        ALOGE("Unable to read FG abnormality path: %s - %s", path.c_str(), strerror(errno));
        return;
    }

    if (sscanf(file_contents.c_str(), "%x", &abnl) != 1) {
        ALOGE("Unable to parse %s from file %s to int", file_contents.c_str(), path.c_str());
        return;
    }

    if (abnl == last_abnl_)
        return;

    ALOGD("reportEvent: FG abnormality %x", abnl);

    VendorAtomValue value;
    std::vector<VendorAtomValue> values(1);
    value.set<VendorAtomValue::intValue>(abnl);
    values[PixelAtoms::FuelGaugeAbnormality::kEventFieldNumber - kVendorAtomOffset] = value;

    VendorAtom atom = {.reverseDomainName = "",
                       .atomId = PixelAtoms::Atom::kFuelGaugeAbnormality,
                       .values = {std::move(values)}};
    const ndk::ScopedAStatus ret = stats_client->reportVendorAtom(atom);

    if (!ret.isOk())
        ALOGE("Unable to report FG abnormality.");

    last_abnl_ = abnl;
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
