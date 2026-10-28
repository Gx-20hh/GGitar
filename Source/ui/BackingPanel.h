#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include "practice/TabPlayer.h"
#include "Text.h"
#include <future>
#include <chrono>

class BackingPanel : public juce::Component, private juce::Timer
{
public:
    BackingPanel (TabPlayer& p, juce::PropertiesFile& s) : player (p), settings (s)
    {
        load.setButtonText (T ("导入 WAV…")); load.onClick = [this] { choose(); };
        clear.setButtonText (T ("移除伴奏")); clear.onClick = [this] { ++generation; player.backing.setClip ({}); file = juce::File(); pendingFile.clear(); removeSaved(); repaint(); };
        mute.setButtonText (T ("静音原曲")); mute.onClick = [this] { player.backing.setMuted (mute.getToggleState()); save(); };
        offset.setRange (-600.0, 600.0, 0.001); offset.setTextValueSuffix (T (" 秒偏移"));
        ratio.setRange (0.5, 2.0, 0.0001); ratio.setTextValueSuffix (T (" 对齐比例")); ratio.setValue (1.0);
        volume.setRange (-60.0, 6.0, 0.5); volume.setTextValueSuffix (" dB"); volume.setValue (-6.0);
        for (auto* slider : { &offset, &ratio, &volume }) slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 140, 24);
        offset.onValueChange = ratio.onValueChange = [this] { player.backing.setAlignment (offset.getValue(), ratio.getValue()); save(); repaint(); };
        volume.onValueChange = [this] { player.backing.setGainDb ((float) volume.getValue()); save(); };
        anchor.setButtonText (T ("对齐第一点")); anchor.onClick = [this] { align (false); };
        second.setButtonText (T ("对齐第二点")); second.onClick = [this] { align (true); };
        reset.setButtonText (T ("重置对齐")); reset.onClick = [this] { anchorSong = -1.0; ratio.setValue (1.0, juce::dontSendNotification); offset.setValue (0.0, juce::dontSendNotification); player.backing.setAlignment (0, 1); save(); repaint(); };
        for (juce::Component* c : std::initializer_list<juce::Component*> { &load, &clear, &mute, &offset, &ratio, &volume, &anchor, &second, &reset, &status }) addAndMakeVisible (c);
        status.setText (T ("先打开谱子，再导入原曲 WAV。变速为随速变调；等待停顿时原曲静音。"), juce::dontSendNotification);
        startTimerHz (10);
    }
    ~BackingPanel() override { stopTimer(); if (task.valid()) task.wait(); }
    void restoreMute() { player.backing.setMuted (mute.getToggleState()); }
    void songChanged (const juce::String& path)
    {
        ++generation; key = path; file = juce::File(); anchorSong = -1.0; selectedSeconds = 0.0;
        player.backing.setClip ({});
        auto data = juce::JSON::parse (settings.getValue ("backingTracks"));
        auto entry = key.isNotEmpty() && data.isObject() ? data.getDynamicObject()->getProperty (key) : juce::var();
        offset.setValue (entry.isObject() ? (double) entry["offset"] : 0.0, juce::dontSendNotification);
        ratio.setValue (entry.isObject() ? juce::jlimit (0.5, 2.0, (double) entry["ratio"]) : 1.0, juce::dontSendNotification);
        volume.setValue (entry.isObject() ? (double) entry["db"] : -6.0, juce::dontSendNotification);
        mute.setToggleState (entry.isObject() && (bool) entry["mute"], juce::dontSendNotification);
        player.backing.setAlignment (offset.getValue(), ratio.getValue());
        player.backing.setGainDb ((float) volume.getValue()); player.backing.setMuted (mute.getToggleState());
        pendingFile = entry["file"].toString();
        status.setText (T ("点击波形选择音频点，再将谱面定位到对应位置进行对齐。变速会随之变调。"), juce::dontSendNotification);
        if (pendingFile.isNotEmpty() && ! juce::File (pendingFile).existsAsFile())
        { status.setText (T ("伴奏文件已移动或删除，请重新导入。"), juce::dontSendNotification); pendingFile.clear(); }
        repaint();
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced (12); auto top = a.removeFromTop (30);
        for (auto* b : { &load, &clear, &anchor, &second, &reset }) b->setBounds (top.removeFromLeft (120).reduced (2, 0));
        mute.setBounds (top);
        status.setBounds (a.removeFromBottom (32));
        auto row = a.removeFromBottom (32); int w = row.getWidth() / 3;
        offset.setBounds (row.removeFromLeft (w)); ratio.setBounds (row.removeFromLeft (w)); volume.setBounds (row);
        wave = a.reduced (0, 12);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b2025)); g.setColour (juce::Colour (0xff111519)); g.fillRoundedRectangle (wave.toFloat(), 6);
        const auto clip = player.backing.getClip();
        if (! clip || clip->peaks.empty())
        {
            g.setColour (juce::Colours::grey); g.drawText (T ("原曲波形 · WAV"), wave, juce::Justification::centred); return;
        }
        g.setColour (juce::Colour (0xff5dcaa5));
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const auto i = (size_t) x * clip->peaks.size() / (size_t) juce::jmax (1, wave.getWidth());
            float h = juce::jmin (1.0f, clip->peaks[i]) * (wave.getHeight() - 40) * 0.45f;
            g.drawVerticalLine (wave.getX() + x, (float) wave.getCentreY() - h, (float) wave.getCentreY() + h);
        }
        auto line = [&] (double sec, juce::Colour colour)
        {
            if (sec >= 0 && sec <= clip->duration()) { g.setColour (colour); int x = wave.getX() + juce::roundToInt (sec / clip->duration() * wave.getWidth()); g.drawVerticalLine (x, (float) wave.getY(), (float) wave.getBottom()); }
        };
        line (selectedSeconds, juce::Colours::orange);
        if (auto song = player.getSong()) line (player.backing.audioSeconds (*song, player.getPosition()), juce::Colours::white);
        g.setColour (juce::Colours::white);
        g.drawText (clip->file.getFileName() + T (" · 选中 ") + juce::String (selectedSeconds, 3) + " s", wave.withHeight (24).reduced (6, 0), juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (wave.contains (e.getPosition())) if (auto clip = player.backing.getClip())
        { selectedSeconds = juce::jlimit (0.0, 1.0, (e.x - wave.getX()) / (double) juce::jmax (1, wave.getWidth())) * clip->duration(); repaint(); }
    }
private:
    TabPlayer& player; juce::PropertiesFile& settings;
    juce::TextButton load, clear, anchor, second, reset;
    juce::ToggleButton mute;
    juce::Slider offset, ratio, volume;
    juce::Label status;
    juce::Rectangle<int> wave;
    juce::String key, pendingFile;
    juce::File file;
    double selectedSeconds = 0.0, anchorSong = -1.0, anchorAudio = 0.0;
    unsigned generation = 0, loadingGeneration = 0;
    std::future<BackingTrack::LoadResult> task;
    std::unique_ptr<juce::FileChooser> chooser;
    void removeSaved()
    {
        auto all = juce::JSON::parse (settings.getValue ("backingTracks"));
        if (! key.isEmpty() && all.isObject())
        {
            all.getDynamicObject()->removeProperty (key);
            settings.setValue ("backingTracks", juce::JSON::toString (all)); settings.saveIfNeeded();
        }
    }
    void save()
    {
        if (key.isEmpty() || file == juce::File()) return;
        auto all = juce::JSON::parse (settings.getValue ("backingTracks"));
        if (! all.isObject()) all = juce::var (new juce::DynamicObject());
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("file", file.getFullPathName()); entry->setProperty ("offset", offset.getValue());
        entry->setProperty ("ratio", ratio.getValue()); entry->setProperty ("db", volume.getValue()); entry->setProperty ("mute", mute.getToggleState());
        all.getDynamicObject()->setProperty (key, juce::var (entry));
        settings.setValue ("backingTracks", juce::JSON::toString (all)); settings.saveIfNeeded();
    }
    void choose()
    {
        if (! player.getSong() || task.valid()) return;
        chooser = std::make_unique<juce::FileChooser> (T ("导入原曲 WAV"), juce::File(), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<BackingPanel> (this)] (const juce::FileChooser& fc)
            { if (safe && fc.getResult().existsAsFile()) { safe->pendingFile = fc.getResult().getFullPathName(); safe->anchorSong = -1; } });
    }
    void align (bool isSecond)
    {
        const auto song = player.getSong(); if (! song || ! player.backing.getClip()) return;
        const double time = song->beatToSeconds (player.getPosition());
        if (! isSecond)
        {
            anchorSong = time; anchorAudio = selectedSeconds;
            offset.setValue (selectedSeconds - time * ratio.getValue());
            status.setText (T ("第一点已对齐。可跳到更后的小节，选中对应波形，再对齐第二点。"), juce::dontSendNotification);
        }
        else
        {
            const double dt = time - anchorSong;
            const double value = dt > 0.1 ? (selectedSeconds - anchorAudio) / dt : 0.0;
            if (anchorSong < 0 || value < 0.5 || value > 2.0)
            { status.setText (T ("先设置第一点；第二点须更靠后，对齐比例范围 0.5–2.0。"), juce::dontSendNotification); return; }
            ratio.setValue (value, juce::dontSendNotification);
            offset.setValue (anchorAudio - anchorSong * value, juce::dontSendNotification);
            player.backing.setAlignment (offset.getValue(), ratio.getValue()); save();
            status.setText (T ("两点对齐完成。播放、暂停、跳转与 A–B 循环共用谱面时间轴。"), juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        player.backing.collect();
        if (task.valid() && task.wait_for (std::chrono::seconds (0)) == std::future_status::ready)
        {
            try
            {
                auto result = task.get();
                if (generation == loadingGeneration)
                {
                    if (result.clip) { file = result.clip->file; player.backing.setClip (result.clip); save(); status.setText (T ("已加载原曲。橙线为选中音频点，白线跟随谱面；变速会随之变调。"), juce::dontSendNotification); }
                    else status.setText (result.error, juce::dontSendNotification);
                }
            }
            catch (...) { status.setText (T ("音频加载失败，请使用较短的 WAV 文件。"), juce::dontSendNotification); }
        }
        if (! task.valid() && pendingFile.isNotEmpty())
        {
            auto next = juce::File (pendingFile); pendingFile.clear(); loadingGeneration = generation;
            status.setText (T ("正在后台解码 WAV…"), juce::dontSendNotification);
            task = std::async (std::launch::async, [next] { return BackingTrack::load (next); });
        }
        load.setEnabled (player.getSong() != nullptr && ! task.valid());
        if (isShowing()) repaint();
    }
};
