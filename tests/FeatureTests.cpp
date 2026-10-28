#include <juce_gui_basics/juce_gui_basics.h>
#include "library/SongLibrary.h"
#include "practice/TrainingPatterns.h"
#include "practice/BackingTrack.h"
#include "practice/TabPlayer.h"
#include "drums/DrumMachine.h"
#include "ui/LibraryPanel.h"
#include "ui/BackingPanel.h"
#include "ui/FretboardPanel.h"
#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    struct TestLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Typeface::Ptr face;
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
        { return face ? face : juce::LookAndFeel_V4::getTypefaceForFont (font); }
        ~TestLookAndFeel() override { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }
    } look;
    const auto fontPath = juce::SystemStats::getEnvironmentVariable ("GGITAR_TEST_FONT", {});
    if (fontPath.isNotEmpty())
    {
        juce::MemoryBlock fontData;
        if (juce::File (fontPath).loadFileAsData (fontData))
            look.face = juce::Typeface::createSystemTypefaceFor (fontData.getData(), fontData.getSize());
    }
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    int failures = 0;
    auto check = [&] (bool ok, const char* name) { std::cout << (ok ? "PASS " : "FAIL ") << name << '\n'; if (! ok) ++failures; };
    auto folder = argc > 1 ? juce::File (argv[1]) : juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("GGitar-features", "", false);
    folder.createDirectory();
    auto settingsFile = folder.getChildFile ("features.settings"); settingsFile.deleteFile();
    juce::PropertiesFile::Options options; options.applicationName = "GGitarFeatureTests";
    juce::PropertiesFile settings (settingsFile, options);
    SongLibrary library (settings);
    library.merge ({ { "/test/a.gp5", "Alpha", "Artist", "rock", true, 2, 75.0 }, { "/test/b.gp5", "Beta", "Other", "blues", false, 0, 100.0 } });
    library.merge ({ { "/test/a.gp5", "Overwrite", "", "", false, 0, 100.0 } });
    juce::PropertiesFile reloadedSettings (settingsFile, options);
    SongLibrary restored (reloadedSettings);
    check (restored.entries.size() == 2 && restored.find ("/test/a.gp5")->title == "Alpha" && restored.find ("/test/a.gp5")->track == 2 && restored.find ("/test/a.gp5")->speed == 75.0, "library round-trip and rescan preserves user metadata");
    check (SongLibrary::matches (*restored.find ("/test/a.gp5"), "ROCK", true) && ! SongLibrary::matches (*restored.find ("/test/b.gp5"), "blues", true), "search is case insensitive; favourites filter");
    bool valid = true;
    for (int root = 0; root < 12; ++root) for (int kind = 1; kind <= 5; ++kind)
    {
        auto song = TrainingPatterns::create (root, kind, 80);
        auto allowed = TrainingPatterns::intervals (kind);
        for (const auto& n : song->tracks[0].notes)
        {
            int degree = (n.midi - root + 120) % 12;
            valid &= n.string >= 0 && n.string < 6 && n.fret >= 0 && n.fret <= 24;
            valid &= n.midi == song->tracks[0].tuning[(size_t) n.string] + n.fret;
            valid &= std::find (allowed.begin(), allowed.end(), degree) != allowed.end();
            valid &= n.start + n.length <= song->lengthBeats;
        }
    }
    check (valid, "all 60 training patterns have playable frets, correct pitch classes and durations");
    auto song = TrainingPatterns::create (0, 1, 120);
    BackingTrack backing;
    auto clip = std::make_shared<BackingTrack::Clip>(); clip->sampleRate = 8; clip->audio.setSize (2, 128); clip->peaks.assign (1000, 0.5f);
    for (int i = 0; i < 128; ++i) { clip->audio.setSample (0, i, (float) i / 128); clip->audio.setSample (1, i, -(float) i / 128); }
    backing.setClip (clip); backing.setGainDb (0);
    float l[8] {}, r[8] {};
    backing.render (l, r, 8, song.get(), true, 0, 0.25, -1, 0);
    check (std::abs (l[7] - 7.0f / 128) < 1e-6 && r[7] == -l[7], "backing stereo sampling follows score time and source sample rate");
    std::fill_n (l, 8, 0); std::fill_n (r, 8, 0);
    backing.render (l, r, 8, song.get(), true, 1, 0.25, 4, 0);
    check (l[0] == 4.0f / 128 && l[3] == 7.0f / 128 && l[4] == 0 && l[7] == 3.0f / 128, "A-B wrap renders both sides at the exact sample boundary");
    std::fill_n (l, 8, 0); std::fill_n (r, 8, 0);
    backing.setAlignment (-0.5, 2);
    backing.render (l, r, 8, song.get(), true, 0, 0.25, -1, 0);
    check (l[0] == 0 && l[1] == 0 && l[2] == 0 && l[3] == 2.0f / 128, "negative offset is silent before audio starts; two-point ratio applied");
    std::fill_n (l, 8, 0); std::fill_n (r, 8, 0);
    backing.render (l, r, 8, song.get(), false, 0, 0.25, -1, 0);
    backing.render (l, r, 8, song.get(), true, 0, 0, -1, 0);
    check (std::all_of (l, l + 8, [] (float v) { return v == 0; }), "pause and wait gate produce silence");
    backing.setMuted (true); backing.render (l, r, 8, song.get(), true, 4, 0.25, -1, 0);
    check (l[7] == 0, "mute produces silence");
    backing.setMuted (false); backing.setAlignment (0, 1);
    backing.render (l, r, 8, song.get(), true, song->lengthBeats, 0.25, -1, 0);
    check (l[7] == 0, "score end produces silence");
    backing.setClip ({}); backing.collect();
    check (! backing.getClip(), "unload removes clip");
    check (! BackingTrack::load (folder.getChildFile ("missing.wav")).clip, "missing WAV reports failure");
    auto wav = folder.getChildFile ("mono.wav"); wav.deleteFile();
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (wav.createOutputStream().release(), 8000, 1, 16, {}, 0));
        juce::AudioBuffer<float> data (1, 100);
        for (int i = 0; i < 100; ++i) data.setSample (0, i, 0.25f);
        check (writer && writer->writeFromAudioSampleBuffer (data, 0, 100), "write mono WAV fixture");
    }
    auto decoded = BackingTrack::load (wav);
    check (decoded.clip && decoded.clip->audio.getNumChannels() == 2 && std::abs (decoded.clip->audio.getSample (0, 20) - decoded.clip->audio.getSample (1, 20)) < 1e-6 && decoded.clip->peaks.size() == 1000, "mono WAV duplicates to stereo and creates waveform");

    clip->sampleRate = 48000;
    TabPlayer player; player.setSong (song); player.prepare (48000); player.setCountIn (false); player.setDemoOn (false); player.setPlaying (true);
    player.backing.setClip (clip); player.backing.setGainDb (0); player.setLoop (0, 8.0 / 48000.0, true);
    DrumMachine drums; drums.prepare (48000);
    std::fill_n (l, 8, 0); std::fill_n (r, 8, 0); player.render (l, r, 8, drums);
    check (std::abs (l[3] - 3.0f/128) < 1e-6 && std::abs (l[4]) < 1e-6, "TabPlayer integration loops original audio with transport");

    LibraryPanel libraryView (library); BackingPanel backingView (player, settings); FretboardPanel fretboard (player);
    int image = 0;
    for (juce::Component* view : std::initializer_list<juce::Component*> { &libraryView, &backingView, &fretboard })
    {
        view->setSize (760, 370);
        auto snapshot = view->createComponentSnapshot (view->getLocalBounds());
        juce::PNGImageFormat png; auto imageFile = folder.getChildFile ("feature-" + juce::String (++image) + ".png");
        imageFile.deleteFile(); auto out = imageFile.createOutputStream();
        check (out && png.writeImageToStream (snapshot, *out), "UI component snapshot at minimum working width");
    }
    std::cout << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
