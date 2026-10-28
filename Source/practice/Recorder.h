#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>

class TabPlayer;

// =============================================================================
// 录音 / 回放
//
// 录音：把吉他的干信号（输入增益后、效果器前）写成 WAV（后台线程写盘），旁边存一个同名 .json，
//       记下当时的谱子、音轨、速度、循环，以及“谱子开始播放时录到了第几个采样、从第几拍开始”。
// 回放：把录音当成吉他输入送进效果器链 —— 可以换音色重新听；录的时候在放谱子的话，
//       回放到同一个采样点时让谱子从同一拍开始，伴奏、谱面、判定都对得上。
// 音频线程只做：拷贝采样、读写原子变量；写盘在后台线程。
// =============================================================================
class Recorder
{
public:
    struct Info                     // 录音旁边的 .json
    {
        juce::File songFile;
        juce::String songTitle;
        int track = 0;
        float speed = 1.0f;
        bool loopOn = false, countIn = true, metronome = false, waitMode = false;
        double loopA = 0.0, loopB = 0.0;
        bool synced = false;        // 录音过程中谱子开始播放了
        int64_t syncSample = 0;     // 谱子开始播放时录到的采样数
        double syncBeat = 0.0;      // 从第几拍开始
        double sampleRate = 48000.0;
        double lengthSeconds = 0.0;
        juce::Time created;

        juce::var toVar() const;
        static Info fromVar (const juce::var& v);
        static Info loadFor (const juce::File& wav);
    };

    explicit Recorder (TabPlayer& player);
    ~Recorder();

    static juce::File getFolder();

    // ---- 界面线程 -----------------------------------------------------------
    void setSampleRate (double sr)            { deviceRate.store (sr); }    // 引擎准备时调用
    bool startRecording (const Info& info, juce::String& error);
    juce::File stopRecording();               // 返回录好的 wav
    bool isRecording() const                  { return recording.load(); }
    double getRecordedSeconds() const;

    bool startPlayback (const juce::File& wav, juce::String& error, Info* infoOut = nullptr);
    void stopPlayback();
    bool isPlayingBack() const                { return playbackOn.load(); }
    double getPlaybackSeconds() const;
    double getPlaybackLength() const          { return playbackLength; }
    void releaseRetired();                    // 定时调用

    // ---- 音频线程 -----------------------------------------------------------
    // 回放时用录音替换吉他输入（返回 true 表示 mono 已经被替换）
    bool replaceInput (float* mono, int n) noexcept;
    // 录音：写入这一块；tabPlaying = 这一块里谱子是否在播放（含预备拍）
    void capture (const float* mono, int n, bool tabPlaying, double tabBeatBeforeRender) noexcept;

private:
    TabPlayer& player;

    // 录音
    juce::TimeSliceThread writerThread { "Lianqin recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;         // 只在开始 / 停止时争用
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    std::atomic<bool> recording { false };
    std::atomic<int64_t> samplesRecorded { 0 };
    std::atomic<bool> syncCaptured { false };
    std::atomic<int64_t> syncSample { 0 };
    std::atomic<double> syncBeat { 0.0 };
    bool lastTabPlaying = false;
    Info pendingInfo;
    juce::File currentWav;
    double recordRate = 48000.0;
    std::atomic<double> deviceRate { 0.0 };

    // 回放
    std::shared_ptr<juce::AudioBuffer<float>> playbackData;
    std::vector<std::shared_ptr<juce::AudioBuffer<float>>> retired;
    std::atomic<juce::AudioBuffer<float>*> playbackPtr { nullptr }, playbackInUse { nullptr };
    std::atomic<bool> playbackOn { false };
    std::atomic<int64_t> playbackPos { 0 };
    std::atomic<bool> playbackSync { false };
    std::atomic<int64_t> playbackSyncSample { 0 };
    std::atomic<double> playbackSyncBeat { 0.0 };
    double playbackRate = 48000.0, playbackLength = 0.0;
};
