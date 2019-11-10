/*
 * Copyright (C) 2017 The Android Open Source Project
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


#include "Vibrator.h"

#include <hardware/hardware.h>
#include <hardware/vibrator.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

static constexpr uint32_t BASE_CONTINUOUS_EFFECT_OFFSET = 32768;

static constexpr uint32_t WAVEFORM_SIMPLE_EFFECT_INDEX = 2;

static constexpr uint32_t WAVEFORM_TEXTURE_TICK_EFFECT_LEVEL = 0;

static constexpr uint32_t WAVEFORM_TICK_EFFECT_LEVEL = 1;

static constexpr uint32_t WAVEFORM_CLICK_EFFECT_LEVEL = 2;

static constexpr uint32_t WAVEFORM_HEAVY_CLICK_EFFECT_LEVEL = 3;

static constexpr uint32_t WAVEFORM_DOUBLE_CLICK_SILENCE_MS = 100;

static constexpr uint32_t WAVEFORM_LONG_VIBRATION_EFFECT_INDEX = 0;
static constexpr uint32_t WAVEFORM_LONG_VIBRATION_THRESHOLD_MS = 50;
static constexpr uint32_t WAVEFORM_SHORT_VIBRATION_EFFECT_INDEX = 3 + BASE_CONTINUOUS_EFFECT_OFFSET;

static constexpr uint32_t WAVEFORM_TRIGGER_QUEUE_INDEX = 65534;

static constexpr uint32_t VOLTAGE_GLOBAL_SCALE_LEVEL = 5;
static constexpr uint8_t VOLTAGE_SCALE_MAX = 100;

static constexpr int8_t MAX_COLD_START_LATENCY_MS = 6;  // I2C Transaction + DSP Return-From-Standby
static constexpr int8_t MAX_PAUSE_TIMING_ERROR_MS = 1;  // ALERT Irq Handling
static constexpr uint32_t MAX_TIME_MS = UINT32_MAX;

static constexpr float AMP_ATTENUATE_STEP_SIZE = 0.125f;
static constexpr float EFFECT_FREQUENCY_KHZ = 48.0f;

static constexpr auto ASYNC_COMPLETION_TIMEOUT = std::chrono::milliseconds(100);

static uint8_t amplitudeToScale(uint8_t amplitude, uint8_t maximum) {
    return std::round((-20 * std::log10(amplitude / static_cast<float>(maximum))) /
                      (AMP_ATTENUATE_STEP_SIZE));
}

Vibrator::Vibrator(std::unique_ptr<HwApi> hwapi, std::unique_ptr<HwCal> hwcal)
    : mHwApi(std::move(hwapi)), mHwCal(std::move(hwcal)), mAsyncHandle(std::async([] {})) {
    uint32_t caldata;
    uint32_t effectDuration;

    if (!mHwApi->setState(true)) {
        ALOGE("Failed to set state (%d): %s", errno, strerror(errno));
    }

    if (mHwCal->getF0(&caldata)) {
        mHwApi->setF0(caldata);
    }
    if (mHwCal->getRedc(&caldata)) {
        mHwApi->setRedc(caldata);
    }
    if (mHwCal->getQ(&caldata)) {
        mHwApi->setQ(caldata);
    }
    mHwCal->getVolLevels(&mVolLevels);

    mHwApi->setEffectIndex(WAVEFORM_SIMPLE_EFFECT_INDEX);
    mHwApi->getEffectDuration(&effectDuration);

    mSimpleEffectDuration = std::ceil(effectDuration / EFFECT_FREQUENCY_KHZ);

    const uint32_t scaleFall =
            amplitudeToScale(mVolLevels[WAVEFORM_CLICK_EFFECT_LEVEL], VOLTAGE_SCALE_MAX);
    const uint32_t scaleRise =
            amplitudeToScale(mVolLevels[WAVEFORM_HEAVY_CLICK_EFFECT_LEVEL], VOLTAGE_SCALE_MAX);

    mHwApi->setGpioFallIndex(WAVEFORM_SIMPLE_EFFECT_INDEX);
    mHwApi->setGpioFallScale(scaleFall);
    mHwApi->setGpioRiseIndex(WAVEFORM_SIMPLE_EFFECT_INDEX);
    mHwApi->setGpioRiseScale(scaleRise);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::getCapabilities");
    int32_t ret = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK;
    if (mHwApi->hasEffectScale()) {
        ret |= IVibrator::CAP_AMPLITUDE_CONTROL;
    }
    if (mHwApi->hasAspEnable()) {
        ret |= IVibrator::CAP_EXTERNAL_CONTROL;
    }
    *_aidl_return = ret;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    ATRACE_NAME("Vibrator::off");
    setGlobalAmplitude(false);
    if (!mHwApi->setActivate(0)) {
        ALOGE("Failed to turn vibrator off (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback> &callback) {
    ATRACE_NAME("Vibrator::on");
    const uint32_t index = timeoutMs < WAVEFORM_LONG_VIBRATION_THRESHOLD_MS
                                   ? WAVEFORM_SHORT_VIBRATION_EFFECT_INDEX
                                   : WAVEFORM_LONG_VIBRATION_EFFECT_INDEX;
    if (MAX_COLD_START_LATENCY_MS <= UINT32_MAX - timeoutMs) {
        timeoutMs += MAX_COLD_START_LATENCY_MS;
    }
    setGlobalAmplitude(true);
    return on(timeoutMs, index, callback);
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback> &callback,
                                     int32_t *_aidl_return) {
    ATRACE_NAME("Vibrator::perform");
    return performEffect(effect, strength, callback, _aidl_return);
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect> *_aidl_return) {
    *_aidl_return = {Effect::TEXTURE_TICK, Effect::TICK, Effect::CLICK, Effect::HEAVY_CLICK,
                     Effect::DOUBLE_CLICK};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(int32_t amplitude) {
    ATRACE_NAME("Vibrator::setAmplitude");
    if (amplitude <= 0 || amplitude > 255) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!isUnderExternalControl()) {
        return setEffectAmplitude(amplitude, UINT8_MAX);
    } else {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
    ATRACE_NAME("Vibrator::setExternalControl");
    setGlobalAmplitude(enabled);

    if (!mHwApi->setAspEnable(enabled)) {
        ALOGE("Failed to set external control (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(uint32_t timeoutMs, uint32_t effectIndex,
                                const std::shared_ptr<IVibratorCallback> &callback) {
    if (mAsyncHandle.wait_for(ASYNC_COMPLETION_TIMEOUT) != std::future_status::ready) {
        ALOGE("Previous vibration pending.");
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    mHwApi->setEffectIndex(effectIndex);
    mHwApi->setDuration(timeoutMs);
    mHwApi->setActivate(1);

    mAsyncHandle = std::async(&Vibrator::waitForComplete, this, callback);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setEffectAmplitude(uint8_t amplitude, uint8_t maximum) {
    int32_t scale = amplitudeToScale(amplitude, maximum);

    if (!mHwApi->setEffectScale(scale)) {
        ALOGE("Failed to set effect amplitude (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setGlobalAmplitude(bool set) {
    uint8_t amplitude = set ? mVolLevels[VOLTAGE_GLOBAL_SCALE_LEVEL] : VOLTAGE_SCALE_MAX;
    int32_t scale = amplitudeToScale(amplitude, VOLTAGE_SCALE_MAX);

    if (!mHwApi->setGlobalScale(scale)) {
        ALOGE("Failed to set global amplitude (%d): %s", errno, strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

bool Vibrator::isUnderExternalControl() {
    bool isAspEnabled;
    mHwApi->getAspEnable(&isAspEnabled);
    return isAspEnabled;
}

binder_status_t Vibrator::dump(int fd, const char **args, uint32_t numArgs) {
    if (fd < 0) {
        ALOGE("Called debug() with invalid fd.");
        return STATUS_OK;
    }

    (void)args;
    (void)numArgs;

    dprintf(fd, "HIDL:\n");

    dprintf(fd, "  Voltage Levels:");
    for (auto v : mVolLevels) {
        dprintf(fd, " %" PRIu32, v);
    }
    dprintf(fd, "\n");

    dprintf(fd, "  Effect Duration: %" PRIu32 "\n", mSimpleEffectDuration);

    dprintf(fd, "\n");

    mHwApi->debug(fd);

    dprintf(fd, "\n");

    mHwCal->debug(fd);

    fsync(fd);
    return STATUS_OK;
}

ndk::ScopedAStatus Vibrator::getSimpleDetails(Effect effect, EffectStrength strength,
                                              uint32_t *outTimeMs, uint32_t *outVolLevel) {
    uint32_t timeMs;
    uint32_t volLevel;
    uint32_t volIndex;
    int8_t volOffset;

    switch (strength) {
        case EffectStrength::LIGHT:
            volOffset = -1;
            break;
        case EffectStrength::MEDIUM:
            volOffset = 0;
            break;
        case EffectStrength::STRONG:
            volOffset = 1;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    switch (effect) {
        case Effect::TEXTURE_TICK:
            volIndex = WAVEFORM_TEXTURE_TICK_EFFECT_LEVEL;
            volOffset = 0;
            break;
        case Effect::TICK:
            volIndex = WAVEFORM_TICK_EFFECT_LEVEL;
            volOffset = 0;
            break;
        case Effect::CLICK:
            volIndex = WAVEFORM_CLICK_EFFECT_LEVEL;
            break;
        case Effect::HEAVY_CLICK:
            volIndex = WAVEFORM_HEAVY_CLICK_EFFECT_LEVEL;
            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    volLevel = mVolLevels[volIndex + volOffset];
    timeMs = mSimpleEffectDuration + MAX_COLD_START_LATENCY_MS;

    *outTimeMs = timeMs;
    *outVolLevel = volLevel;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompoundDetails(Effect effect, EffectStrength strength,
                                                uint32_t *outTimeMs, uint32_t * /*outVolLevel*/,
                                                std::string *outEffectQueue) {
    ndk::ScopedAStatus status;
    uint32_t timeMs;
    std::ostringstream effectBuilder;
    uint32_t thisTimeMs;
    uint32_t thisVolLevel;

    switch (effect) {
        case Effect::DOUBLE_CLICK:
            timeMs = 0;

            status = getSimpleDetails(Effect::CLICK, strength, &thisTimeMs, &thisVolLevel);
            if (!status.isOk()) {
                return status;
            }
            effectBuilder << WAVEFORM_SIMPLE_EFFECT_INDEX << "." << thisVolLevel;
            timeMs += thisTimeMs;

            effectBuilder << ",";

            effectBuilder << WAVEFORM_DOUBLE_CLICK_SILENCE_MS;
            timeMs += WAVEFORM_DOUBLE_CLICK_SILENCE_MS + MAX_PAUSE_TIMING_ERROR_MS;

            effectBuilder << ",";

            status = getSimpleDetails(Effect::HEAVY_CLICK, strength, &thisTimeMs, &thisVolLevel);
            if (!status.isOk()) {
                return status;
            }
            effectBuilder << WAVEFORM_SIMPLE_EFFECT_INDEX << "." << thisVolLevel;
            timeMs += thisTimeMs;

            break;
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *outTimeMs = timeMs;
    *outEffectQueue = effectBuilder.str();

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setEffectQueue(const std::string &effectQueue) {
    if (!mHwApi->setEffectQueue(effectQueue)) {
        ALOGE("Failed to write \"%s\" to effect queue (%d): %s", effectQueue.c_str(), errno,
              strerror(errno));
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::performEffect(Effect effect, EffectStrength strength,
                                           const std::shared_ptr<IVibratorCallback> &callback,
                                           int32_t *outTimeMs) {
    ndk::ScopedAStatus status;
    uint32_t timeMs = 0;
    uint32_t effectIndex;
    uint32_t volLevel;
    std::string effectQueue;

    switch (effect) {
        case Effect::TEXTURE_TICK:
            // fall-through
        case Effect::TICK:
            // fall-through
        case Effect::CLICK:
            // fall-through
        case Effect::HEAVY_CLICK:
            status = getSimpleDetails(effect, strength, &timeMs, &volLevel);
            break;
        case Effect::DOUBLE_CLICK:
            status = getCompoundDetails(effect, strength, &timeMs, &volLevel, &effectQueue);
            break;
        default:
            status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
            break;
    }
    if (!status.isOk()) {
        goto exit;
    }

    if (!effectQueue.empty()) {
        status = setEffectQueue(effectQueue);
        if (!status.isOk()) {
            goto exit;
        }
        setEffectAmplitude(VOLTAGE_SCALE_MAX, VOLTAGE_SCALE_MAX);
        effectIndex = WAVEFORM_TRIGGER_QUEUE_INDEX;
    } else {
        setEffectAmplitude(volLevel, VOLTAGE_SCALE_MAX);
        effectIndex = WAVEFORM_SIMPLE_EFFECT_INDEX;
    }

    status = on(MAX_TIME_MS, effectIndex, callback);

exit:

    *outTimeMs = timeMs;
    return status;
}

void Vibrator::waitForComplete(std::shared_ptr<IVibratorCallback> &&callback) {
    mHwApi->pollVibeState(false);
    mHwApi->setActivate(false);

    if (callback) {
        auto ret = callback->onComplete();
        if (!ret.isOk()) {
            ALOGE("Failed completion callback: %d", ret.getExceptionCode());
        }
    }
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
