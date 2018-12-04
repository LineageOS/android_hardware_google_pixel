/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "libpixelpowerstats"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <pixelpowerstats/GenericStateResidencyDataProvider.h>
#include <pixelpowerstats/PowerStatsUtils.h>
#include <fstream>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

PowerEntityConfig::PowerEntityConfig(std::vector<StateResidencyConfig> stateResidencyConfigs)
    : PowerEntityConfig("", stateResidencyConfigs) {}

PowerEntityConfig::PowerEntityConfig(std::string header,
                                     std::vector<StateResidencyConfig> stateResidencyConfigs)
    : mHeader(header) {
    for (uint32_t i = 0; i < stateResidencyConfigs.size(); ++i) {
        mStateResidencyConfigs.push_back(std::make_pair(i, stateResidencyConfigs[i]));
    }
}

static bool parseState(PowerEntityStateResidencyData &data, const StateResidencyConfig &config,
                       std::istream &inFile) {
    size_t numFieldsRead = 0;
    const size_t numFields =
        config.entryCountSupported + config.totalTimeSupported + config.lastEntrySupported;
    std::string line;
    while (numFieldsRead < numFields && std::getline(inFile, line)) {
        uint64_t stat = 0;
        // Attempt to extract data from the current line
        if (config.entryCountSupported && utils::extractStat(line, config.entryCountPrefix, stat)) {
            data.totalStateEntryCount =
                config.entryCountTransform ? config.entryCountTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.totalTimeSupported &&
                   utils::extractStat(line, config.totalTimePrefix, stat)) {
            data.totalTimeInStateMs =
                config.totalTimeTransform ? config.totalTimeTransform(stat) : stat;
            ++numFieldsRead;
        } else if (config.lastEntrySupported &&
                   utils::extractStat(line, config.lastEntryPrefix, stat)) {
            data.lastEntryTimestampMs =
                config.lastEntryTransform ? config.lastEntryTransform(stat) : stat;
            ++numFieldsRead;
        }
    }

    // End of file was reached and not all state data was parsed. Something
    // went wrong
    if (numFieldsRead != numFields) {
        LOG(ERROR) << __func__ << ": failed to parse stats for:" << config.name;
        return false;
    }

    return true;
}

template <class T>
static auto findNext(const std::vector<T> &collection, std::istream &inFile,
                     const std::function<bool(T const &, std::string)> &pred) {
    // handling the case when there is no header to look for
    if (pred(collection.front(), "")) {
        return collection.cbegin();
    }

    std::string line;
    while (std::getline(inFile, line)) {
        for (auto it = collection.cbegin(); it != collection.cend(); ++it) {
            if (pred(*it, line)) {
                return it;
            }
        }
    }

    return collection.cend();
}

static bool getStateData(
    PowerEntityStateResidencyResult &result,
    std::vector<std::pair<uint32_t, StateResidencyConfig>> stateResidencyConfigs,
    std::istream &inFile) {
    size_t numStatesRead = 0;
    size_t numStates = stateResidencyConfigs.size();
    auto nextState = stateResidencyConfigs.cbegin();
    auto endState = stateResidencyConfigs.cend();
    auto pred = [](auto a, std::string b) { return (base::Trim(b) == a.second.header); };

    result.stateResidencyData.resize(numStates);

    // Search for state headers until we have found them all or can't find anymore
    while ((numStatesRead < numStates) &&
           (nextState = findNext<decltype(stateResidencyConfigs)::value_type>(
                stateResidencyConfigs, inFile, pred)) != endState) {
        // Found a matching state header. Parse the contents
        PowerEntityStateResidencyData data = {.powerEntityStateId = nextState->first};
        if (parseState(data, nextState->second, inFile)) {
            result.stateResidencyData[numStatesRead] = data;
            ++numStatesRead;
        } else {
            break;
        }
    }

    // There was a problem parsing and we failed to get data for all of the states
    if (numStatesRead != numStates) {
        return false;
    }

    return true;
}

bool GenericStateResidencyDataProvider::getResults(
    std::map<uint32_t, PowerEntityStateResidencyResult> &results) {
    std::ifstream inFile(mPath, std::ifstream::in);
    if (!inFile.is_open()) {
        PLOG(ERROR) << __func__ << ":Failed to open file " << mPath;
        return false;
    }

    size_t numEntitiesRead = 0;
    size_t numEntities = mPowerEntityConfigs.size();
    auto nextConfig = mPowerEntityConfigs.cbegin();
    auto endConfig = mPowerEntityConfigs.cend();
    auto pred = [](auto a, std::string b) { return (base::Trim(b) == a.second.mHeader); };

    // Search for entity headers until we have found them all or can't find anymore
    while ((numEntitiesRead < numEntities) &&
           (nextConfig = findNext<decltype(mPowerEntityConfigs)::value_type>(
                mPowerEntityConfigs, inFile, pred)) != endConfig) {
        // Found a matching header. Retrieve its state data
        PowerEntityStateResidencyResult result = {.powerEntityId = nextConfig->first};
        if (getStateData(result, nextConfig->second.mStateResidencyConfigs, inFile)) {
            results.insert(std::make_pair(nextConfig->first, result));
            ++numEntitiesRead;
        } else {
            break;
        }
    }

    // There was a problem gathering state residency data for one or more entities
    if (numEntitiesRead != numEntities) {
        LOG(ERROR) << __func__ << ":Failed to get results for " << mPath;
        return false;
    }

    return true;
}

void GenericStateResidencyDataProvider::addEntity(uint32_t id, PowerEntityConfig config) {
    mPowerEntityConfigs.push_back(std::make_pair(id, std::move(config)));
}

std::vector<PowerEntityStateSpace> GenericStateResidencyDataProvider::getStateSpaces() {
    std::vector<PowerEntityStateSpace> stateSpaces;
    stateSpaces.reserve(mPowerEntityConfigs.size());
    for (auto config : mPowerEntityConfigs) {
        PowerEntityStateSpace s = {.powerEntityId = config.first};
        s.states.resize(config.second.mStateResidencyConfigs.size());

        for (uint32_t i = 0; i < config.second.mStateResidencyConfigs.size(); ++i) {
            s.states[i] = {
                .powerEntityStateId = config.second.mStateResidencyConfigs[i].first,
                .powerEntityStateName = config.second.mStateResidencyConfigs[i].second.name};
        }
        stateSpaces.push_back(s);
    }
    return stateSpaces;
}

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android