/* * Copyright (C) 2019 The Android Open Source Project *
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

#include "benchmark/benchmark.h"

#include <android-base/file.h>
#include <android-base/properties.h>

#include "Hardware.h"
#include "Vibrator.h"

using ::android::sp;
using ::android::hardware::hidl_enum_range;

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_2 {
namespace implementation {

using ::android::base::SetProperty;
using ::android::hardware::vibrator::V1_0::EffectStrength;
using ::android::hardware::vibrator::V1_0::Status;

class VibratorBench : public benchmark::Fixture {
  private:
    static constexpr char PROPERTY_PREFIX[] = "test.vibrator.hal.";

  public:
    void SetUp(::benchmark::State &state) override {
        setenv("AUTOCAL_FILEPATH", "/dev/null", true);
        setenv("OL_LRA_PERIOD_FILEPATH", "/dev/null", true);
        setenv("ACTIVATE_PATH", "/dev/null", true);
        setenv("DURATION_PATH", "/dev/null", true);
        setenv("STATE_PATH", "/dev/null", true);
        setenv("RTP_INPUT_PATH", "/dev/null", true);
        setenv("MODE_PATH", "/dev/null", true);
        setenv("SEQUENCER_PATH", "/dev/null", true);
        setenv("SCALE_PATH", "/dev/null", true);
        setenv("CTRL_LOOP_PATH", "/dev/null", true);
        setenv("LP_TRIGGER_PATH", "/dev/null", true);
        setenv("LRA_WAVE_SHAPE_PATH", "/dev/null", true);
        setenv("OD_CLAMP_PATH", "/dev/null", true);

        setenv("PROPERTY_PREFIX", PROPERTY_PREFIX, true);

        SetProperty(std::string() + PROPERTY_PREFIX + "config.dynamic", getDynamicConfig(state));

        mVibrator = new Vibrator(HwApi::Create(), std::make_unique<HwCal>());
    }

    static void DefaultConfig(benchmark::internal::Benchmark *b) {
        b->Unit(benchmark::kMicrosecond);
    }

    static void DefaultArgs(benchmark::internal::Benchmark *b) {
        b->ArgNames({"DynamicConfig"});
        b->Args({false});
        b->Args({true});
    }

  protected:
    std::string getDynamicConfig(const ::benchmark::State &state) const {
        return std::to_string(state.range(0));
    }

    auto getOtherArg(const ::benchmark::State &state, std::size_t index) const {
        return state.range(index + 1);
    }

  protected:
    sp<IVibrator> mVibrator;
};

#define BENCHMARK_WRAPPER(fixt, test, code)                           \
    BENCHMARK_DEFINE_F(fixt, test)                                    \
    /* NOLINTNEXTLINE */                                              \
    (benchmark::State & state){code} BENCHMARK_REGISTER_F(fixt, test) \
            ->Apply(fixt::DefaultConfig)                              \
            ->Apply(fixt::DefaultArgs)

BENCHMARK_WRAPPER(VibratorBench, on, {
    uint32_t duration = std::rand() ?: 1;

    for (auto _ : state) {
        mVibrator->on(duration);
    }
});

BENCHMARK_WRAPPER(VibratorBench, off, {
    for (auto _ : state) {
        mVibrator->off();
    }
});

BENCHMARK_WRAPPER(VibratorBench, supportsAmplitudeControl, {
    for (auto _ : state) {
        mVibrator->supportsAmplitudeControl();
    }
});

BENCHMARK_WRAPPER(VibratorBench, setAmplitude, {
    uint8_t amplitude = std::rand() ?: 1;

    for (auto _ : state) {
        mVibrator->setAmplitude(amplitude);
    }
});

class VibratorEffectsBench : public VibratorBench {
  public:
    static void DefaultArgs(benchmark::internal::Benchmark *b) {
        b->ArgNames({"DynamicConfig", "Effect", "Strength"});
        for (const auto &dynamic : {false, true}) {
            for (const auto &effect : hidl_enum_range<Effect>()) {
                for (const auto &strength : hidl_enum_range<EffectStrength>()) {
                    b->Args({dynamic, static_cast<long>(effect), static_cast<long>(strength)});
                }
            }
        }
    }

  protected:
    auto getEffect(const ::benchmark::State &state) const {
        return static_cast<Effect>(getOtherArg(state, 0));
    }

    auto getStrength(const ::benchmark::State &state) const {
        return static_cast<EffectStrength>(getOtherArg(state, 1));
    }
};

BENCHMARK_WRAPPER(VibratorEffectsBench, perform_1_2,
                  {
                      Effect effect = getEffect(state);
                      EffectStrength strength = getStrength(state);
                      bool supported = true;

                      mVibrator->perform_1_2(effect, strength,
                                             [&](Status status, uint32_t /*lengthMs*/) {
                                                 if (status == Status::UNSUPPORTED_OPERATION) {
                                                     supported = false;
                                                 }
                                             });

                      if (!supported) {
                          return;
                      }

                      for (auto _ : state) {
                          mVibrator->perform_1_2(effect, strength,
                                                 [](Status /*status*/, uint32_t /*lengthMs*/) {});
                      }
                  });

}  // namespace implementation
}  // namespace V1_2
}  // namespace vibrator
}  // namespace hardware
}  // namespace android

BENCHMARK_MAIN();
