#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "library/SongLibrary.h"
#include "Text.h"
#include <future>
#include <chrono>

class LibraryPanel : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit LibraryPanel (SongLibrary& m) : model (m), list ("Library", this)
    {
        search.setTextToShowWhenEmpty (T ("搜索曲名、歌手、标签或路径"), juce::Colours::grey);
        search.onTextChange = [this] { refresh(); };
        onlyFav.setButtonText (T ("仅收藏")); onlyFav.onClick = [this] { refresh(); };
        scan.setButtonText (T ("扫描文件夹…")); scan.onClick = [this] { chooseFolder(); };
        open.setButtonText (T ("打开练习")); open.onClick = [this] { openSelected(); };
        favourite.setButtonText (T ("收藏 / 取消")); favourite.onClick = [this]
        { if (auto* e = selected()) { e->favourite = ! e->favourite; model.save(); refresh(); } };
        save.setButtonText (T ("保存信息")); save.onClick = [this]
        { if (auto* e = selected()) { e->title = title.getText().trim(); e->artist = artist.getText().trim(); e->tags = tags.getText().trim(); model.save(); refresh(); } };
        remove.setButtonText (T ("移出曲库")); remove.onClick = [this]
        {
            if (auto* e = selected())
            {
                auto path = e->path;
                std::erase_if (model.entries, [&path] (const auto& row) { return row.path == path; });
                model.save(); refresh();
            }
        };
        remove.setTooltip (T ("只移除索引，不删除磁盘上的谱子"));
        title.setTextToShowWhenEmpty (T ("曲名"), juce::Colours::grey);
        artist.setTextToShowWhenEmpty (T ("歌手"), juce::Colours::grey);
        tags.setTextToShowWhenEmpty (T ("标签，例如：摇滚 练习中"), juce::Colours::grey);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &search, &onlyFav, &scan, &list, &open, &favourite, &save, &remove, &title, &artist, &tags, &status }) addAndMakeVisible (c);
        list.setRowHeight (48); refresh(); startTimerHz (5);
    }
    ~LibraryPanel() override { stopTimer(); cancel.store (true); if (scanTask.valid()) scanTask.wait(); list.setModel (nullptr); }
    std::function<void (const juce::File&)> onOpen;
    void refresh()
    {
        visible.clear();
        for (size_t i = 0; i < model.entries.size(); ++i)
            if (SongLibrary::matches (model.entries[i], search.getText(), onlyFav.getToggleState())) visible.push_back (i);
        list.deselectAllRows(); list.updateContent(); list.repaint(); selectedRowsChanged (-1);
        if (! scanTask.valid()) status.setText (juce::String ((int) visible.size()) + T (" 首 · 双击打开 · 移出曲库不会删除原文件"), juce::dontSendNotification);
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1b2025)); }
    void resized() override
    {
        auto a = getLocalBounds().reduced (10); auto row = a.removeFromTop (30);
        scan.setBounds (row.removeFromRight (140)); onlyFav.setBounds (row.removeFromRight (100)); search.setBounds (row.reduced (0, 1));
        status.setBounds (a.removeFromBottom (26)); auto buttons = a.removeFromBottom (32);
        for (auto* b : { &open, &favourite, &save, &remove }) { b->setBounds (buttons.removeFromLeft (125).reduced (3, 0)); }
        auto edits = a.removeFromBottom (34); int w = edits.getWidth() / 3;
        title.setBounds (edits.removeFromLeft (w).reduced (3)); artist.setBounds (edits.removeFromLeft (w).reduced (3)); tags.setBounds (edits.reduced (3));
        list.setBounds (a.reduced (0, 6));
    }
private:
    SongLibrary& model;
    juce::TextEditor search, title, artist, tags;
    juce::ToggleButton onlyFav;
    juce::TextButton scan, open, favourite, save, remove;
    juce::Label status;
    juce::ListBox list;
    std::vector<size_t> visible;
    std::unique_ptr<juce::FileChooser> chooser;
    struct ScanResult { std::vector<SongLibrary::Entry> entries; int failed = 0; };
    std::future<ScanResult> scanTask;
    std::atomic<bool> cancel { false };
    SongLibrary::Entry* selected()
    {
        int row = list.getSelectedRow();
        return juce::isPositiveAndBelow (row, (int) visible.size()) ? &model.entries[visible[(size_t) row]] : nullptr;
    }
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selectedRow) override
    {
        if (! juce::isPositiveAndBelow (row, (int) visible.size())) return;
        const auto& e = model.entries[visible[(size_t) row]];
        g.fillAll (juce::Colour (selectedRow ? 0xff264d47 : 0xff202428));
        g.setColour (juce::Colours::white); g.setFont (15.0f);
        g.drawText ((e.favourite ? T ("★ ") : "") + e.title + "  —  " + e.artist, 10, 2, w - 20, 24, juce::Justification::centredLeft);
        g.setColour (juce::Colours::grey); g.setFont (12.0f);
        g.drawText (e.tags + "   " + e.path, 10, 25, w - 20, h - 26, juce::Justification::centredLeft);
    }
    void selectedRowsChanged (int) override
    {
        auto* e = selected();
        title.setText (e ? e->title : "", false); artist.setText (e ? e->artist : "", false); tags.setText (e ? e->tags : "", false);
        for (auto* b : { &open, &favourite, &save, &remove }) b->setEnabled (e != nullptr);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { openSelected(); }
    void openSelected()
    {
        if (auto* e = selected())
        {
            const juce::File f (e->path);
            if (f.existsAsFile()) { if (onOpen) onOpen (f); }
            else status.setText (T ("文件已移动或删除，请重新扫描所在文件夹。"), juce::dontSendNotification);
        }
    }
    void chooseFolder()
    {
        if (scanTask.valid()) return;
        chooser = std::make_unique<juce::FileChooser> (T ("扫描 GP 谱库（包含子文件夹）"));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<LibraryPanel> (this)] (const juce::FileChooser& fc)
            {
                if (! safe || ! fc.getResult().isDirectory()) return;
                auto folder = fc.getResult();
                safe->scan.setEnabled (false); safe->status.setText (T ("正在后台扫描…"), juce::dontSendNotification);
                auto* stop = &safe->cancel;
                safe->scanTask = std::async (std::launch::async, [folder, stop]
                {
                    ScanResult result;
                    for (auto entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
                    {
                        if (stop->load()) break;
                        auto f = entry.getFile();
                        if (! f.hasFileExtension ("gp3;gp4;gp5;gpx;gp")) continue;
                        juce::String error;
                        if (auto song = loadSongFile (f, error))
                            result.entries.push_back ({ f.getFullPathName(), song->title.isNotEmpty() ? song->title : f.getFileNameWithoutExtension(), song->artist, {}, false, song->defaultTrack(), 100.0 });
                        else ++result.failed;
                    }
                    return result;
                });
            });
    }
    void timerCallback() override
    {
        if (scanTask.valid() && scanTask.wait_for (std::chrono::seconds (0)) == std::future_status::ready)
        {
            try { auto result = scanTask.get(); model.merge (result.entries); refresh(); status.setText (T ("扫描完成：识别 ") + juce::String ((int) result.entries.size()) + T (" 首，跳过损坏/不支持文件 ") + juce::String (result.failed), juce::dontSendNotification); }
            catch (...) { status.setText (T ("扫描失败，请缩小文件夹范围后重试。"), juce::dontSendNotification); }
            scan.setEnabled (true);
        }
    }
};
