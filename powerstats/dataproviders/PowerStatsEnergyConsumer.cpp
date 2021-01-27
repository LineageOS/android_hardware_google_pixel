/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "android.hardware.power.stats-service.pixel"

#include <dataproviders/PowerStatsEnergyConsumer.h>

#include <android-base/logging.h>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace stats {

PowerStatsEnergyConsumer::PowerStatsEnergyConsumer(std::shared_ptr<PowerStats> p,
                                                   EnergyConsumerType type, std::string name)
    : kType(type), kName(name), mPowerStats(p) {}

sp<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createMeterConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::set<std::string> channelNames) {
    return createMeterAndEntityConsumer(p, type, name, channelNames, "", {});
}

sp<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createEntityConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::string powerEntityName, std::map<std::string, int32_t> stateCoeffs) {
    return createMeterAndEntityConsumer(p, type, name, {}, powerEntityName, stateCoeffs);
}

sp<PowerStatsEnergyConsumer> PowerStatsEnergyConsumer::createMeterAndEntityConsumer(
        std::shared_ptr<PowerStats> p, EnergyConsumerType type, std::string name,
        std::set<std::string> channelNames, std::string powerEntityName,
        std::map<std::string, int32_t> stateCoeffs) {
    sp<PowerStatsEnergyConsumer> ret = new PowerStatsEnergyConsumer(p, type, name);
    if (ret->addEnergyMeter(channelNames) && ret->addPowerEntity(powerEntityName, stateCoeffs)) {
        return ret;
    }

    LOG(ERROR) << "Failed to create PowerStatsEnergyConsumer for " << name;
    return nullptr;
}

bool PowerStatsEnergyConsumer::addEnergyMeter(std::set<std::string> channelNames) {
    if (channelNames.empty()) {
        return true;
    }

    std::vector<Channel> channels;
    mPowerStats->getEnergyMeterInfo(&channels);

    for (const auto &c : channels) {
        if (channelNames.count(c.name)) {
            mChannelIds.push_back(c.id);
        }
    }

    return (mChannelIds.size() == channelNames.size());
}

bool PowerStatsEnergyConsumer::addPowerEntity(std::string powerEntityName,
                                              std::map<std::string, int32_t> stateCoeffs) {
    if (powerEntityName.empty() || stateCoeffs.empty()) {
        return true;
    }

    std::vector<PowerEntity> powerEntities;
    mPowerStats->getPowerEntityInfo(&powerEntities);

    for (const auto &p : powerEntities) {
        if (powerEntityName == p.name) {
            mPowerEntityId = p.id;
            for (const auto &s : p.states) {
                if (stateCoeffs.count(s.name)) {
                    mCoefficients.emplace(s.id, stateCoeffs.at(s.name));
                }
            }
            break;
        }
    }

    return (mCoefficients.size() == stateCoeffs.size());
}

std::optional<EnergyConsumerResult> PowerStatsEnergyConsumer::getEnergyConsumed() {
    int64_t totalEnergyUWs = 0;

    if (!mChannelIds.empty()) {
        std::vector<EnergyMeasurement> measurements;
        if (mPowerStats->readEnergyMeters(mChannelIds, &measurements).isOk()) {
            for (const auto &m : measurements) {
                totalEnergyUWs += m.energyUWs;
            }
        } else {
            LOG(ERROR) << "Failed to read energy meter";
            return {};
        }
    }

    if (!mCoefficients.empty()) {
        std::vector<StateResidencyResult> results;
        if (mPowerStats->getStateResidency({mPowerEntityId}, &results).isOk()) {
            for (const auto &s : results[0].stateResidencyData) {
                if (mCoefficients.count(s.id)) {
                    totalEnergyUWs += mCoefficients.at(s.id) * s.totalTimeInStateMs;
                }
            }
        } else {
            LOG(ERROR) << "Failed to get state residency";
            return {};
        }
    }

    return EnergyConsumerResult{.timestampMs = 0,  // What should this timestamp be?
                                .energyUWs = totalEnergyUWs};
}

}  // namespace stats
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
