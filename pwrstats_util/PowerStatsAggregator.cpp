/*
 * Copyright (C) 2019 The Android Open Source Project
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
#define LOG_TAG "pwrstats_util"

#include "PowerStatsAggregator.h"

#include <android-base/logging.h>
#include <android-base/parsedouble.h>

#include <iostream>
#include <memory>
#include <unordered_map>

void PowerStatsAggregator::addDataProvider(std::unique_ptr<IPowerStatsDataProvider> p) {
    mDataProviders.emplace_back(std::move(p));
}

int PowerStatsAggregator::getData(std::unordered_map<std::string, uint64_t>* data) const {
    data->clear();
    for (auto&& provider : mDataProviders) {
        int ret = provider->get(data);
        if (ret != 0) {
            data->clear();
            return ret;
        }
    }
    return 0;
}
