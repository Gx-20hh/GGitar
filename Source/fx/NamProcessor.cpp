#include "NamProcessor.h"
#include "dsp/Biquad.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <filesystem>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace
{
    // juce::File -> std::filesystem::path，Windows 上中文路径必须按 UTF-8 走
    std::filesystem::path toPath (const juce::File& f)
    {
        const auto s = f.getFullPathName();
        return std::filesystem::path (reinterpret_cast<const char8_t*> (s.toRawUTF8()));
    }

    constexpr double targetLoudnessDb = -18.0;   // 与 NAM 官方插件一致
}

// =============================================================================
// NAM 后面的三段音色（和官方 NAM 插件一样）
struct NamProcessor::ToneStack
{
    Biquad low, mid, high;
    float last[3] { -100.0f, -100.0f, -100.0f };
};

NamProcessor::NamProcessor() : BuiltinEffect ("nam", T ("NAM 箱头")), toneStack (std::make_unique<ToneStack>())
{
    inputDb  = addFloat ("input", "输入", -24.0f, 24.0f, 0.0f, "dB");
    outputDb = addFloat ("output", "输出", -36.0f, 12.0f, 0.0f, "dB");
    bass     = addFloat ("bass", "低音", -10.0f, 10.0f, 0.0f, "dB");
    mid      = addFloat ("mid", "中音", -10.0f, 10.0f, 0.0f, "dB");
    treble   = addFloat ("treble", "高音", -10.0f, 10.0f, 0.0f, "dB");
}

NamProcessor::~NamProcessor() = default;

bool NamProcessor::loadModel (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = T ("找不到模型文件：") + file.getFullPathName();
        return false;
    }

    try
    {
        auto dsp = nam::get_dsp (toPath (file));
        if (dsp == nullptr)
        {
            error = T ("无法读取模型：") + file.getFileName();
            return false;
        }

        expectedSampleRate = dsp->GetExpectedSampleRate() > 0 ? dsp->GetExpectedSampleRate() : 48000.0;
        loudnessGain = dsp->HasLoudness()
                         ? (float) std::pow (10.0, (targetLoudnessDb - dsp->GetLoudness()) / 20.0)
                         : 1.0f;

        if (preparedBlock > 0)
            dsp->ResetAndPrewarm (currentSampleRate, preparedBlock);

        model = std::move (dsp);
        modelFile = file;
        return true;
    }
    catch (const std::exception& e)
    {
        error = T ("模型加载失败：") + juce::String::fromUTF8 (e.what());
        return false;
    }
}

bool NamProcessor::hasSampleRateMismatch() const
{
    return model != nullptr && std::abs (currentSampleRate - expectedSampleRate) > 1.0;
}

void NamProcessor::prepareEffect (double sampleRate, int maxBlockSize)
{
    preparedBlock = maxBlockSize;
    inBuf.assign ((size_t) maxBlockSize, 0.0f);
    outBuf.assign ((size_t) maxBlockSize, 0.0f);

    if (model != nullptr)
        model->ResetAndPrewarm (sampleRate, maxBlockSize);
}

void NamProcessor::processEffect (juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples();
    auto* l = b.getWritePointer (0);
    auto* r = b.getWritePointer (1);

    if (model == nullptr || n > preparedBlock)
        return;   // 没有模型：直通

    const float inG = juce::Decibels::decibelsToGain (inputDb->get());
    const float outG = juce::Decibels::decibelsToGain (outputDb->get()) * loudnessGain;

    // 吉他是单声道：左右平均后送进模型
    for (int i = 0; i < n; ++i)
        inBuf[(size_t) i] = 0.5f * (l[i] + r[i]) * inG;

    float* in[] = { inBuf.data() };
    float* out[] = { outBuf.data() };
    model->process (in, out, n);

    auto& ts = *toneStack;
    const float vals[3] = { bass->get(), mid->get(), treble->get() };
    if (! juce::approximatelyEqual (vals[0], ts.last[0]) || ! juce::approximatelyEqual (vals[1], ts.last[1])
        || ! juce::approximatelyEqual (vals[2], ts.last[2]))
    {
        ts.low.setLowShelf (currentSampleRate, 150.0, vals[0]);
        ts.mid.setPeak (currentSampleRate, 650.0, 0.7, vals[1]);
        ts.high.setHighShelf (currentSampleRate, 3000.0, vals[2]);
        std::copy (vals, vals + 3, ts.last);
    }
    const bool eq = std::abs (vals[0]) > 0.05f || std::abs (vals[1]) > 0.05f || std::abs (vals[2]) > 0.05f;

    for (int i = 0; i < n; ++i)
    {
        float y = outBuf[(size_t) i] * outG;
        if (eq)
            y = ts.high.process (ts.mid.process (ts.low.process (y, 0), 0), 0);
        l[i] = r[i] = y;
    }
}

// =============================================================================
struct IrProcessor::Impl
{
    juce::dsp::Convolution convolution { juce::dsp::Convolution::Latency { 0 } };
    juce::AudioBuffer<float> dry;
    Biquad lowCutFilter, highCutFilter;
    float lastLow = -1.0f, lastHigh = -1.0f;
};

IrProcessor::IrProcessor() : BuiltinEffect ("ir", T ("箱体 IR")), impl (std::make_unique<Impl>())
{
    levelDb = addFloat ("level", "音量", -24.0f, 12.0f, 0.0f, "dB");
    lowCut  = addFloat ("lowcut", "低切", 20.0f, 400.0f, 70.0f, "Hz", 80.0f);
    highCut = addFloat ("highcut", "高切", 2000.0f, 20000.0f, 12000.0f, "Hz", 8000.0f);
    mix     = addFloat ("mix", "混合", 0.0f, 1.0f, 1.0f);
}

IrProcessor::~IrProcessor() = default;

bool IrProcessor::loadIr (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = T ("找不到 IR 文件：") + file.getFullPathName();
        return false;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    if (std::unique_ptr<juce::AudioFormatReader> reader { fm.createReaderFor (file) }; reader == nullptr)
    {
        error = T ("无法读取 IR 文件（需要 wav 格式）：") + file.getFileName();
        return false;
    }

    // Convolution 会在后台线程里加载并安全替换，不会打断音频
    impl->convolution.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no,
                                           juce::dsp::Convolution::Trim::yes, 0,
                                           juce::dsp::Convolution::Normalise::yes);
    irFile = file;
    return true;
}

void IrProcessor::prepareEffect (double sampleRate, int maxBlockSize)
{
    impl->convolution.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 2 });
    impl->dry.setSize (2, maxBlockSize, false, true, false);
    impl->lastLow = impl->lastHigh = -1.0f;
    impl->lowCutFilter.reset();
    impl->highCutFilter.reset();
}

void IrProcessor::resetEffect()
{
    impl->convolution.reset();
}

void IrProcessor::processEffect (juce::AudioBuffer<float>& b)
{
    if (! juce::approximatelyEqual (impl->lastLow, lowCut->get()) || ! juce::approximatelyEqual (impl->lastHigh, highCut->get()))
    {
        impl->lastLow = lowCut->get();
        impl->lastHigh = highCut->get();
        impl->lowCutFilter.setHighpass (currentSampleRate, impl->lastLow, 0.7);
        impl->highCutFilter.setLowpass (currentSampleRate, impl->lastHigh, 0.7);
    }

    const int n = b.getNumSamples();
    const float m = mix->get();
    const bool blend = m < 0.999f && n <= impl->dry.getNumSamples();
    if (blend)
        for (int ch = 0; ch < 2; ++ch)
            impl->dry.copyFrom (ch, 0, b, ch, 0, n);

    juce::dsp::AudioBlock<float> block (b);
    impl->convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = b.getWritePointer (ch);
        impl->lowCutFilter.processBlock (d, b.getNumSamples(), ch);
        impl->highCutFilter.processBlock (d, b.getNumSamples(), ch);
    }

    b.applyGain (juce::Decibels::decibelsToGain (levelDb->get()));

    if (blend)
        for (int ch = 0; ch < 2; ++ch)
        {
            b.applyGain (ch, 0, n, m);
            b.addFrom (ch, 0, impl->dry, ch, 0, n, 1.0f - m);
        }
}
