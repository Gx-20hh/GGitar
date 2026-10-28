#pragma once
#include "song/Song.h"

namespace TrainingPatterns
{
    inline std::vector<int> intervals (int kind)
    {
        switch (kind)
        {
            case 1: return { 0, 2, 4, 5, 7, 9, 11 }; // major
            case 2: return { 0, 2, 3, 5, 7, 8, 10 }; // natural minor
            case 3: return { 0, 3, 5, 7, 10 }; // minor pentatonic
            case 4: return { 0, 4, 7 }; // major chord
            case 5: return { 0, 3, 7 }; // minor chord
            default: return {};
        }
    }
    inline std::shared_ptr<Song> create (int root, int kind, double bpm)
    {
        root = ((root % 12) + 12) % 12;
        auto song = std::make_shared<Song>();
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* kinds[] { "", "大调音阶", "自然小调音阶", "小调五声音阶", "大三和弦", "小三和弦" };
        kind = juce::jlimit (1, 5, kind);
        song->title = juce::String (names[root]) + " " + juce::String::fromUTF8 (kinds[kind]);
        song->artist = "GGitar"; song->sourceFormat = "GGitar exercise";
        song->tempos.push_back ({ 0, juce::jlimit (30.0, 240.0, bpm) }); song->tempoStartSeconds.push_back (0);
        SongTrack track; track.name = song->title; track.tuning = { 64, 59, 55, 50, 45, 40 };
        if (kind >= 4)
        {
            const int fret = (root - 4 + 12) % 12;
            const int major[] { 0, 0, 1, 2, 2, 0 };
            const int minor[] { 0, 0, 0, 2, 2, 0 };
            for (int bar = 0; bar < 4; ++bar)
                for (int string = 0; string < 6; ++string)
                {
                    SongNote note; note.start = bar * 4.0; note.length = 3.0;
                    note.string = string; note.fret = fret + (kind == 4 ? major[string] : minor[string]);
                    note.midi = track.tuning[(size_t) string] + note.fret;
                    track.notes.push_back (note);
                }
            song->lengthBeats = 16.0;
        }
        else
        {
            auto steps = intervals (kind); steps.push_back (12);
            auto ascending = steps;
            for (int i = (int) ascending.size() - 2; i >= 0; --i) steps.push_back (ascending[(size_t) i]);
            double beat = 0.0;
            for (int interval : steps)
            {
                SongNote note; note.start = beat++; note.length = 0.85; note.midi = 48 + root + interval;
                int best = 100;
                for (int string = 0; string < 6; ++string)
                {
                    int fret = note.midi - track.tuning[(size_t) string];
                    if (fret >= 0 && fret <= 24 && fret < best) { best = fret; note.fret = fret; note.string = string; }
                }
                track.notes.push_back (note);
            }
            song->lengthBeats = std::ceil (beat / 4.0) * 4.0;
        }
        for (double beat = 0; beat < song->lengthBeats; beat += 4.0)
        { SongBar bar; bar.start = beat; bar.sourceIndex = (int) (beat / 4); song->bars.push_back (bar); }
        song->tracks.push_back (std::move (track));
        return song;
    }
}
