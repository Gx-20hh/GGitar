#include "PresetBar.h"
#include "Text.h"
#include "fx/DefaultPresets.h"
#include "fx/BuiltinEffects.h"
#include <map>


PresetBar::PresetBar (EffectChain& c, juce::PropertiesFile& s) : chain (c), settings (s)
{
    label.setText (T ("音色预设"), juce::dontSendNotification);
    label.setFont (juce::FontOptions (14.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff9a9a9a));
    addAndMakeVisible (label);

    presetBox.setTextWhenNothingSelected (T ("（未保存的音色）"));
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != currentName)
            loadPreset (name);
    };
    addAndMakeVisible (presetBox);

    saveButton.setButtonText (T ("保存"));
    saveButton.setTooltip (T ("覆盖保存当前预设"));
    saveButton.onClick = [this]
    {
        if (currentName.isEmpty()) askForNameAndSave();
        else savePreset (currentName);
    };
    addAndMakeVisible (saveButton);

    saveAsButton.setButtonText (T ("另存为…"));
    saveAsButton.onClick = [this] { askForNameAndSave(); };
    addAndMakeVisible (saveAsButton);

    deleteButton.setButtonText (T ("删除"));
    deleteButton.onClick = [this] { deleteCurrent(); };
    addAndMakeVisible (deleteButton);

    folderButton.setButtonText (T ("打开文件夹"));
    folderButton.onClick = [] { getPresetFolder().startAsProcess(); };
    addAndMakeVisible (folderButton);

    currentName = settings.getValue ("currentPreset");
    createDefaultPresetsIfNeeded();
    refreshList();
}

juce::File PresetBar::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("LianqinHost").getChildFile (T ("音色预设"));
}

void PresetBar::createDefaultPresetsIfNeeded()
{
    auto folder = getPresetFolder();
    folder.createDirectory();
    const bool firstRun = folder.findChildFiles (juce::File::findFiles, false, "*.xml").isEmpty();
    const int seen = settings.getIntValue ("defaultPresetsVersion", firstRun ? 0 : 1);

    // 只补新版本加的预设，不覆盖、也不把用户删掉的旧预设再生成回来
    auto write = [&folder] (const char* name, const char* xml)
    {
        auto f = folder.getChildFile (T (name) + ".xml");
        if (f.existsAsFile())
            return;
        // 用到音色库里的 NAM 模型：文件不在（比如只拷了 exe）就不生成这个预设
        if (auto doc = juce::parseXML (T (xml)))
            for (auto* e : doc->getChildWithTagNameIterator ("Slot"))
                if (e->getStringAttribute ("kind") == "nam"
                    && ! EffectChain::resolvePortablePath (e->getStringAttribute ("path")).existsAsFile())
                    return;
        f.replaceWithText (T (xml));
    };

    if (seen < 1)
        for (auto& p : DefaultPresets::list)
            write (p[0], p[1]);
    if (seen < 2)
        for (auto& p : DefaultPresets::listV2)
            write (p[0], p[1]);
    if (seen < 3)
        for (auto& p : DefaultPresets::listV3)
            write (p[0], p[1]);
    if (seen < 4)
        for (auto& p : DefaultPresets::listGervana)
            write (p[0], p[1]);

    settings.setValue ("defaultPresetsVersion", DefaultPresets::version);

    // 压缩包里带的预设（程序旁边的“音色预设”文件夹）：补进来，不覆盖同名的。
    // 只在压缩包换了版本时补一次，用户自己删掉的不会每次都冒出来
    const auto bundled = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory()
                             .getChildFile (T ("音色预设"));
    if (bundled.isDirectory() && bundled != folder)
    {
        auto files = bundled.findChildFiles (juce::File::findFiles, false, "*.xml");
        juce::int64 stamp = 0;
        for (auto& f : files) stamp += f.getSize() + f.getLastModificationTime().toMilliseconds() / 1000;
        if (settings.getValue ("bundledPresetsStamp") != juce::String (stamp))
        {
            for (auto& f : files)
                if (! folder.getChildFile (f.getFileName()).existsAsFile())
                    f.copyFileTo (folder.getChildFile (f.getFileName()));
            settings.setValue ("bundledPresetsStamp", juce::String (stamp));
        }
    }
}

const juce::StringArray& PresetBar::categoryOrder()
{
    static const juce::StringArray order { "Gervana", T ("基础"), T ("经典"), T ("真实采样"), T ("先锋"), T ("后摇"),
                                           T ("迷幻"), T ("后朋克"), T ("怪异"), T ("贝斯"), T ("我的预设") };
    return order;
}

juce::String PresetBar::categoryOf (const juce::String& name)
{
    if (Gervana::isGervanaPreset (name))
        return "Gervana";
    for (auto& p : DefaultPresets::list)
        if (name == T (p[0]))
            return T ("基础");
    const auto sep = juce::String::fromUTF8 (" \xc2\xb7 ");
    if (name.contains (sep))
    {
        const auto prefix = name.upToFirstOccurrenceOf (sep, false, false).trim();
        if (categoryOrder().contains (prefix))
            return prefix;
    }
    return T ("我的预设");
}

void PresetBar::refreshList()
{
    presetBox.clear (juce::dontSendNotification);
    auto files = getPresetFolder().findChildFiles (juce::File::findFiles, false, "*.xml");
    files.sort();

    // 按分类分组：Gervana 直接列在最上面（金色字），其余每类一个子菜单
    std::map<juce::String, juce::StringArray> groups;
    for (auto& f : files)
        groups[categoryOf (f.getFileNameWithoutExtension())].add (f.getFileNameWithoutExtension());

    auto* root = presetBox.getRootMenu();
    int id = 1, selectedId = 0;
    for (auto& cat : categoryOrder())
    {
        auto it = groups.find (cat);
        if (it == groups.end() || it->second.isEmpty())
            continue;

        const bool gv = cat == "Gervana";
        juce::PopupMenu sub;
        if (gv)
            root->addSectionHeader (juce::String::fromUTF8 ("\xe2\x98\x85 Gervana"));
        for (auto& name : it->second)
        {
            juce::PopupMenu::Item item (name);
            item.itemID = id;
            if (gv) item.colour = Gervana::gold();
            (gv ? *root : sub).addItem (item);
            if (name == currentName) selectedId = id;
            ++id;
        }
        if (gv)
            root->addSeparator();
        else
            root->addSubMenu (cat + "  (" + juce::String (it->second.size()) + ")", sub);
    }
    if (selectedId > 0)
        presetBox.setSelectedId (selectedId, juce::dontSendNotification);

    // 当前是 Gervana 预设：下拉框里的字也换成金色
    presetBox.setColour (juce::ComboBox::textColourId,
                         Gervana::isGervanaPreset (currentName) ? Gervana::gold()
                                                                : getLookAndFeel().findColour (juce::Label::textColourId));
}

bool PresetBar::loadPreset (const juce::String& name)
{
    const auto file = getPresetFolder().getChildFile (name + ".xml");
    auto xml = juce::parseXML (file);
    if (xml == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, T ("读取预设失败"), file.getFullPathName());
        return false;
    }

    juce::StringArray problems;
    chain.loadFromXml (*xml, problems);
    currentName = name;
    settings.setValue ("currentPreset", currentName);
    refreshList();

    if (! problems.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                T ("部分效果器没有加载"),
                                                problems.joinIntoString ("\n"));
    return true;
}

void PresetBar::savePreset (const juce::String& name)
{
    auto xml = chain.toXml();
    if (xml->writeTo (getPresetFolder().getChildFile (name + ".xml")))
    {
        currentName = name;
        settings.setValue ("currentPreset", currentName);
        refreshList();
    }
}

void PresetBar::askForNameAndSave()
{
    nameDialog = std::make_unique<juce::AlertWindow> (T ("保存音色预设"), T ("给这个音色起个名字："), juce::MessageBoxIconType::NoIcon);
    nameDialog->addTextEditor ("name", currentName.isNotEmpty() ? currentName + T (" 副本") : T ("我的音色"));
    nameDialog->addButton (T ("保存"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton (T ("取消"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && nameDialog != nullptr)
        {
            const auto name = juce::File::createLegalFileName (nameDialog->getTextEditorContents ("name").trim());
            if (name.isNotEmpty())
                savePreset (name);
        }
        nameDialog.reset();
    }), false);
}

void PresetBar::deleteCurrent()
{
    if (currentName.isEmpty())
        return;

    const auto name = currentName;
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, T ("删除预设"),
                                        T ("确定删除“") + name + T ("”吗？"), T ("删除"), T ("取消"), this,
                                        juce::ModalCallbackFunction::create ([this, name] (int r)
    {
        if (r == 1)
        {
            getPresetFolder().getChildFile (name + ".xml").deleteFile();
            currentName = {};
            settings.setValue ("currentPreset", currentName);
            refreshList();
        }
    }));
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromLeft (76));
    folderButton.setBounds (r.removeFromRight (96).reduced (2));
    deleteButton.setBounds (r.removeFromRight (64).reduced (2));
    saveAsButton.setBounds (r.removeFromRight (84).reduced (2));
    saveButton.setBounds (r.removeFromRight (64).reduced (2));
    r.removeFromRight (8);
    presetBox.setBounds (r.reduced (0, 2));
}
