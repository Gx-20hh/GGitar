#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "practice/TabPlayer.h"
#include "practice/TrainingPatterns.h"
#include "Text.h"

class FretboardPanel : public juce::Component, private juce::Timer
{
public:
    explicit FretboardPanel (TabPlayer& p) : player (p)
    {
        mode.addItem (T ("跟随当前谱面"), 1);
        mode.addItem (T ("大调音阶"), 2); mode.addItem (T ("自然小调音阶"), 3); mode.addItem (T ("小调五声音阶"), 4);
        mode.addItem (T ("大三和弦"), 5); mode.addItem (T ("小三和弦"), 6); mode.setSelectedId (1);
        const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i) root.addItem (names[i], i + 1);
        root.setSelectedId (1); mode.onChange = root.onChange = [this] { repaint(); refresh(); };
        intervals.setButtonText (T ("显示音程")); intervals.onClick = [this] { repaint(); };
        practise.setButtonText (T ("生成跟弹练习")); practise.onClick = [this]
        { if (mode.getSelectedId() > 1 && onPractice) onPractice (TrainingPatterns::create (root.getSelectedId() - 1, mode.getSelectedId() - 1, tempo.getValue())); };
        tempo.setRange (30, 240, 1); tempo.setValue (80); tempo.setTextValueSuffix (" BPM"); tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 75, 24);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &mode, &root, &intervals, &practise, &tempo, &info }) addAndMakeVisible (c);
        refresh(); startTimerHz (20);
    }
    ~FretboardPanel() override { stopTimer(); }
    std::function<void (std::shared_ptr<Song>)> onPractice;
    void resized() override
    {
        auto a = getLocalBounds().reduced (12); auto top = a.removeFromTop (30);
        mode.setBounds (top.removeFromLeft (160)); top.removeFromLeft (8); root.setBounds (top.removeFromLeft (70));
        intervals.setBounds (top.removeFromLeft (110)); practise.setBounds (top.removeFromRight (140)); tempo.setBounds (top.reduced (6, 0));
        info.setBounds (a.removeFromBottom (32)); board = a.reduced (30, 30);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b2025));
        std::vector<int> tuning { 64, 59, 55, 50, 45, 40 };
        const SongTrack* track = nullptr; int capo = 0;
        const auto song = player.getSong(); const int t = player.getTrack();
        bool follow = mode.getSelectedId() == 1;
        if (follow && song && juce::isPositiveAndBelow (t, (int) song->tracks.size()))
        { track = &song->tracks[(size_t) t]; tuning = track->tuning; capo = track->capo; }
        if (tuning.empty() || (follow && (! track || track->percussion)))
        { g.setColour (juce::Colours::grey); g.drawText (T ("请选择有弦品信息的吉他或贝斯音轨"), board, juce::Justification::centred); return; }
        const int frets = follow ? 24 : 12;
        const float dx = board.getWidth() / (float) (frets + 1), dy = board.getHeight() / (float) juce::jmax (1, (int) tuning.size() - 1);
        auto xAt = [&] (int fret) { return board.getX() + dx * (fret + 0.5f); };
        auto yAt = [&] (int string) { return board.getY() + dy * string; };
        g.setFont (11.0f); g.setColour (juce::Colour (0xff71808c));
        for (int fret = 0; fret <= frets; ++fret)
        {
            float x = xAt (fret); g.drawLine (x + dx * 0.5f, (float) board.getY(), x + dx * 0.5f, (float) board.getBottom(), fret == 0 ? 3.0f : 1.0f);
            g.drawText (juce::String (fret), (int) x - 12, board.getBottom() + 8, 24, 18, juce::Justification::centred);
        }
        for (int string = 0; string < (int) tuning.size(); ++string)
        { float y = yAt (string); g.drawHorizontalLine ((int) y, (float) board.getX(), (float) board.getRight()); g.drawText (juce::String (string + 1), board.getX() - 24, (int) y - 9, 20, 18, juce::Justification::centred); }
        const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const char* degrees[] { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
        auto dot = [&] (int string, int fret, int midi, bool tonic)
        {
            if (string < 0 || string >= (int) tuning.size() || fret < 0 || fret > frets) return;
            float size = juce::jmin (26.0f, dx - 2.0f); auto rect = juce::Rectangle<float> (size, size).withCentre ({ xAt (fret), yAt (string) });
            g.setColour (tonic ? juce::Colour (0xffe8ad61) : juce::Colour (0xff5dcaa5)); g.fillEllipse (rect);
            g.setColour (juce::Colour (0xff101820));
            int interval = ((midi - (root.getSelectedId() - 1)) % 12 + 12) % 12;
            g.drawText (intervals.getToggleState() ? degrees[interval] : names[(midi % 12 + 12) % 12], rect.toNearestInt(), juce::Justification::centred);
        };
        if (follow && track)
        {
            const double beat = player.getPosition();
            for (const auto& note : track->notes)
            {
                if (note.start > beat + 0.05) break;
                if (! note.dead && note.start + juce::jmax (0.15, note.length) > beat) dot (note.string, note.fret, note.midi, false);
            }
        }
        else
        {
            auto steps = TrainingPatterns::intervals (mode.getSelectedId() - 1);
            for (int string = 0; string < (int) tuning.size(); ++string)
                for (int fret = 0; fret <= frets; ++fret)
                {
                    int midi = tuning[(size_t) string] + capo + fret;
                    int pc = (midi - root.getSelectedId() + 1 + 120) % 12;
                    if (std::find (steps.begin(), steps.end(), pc) != steps.end()) dot (string, fret, midi, pc == 0);
                }
        }
    }
private:
    TabPlayer& player;
    juce::ComboBox mode, root;
    juce::ToggleButton intervals;
    juce::TextButton practise;
    juce::Slider tempo;
    juce::Label info;
    juce::Rectangle<int> board;
    void refresh()
    {
        practise.setEnabled (mode.getSelectedId() > 1);
        info.setText (mode.getSelectedId() == 1 ? T ("显示谱面目标弦品（相对变调夹），不代表识别了实际手指位置。上方为最细弦。")
                                               : T ("标准调弦 · 橙色为根音 · 和弦视图显示组成音，生成练习使用可演奏的 E 型指法。"), juce::dontSendNotification);
    }
    void timerCallback() override { if (isShowing()) repaint(); }
};
