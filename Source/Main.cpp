#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"
#include "AppInfo.h"
#include "Text.h"

namespace
{
    class HostLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        HostLookAndFeel() : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::getDarkColourScheme())
        {
           #if JUCE_WINDOWS
            // 微软雅黑，保证中文显示正常
            setDefaultSansSerifTypefaceName ("Microsoft YaHei UI");
           #endif
            setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff151515));
            setColour (juce::Slider::thumbColourId, juce::Colour (0xff5dcaa5));
            setColour (juce::Slider::trackColourId, juce::Colour (0xff1d9e75));
        }
    };
}

class LianqinApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (T (AppInfo::name) + T ("  ·  ") + T (AppInfo::author));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, juce::Colour (0xff151515), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (1100, 720, 2400, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    HostLookAndFeel lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (LianqinApplication)
