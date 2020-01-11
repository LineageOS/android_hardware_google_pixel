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

#include <aidl/android/hardware/vibrator/BnVibratorCallback.h>
#include <android-base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>

#include "Vibrator.h"
#include "mocks.h"
#include "types.h"
#include "utils.h"

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::DoDefault;
using ::testing::Exactly;
using ::testing::Expectation;
using ::testing::ExpectationSet;
using ::testing::Ge;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Range;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::Test;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

// Forward Declarations

static EffectQueue Queue(const QueueEffect &effect);
static EffectQueue Queue(const QueueDelay &delay);
template <typename T, typename U, typename... Args>
static EffectQueue Queue(const T &first, const U &second, Args... rest);

// Constants With Arbitrary Values

static constexpr std::array<EffectLevel, 6> V_LEVELS{40, 50, 60, 70, 80, 90};
static constexpr EffectDuration EFFECT_DURATION{15};

// Constants With Prescribed Values

static constexpr EffectIndex EFFECT_INDEX{2};
static constexpr EffectIndex QUEUE_INDEX{65534};

static const EffectScale ON_GLOBAL_SCALE{levelToScale(V_LEVELS[5])};
static const EffectIndex ON_EFFECT_INDEX{0};

static const std::map<EffectTuple, EffectScale> EFFECT_SCALE{
        {{Effect::CLICK, EffectStrength::LIGHT}, levelToScale(V_LEVELS[1])},
        {{Effect::CLICK, EffectStrength::MEDIUM}, levelToScale(V_LEVELS[2])},
        {{Effect::CLICK, EffectStrength::STRONG}, levelToScale(V_LEVELS[3])},
        {{Effect::TICK, EffectStrength::LIGHT}, levelToScale(V_LEVELS[1])},
        {{Effect::TICK, EffectStrength::MEDIUM}, levelToScale(V_LEVELS[1])},
        {{Effect::TICK, EffectStrength::STRONG}, levelToScale(V_LEVELS[1])},
        {{Effect::HEAVY_CLICK, EffectStrength::LIGHT}, levelToScale(V_LEVELS[2])},
        {{Effect::HEAVY_CLICK, EffectStrength::MEDIUM}, levelToScale(V_LEVELS[3])},
        {{Effect::HEAVY_CLICK, EffectStrength::STRONG}, levelToScale(V_LEVELS[4])},
        {{Effect::TEXTURE_TICK, EffectStrength::LIGHT}, levelToScale(V_LEVELS[0])},
        {{Effect::TEXTURE_TICK, EffectStrength::MEDIUM}, levelToScale(V_LEVELS[0])},
        {{Effect::TEXTURE_TICK, EffectStrength::STRONG}, levelToScale(V_LEVELS[0])},
};

static const std::map<EffectTuple, EffectQueue> EFFECT_QUEUE{
        {{Effect::DOUBLE_CLICK, EffectStrength::LIGHT},
         Queue(QueueEffect{EFFECT_INDEX, V_LEVELS[1]}, 100,
               QueueEffect{EFFECT_INDEX, V_LEVELS[2]})},
        {{Effect::DOUBLE_CLICK, EffectStrength::MEDIUM},
         Queue(QueueEffect{EFFECT_INDEX, V_LEVELS[2]}, 100,
               QueueEffect{EFFECT_INDEX, V_LEVELS[3]})},
        {{Effect::DOUBLE_CLICK, EffectStrength::STRONG},
         Queue(QueueEffect{EFFECT_INDEX, V_LEVELS[3]}, 100,
               QueueEffect{EFFECT_INDEX, V_LEVELS[4]})},
};

EffectQueue Queue(const QueueEffect &effect) {
    auto index = std::get<0>(effect);
    auto level = std::get<1>(effect);
    auto string = std::to_string(index) + "." + std::to_string(level);
    auto duration = EFFECT_DURATION;
    return {string, duration};
}

EffectQueue Queue(const QueueDelay &delay) {
    auto string = std::to_string(delay);
    return {string, delay};
}

template <typename T, typename U, typename... Args>
EffectQueue Queue(const T &first, const U &second, Args... rest) {
    auto head = Queue(first);
    auto tail = Queue(second, rest...);
    auto string = std::get<0>(head) + "," + std::get<0>(tail);
    auto duration = std::get<1>(head) + std::get<1>(tail);
    return {string, duration};
}

class VibratorTest : public Test {
  public:
    void SetUp() override {
        std::unique_ptr<MockApi> mockapi;
        std::unique_ptr<MockCal> mockcal;

        createMock(&mockapi, &mockcal);
        createVibrator(std::move(mockapi), std::move(mockcal));
    }

    void TearDown() override { deleteVibrator(); }

  protected:
    void createMock(std::unique_ptr<MockApi> *mockapi, std::unique_ptr<MockCal> *mockcal) {
        *mockapi = std::make_unique<MockApi>();
        *mockcal = std::make_unique<MockCal>();

        mMockApi = mockapi->get();
        mMockCal = mockcal->get();

        ON_CALL(*mMockApi, destructor()).WillByDefault(Assign(&mMockApi, nullptr));

        ON_CALL(*mMockApi, getEffectDuration(_))
                .WillByDefault(DoAll(SetArgPointee<0>(msToCycles(EFFECT_DURATION)), Return(true)));

        ON_CALL(*mMockCal, destructor()).WillByDefault(Assign(&mMockCal, nullptr));

        ON_CALL(*mMockCal, getVolLevels(_))
                .WillByDefault(DoAll(SetArgPointee<0>(V_LEVELS), Return(true)));

        relaxMock(false);
    }

    void createVibrator(std::unique_ptr<MockApi> mockapi, std::unique_ptr<MockCal> mockcal,
                        bool relaxed = true) {
        if (relaxed) {
            relaxMock(true);
        }
        mVibrator = ndk::SharedRefBase::make<Vibrator>(std::move(mockapi), std::move(mockcal));
        if (relaxed) {
            relaxMock(false);
        }
    }

    void deleteVibrator(bool relaxed = true) {
        if (relaxed) {
            relaxMock(true);
        }
        mVibrator.reset();
    }

  private:
    void relaxMock(bool relax) {
        auto times = relax ? AnyNumber() : Exactly(0);

        Mock::VerifyAndClearExpectations(mMockApi);
        Mock::VerifyAndClearExpectations(mMockCal);

        EXPECT_CALL(*mMockApi, destructor()).Times(times);
        EXPECT_CALL(*mMockApi, setF0(_)).Times(times);
        EXPECT_CALL(*mMockApi, setRedc(_)).Times(times);
        EXPECT_CALL(*mMockApi, setQ(_)).Times(times);
        EXPECT_CALL(*mMockApi, setActivate(_)).Times(times);
        EXPECT_CALL(*mMockApi, setDuration(_)).Times(times);
        EXPECT_CALL(*mMockApi, getEffectDuration(_)).Times(times);
        EXPECT_CALL(*mMockApi, setEffectIndex(_)).Times(times);
        EXPECT_CALL(*mMockApi, setEffectQueue(_)).Times(times);
        EXPECT_CALL(*mMockApi, hasEffectScale()).Times(times);
        EXPECT_CALL(*mMockApi, setEffectScale(_)).Times(times);
        EXPECT_CALL(*mMockApi, setGlobalScale(_)).Times(times);
        EXPECT_CALL(*mMockApi, setState(_)).Times(times);
        EXPECT_CALL(*mMockApi, hasAspEnable()).Times(times);
        EXPECT_CALL(*mMockApi, getAspEnable(_)).Times(times);
        EXPECT_CALL(*mMockApi, setAspEnable(_)).Times(times);
        EXPECT_CALL(*mMockApi, setGpioFallIndex(_)).Times(times);
        EXPECT_CALL(*mMockApi, setGpioFallScale(_)).Times(times);
        EXPECT_CALL(*mMockApi, setGpioRiseIndex(_)).Times(times);
        EXPECT_CALL(*mMockApi, setGpioRiseScale(_)).Times(times);
        EXPECT_CALL(*mMockApi, debug(_)).Times(times);

        EXPECT_CALL(*mMockCal, destructor()).Times(times);
        EXPECT_CALL(*mMockCal, getF0(_)).Times(times);
        EXPECT_CALL(*mMockCal, getRedc(_)).Times(times);
        EXPECT_CALL(*mMockCal, getQ(_)).Times(times);
        EXPECT_CALL(*mMockCal, getVolLevels(_)).Times(times);
        EXPECT_CALL(*mMockCal, debug(_)).Times(times);
    }

  protected:
    MockApi *mMockApi;
    MockCal *mMockCal;
    std::shared_ptr<IVibrator> mVibrator;
};

TEST_F(VibratorTest, Constructor) {
    std::unique_ptr<MockApi> mockapi;
    std::unique_ptr<MockCal> mockcal;
    uint32_t f0Val = std::rand();
    uint32_t redcVal = std::rand();
    uint32_t qVal = std::rand();
    Expectation volGet;
    Sequence f0Seq, redcSeq, qSeq, volSeq, durSeq;

    EXPECT_CALL(*mMockApi, destructor()).WillOnce(DoDefault());
    EXPECT_CALL(*mMockCal, destructor()).WillOnce(DoDefault());

    deleteVibrator(false);

    createMock(&mockapi, &mockcal);

    EXPECT_CALL(*mMockCal, getF0(_))
            .InSequence(f0Seq)
            .WillOnce(DoAll(SetArgPointee<0>(f0Val), Return(true)));
    EXPECT_CALL(*mMockApi, setF0(f0Val)).InSequence(f0Seq).WillOnce(Return(true));

    EXPECT_CALL(*mMockCal, getRedc(_))
            .InSequence(redcSeq)
            .WillOnce(DoAll(SetArgPointee<0>(redcVal), Return(true)));
    EXPECT_CALL(*mMockApi, setRedc(redcVal)).InSequence(redcSeq).WillOnce(Return(true));

    EXPECT_CALL(*mMockCal, getQ(_))
            .InSequence(qSeq)
            .WillOnce(DoAll(SetArgPointee<0>(qVal), Return(true)));
    EXPECT_CALL(*mMockApi, setQ(qVal)).InSequence(qSeq).WillOnce(Return(true));

    volGet = EXPECT_CALL(*mMockCal, getVolLevels(_)).WillOnce(DoDefault());

    EXPECT_CALL(*mMockApi, setState(true)).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setEffectIndex(EFFECT_INDEX)).InSequence(durSeq).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, getEffectDuration(_)).InSequence(durSeq).WillOnce(DoDefault());

    createVibrator(std::move(mockapi), std::move(mockcal), false);
}

TEST_F(VibratorTest, on) {
    Sequence s1, s2, s3;
    uint16_t duration = std::rand() + 1;

    EXPECT_CALL(*mMockApi, setGlobalScale(ON_GLOBAL_SCALE)).InSequence(s1).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setEffectIndex(ON_EFFECT_INDEX)).InSequence(s2).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setDuration(Ge(duration))).InSequence(s3).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setActivate(true)).InSequence(s1, s2, s3).WillOnce(Return(true));

    EXPECT_TRUE(mVibrator->on(duration, nullptr).isOk());
}

TEST_F(VibratorTest, off) {
    EXPECT_CALL(*mMockApi, setActivate(false)).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setGlobalScale(0)).WillOnce(Return(true));

    EXPECT_TRUE(mVibrator->off().isOk());
}

TEST_F(VibratorTest, supportsAmplitudeControl_supported) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(true));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_GT(capabilities & IVibrator::CAP_AMPLITUDE_CONTROL, 0);
}

TEST_F(VibratorTest, supportsAmplitudeControl_unsupported1) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(false));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(true));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_EQ(capabilities & IVibrator::CAP_AMPLITUDE_CONTROL, 0);
}

TEST_F(VibratorTest, supportsAmplitudeControl_unsupported2) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(false));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(false));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_EQ(capabilities & IVibrator::CAP_AMPLITUDE_CONTROL, 0);
}

TEST_F(VibratorTest, supportsExternalAmplitudeControl_unsupported) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(true));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_EQ(capabilities & IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL, 0);
}

TEST_F(VibratorTest, setAmplitude_supported) {
    Sequence s;
    EffectAmplitude amplitude = static_cast<float>(std::rand()) / RAND_MAX ?: 1.0f;

    EXPECT_CALL(*mMockApi, getAspEnable(_))
            .InSequence(s)
            .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)));
    EXPECT_CALL(*mMockApi, setEffectScale(amplitudeToScale(amplitude)))
            .InSequence(s)
            .WillOnce(Return(true));

    EXPECT_TRUE(mVibrator->setAmplitude(amplitude).isOk());
}

TEST_F(VibratorTest, setAmplitude_unsupported) {
    EXPECT_CALL(*mMockApi, getAspEnable(_)).WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));

    EXPECT_EQ(EX_UNSUPPORTED_OPERATION, mVibrator->setAmplitude(1).getExceptionCode());
}

TEST_F(VibratorTest, supportsExternalControl_supported) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(true));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_GT(capabilities & IVibrator::CAP_EXTERNAL_CONTROL, 0);
}

TEST_F(VibratorTest, supportsExternalControl_unsupported) {
    EXPECT_CALL(*mMockApi, hasEffectScale()).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, hasAspEnable()).WillOnce(Return(false));

    int32_t capabilities;
    EXPECT_TRUE(mVibrator->getCapabilities(&capabilities).isOk());
    EXPECT_EQ(capabilities & IVibrator::CAP_EXTERNAL_CONTROL, 0);
}

TEST_F(VibratorTest, setExternalControl_enable) {
    Sequence s;

    EXPECT_CALL(*mMockApi, setGlobalScale(ON_GLOBAL_SCALE)).InSequence(s).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setAspEnable(true)).InSequence(s).WillOnce(Return(true));

    EXPECT_TRUE(mVibrator->setExternalControl(true).isOk());
}

TEST_F(VibratorTest, setExternalControl_disable) {
    EXPECT_CALL(*mMockApi, setAspEnable(false)).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setGlobalScale(0)).WillOnce(Return(true));

    EXPECT_TRUE(mVibrator->setExternalControl(false).isOk());
}

class EffectsTest : public VibratorTest, public WithParamInterface<EffectTuple> {
  public:
    static auto PrintParam(const TestParamInfo<ParamType> &info) {
        auto param = info.param;
        auto effect = std::get<0>(param);
        auto strength = std::get<1>(param);
        return toString(effect) + "_" + toString(strength);
    }
};

TEST_P(EffectsTest, perform) {
    auto param = GetParam();
    auto effect = std::get<0>(param);
    auto strength = std::get<1>(param);
    auto scale = EFFECT_SCALE.find(param);
    auto queue = EFFECT_QUEUE.find(param);
    EffectDuration duration;
    auto callback = ndk::SharedRefBase::make<MockVibratorCallback>();
    std::promise<void> promise;
    std::future<void> future{promise.get_future()};
    auto complete = [&promise] {
        promise.set_value();
        return ndk::ScopedAStatus::ok();
    };

    ExpectationSet eSetup;
    Expectation eActivate, ePoll;

    if (scale != EFFECT_SCALE.end()) {
        duration = EFFECT_DURATION;

        eSetup += EXPECT_CALL(*mMockApi, setEffectIndex(EFFECT_INDEX)).WillOnce(Return(true));
        eSetup += EXPECT_CALL(*mMockApi, setEffectScale(scale->second)).WillOnce(Return(true));
    } else if (queue != EFFECT_QUEUE.end()) {
        duration = std::get<1>(queue->second);

        eSetup += EXPECT_CALL(*mMockApi, setEffectIndex(QUEUE_INDEX)).WillOnce(Return(true));
        eSetup += EXPECT_CALL(*mMockApi, setEffectQueue(std::get<0>(queue->second)))
                          .WillOnce(Return(true));
        eSetup += EXPECT_CALL(*mMockApi, setEffectScale(0)).WillOnce(Return(true));
    } else {
        duration = 0;
    }

    if (duration) {
        eSetup += EXPECT_CALL(*mMockApi, setDuration(Ge(duration))).WillOnce(Return(true));
        eActivate = EXPECT_CALL(*mMockApi, setActivate(true)).After(eSetup).WillOnce(Return(true));
        ePoll = EXPECT_CALL(*mMockApi, pollVibeState(false))
                        .After(eActivate)
                        .WillOnce(Return(true));
        EXPECT_CALL(*mMockApi, setActivate(false)).After(ePoll).WillOnce(Return(true));
        EXPECT_CALL(*callback, onComplete()).After(ePoll).WillOnce(complete);
    }

    int32_t lengthMs;
    ndk::ScopedAStatus status = mVibrator->perform(effect, strength, callback, &lengthMs);
    if (status.isOk()) {
        EXPECT_LE(duration, lengthMs);
    } else {
        EXPECT_EQ(EX_UNSUPPORTED_OPERATION, status.getExceptionCode());
        EXPECT_EQ(0, lengthMs);
    }

    if (duration) {
        EXPECT_EQ(future.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
    }
}

TEST_P(EffectsTest, alwaysOnEnable) {
    auto param = GetParam();
    auto effect = std::get<0>(param);
    auto strength = std::get<1>(param);
    auto scale = EFFECT_SCALE.find(param);
    bool supported = (scale != EFFECT_SCALE.end());

    if (supported) {
        EXPECT_CALL(*mMockApi, setGpioRiseIndex(EFFECT_INDEX)).WillOnce(Return(true));
        EXPECT_CALL(*mMockApi, setGpioRiseScale(scale->second)).WillOnce(Return(true));
    }

    ndk::ScopedAStatus status = mVibrator->alwaysOnEnable(0, effect, strength);
    if (supported) {
        EXPECT_EQ(EX_NONE, status.getExceptionCode());
    } else {
        EXPECT_EQ(EX_UNSUPPORTED_OPERATION, status.getExceptionCode());
    }
}

const std::vector<Effect> kEffects{ndk::enum_range<Effect>().begin(),
                                   ndk::enum_range<Effect>().end()};
const std::vector<EffectStrength> kEffectStrengths{ndk::enum_range<EffectStrength>().begin(),
                                                   ndk::enum_range<EffectStrength>().end()};

INSTANTIATE_TEST_CASE_P(VibratorTests, EffectsTest,
                        Combine(ValuesIn(kEffects.begin(), kEffects.end()),
                                ValuesIn(kEffectStrengths.begin(), kEffectStrengths.end())),
                        EffectsTest::PrintParam);

struct ComposeParam {
    std::string name;
    std::vector<CompositeEffect> composite;
    EffectQueue queue;
};

class ComposeTest : public VibratorTest, public WithParamInterface<ComposeParam> {
  public:
    static auto PrintParam(const TestParamInfo<ParamType> &info) { return info.param.name; }
};

TEST_P(ComposeTest, compose) {
    auto param = GetParam();
    auto composite = param.composite;
    auto queue = std::get<0>(param.queue);
    ExpectationSet eSetup;
    Expectation eActivate, ePoll;
    auto callback = ndk::SharedRefBase::make<MockVibratorCallback>();
    std::promise<void> promise;
    std::future<void> future{promise.get_future()};
    auto complete = [&promise] {
        promise.set_value();
        return ndk::ScopedAStatus::ok();
    };

    eSetup += EXPECT_CALL(*mMockApi, setEffectIndex(QUEUE_INDEX)).WillOnce(Return(true));
    eSetup += EXPECT_CALL(*mMockApi, setEffectQueue(queue)).WillOnce(Return(true));
    eSetup += EXPECT_CALL(*mMockApi, setEffectScale(0)).WillOnce(Return(true));
    eSetup += EXPECT_CALL(*mMockApi, setDuration(UINT32_MAX)).WillOnce(Return(true));
    eActivate = EXPECT_CALL(*mMockApi, setActivate(true)).After(eSetup).WillOnce(Return(true));
    ePoll = EXPECT_CALL(*mMockApi, pollVibeState(false)).After(eActivate).WillOnce(Return(true));
    EXPECT_CALL(*mMockApi, setActivate(false)).After(ePoll).WillOnce(Return(true));
    EXPECT_CALL(*callback, onComplete()).After(ePoll).WillOnce(complete);

    EXPECT_EQ(EX_NONE, mVibrator->compose(composite, callback).getExceptionCode());

    EXPECT_EQ(future.wait_for(std::chrono::milliseconds(100)), std::future_status::ready);
}

const std::vector<ComposeParam> kComposeParams = {
        {"click",
         {{0, CompositePrimitive::CLICK, 1.0f}},
         Queue(QueueEffect(2, 1.0f * V_LEVELS[4]), 0)},
        {"thud",
         {{1, CompositePrimitive::THUD, 0.8f}},
         Queue(1, QueueEffect(4, 0.8f * V_LEVELS[4]), 0)},
        {"spin",
         {{2, CompositePrimitive::SPIN, 0.6f}},
         Queue(2, QueueEffect(5, 0.6f * V_LEVELS[4]), 0)},
        {"quick_rise",
         {{3, CompositePrimitive::QUICK_RISE, 0.4f}},
         Queue(3, QueueEffect(6, 0.4f * V_LEVELS[4]), 0)},
        {"slow_rise",
         {{4, CompositePrimitive::SLOW_RISE, 0.2f}},
         Queue(4, QueueEffect(7, 0.2f * V_LEVELS[4]), 0)},
        {"quick_fall",
         {{5, CompositePrimitive::QUICK_FALL, 1.0f}},
         Queue(5, QueueEffect(8, V_LEVELS[4]), 0)},
        {"pop",
         {{6, CompositePrimitive::SLOW_RISE, 1.0f}, {50, CompositePrimitive::THUD, 1.0f}},
         Queue(6, QueueEffect(7, V_LEVELS[4]), 50, QueueEffect(4, V_LEVELS[4]), 0)},
        {"snap",
         {{7, CompositePrimitive::QUICK_RISE, 1.0f}, {0, CompositePrimitive::QUICK_FALL, 1.0f}},
         Queue(7, QueueEffect(6, V_LEVELS[4]), QueueEffect(8, V_LEVELS[4]), 0)},
};

INSTANTIATE_TEST_CASE_P(VibratorTests, ComposeTest,
                        ValuesIn(kComposeParams.begin(), kComposeParams.end()),
                        ComposeTest::PrintParam);

class AlwaysOnTest : public VibratorTest, public WithParamInterface<int32_t> {
  public:
    static auto PrintParam(const TestParamInfo<ParamType> &info) {
        return std::to_string(info.param);
    }
};

TEST_P(AlwaysOnTest, alwaysOnEnable) {
    auto param = GetParam();
    auto scale = EFFECT_SCALE.begin();

    std::advance(scale, std::rand() % EFFECT_SCALE.size());

    switch (param) {
        case 0:
            EXPECT_CALL(*mMockApi, setGpioRiseIndex(EFFECT_INDEX)).WillOnce(Return(true));
            EXPECT_CALL(*mMockApi, setGpioRiseScale(scale->second)).WillOnce(Return(true));
            break;
        case 1:
            EXPECT_CALL(*mMockApi, setGpioFallIndex(EFFECT_INDEX)).WillOnce(Return(true));
            EXPECT_CALL(*mMockApi, setGpioFallScale(scale->second)).WillOnce(Return(true));
            break;
    }

    auto effect = std::get<0>(scale->first);
    auto strength = std::get<1>(scale->first);

    ndk::ScopedAStatus status = mVibrator->alwaysOnEnable(param, effect, strength);
    EXPECT_EQ(EX_NONE, status.getExceptionCode());
}

TEST_P(AlwaysOnTest, alwaysOnDisable) {
    auto param = GetParam();

    switch (param) {
        case 0:
            EXPECT_CALL(*mMockApi, setGpioRiseIndex(0)).WillOnce(Return(true));
            break;
        case 1:
            EXPECT_CALL(*mMockApi, setGpioFallIndex(0)).WillOnce(Return(true));
            break;
    }

    ndk::ScopedAStatus status = mVibrator->alwaysOnDisable(param);
    EXPECT_EQ(EX_NONE, status.getExceptionCode());
}

INSTANTIATE_TEST_CASE_P(VibratorTests, AlwaysOnTest, Range(0, 1), AlwaysOnTest::PrintParam);

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
