#pragma once
#include "song/Song.h"
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>

// Message-thread model. Scanning returns independent entries for merging on that thread.
class SongLibrary
{
public:
    struct Entry
    {
        juce::String path, title, artist, tags;
        bool favourite = false;
        int track = 0;
        double speed = 100.0;
    };
    explicit SongLibrary (juce::PropertiesFile& s) : settings (s)
    {
        auto json = juce::JSON::parse (settings.getValue ("songLibrary"));
        if (auto* rows = json.getArray())
            for (auto& v : *rows)
                if (v.isObject())
                {
                    Entry e { v["path"].toString(), v["title"].toString(), v["artist"].toString(), v["tags"].toString(),
                              (bool) v["favourite"], (int) v["track"], (double) v["speed"] };
                    e.speed = juce::jlimit (25.0, 150.0, e.speed);
                    if (e.path.isNotEmpty() && find (e.path) == nullptr) entries.push_back (e);
                }
    }
    Entry* find (const juce::String& path)
    {
        for (auto& e : entries) if (e.path == path) return &e;
        return nullptr;
    }
    void merge (const std::vector<Entry>& incoming)
    {
        for (auto& e : incoming) if (find (e.path) == nullptr) entries.push_back (e);
        save();
    }
    void remember (const Song& song, int track, double speed)
    {
        if (song.file == juce::File()) return;
        const auto path = song.file.getFullPathName();
        auto* e = find (path);
        if (e == nullptr)
        {
            entries.push_back ({ path, song.title, song.artist, {}, false, track, speed });
            e = &entries.back();
        }
        e->track = track; e->speed = speed;
        save();
    }
    void save()
    {
        juce::Array<juce::var> rows;
        for (auto& e : entries)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("path", e.path); o->setProperty ("title", e.title); o->setProperty ("artist", e.artist);
            o->setProperty ("tags", e.tags); o->setProperty ("favourite", e.favourite);
            o->setProperty ("track", e.track); o->setProperty ("speed", e.speed);
            rows.add (juce::var (o));
        }
        settings.setValue ("songLibrary", juce::JSON::toString (juce::var (rows)));
        settings.saveIfNeeded();
    }
    static bool matches (const Entry& e, const juce::String& query, bool favouritesOnly)
    {
        return (! favouritesOnly || e.favourite)
            && (e.title + " " + e.artist + " " + e.tags + " " + e.path).containsIgnoreCase (query.trim());
    }
    std::vector<Entry> entries;
private:
    juce::PropertiesFile& settings;
};
