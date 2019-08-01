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
#ifndef RAILENERGYDATAPROVIDER_H
#define RAILENERGYDATAPROVIDER_H

#include "PowerStatsAggregator.h"

/**
 * Rail Energy data provider:
 * Provides data via Power Stats HAL 1.0
 * data is in units of microwatt-seconds (uWs)
 **/
class RailEnergyDataProvider : public IPowerStatsDataProvider {
  public:
    RailEnergyDataProvider() = default;

    int get(std::unordered_map<std::string, uint64_t>* data) override;
};

#endif  // RAILENERGYDATAPROVIDER_H
