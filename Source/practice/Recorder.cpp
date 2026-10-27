#include "Recorder.h"
#include "TabPlayer.h"

namespace
{
    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
}

// ---- 旁注文件 --------------------------------------------------------------------
juce::var Recorder::Info::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("song", songFile.getFullPathName());
    o->setProperty ("title", songTitle);
    o->setProperty ("track", track);
    o->setProperty ("speed", speed);
    o->setProperty ("loopOn", loopOn);
    o->setProperty ("loopA", loopA);
    o->setProperty ("loopB", loopB);
    o->setProperty ("countIn", countIn);
    o->setProperty ("metronome", metronome);
    o->setProperty ("waitMode", waitMode);
    o->setProperty ("synced", synced);
    o->setProperty ("syncSample", (juce::int64) syncSample);
    o->setProperty ("syncBeat", syncBeat);
    o->setProperty ("sampleRate", sampleRate);
    o->setProperty ("length", lengthSeconds);
    o->setProperty ("created", created.toMilliseconds());
    return juce::var (o);
}

Recorder::Info Recorder::Info::fromVar (const juce::var& v)
{
    Info i;
    if (! v.isObject())
        return i;
    i.songFile = juce::File (v["song"].toString());
    i.songTitle = v["title"].toString();
    i.track = (int) v["track"];
    i.speed = (float) (double) v["speed"];
    i.loopOn = (bool) v["loopOn"];
    i.loopA = (double) v["loopA"];
    i.loopB = (double) v["loopB"];
    i.countIn = (bool) v["countIn"];
    i.metronome = (bool) v["metronome"];
    i.waitMode = (bool) v["waitMode"];
    i.synced = (bool) v["synced"];
    i.syncSample = (int64_t) (juce::int64) v["syncSample"];
    i.syncBeat = (double) v["syncBeat"];
    i.sampleRate = (double) v["sampleRate"];
    i.lengthSeconds = (double) v["length"];
    i.created = juce::Time ((juce::int64) v["created"]);
    if (i.speed <= 0.0f) i.speed = 1.0f;
    return i;
}

Recorder::Info Recorder::Info::loadFor (const juce::File& wav)
{
    auto info = fromVar (juce::JSON::parse (wav.withFileExtension ("json")));
    if (info.created.toMilliseconds() == 0)
        info.created = wav.getLastModificationTime();
    return info;
}

// =============================================================================
Recorder::Recorder (TabPlayer& p) : player (p) {}

Recorder::~Recorder()
{
    stopRecording();
    stopPlayback();
    writerThread.stopThread (2000);
}

juce::File Recorder::getFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("LianqinHost").getChildFile (T ("录音"));
}

// ---- 录音（界面线程）-------------------------------------------------------------
bool Recorder::startRecording (const Info& info, juce::String& error)
{
    stopRecording();
    const double sampleRate = deviceRate.load();
    if (sampleRate <= 0.0)
    {
        error = T ("声卡没有打开");
        return false;
    }

    auto folder = getFolder();
    folder.createDirectory();
    juce::String name = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    if (info.songTitle.isNotEmpty())
        name << " " << juce::File::createLegalFileName (info.songTitle).substring (0, 40);
    auto wav = folder.getChildFile (name + ".wav").getNonexistentSibling();

    std::unique_ptr<juce::OutputStream> stream (wav.createOutputStream());
    if (stream == nullptr)
    {
        error = T ("没法在这里写文件：") + folder.getFullPathName();
        return false;
    }

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), sampleRate, 1, 24, {}, 0));
    if (writer == nullptr)
    {
        error = T ("创建 WAV 文件失败");
        return false;
    }
    stream.release();      // 归 writer 了

    writerThread.startThread();
    auto tw = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 1 << 17);

    pendingInfo = info;
    pendingInfo.sampleRate = sampleRate;
    pendingInfo.created = juce::Time::getCurrentTime();
    currentWav = wav;
    recordRate = sampleRate;
    samplesRecorded.store (0);
    syncCaptured.store (false);

    {
        const juce::ScopedLock sl (writerLock);
        threadedWriter = std::move (tw);
        activeWriter.store (threadedWriter.get());
    }
    recording.store (true);
    return true;
}

juce::File Recorder::stopRecording()
{
    if (! recording.exchange (false))
        return {};

    {
        const juce::ScopedLock sl (writerLock);
        activeWriter.store (nullptr);
    }
    threadedWriter = nullptr;      // 析构时把剩下的数据写完并关闭文件

    pendingInfo.synced = syncCaptured.load();
    pendingInfo.syncSample = syncSample.load();
    pendingInfo.syncBeat = syncBeat.load();
    pendingInfo.lengthSeconds = (double) samplesRecorded.load() / recordRate;
    currentWav.withFileExtension ("json").replaceWithText (juce::JSON::toString (pendingInfo.toVar()));
    return currentWav;
}

double Recorder::getRecordedSeconds() const
{
    return recording.load() ? (double) samplesRecorded.load() / recordRate : 0.0;
}

// ---- 回放（界面线程）-------------------------------------------------------------
bool Recorder::startPlayback (const juce::File& wav, juce::String& error, Info* infoOut)
{
    stopPlayback();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (wav));
    if (reader == nullptr)
    {
        error = T ("读不了这个录音：") + wav.getFileName();
        return false;
    }
    if (reader->lengthInSamples > (juce::int64) (reader->sampleRate * 60 * 30))
    {
        error = T ("录音太长了（超过 30 分钟）");
        return false;
    }

    auto data = std::make_shared<juce::AudioBuffer<float>> (1, (int) reader->lengthInSamples);
    reader->read (data.get(), 0, (int) reader->lengthInSamples, 0, true, false);

    const auto info = Info::loadFor (wav);
    if (infoOut != nullptr) *infoOut = info;

    playbackRate = reader->sampleRate;
    playbackLength = (double) reader->lengthInSamples / reader->sampleRate;
    playbackSync.store (info.synced && ! info.waitMode);
    playbackSyncSample.store (info.syncSample);
    playbackSyncBeat.store (info.syncBeat);
    playbackPos.store (0);

    if (playbackData != nullptr)
        retired.push_back (playbackData);
    playbackData = std::move (data);
    playbackPtr.store (playbackData.get());
    playbackOn.store (true);
    return true;
}

void Recorder::stopPlayback()
{
    playbackOn.store (false);
    playbackPtr.store (nullptr);
}

double Recorder::getPlaybackSeconds() const
{
    return (double) playbackPos.load() / playbackRate;
}

void Recorder::releaseRetired()
{
    auto* inUse = playbackInUse.load();
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [inUse] (const std::shared_ptr<juce::AudioBuffer<float>>& b) { return b.get() != inUse; }),
                   retired.end());
    if (! playbackOn.load() && playbackData != nullptr && inUse != playbackData.get())
        playbackData = nullptr;
}

// ---- 音频线程 --------------------------------------------------------------------
bool Recorder::replaceInput (float* mono, int n) noexcept
{
    auto* buf = playbackOn.load() ? playbackPtr.load() : nullptr;
    playbackInUse.store (buf);
    if (buf == nullptr)
        return false;

    const int64_t pos = playbackPos.load();
    const int64_t len = buf->getNumSamples();
    const int avail = (int) juce::jlimit ((int64_t) 0, (int64_t) n, len - pos);
    if (avail > 0)
        juce::FloatVectorOperations::copy (mono, buf->getReadPointer (0, (int) pos), avail);
    if (avail < n)
        juce::FloatVectorOperations::clear (mono + avail, n - avail);

    // 录音里谱子是从这里开始放的：让谱子从同一拍开始
    if (playbackSync.load() && pos <= playbackSyncSample.load() && playbackSyncSample.load() < pos + n)
    {
        playbackSync.store (false);
        player.seek (playbackSyncBeat.load());
        player.startFromAudioThread();
    }

    playbackPos.store (pos + n);
    if (pos + n >= len)
    {
        playbackOn.store (false);
        player.setPlayingFlag (false);
    }
    return true;
}

void Recorder::capture (const float* mono, int n, bool tabPlaying, double tabBeatBeforeRender) noexcept
{
    if (! recording.load (std::memory_order_relaxed))
    {
        lastTabPlaying = tabPlaying;
        return;
    }

    if (tabPlaying && ! lastTabPlaying && ! syncCaptured.load())
    {
        syncSample.store (samplesRecorded.load());
        syncBeat.store (tabBeatBeforeRender);
        syncCaptured.store (true);
    }
    lastTabPlaying = tabPlaying;

    const juce::ScopedLock sl (writerLock);
    if (auto* w = activeWriter.load())
    {
        const float* chans[] = { mono };
        w->write (chans, n);
        samplesRecorded.fetch_add (n);
    }
}
