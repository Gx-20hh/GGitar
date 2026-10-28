#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "song/Song.h"
#include <atomic>
#include <memory>
#include <vector>

// Decoded off the audio thread. Immutable clips are published with a hazard pointer;
// the realtime thread never owns/deletes buffers, reads files or acquires a mutex.
class BackingTrack
{
public:
    struct Clip
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 48000.0;
        juce::File file;
        std::vector<float> peaks;
        double duration() const { return audio.getNumSamples() / sampleRate; }
    };
    struct LoadResult { std::shared_ptr<const Clip> clip; juce::String error; };
    static LoadResult load (const juce::File& file)
    {
        juce::WavAudioFormat format;
        auto stream = file.createInputStream();
        if (! stream) return { {}, juce::String::fromUTF8 ("无法读取文件") };
        std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (stream.release(), true));
        if (! reader || reader->sampleRate <= 0.0 || reader->lengthInSamples < 2 || reader->numChannels < 1)
            return { {}, juce::String::fromUTF8 ("请选择有效 WAV 文件") };
        // A bounded stereo buffer (256 MiB) prevents huge allocations from malformed files.
        if (reader->lengthInSamples > 32 * 1024 * 1024)
            return { {}, juce::String::fromUTF8 ("文件过长：解码后最多支持 256 MiB 立体声音频，请截取练习片段") };
        auto clip = std::make_shared<Clip>(); clip->sampleRate = reader->sampleRate; clip->file = file;
        const int count = (int) reader->lengthInSamples;
        clip->audio.setSize (2, count);
        if (! reader->read (&clip->audio, 0, count, 0, true, true))
            return { {}, juce::String::fromUTF8 ("WAV 解码失败") };
        if (reader->numChannels == 1) clip->audio.copyFrom (1, 0, clip->audio, 0, 0, count);
        clip->peaks.resize (1000, 0.0f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < count; ++i)
            {
                auto& sample = clip->audio.getWritePointer (c)[i];
                if (! std::isfinite (sample)) sample = 0.0f;
                const size_t bin = juce::jmin ((size_t) 999, (size_t) i * 1000 / (size_t) count);
                clip->peaks[bin] = juce::jmax (clip->peaks[bin], std::abs (sample));
            }
        return { clip, {} };
    }
    void setClip (std::shared_ptr<const Clip> next)
    {
        if (current) retired.push_back (current);
        current = std::move (next); published.store (current.get()); collect();
    }
    void collect()
    {
        const auto* inUse = hazard.load();
        std::erase_if (retired, [inUse] (auto& clip) { return clip.get() != inUse; });
    }
    std::shared_ptr<const Clip> getClip() const { return current; } // UI thread only
    void setAlignment (double seconds, double ratio) { offset.store (seconds); scale.store (juce::jlimit (0.5, 2.0, ratio)); }
    double getOffset() const { return offset.load(); }
    double getScale() const { return scale.load(); }
    void setGainDb (float db) { gain.store (juce::Decibels::decibelsToGain (db, -60.0f)); }
    void setMuted (bool value) { muted.store (value); }
    bool isMuted() const { return muted.load(); }
    double audioSeconds (const Song& song, double beat) const { return song.beatToSeconds (beat) * scale.load() + offset.load(); }

    void render (float* l, float* r, int n, const Song* song, bool rolling,
                 double beat, double bps, int wrapOffset, double wrapBeat) noexcept
    {
        if (! song || ! rolling || bps <= 0.0 || muted.load()) return;
        const Clip* clip;
        do { clip = published.load(); hazard.store (clip); } while (clip != published.load());
        if (clip)
        {
            const auto off = offset.load(), ratio = scale.load(); const float volume = gain.load();
            for (int i = 0; i < n; ++i)
            {
                const double pos = wrapOffset >= 0 && i >= wrapOffset ? wrapBeat + (i - wrapOffset) * bps : beat + i * bps;
                if (pos >= song->lengthBeats) continue;
                const double sample = (song->beatToSeconds (pos) * ratio + off) * clip->sampleRate;
                if (sample < 0.0 || sample >= clip->audio.getNumSamples() - 1) continue;
                const int index = (int) sample; const float frac = (float) (sample - index);
                for (int c = 0; c < 2; ++c)
                {
                    const auto* data = clip->audio.getReadPointer (c);
                    (c == 0 ? l[i] : r[i]) += (data[index] + frac * (data[index + 1] - data[index])) * volume;
                }
            }
        }
        hazard.store (nullptr);
    }
private:
    std::shared_ptr<const Clip> current;
    std::vector<std::shared_ptr<const Clip>> retired;
    std::atomic<const Clip*> published { nullptr }, hazard { nullptr };
    std::atomic<double> offset { 0.0 }, scale { 1.0 };
    std::atomic<float> gain { 0.5f };
    std::atomic<bool> muted { false };
};
