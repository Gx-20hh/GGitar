#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include "dsp/Biquad.h"
#include "fx/HostTempo.h"

// 鼓件
enum DrumInstrument
{
    kick, snare, sideStick, closedHat, openHat, crash, ride, tomHigh, tomMid, tomLow,
    clickHigh, clickLow,    // 节拍器 / 预备拍
    numDrumInstruments
};

// 一小节的节奏型：每个鼓件每一步的力度（0 = 不打）
struct DrumPattern
{
    int steps = 16;
    std::array<std::array<float, 16>, numDrumInstruments> velocity {};
};

// 一种风格：主歌 / 副歌 / 加花三种小节
struct DrumStyle
{
    juce::String name;
    int beatsPerBar = 4;
    int stepsPerBeat = 4;        // 4 = 十六分音符网格；3 = 三连音（shuffle、12/8）
    float defaultBpm = 100.0f;
    float defaultSwing = 0.0f;   // 0..1
    bool clickOnly = false;      // 只有节拍器
    DrumPattern verse, chorus, fill;
};

const std::vector<DrumStyle>& getDrumStyles();

// =============================================================================
// 合成鼓声（不需要采样文件）
class DrumVoice
{
public:
    void setup (DrumInstrument type, double sampleRate, float pan);
    void trigger (float velocity) noexcept;
    void choke() noexcept { if (active) release = true; }
    void render (float* left, float* right, int numSamples) noexcept;

private:
    float next() noexcept;
    float noise() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (int32_t) seed * (1.0f / 2147483648.0f); }

    float decayFactor (double seconds) const noexcept { return (float) std::exp (-1.0 / (seconds * sr)); }

    DrumInstrument type = kick;
    double sr = 48000.0;
    float gainL = 0.7f, gainR = 0.7f;
    bool active = false, release = false;

    // 音调部分：频率从 fStart 指数滑到 fEnd
    double phase = 0.0, fStart = 100.0, fEnd = 50.0;
    float pitchEnv = 0.0f, pitchDecay = 0.99f;
    float toneAmp = 0.0f, toneDecay = 0.999f, toneLevel = 0.0f;

    // 噪声 / 金属声部分（镲片）
    float noiseAmp = 0.0f, noiseDecay = 0.99f, noiseLevel = 0.0f;
    bool metallic = false;
    double metalPhase[6] {}, metalFreq[6] {};

    float vel = 1.0f, releaseDecay = 0.99f;
    uint32_t seed = 22222u;
    Biquad filterA, filterB;
};

// =============================================================================
// 鼓机：在音频线程里按采样精确地走节奏，参数用原子变量从界面线程设置
class DrumMachine
{
public:
    DrumMachine();

    // ---- 界面线程 -----------------------------------------------------------
    void setPlaying (bool shouldPlay);
    bool isPlaying() const              { return playing.load(); }
    void setStyle (int index)           { styleIndex.store (index); }
    int  getStyle() const               { return styleIndex.load(); }
    void setChorus (bool c)             { chorusSection.store (c); }
    bool isChorus() const               { return chorusSection.load(); }
    void setBpm (float b)               { bpm.store (juce::jlimit (30.0f, 300.0f, b)); HostTempo::bpm.store (bpm.load()); }
    float getBpm() const                { return bpm.load(); }
    void setSwing (float s)             { swing.store (juce::jlimit (0.0f, 1.0f, s)); }
    float getSwing() const              { return swing.load(); }
    void setFillEvery (int bars)        { fillEvery.store (bars); }
    int  getFillEvery() const           { return fillEvery.load(); }
    void setCountIn (bool c)            { countIn.store (c); }
    bool getCountIn() const             { return countIn.load(); }
    void setMetronome (bool m)          { metronome.store (m); }
    bool getMetronome() const           { return metronome.load(); }
    void setVolumeDb (float db)         { volumeDb.store (db); }
    float getVolumeDb() const           { return volumeDb.load(); }
    void triggerFillNow()               { fillRequested.store (true); }

    // 界面显示：当前第几拍（从 0 开始）、是否在预备拍、是否在加花
    int getCurrentBeat() const          { return currentBeat.load(); }
    bool isInCountIn() const            { return inCountIn.load(); }
    bool isInFill() const               { return inFill.load(); }

    // ---- 音频线程 -----------------------------------------------------------
    void prepare (double sampleRate);
    // 谱子里的鼓轨 / 预备拍直接敲（音频线程调用）
    void triggerFromSong (DrumInstrument inst, float velocity) noexcept { hit (inst, velocity); }
    // 把鼓声叠加进 left / right
    void render (float* left, float* right, int numSamples) noexcept;

private:
    void startStep() noexcept;
    double stepLengthSamples (int stepIndex) const noexcept;
    void hit (DrumInstrument inst, float velocity) noexcept;

    std::atomic<bool> playing { false }, chorusSection { false }, countIn { true }, metronome { false };
    std::atomic<bool> fillRequested { false }, restartRequested { false };
    std::atomic<int> styleIndex { 0 }, fillEvery { 4 };
    std::atomic<float> bpm { 100.0f }, swing { 0.0f }, volumeDb { -6.0f };
    std::atomic<int> currentBeat { -1 };
    std::atomic<bool> inCountIn { false }, inFill { false };

    double sampleRate = 48000.0;
    std::array<DrumVoice, numDrumInstruments> voices;
    juce::SmoothedValue<float> gain { 0.5f };

    // 音频线程内部状态
    bool running = false;
    int activeStyle = 0, step = 0, bar = 0, countInBeatsLeft = 0;
    bool barIsFill = false, crashNext = false;
    double samplesToNextStep = 0.0;
};
