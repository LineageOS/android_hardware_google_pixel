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
#pragma once

#include "HardwareBase.h"
#include "Vibrator.h"

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

class HwApi : public Vibrator::HwApi, private HwApiBase {
  public:
    HwApi() {
        open("calibration/f0_stored", &mF0);
        open("default/f0_offset", &mF0Offset);
        open("calibration/redc_stored", &mRedc);
        open("calibration/q_stored", &mQ);
        open("default/vibe_state", &mVibeState);
        open("default/num_waves", &mEffectCount);
        open("default/owt_free_space", &mOwtFreeSpace);
    }

    bool setF0(std::string value) override { return set(value, &mF0); }
    bool setF0Offset(uint32_t value) override { return set(value, &mF0Offset); }
    bool setRedc(std::string value) override { return set(value, &mRedc); }
    bool setQ(std::string value) override { return set(value, &mQ); }
    bool setActivate(bool value) override { return set(value, &mActivate); }
    bool setDuration(uint32_t value) override { return set(value, &mDuration); }
    bool getEffectCount(uint32_t *value) override { return getStr(value, &mEffectCount); }
    bool getEffectDuration(uint32_t *value) override { return getStr(value, &mEffectDuration); }
    bool setEffectIndex(uint32_t value) override { return set(value, &mEffectIndex); }
    bool setEffectQueue(std::string value) override { return set(value, &mEffectQueue); }
    bool hasEffectScale() override { return has(mEffectScale); }
    bool setEffectScale(uint32_t value) override { return set(value, &mEffectScale); }
    bool setGlobalScale(uint32_t value) override { return set(value, &mGlobalScale); }
    bool setState(bool value) override { return set(value, &mState); }
    bool hasAspEnable() override { return false; }
    bool getAspEnable(bool *value) override { return getStr(value, &mAspEnable); }
    bool setAspEnable(bool value) override { return set(value, &mAspEnable); }
    bool setGpioFallIndex(uint32_t value) override { return set(value, &mGpioFallIndex); }
    bool setGpioFallScale(uint32_t value) override { return set(value, &mGpioFallScale); }
    bool setGpioRiseIndex(uint32_t value) override { return set(value, &mGpioRiseIndex); }
    bool setGpioRiseScale(uint32_t value) override { return set(value, &mGpioRiseScale); }
    bool pollVibeState(std::string value, int32_t timeoutMs) override {
        return pollStr(value, &mVibeState, timeoutMs);
    }
    bool setClabEnable(bool value) override { return set(value, &mClabEnable); }
    bool getAvailablePwleSegments(uint32_t *value) override {
        return getStr(value, &mAvailablePwleSegments);
    }
    bool setPwle(std::string value) override { return set(value, &mPwle); }
    bool hasOwtFreeSpace() override { return has(mOwtFreeSpace); }
    bool getOwtFreeSpace(uint32_t *value) override { return get(value, &mOwtFreeSpace); }
    void debug(int fd) override { HwApiBase::debug(fd); }

  private:
    std::ofstream mF0;
    std::ofstream mF0Offset;
    std::ofstream mRedc;
    std::ofstream mQ;
    std::ofstream mActivate;
    std::ofstream mDuration;
    std::ifstream mEffectCount;
    std::ifstream mEffectDuration;
    std::ofstream mEffectIndex;
    std::ofstream mEffectQueue;
    std::ofstream mEffectScale;
    std::ofstream mGlobalScale;
    std::ofstream mState;
    std::fstream mAspEnable;
    std::ofstream mGpioFallIndex;
    std::ofstream mGpioFallScale;
    std::ofstream mGpioRiseIndex;
    std::ofstream mGpioRiseScale;
    std::ifstream mVibeState;
    std::ofstream mClabEnable;
    std::ifstream mAvailablePwleSegments;
    std::ofstream mPwle;
    std::ifstream mOwtFreeSpace;
};

class HwCal : public Vibrator::HwCal, private HwCalBase {
  private:
    static constexpr char VERSION[] = "version";
    static constexpr char F0_CONFIG[] = "f0_measured";
    static constexpr char REDC_CONFIG[] = "redc_measured";
    static constexpr char Q_CONFIG[] = "q_measured";
    static constexpr char Q_INDEX[] = "q_index";
    static constexpr char VOLTAGES_CONFIG[] = "v_levels";
    static constexpr char TICK_VOLTAGES_CONFIG[] = "v_tick";
    static constexpr char CLICK_VOLTAGES_CONFIG[] = "v_click";
    static constexpr char LONG_VOLTAGES_CONFIG[] = "v_long";

    static constexpr uint32_t Q_FLOAT_TO_FIXED = 1 << 16;
    static constexpr float Q_INDEX_TO_FLOAT = 1.5f;
    static constexpr uint32_t Q_INDEX_TO_FIXED = Q_INDEX_TO_FLOAT * Q_FLOAT_TO_FIXED;
    static constexpr uint32_t Q_INDEX_OFFSET = 2.0f * Q_FLOAT_TO_FIXED;

    static constexpr uint32_t VERSION_DEFAULT = 1;
    static constexpr int32_t DEFAULT_FREQUENCY_SHIFT = 0;
    static constexpr uint32_t Q_DEFAULT = 15.5 * Q_FLOAT_TO_FIXED;
    static constexpr std::array<uint32_t, 6> V_LEVELS_DEFAULT = {60, 70, 80, 90, 100, 76};
    static constexpr std::array<uint32_t, 2> V_TICK_DEFAULT = {1, 100};
    static constexpr std::array<uint32_t, 2> V_CTICK_DEFAULT = {1, 100};
    static constexpr std::array<uint32_t, 2> V_LONG_DEFAULT = {1, 100};

  public:
    HwCal() {}

    bool getVersion(uint32_t *value) override {
        if (getPersist(VERSION, value)) {
            return true;
        }
        *value = VERSION_DEFAULT;
        return true;
    }
    bool getLongFrequencyShift(int32_t *value) override {
        return getProperty("long.frequency.shift", value, DEFAULT_FREQUENCY_SHIFT);
    }
    bool getF0(std::string *value) override { return getPersist(F0_CONFIG, value); }
    bool getRedc(std::string *value) override { return getPersist(REDC_CONFIG, value); }
    bool getQ(std::string *value) override { return getPersist(Q_CONFIG, value); }
    bool getVolLevels(std::array<uint32_t, 6> *value) override {
        if (getPersist(VOLTAGES_CONFIG, value)) {
            return true;
        }
        *value = V_LEVELS_DEFAULT;
        return true;
    }
    bool getTickVolLevels(std::array<uint32_t, 2> *value) override {
        if (getPersist(TICK_VOLTAGES_CONFIG, value)) {
            return true;
        }
        *value = V_TICK_DEFAULT;
        return true;
    }
    bool getClickVolLevels(std::array<uint32_t, 2> *value) override {
        if (getPersist(CLICK_VOLTAGES_CONFIG, value)) {
            return true;
        }
        *value = V_CTICK_DEFAULT;
        return true;
    }
    bool getLongVolLevels(std::array<uint32_t, 2> *value) override {
        if (getPersist(LONG_VOLTAGES_CONFIG, value)) {
            return true;
        }
        *value = V_LONG_DEFAULT;
        return true;
    }
    void debug(int fd) override { HwCalBase::debug(fd); }
};

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
