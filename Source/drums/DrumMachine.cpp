#include "DrumMachine.h"
#include <cstring>
#include "Text.h"

// =============================================================================
// 节奏型
namespace
{
    struct Track { const char* key; const char* hits; };

    int instrumentForKey (const juce::String& k)
    {
        if (k == "K")  return kick;
        if (k == "S")  return snare;
        if (k == "R")  return sideStick;
        if (k == "H")  return closedHat;
        if (k == "O")  return openHat;
        if (k == "C")  return crash;
        if (k == "D")  return ride;
        if (k == "T1") return tomHigh;
        if (k == "T2") return tomMid;
        if (k == "T3") return tomLow;
        jassertfalse;
        return -1;
    }

    float velocityForChar (char c)
    {
        switch (c)
        {
            case 'X': return 1.0f;
            case 'x': return 0.7f;
            case 'o': return 0.5f;
            case 'g': return 0.28f;   // 鬼音
            default:  return 0.0f;
        }
    }

    DrumPattern pattern (int steps, std::initializer_list<Track> tracks)
    {
        DrumPattern p;
        p.steps = steps;
        for (auto& t : tracks)
        {
            const int inst = instrumentForKey (t.key);
            jassert ((int) std::strlen (t.hits) == steps);   // 每一行长度必须等于步数
            for (int i = 0; i < steps && t.hits[i] != 0; ++i)
                if (inst >= 0)
                    p.velocity[(size_t) inst][(size_t) i] = velocityForChar (t.hits[i]);
        }
        return p;
    }

    DrumStyle style (const char* name, int beats, int stepsPerBeat, float bpm, float swing,
                     DrumPattern verse, DrumPattern chorus, DrumPattern fill, bool clickOnly = false)
    {
        DrumStyle s;
        s.name = T (name);
        s.beatsPerBar = beats;
        s.stepsPerBeat = stepsPerBeat;
        s.defaultBpm = bpm;
        s.defaultSwing = swing;
        s.clickOnly = clickOnly;
        s.verse = verse; s.chorus = chorus; s.fill = fill;
        return s;
    }
}

const std::vector<DrumStyle>& getDrumStyles()
{
    // 每行一个鼓件，每个字符一步：X 重音  x 普通  o 轻  g 鬼音  . 不打
    // K 底鼓  S 军鼓  R 边击  H 闭镲  O 开镲  C 吊镲  D 叮叮镲  T1/T2/T3 高/中/落地通鼓
    static const std::vector<DrumStyle> styles {
        style ("摇滚", 4, 4, 110, 0,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "X.....X.X......." }, { "S", "....X.......X..." } }),
               pattern (16, { { "D", "X.x.X.x.X.x.X.x." }, { "K", "X.....X.X.X....." }, { "S", "....X.......X..." } }),
               pattern (16, { { "H", "x.x.x.x........." }, { "K", "X.....X........." }, { "S", "....X...Xx......" },
                              { "T1", "..........Xx...." }, { "T2", "............Xx.." }, { "T3", "..............XX" } })),

        style ("流行", 4, 4, 96, 0,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "X.......X.X....." }, { "S", "....X.......X..." } }),
               pattern (16, { { "H", "x...x...x...x..." }, { "O", "..x...x...x...x." }, { "K", "X...X...X...X..." }, { "S", "....X.......X..." } }),
               pattern (16, { { "H", "x.x.x.x........." }, { "K", "X.......X......." }, { "S", "....X...X.X.XXXX" } })),

        style ("慢歌", 4, 4, 72, 0,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "X.......X.....o." }, { "R", "....X.......X..." } }),
               pattern (16, { { "D", "x.x.x.x.x.x.x.x." }, { "K", "X.......X.X....." }, { "S", "....X.......X..." } }),
               pattern (16, { { "K", "X..............." }, { "S", "....X.....xx...." }, { "T1", "........X......." }, { "T3", "............X.X." } })),

        style ("放克", 4, 4, 96, 0.15f,
               pattern (16, { { "H", "XxxxXxxxXxxxXxxx" }, { "K", "X..X..X...X..X.." }, { "S", "....X..g.g..X..g" } }),
               pattern (16, { { "H", "XxxxXxxxXxxxXx.." }, { "O", "..............X." }, { "K", "X..X..X...XX..X." }, { "S", "....X..g.g..X..g" } }),
               pattern (16, { { "H", "XxxxXxxx........" }, { "K", "X..X..X........." }, { "S", "....X..gXgXgXXXX" } })),

        style ("迪斯科", 4, 4, 118, 0,
               pattern (16, { { "K", "X...X...X...X..." }, { "S", "....X.......X..." }, { "H", "x...x...x...x..." }, { "O", "..X...X...X...X." } }),
               pattern (16, { { "K", "X...X...X...X..." }, { "S", "....X.......X..." }, { "H", "xx.xxx.xxx.xxx.x" }, { "O", "..X...X...X...X." } }),
               pattern (16, { { "K", "X...X...X...X..." }, { "S", "....X...XXXXXXXX" }, { "H", "x...x..........." } })),

        style ("朋克", 4, 4, 180, 0,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "X.....X.X......." }, { "S", "....X.......X..." } }),
               pattern (16, { { "D", "x.x.x.x.x.x.x.x." }, { "K", "X...X...X...X..." }, { "S", "..X...X...X...X." } }),
               pattern (16, { { "K", "X..............." }, { "S", "X.X.X.X.XXXXXXXX" } })),

        style ("金属", 4, 4, 150, 0,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "X.X.X.X.X.X.X.X." }, { "S", "....X.......X..." } }),
               pattern (16, { { "D", "X...X...X...X..." }, { "K", "XXXXXXXXXXXXXXXX" }, { "S", "....X.......X..." } }),
               pattern (16, { { "K", "X.X.X.X.X.X.X.X." }, { "S", "XxXxXxXx........" }, { "T1", "........XxXx...." }, { "T3", "............XxXx" } })),

        style ("蓝调 Shuffle", 4, 3, 84, 0,
               pattern (12, { { "H", "X.xX.xX.xX.x" }, { "K", "X.....X....." }, { "S", "...X.....X.." } }),
               pattern (12, { { "D", "X.xX.xX.xX.x" }, { "K", "X..X..X..X.." }, { "S", "...X.....X.." } }),
               pattern (12, { { "H", "X.xX.x......" }, { "K", "X..........." }, { "S", "...X..XxxXxx" } })),

        style ("雷鬼", 4, 4, 76, 0.2f,
               pattern (16, { { "H", "x.x.x.x.x.x.x.x." }, { "K", "........X......." }, { "R", "........X......." } }),
               pattern (16, { { "H", "x...x...x...x..." }, { "O", "..x...x...x...x." }, { "K", "X...X...X...X..." }, { "R", "....X.......X..." } }),
               pattern (16, { { "K", "........X......." }, { "T1", "........XX......" }, { "T2", "..........XX...." }, { "T3", "............XX.." }, { "S", "..............X." } })),

        style ("爵士 Swing", 4, 3, 140, 0,
               pattern (12, { { "D", "X..X.xX..X.x" }, { "H", "...X.....X.." }, { "K", "o..o..o..o.." }, { "S", "........g..." } }),
               pattern (12, { { "D", "X..X.xX..X.x" }, { "H", "...X.....X.." }, { "K", "o..o..o..o.." }, { "S", ".....g..g..x" } }),
               pattern (12, { { "D", "X..X.x......" }, { "H", "...X.....X.." }, { "K", "o..o........" }, { "S", "......XxxXxx" } })),

        style ("华尔兹 3/4", 3, 4, 120, 0,
               pattern (12, { { "K", "X..........." }, { "S", "....x...x..." }, { "H", "x.x.x.x.x.x." } }),
               pattern (12, { { "K", "X..........." }, { "S", "....X...X..." }, { "D", "x.x.x.x.x.x." } }),
               pattern (12, { { "K", "X..........." }, { "S", "....X...XxXx" } })),

        style ("仅节拍器", 4, 4, 100, 0, pattern (16, {}), pattern (16, {}), pattern (16, {}), true),
    };
    return styles;
}

// =============================================================================
// 合成鼓声
void DrumVoice::setup (DrumInstrument t, double sampleRate, float pan)
{
    type = t;
    sr = sampleRate;
    const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;   // 等功率声像
    gainL = std::cos (angle);
    gainR = std::sin (angle);
    seed = 12345u + (uint32_t) t * 7919u;
    releaseDecay = decayFactor (0.012);
    filterA.reset();
    filterB.reset();

    metallic = false;
    toneLevel = noiseLevel = 0.0f;

    auto setMetal = [this] (double base)
    {
        static const double ratios[6] = { 1.0, 1.304, 1.466, 1.787, 1.932, 2.536 };   // 808 镲片的方波比例
        for (int i = 0; i < 6; ++i)
            metalFreq[i] = base * ratios[i];
        metallic = true;
    };

    switch (type)
    {
        case kick:
            fStart = 150; fEnd = 46; pitchDecay = decayFactor (0.035);
            toneLevel = 1.0f; toneDecay = decayFactor (0.32);
            noiseLevel = 0.3f; noiseDecay = decayFactor (0.004);
            filterA.setHighpass (sr, 1500.0); filterB.setLowpass (sr, 8000.0);
            break;
        case snare:
            fStart = 240; fEnd = 185; pitchDecay = decayFactor (0.02);
            toneLevel = 0.55f; toneDecay = decayFactor (0.09);
            noiseLevel = 0.75f; noiseDecay = decayFactor (0.15);
            filterA.setHighpass (sr, 1100.0); filterB.setLowpass (sr, 9500.0);
            break;
        case sideStick:
            fStart = 900; fEnd = 820; pitchDecay = decayFactor (0.01);
            toneLevel = 0.5f; toneDecay = decayFactor (0.025);
            noiseLevel = 0.6f; noiseDecay = decayFactor (0.012);
            filterA.setBandpass (sr, 2600.0, 2.0); filterB.setLowpass (sr, 9000.0);
            break;
        case closedHat:
            setMetal (410);
            noiseLevel = 0.42f; noiseDecay = decayFactor (0.045);
            filterA.setHighpass (sr, 7500.0); filterB.setLowpass (sr, 15000.0);
            break;
        case openHat:
            setMetal (410);
            noiseLevel = 0.36f; noiseDecay = decayFactor (0.32);
            filterA.setHighpass (sr, 7000.0); filterB.setLowpass (sr, 15000.0);
            break;
        case crash:
            setMetal (330);
            noiseLevel = 0.4f; noiseDecay = decayFactor (1.3);
            filterA.setHighpass (sr, 4200.0); filterB.setLowpass (sr, 13000.0);
            break;
        case ride:
            setMetal (520);
            fStart = fEnd = 2350; pitchDecay = 0.0f;
            toneLevel = 0.05f; toneDecay = decayFactor (0.5);
            noiseLevel = 0.22f; noiseDecay = decayFactor (0.7);
            filterA.setHighpass (sr, 5200.0); filterB.setLowpass (sr, 12000.0);
            break;
        case tomHigh:
        case tomMid:
        case tomLow:
        {
            const double f = type == tomHigh ? 205.0 : (type == tomMid ? 150.0 : 105.0);
            fStart = f * 1.5; fEnd = f; pitchDecay = decayFactor (0.06);
            toneLevel = 0.9f; toneDecay = decayFactor (0.35);
            noiseLevel = 0.15f; noiseDecay = decayFactor (0.02);
            filterA.setHighpass (sr, 400.0); filterB.setLowpass (sr, 3500.0);
            break;
        }
        case clickHigh:
        case clickLow:
            fStart = fEnd = (type == clickHigh ? 1760.0 : 1175.0); pitchDecay = 0.0f;
            toneLevel = type == clickHigh ? 0.55f : 0.42f; toneDecay = decayFactor (0.018);
            break;
        case numDrumInstruments:
        default:
            break;
    }
}

void DrumVoice::trigger (float velocity) noexcept
{
    vel = velocity;
    active = true;
    release = false;
    phase = 0.0;
    pitchEnv = 1.0f;
    toneAmp = toneLevel;
    noiseAmp = noiseLevel;
}

float DrumVoice::next() noexcept
{
    float out = 0.0f;

    if (toneAmp > 0.0f)
    {
        const double f = fEnd + (fStart - fEnd) * pitchEnv;
        pitchEnv *= pitchDecay;
        phase += f / sr;
        if (phase >= 1.0) phase -= 1.0;
        out += toneAmp * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        toneAmp *= release ? releaseDecay : toneDecay;
        if (toneAmp < 1.0e-4f) toneAmp = 0.0f;
    }

    if (noiseAmp > 0.0f)
    {
        float src = noise();
        if (metallic)
        {
            float m = 0.0f;
            for (int i = 0; i < 6; ++i)
            {
                metalPhase[i] += metalFreq[i] / sr;
                if (metalPhase[i] >= 1.0) metalPhase[i] -= 1.0;
                m += metalPhase[i] < 0.5 ? 1.0f : -1.0f;
            }
            src = 0.5f * src + 0.12f * m;
        }
        out += noiseAmp * filterB.process (filterA.process (src, 0), 0);
        noiseAmp *= release ? releaseDecay : noiseDecay;
        if (noiseAmp < 1.0e-4f) noiseAmp = 0.0f;
    }

    if (toneAmp <= 0.0f && noiseAmp <= 0.0f)
        active = false;

    return out * vel;
}

void DrumVoice::render (float* left, float* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples && active; ++i)
    {
        const float s = next();
        left[i] += s * gainL;
        right[i] += s * gainR;
    }
}

// =============================================================================
// 鼓机
DrumMachine::DrumMachine() = default;

void DrumMachine::prepare (double sr)
{
    sampleRate = sr;
    static const float pans[numDrumInstruments] = { 0.0f, 0.05f, 0.05f, 0.3f, 0.3f, -0.4f, 0.4f, 0.25f, 0.0f, -0.25f, 0.0f, 0.0f };
    for (int i = 0; i < numDrumInstruments; ++i)
        voices[(size_t) i].setup ((DrumInstrument) i, sr, pans[i]);

    gain.reset (sr, 0.05);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeDb.load()));
}

void DrumMachine::setPlaying (bool shouldPlay)
{
    playing.store (shouldPlay);
}

void DrumMachine::hit (DrumInstrument inst, float velocity) noexcept
{
    if (inst == closedHat)
        voices[openHat].choke();
    voices[(size_t) inst].trigger (velocity);
}

double DrumMachine::stepLengthSamples (int stepIndex) const noexcept
{
    const auto& styles = getDrumStyles();
    const auto& st = styles[(size_t) activeStyle];
    const double base = sampleRate * 60.0 / (double) bpm.load() / (double) st.stepsPerBeat;

    if (st.stepsPerBeat != 4)
        return base;

    // 摇摆：每拍前半（两步）拉长、后半缩短；100% 相当于三连音 shuffle
    const double s = (double) swing.load() / 3.0;
    return (stepIndex % 4) < 2 ? base * (1.0 + s) : base * (1.0 - s);
}

void DrumMachine::startStep() noexcept
{
    const auto& styles = getDrumStyles();

    if (step == 0)
    {
        // 风格切换只在小节开头生效，保证不乱拍
        const int wanted = juce::jlimit (0, (int) styles.size() - 1, styleIndex.load());
        if (wanted != activeStyle && countInBeatsLeft == 0)
            activeStyle = wanted;

        const int fe = fillEvery.load();
        const bool requested = fillRequested.exchange (false);
        barIsFill = countInBeatsLeft == 0 && (requested || (fe > 0 && (bar % fe) == fe - 1));
        inFill.store (barIsFill);
    }

    const auto& st = styles[(size_t) activeStyle];
    const int spb = st.stepsPerBeat;
    const bool onBeat = (step % spb) == 0;

    if (onBeat)
        currentBeat.store (step / spb);

    if (countInBeatsLeft > 0)
    {
        inCountIn.store (true);
        if (onBeat)
            hit (step == 0 ? clickHigh : clickLow, 1.0f);
        return;
    }

    inCountIn.store (false);

    if (! st.clickOnly)
    {
        const auto& p = barIsFill ? st.fill : (chorusSection.load() ? st.chorus : st.verse);
        for (int inst = 0; inst < clickHigh; ++inst)
        {
            const float v = p.velocity[(size_t) inst][(size_t) step];
            if (v > 0.0f)
                hit ((DrumInstrument) inst, v);
        }

        // 加花之后的下一小节第一拍补一个吊镲
        if (step == 0 && crashNext)
        {
            hit (crash, 0.9f);
            if (p.velocity[kick][0] <= 0.0f)
                hit (kick, 0.9f);
        }
    }

    if (step == 0)
        crashNext = false;

    if ((metronome.load() || st.clickOnly) && onBeat)
        hit (step == 0 ? clickHigh : clickLow, 0.9f);
}

void DrumMachine::render (float* left, float* right, int numSamples) noexcept
{
    const auto& styles = getDrumStyles();
    const bool wantPlay = playing.load();

    if (wantPlay && ! running)
    {
        running = true;
        activeStyle = juce::jlimit (0, (int) styles.size() - 1, styleIndex.load());
        step = 0;
        bar = 0;
        crashNext = false;
        countInBeatsLeft = countIn.load() ? styles[(size_t) activeStyle].beatsPerBar : 0;
        samplesToNextStep = 0.0;
    }
    else if (! wantPlay && running)
    {
        running = false;
        currentBeat.store (-1);
        inCountIn.store (false);
        inFill.store (false);
    }

    gain.setTargetValue (juce::Decibels::decibelsToGain (volumeDb.load(), -60.0f));

    int pos = 0;
    while (pos < numSamples)
    {
        int chunk = numSamples - pos;

        if (running)
        {
            if (samplesToNextStep <= 0.0)
            {
                startStep();
                samplesToNextStep += stepLengthSamples (step);

                const auto& st = styles[(size_t) activeStyle];
                if (++step >= st.beatsPerBar * st.stepsPerBeat)
                {
                    step = 0;
                    if (countInBeatsLeft > 0)
                        countInBeatsLeft = 0;           // 预备拍只有一小节
                    else
                    {
                        if (barIsFill)
                            crashNext = true;
                        ++bar;
                    }
                }
            }

            chunk = juce::jmin (chunk, juce::jmax (1, (int) std::ceil (samplesToNextStep)));
        }

        // 先把鼓声渲染到输出（未乘音量），再统一乘音量
        float* l = left + pos;
        float* r = right + pos;
        float tmpL[256], tmpR[256];
        for (int done = 0; done < chunk; )
        {
            const int n = juce::jmin (256, chunk - done);
            std::fill (tmpL, tmpL + n, 0.0f);
            std::fill (tmpR, tmpR + n, 0.0f);
            for (auto& v : voices)
                v.render (tmpL, tmpR, n);
            for (int i = 0; i < n; ++i)
            {
                const float g = gain.getNextValue();
                l[done + i] += tmpL[i] * g;
                r[done + i] += tmpR[i] * g;
            }
            done += n;
        }

        if (running)
            samplesToNextStep -= chunk;
        pos += chunk;
    }
}
