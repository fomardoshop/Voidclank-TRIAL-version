#include "PluginEditor.h"
#include <optional>

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace
{
    constexpr int marginX = 24;
    constexpr int knobW = 96;
    constexpr int rowH = 132;
    constexpr int topY = 112;
    constexpr int statusH = 40;
    constexpr int meterW = 16;
    constexpr int cols = 7;
    constexpr int rows = 4;
    constexpr int sideCols = 2;          // always-visible side panel: pitch / tuner / tempo
    constexpr int winW = marginX * 2 + knobW * (cols + sideCols) + 18 + meterW;
    constexpr int winH = topY + rowH * rows + statusH;
    const juce::Colour amber { 0xffffc23d };

    struct DeviceInfo { juce::String name; int bufferSize; double roundTripMs; };

    bool isStandaloneApp()
    {
       #if JucePlugin_Build_Standalone
        return juce::StandalonePluginHolder::getInstance() != nullptr;
       #else
        return false;
       #endif
    }

    void openAudioSettings()
    {
       #if JucePlugin_Build_Standalone
        if (auto* h = juce::StandalonePluginHolder::getInstance())
            h->showAudioSettingsDialog();
       #endif
    }

    std::optional<DeviceInfo> getStandaloneDeviceInfo()
    {
       #if JucePlugin_Build_Standalone
        if (auto* h = juce::StandalonePluginHolder::getInstance())
            if (auto* dev = h->deviceManager.getCurrentAudioDevice())
            {
                const int buf = dev->getCurrentBufferSizeSamples();
                int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
                if (lat <= 0) lat = 2 * buf;
                return DeviceInfo { dev->getName(), buf, 1000.0 * lat / juce::jmax (1.0, dev->getCurrentSampleRate()) };
            }
       #endif
        return std::nullopt;
    }

    juce::Font fnt (float h, bool bold = false, float kern = 0.f)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kern));
    }
}

//==============================================================================
// ActivationPanel
//==============================================================================
ActivationPanel::ActivationPanel (MetalBassForgeAudioProcessor& p, std::function<void()> onCloseFn)
    : proc (p), onClose (std::move (onCloseFn))
{
    emailEd.setTextToShowWhenEmpty (loc::tr ("e-mail used for the purchase"), vc::dim);
    emailEd.setFont (fnt (15.f));

    codeEd.setMultiLine (true, true);
    codeEd.setReturnKeyStartsNewLine (false);
    codeEd.setTextToShowWhenEmpty (loc::tr ("paste licence code here"), vc::dim);
    codeEd.setFont (fnt (13.f));

    activateBtn.setColour (juce::TextButton::buttonOnColourId, vc::cyan);
    buyBtn.setColour (juce::TextButton::buttonOnColourId, vc::magenta);
    closeBtn.setColour (juce::TextButton::buttonOnColourId, vc::dim);

    activateBtn.onClick = [this]
    {
        juce::String err;
        if (proc.license.activate (emailEd.getText(), codeEd.getText(), err))
        {
            message = loc::tr ("Licence activated. Thank you for supporting Voidclank!");
            messageIsError = false;
            repaint();
            juce::Component::SafePointer<ActivationPanel> safe (this);
            std::function<void()> closeLater = [safe] { if (safe != nullptr) safe->closeNow(); };
            juce::Timer::callAfterDelay (1400, closeLater);
        }
        else
        {
            message = err;
            messageIsError = true;
            repaint();
        }
    };
    buyBtn.onClick = [] { juce::URL (LicenseManager::purchaseUrl).launchInDefaultBrowser(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };

    for (auto* c : std::initializer_list<juce::Component*> { &emailEd, &codeEd, &activateBtn, &buyBtn, &closeBtn })
        addAndMakeVisible (c);
}

void ActivationPanel::applyLanguage()
{
    emailEd.setTextToShowWhenEmpty (loc::tr ("e-mail used for the purchase"), vc::dim);
    codeEd.setTextToShowWhenEmpty (loc::tr ("paste licence code here"), vc::dim);
    activateBtn.setButtonText (loc::tr ("ACTIVATE"));
    buyBtn.setButtonText (loc::tr ("BUY"));
    closeBtn.setButtonText (loc::tr ("CLOSE"));
    message = {};
    repaint();
}

juce::Rectangle<int> ActivationPanel::card() const
{
    return juce::Rectangle<int> (440, 340).withCentre (getLocalBounds().getCentre());
}

void ActivationPanel::refresh()
{
    message = {};
    repaint();
}

void ActivationPanel::resized()
{
    auto c = card().reduced (24);
    c.removeFromTop (92);
    emailEd.setBounds (c.removeFromTop (30));
    c.removeFromTop (10);
    codeEd.setBounds (c.removeFromTop (84));
    c.removeFromTop (34);   // message line
    auto row = c.removeFromTop (32);
    activateBtn.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    buyBtn.setBounds (row.removeFromLeft (80));
    closeBtn.setBounds (row.removeFromRight (80));
}

void ActivationPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xdd03030a));

    auto c = card().toFloat();
    g.setColour (vc::panel);
    g.fillRoundedRectangle (c, 12.f);
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (vc::cyan.withAlpha (0.05f * (float) (4 - i)));
        g.drawRoundedRectangle (c.expanded ((float) i * 2.f), 12.f, 2.f);
    }
    g.setColour (vc::cyan.withAlpha (0.8f));
    g.drawRoundedRectangle (c, 12.f, 1.4f);

    auto inner = card().reduced (24);
    g.setColour (vc::cyan);
    g.setFont (fnt (20.f, true, 0.15f));
    g.drawText (loc::tr ("ACTIVATE VOIDCLANK"), inner.removeFromTop (28), juce::Justification::centredLeft);

    const auto st = proc.license.getState();
    juce::String info;
    if (st == LicenseManager::Licensed)
        info = loc::tr ("Licensed to ") + proc.license.getLicensee();
    else if (st == LicenseManager::Expired)
        info = loc::tr ("Your 14-day trial has ended. The plugin is muted until you activate a licence.");
    else
        info = loc::tr ("Trial: %d day(s) left. Enter your licence to unlock permanently.")
                   .replace ("%d", juce::String (proc.license.getDaysLeft()));

    g.setColour (vc::text);
    g.setFont (fnt (13.f));
    g.drawFittedText (info, inner.removeFromTop (60).withTrimmedTop (8), juce::Justification::topLeft, 3);

    if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? vc::magenta : vc::cyan);
        g.setFont (fnt (13.f, true));
        auto m = card().reduced (24);
        m.removeFromTop (92 + 30 + 10 + 84);
        g.drawFittedText (message, m.removeFromTop (34), juce::Justification::centredLeft, 2);
    }
}

//==============================================================================
// SettingsPanel
//==============================================================================
SettingsPanel::SettingsPanel (MetalBassForgeAudioProcessor& p, std::function<void()> onCloseFn,
                              std::function<void (int)> onScaleFn, std::function<void (bool)> onAnimFn)
    : proc (p), onClose (std::move (onCloseFn)), onScale (std::move (onScaleFn)), onAnim (std::move (onAnimFn))
{
    for (auto* t : { &tabAudio, &tabUi, &tabInfo })
    {
        t->setColour (juce::TextButton::buttonOnColourId, vc::cyan);
        t->setRadioGroupId (202);
        t->setClickingTogglesState (true);
        addAndMakeVisible (t);
    }
    tabAudio.setToggleState (true, juce::dontSendNotification);
    tabAudio.onClick = [this] { showTab (0); };
    tabUi.onClick    = [this] { showTab (1); };
    tabInfo.onClick  = [this] { showTab (2); };

    closeBtn.setColour (juce::TextButton::buttonOnColourId, vc::dim);
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeBtn);

    deviceBtn.setColour (juce::TextButton::buttonOnColourId, vc::cyan);
    deviceBtn.onClick = [] { openAudioSettings(); };
    addChildComponent (deviceBtn);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("quality")))
        for (int i = 0; i < ch->choices.size(); ++i)
            qualityBox.addItem (ch->choices[i], i + 1);
    addChildComponent (qualityBox);
    qualityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "quality", qualityBox);

    const int curScale = vcset::getInt ("scale", 100);
    int idx = 0;
    for (auto* b : { &scale75, &scale100, &scale125, &scale150 })
    {
        const int pct = (idx == 0 ? 75 : idx == 1 ? 100 : idx == 2 ? 125 : 150);
        b->setColour (juce::TextButton::buttonOnColourId, vc::cyan);
        b->setRadioGroupId (203);
        b->setClickingTogglesState (true);
        b->setToggleState (pct == curScale, juce::dontSendNotification);
        b->onClick = [this, pct] { vcset::setInt ("scale", pct); if (onScale) onScale (pct); };
        addChildComponent (b);
        ++idx;
    }
    animBtn.setColour (juce::TextButton::buttonOnColourId, vc::magenta);
    animBtn.setClickingTogglesState (true);
    animBtn.setToggleState (vcset::getInt ("animations", 1) != 0, juce::dontSendNotification);
    animBtn.onClick = [this]
    {
        vcset::setInt ("animations", animBtn.getToggleState() ? 1 : 0);
        if (onAnim) onAnim (animBtn.getToggleState());
    };
    addChildComponent (animBtn);

    paypalBtn.onClick = [] { juce::SystemClipboard::copyTextToClipboard ("blackydesign82@gmail.com"); };
    mailBtn.onClick   = [] { juce::SystemClipboard::copyTextToClipboard ("fomardo.shop@gmail.com"); };
    for (auto* b : { &paypalBtn, &mailBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, vc::magenta);
        addChildComponent (b);
    }
    applyLanguage();
}

juce::Rectangle<int> SettingsPanel::card() const
{
    return juce::Rectangle<int> (580, 420).withCentre (getLocalBounds().getCentre());
}

void SettingsPanel::applyLanguage()
{
    tabAudio.setButtonText (loc::tr ("AUDIO"));
    tabUi.setButtonText (loc::tr ("INTERFACE"));
    tabInfo.setButtonText (loc::tr ("INFO"));
    closeBtn.setButtonText (loc::tr ("CLOSE"));
    deviceBtn.setButtonText (loc::tr ("OPEN AUDIO DEVICE SETTINGS"));
    paypalBtn.setButtonText (loc::tr ("COPY"));
    mailBtn.setButtonText (loc::tr ("COPY"));
    animBtn.setButtonText (animBtn.getToggleState() ? "ON" : "OFF");
    repaint();
}

void SettingsPanel::showTab (int t)
{
    tab = t;
    tabAudio.setToggleState (t == 0, juce::dontSendNotification);
    tabUi.setToggleState (t == 1, juce::dontSendNotification);
    tabInfo.setToggleState (t == 2, juce::dontSendNotification);

    qualityBox.setVisible (t == 0);
    deviceBtn.setVisible (t == 0 && isStandaloneApp());
    for (auto* b : { &scale75, &scale100, &scale125, &scale150, &animBtn })
        b->setVisible (t == 1);
    paypalBtn.setVisible (t == 2);
    mailBtn.setVisible (t == 2);
    resized();
    repaint();
}

void SettingsPanel::resized()
{
    auto c = card().reduced (28);
    auto tabs = c.removeFromTop (74).removeFromBottom (34);
    const int tw = 120;
    tabAudio.setBounds (tabs.removeFromLeft (tw));
    tabs.removeFromLeft (6);
    tabUi.setBounds (tabs.removeFromLeft (tw));
    tabs.removeFromLeft (6);
    tabInfo.setBounds (tabs.removeFromLeft (tw));
    closeBtn.setBounds (card().reduced (28).removeFromBottom (32).removeFromRight (100));

    // content area starts ~ card.y + 28 + 74 + 10
    const auto content = card().reduced (28).withTrimmedTop (74 + 14).withTrimmedBottom (40);
    qualityBox.setBounds (content.getX() + 190, content.getY() + 120, 220, 28);
    deviceBtn.setBounds (content.getX(), content.getY() + 160, 330, 30);

    int x = content.getX() + 190;
    for (auto* b : { &scale75, &scale100, &scale125, &scale150 })
    {
        b->setBounds (x, content.getY() + 6, 68, 32);
        x += 72;
    }
    animBtn.setBounds (content.getX() + 190, content.getY() + 62, 80, 32);

    paypalBtn.setBounds (content.getRight() - 90, content.getY() + 128, 90, 26);
    mailBtn.setBounds   (content.getRight() - 90, content.getY() + 160, 90, 26);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xdd03030a));

    auto c = card().toFloat();
    g.setColour (vc::panel);
    g.fillRoundedRectangle (c, 12.f);
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (vc::cyan.withAlpha (0.05f * (float) (4 - i)));
        g.drawRoundedRectangle (c.expanded ((float) i * 2.f), 12.f, 2.f);
    }
    g.setColour (vc::cyan.withAlpha (0.8f));
    g.drawRoundedRectangle (c, 12.f, 1.4f);

    auto inner = card().reduced (28);
    g.setColour (vc::cyan);
    g.setFont (fnt (20.f, true, 0.15f));
    g.drawText (loc::tr ("SETTINGS"), inner.removeFromTop (30), juce::Justification::centredLeft);

    const auto content = card().reduced (28).withTrimmedTop (74 + 14).withTrimmedBottom (40);
    auto label = [&] (const juce::String& t, int y)
    {
        g.setColour (vc::dim.brighter (0.4f));
        g.setFont (fnt (12.f, true, 0.05f));
        g.drawText (t, content.getX(), content.getY() + y, 185, 24, juce::Justification::centredLeft);
    };
    auto value = [&] (const juce::String& t, int y, juce::Colour col = vc::text)
    {
        g.setColour (col);
        g.setFont (fnt (14.f, true));
        g.drawText (t, content.getX() + 190, content.getY() + y, content.getWidth() - 190, 24, juce::Justification::centredLeft);
    };

    if (tab == 0)
    {
        const double sr = proc.getCurrentSampleRate();
        const int lat = proc.getLatencySamples();
        label (loc::tr ("Sample rate"), 0);  value (juce::String (sr / 1000.0, 1) + " kHz", 0);
        label (loc::tr ("Buffer"), 32);
        juce::String bufTxt = juce::String (proc.getCurrentBlockSize()) + " smp";
        if (auto dev = getStandaloneDeviceInfo())
            bufTxt = dev->name + "  |  " + juce::String (dev->bufferSize) + " smp";
        value (bufTxt, 32);
        label (loc::tr ("Latency"), 64);
        value ("+" + juce::String (1000.0 * lat / juce::jmax (1.0, sr), 2) + " ms  (" + juce::String (lat) + " smp)", 64);
        label (loc::tr ("Latency mode"), 124);

        if (! isStandaloneApp())
        {
            g.setColour (vc::dim.brighter (0.2f));
            g.setFont (fnt (12.f));
            g.drawFittedText (loc::tr ("In a DAW the audio device and buffer size are set by the host (e.g. Reaper: Options > Preferences > Audio > Device)."),
                              content.getX(), content.getY() + 170, content.getWidth(), 50, juce::Justification::topLeft, 3, 1.0f);
        }
    }
    else if (tab == 1)
    {
        label (loc::tr ("Interface size"), 10);
        label (loc::tr ("Animations"), 66);
    }
    else
    {
        const juce::String ver = vcset::versionString();
        g.setColour (vc::text);
        g.setFont (fnt (12.f));
        g.drawFittedText (loc::tr ("Bass processor for metal: clean sub + grit split, amp models, cab / IR loader, FX and tuner."),
                          content.getX(), content.getY() - 4, content.getWidth(), 34, juce::Justification::topLeft, 2, 1.0f);

        label (loc::tr ("Version"), 40);   value ("Voidclank " + ver + "  -  Fomardo Shop", 40);

        const auto st = proc.license.getState();
        juce::String lic;
       #if VC_FRIENDS_BUILD
        lic = loc::tr ("FULL VERSION  //  TESTER");
       #else
        lic = st == LicenseManager::Licensed ? loc::tr ("LICENSED") + "  " + proc.license.getLicensee()
            : st == LicenseManager::Expired  ? loc::tr ("TRIAL EXPIRED  //  AUDIO MUTED")
                                              : loc::tr ("TRIAL") + "  " + juce::String (proc.license.getDaysLeft()) + " d";
       #endif
        label (loc::tr ("Licence"), 72);   value (lic, 72);
        label (loc::tr ("Support us (PayPal)"), 128);  value ("blackydesign82@gmail.com", 128, vc::magenta);
        label (loc::tr ("Contact"), 160);              value ("fomardo.shop@gmail.com", 160, vc::magenta);
       #if VC_FRIENDS_BUILD
        g.setColour (vc::dim.brighter (0.2f));
        g.setFont (fnt (11.f));
        g.drawFittedText (loc::tr ("Personal-use tester build - please do not copy or share."),
                          content.getX(), content.getY() + 200, content.getWidth(), 20, juce::Justification::topLeft, 1, 1.0f);
       #endif
    }
}

//==============================================================================
// NoticePanel
//==============================================================================
NoticePanel::NoticePanel (std::function<void()> onCloseFn) : onClose (std::move (onCloseFn))
{
    okBtn.setButtonText (loc::tr ("OK"));
    okBtn.setColour (juce::TextButton::buttonOnColourId, vc::cyan);
    okBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (okBtn);
}

void NoticePanel::applyLanguage()
{
    okBtn.setButtonText (loc::tr ("OK"));
    repaint();
}

juce::Rectangle<int> NoticePanel::card() const
{
    return juce::Rectangle<int> (540, 490).withCentre (getLocalBounds().getCentre());
}

void NoticePanel::resized()
{
    auto c = card().reduced (24);
    okBtn.setBounds (c.removeFromBottom (34).withSizeKeepingCentre (190, 34));
}

void NoticePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xdd03030a));

    auto c = card().toFloat();
    g.setColour (vc::panel);
    g.fillRoundedRectangle (c, 12.f);
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (vc::cyan.withAlpha (0.05f * (float) (4 - i)));
        g.drawRoundedRectangle (c.expanded ((float) i * 2.f), 12.f, 2.f);
    }
    g.setColour (vc::cyan.withAlpha (0.8f));
    g.drawRoundedRectangle (c, 12.f, 1.4f);

    auto in = card().reduced (28);
    in.removeFromBottom (44);

    g.setColour (vc::cyan);
    g.setFont (fnt (22.f, true, 0.2f));
    g.drawText ("VOIDCLANK", in.removeFromTop (30), juce::Justification::centred);
    g.setColour (vc::dim.brighter (0.5f));
    g.setFont (fnt (11.f, true, 0.2f));
    g.drawText ("v" + vcset::versionString() + "  //  FOMARDO SHOP", in.removeFromTop (16), juce::Justification::centred);
    g.setColour (vc::magenta);
    g.setFont (fnt (11.f, true, 0.2f));
    g.drawText (juce::String::fromUTF8 ("WERSJA FULL  //  FULL VERSION  //  TESTER BUILD"),
                in.removeFromTop (20), juce::Justification::centred);
    in.removeFromTop (10);

    auto block = [&] (const juce::String& lang, const juce::String& head, const juce::String& body,
                      const juce::String& support, juce::Colour col)
    {
        auto r = in.removeFromTop (150);
        g.setColour (col);
        g.setFont (fnt (11.f, true, 0.25f));
        g.drawText (lang, r.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.setFont (fnt (15.f, true));
        g.drawFittedText (head, r.removeFromTop (22), juce::Justification::centredLeft, 1);
        g.setColour (vc::text);
        g.setFont (fnt (13.f));
        g.drawFittedText (body, r.removeFromTop (40).reduced (0, 2), juce::Justification::topLeft, 3, 1.0f);
        if (support.isNotEmpty())
        {
            g.setColour (col);
            g.setFont (fnt (12.f, true));
            g.drawFittedText (support, r.reduced (0, 2), juce::Justification::topLeft, 3, 1.0f);
        }
    };

    // first block: the selected language (Polish when English is selected), second block: English
    const int sel = loc::current();
    const int first = (sel == loc::EN) ? (int) loc::PL : sel;
    auto nt1 = loc::notice (first);
    block (loc::codes()[first], juce::String::fromUTF8 (nt1.head), juce::String::fromUTF8 (nt1.body),
           juce::String::fromUTF8 (nt1.support), vc::cyan);
    g.setColour (vc::cyan.withAlpha (0.2f));
    g.drawHorizontalLine (in.getY() - 4, (float) in.getX(), (float) in.getRight());
    auto nt2 = loc::notice (loc::EN);
    block ("EN", juce::String::fromUTF8 (nt2.head), juce::String::fromUTF8 (nt2.body),
           juce::String::fromUTF8 (nt2.support), vc::magenta);
}

//==============================================================================
// Editor
//==============================================================================
MetalBassForgeAudioProcessorEditor::MetalBassForgeAudioProcessorEditor (MetalBassForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      activation (p, [this] { activation.setVisible (false); updateStatusBar(); }),
      notice ([this] { notice.setVisible (false); }),
      settings (p, [this] { settings.setVisible (false); },
                [this] (int pct) { setScaleFactor ((float) pct / 100.f); },
                [this] (bool on) { animations = on; repaint(); })
{
    animations = vcset::getInt ("animations", 1) != 0;
    setLookAndFeel (&laf);

    sections = {
        { "INPUT  /  SPLIT  /  MIX",      0, 0, 0, 4 },
        { "SOLO",                         0, 0, 5, 6 },
        { "LOW  -  CLEAN SUB",            0, 1, 0, 1 },
        { "GRIT  -  BOD <> DARKGLASS",    0, 1, 2, 6 },
        { "AMP MODEL  -  TONE STACK",     0, 2, 0, 6 },
        { "CAB  -  IR LOADER",            0, 3, 0, 4 },
        { "CLANK",                        0, 3, 5, 6 },

        { "WAH",                          1, 0, 0, 6 },
        { "DELAY",                        1, 1, 0, 4 },
        { "REVERB",                       1, 1, 5, 6 },
        { "NOISE GATE",                   1, 2, 0, 1 },
        { "FX MASTER",                    1, 2, 2, 6 },
        { "SIGNAL CHAIN",                 1, 3, 0, 6 },

        // side panel, visible on every page (page -1)
        { "PITCH  /  RETUNE",            -1, 0, 7, 8, 2 },
        { "TUNER",                       -1, 2, 7, 8 },
        { "TEMPO  /  BPM",               -1, 3, 7, 8 },

        { "CABINET  /  MICROPHONE",       2, 0, 0, 3, 4 },
        { "CAB MODEL",                    2, 0, 4, 6 },
        { "MICROPHONE",                   2, 1, 4, 6 },
        { "LEVEL",                        2, 2, 4, 6 },
        { "MIC NOTES",                    2, 3, 4, 6 },
    };

    addKnob (0, 0, "inGain",    "Input",      " dB");
    addKnob (0, 1, "tight",     "Tight",      " Hz");
    addKnob (0, 2, "xover",     "Crossover",  " Hz");
    addKnob (0, 3, "dry",       "Dry DI",     "");
    addKnob (0, 4, "outGain",   "Output",     " dB");

    addKnob (1, 0, "lowComp",   "Comp",       "");
    addKnob (1, 1, "lowLevel",  "Level",      " dB");
    addKnob (1, 2, "drive",     "Drive",      "");
    addKnob (1, 3, "character", "BOD <> Dark","");
    addKnob (1, 4, "scoop",     "Mid Scoop",  " dB");
    addKnob (1, 5, "gritLevel", "Grit Level", " dB");
    addKnob (1, 6, "fizz",      "Hi-Cut",     " Hz");

    addKnob (2, 3, "ampBass",     "Bass",     " dB");
    addKnob (2, 4, "ampMid",      "Mid",      " dB");
    addKnob (2, 5, "ampTreble",   "Treble",   " dB");
    addKnob (2, 6, "ampPresence", "Presence", " dB");

    addKnob (3, 3, "cabMix",    "Cab Mix",    "");
    addKnob (3, 4, "cabLevel",  "Cab Level",  " dB");
    addKnob (3, 5, "clank",     "Clank",      " dB");
    addKnob (3, 6, "clankFreq", "Clank Freq", " Hz");

    // solo pads
    soloLowBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    soloGritBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.b);
    soloDryBtn.setColour (juce::TextButton::buttonOnColourId, amber);
    for (auto* b : { &soloLowBtn, &soloGritBtn, &soloDryBtn })
        addAndMakeVisible (b);
    soloLowAtt  = std::make_unique<BtnAtt> (processor.apvts, "soloLow",  soloLowBtn);
    soloGritAtt = std::make_unique<BtnAtt> (processor.apvts, "soloGrit", soloGritBtn);
    soloDryAtt  = std::make_unique<BtnAtt> (processor.apvts, "soloDry",  soloDryBtn);

    // amp model
    ampDescriptions = MetalBassForgeAudioProcessor::getAmpDescriptions();
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("amp")))
        for (int i = 0; i < ch->choices.size(); ++i)
            ampBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (ampBox);
    ampAtt = std::make_unique<BoxAtt> (processor.apvts, "amp", ampBox);
    ampBox.onChange = [this] { updateAmpDescription(); };

    ampDesc.setFont (fnt (12.f));
    ampDesc.setJustificationType (juce::Justification::topLeft);
    ampDesc.setColour (juce::Label::textColourId, vc::text.withAlpha (0.8f));
    addAndMakeVisible (ampDesc);

    // cabinet
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("cabType")))
        for (int i = 0; i < ch->choices.size(); ++i)
            cabBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (cabBox);
    cabAtt = std::make_unique<BoxAtt> (processor.apvts, "cabType", cabBox);
    cabBox.onChange = [this]
    {
        if (cabBox.getSelectedId() == 6 && processor.getUserIRName().isEmpty())
            chooseIR();   // "User IR" with nothing loaded yet -> ask for a file straight away
    };

    loadIrBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    loadIrBtn.onClick = [this] { chooseIR(); };
    addAndMakeVisible (loadIrBtn);

    cabAllBtn.setButtonText ("CAB ON GRIT ONLY");
    cabAllBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.b);
    addAndMakeVisible (cabAllBtn);
    cabAllAtt = std::make_unique<BtnAtt> (processor.apvts, "cabAll", cabAllBtn);

    irLabel.setFont (fnt (11.f));
    irLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (irLabel);


   #if VC_WITH_NAM
    namOnBtn.setColour (juce::TextButton::buttonOnColourId, amber);
    loadNamBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    loadNamBtn.onClick = [this] { chooseNam(); };
    namLabel.setFont (fnt (11.f));
    namLabel.setJustificationType (juce::Justification::centredLeft);
    namAtt = std::make_unique<BtnAtt> (processor.apvts, "namOn", namOnBtn);
    for (auto* c : std::initializer_list<juce::Component*> { &namOnBtn, &loadNamBtn, &namLabel })
    {
        addAndMakeVisible (c);
        mainComps.push_back (c);
    }
   #endif

    // main-page non-knob controls
    for (auto* c : std::initializer_list<juce::Component*> { &soloLowBtn, &soloGritBtn, &soloDryBtn, &ampBox, &ampDesc,
                                                              &cabBox, &loadIrBtn, &cabAllBtn, &irLabel })
        mainComps.push_back (c);

    // ================= FX page =================
    buildPage = 1;
    addKnob (2, 0, "gateThresh",  "Threshold", " dB");
    addKnob (2, 1, "gateRelease", "Release",   " ms");

    addKnob (0, 2, "wahPos",   "Pedal", "");
    addKnob (0, 3, "wahRate",  "Rate",  " Hz");
    addKnob (0, 4, "wahDepth", "Depth", "");
    addKnob (0, 5, "wahQ",     "Q",     "");
    addKnob (0, 6, "wahMix",   "Mix",   "");

    addKnob (1, 1, "delayTime",     "Time",     " ms");
    addKnob (1, 2, "delayFeedback", "Feedback", "");
    addKnob (1, 3, "delayTone",     "Tone",     " Hz");
    addKnob (1, 4, "delayMix",      "Mix",      "");
    addKnob (1, 5, "revSize",       "Size",     "");
    addKnob (1, 6, "revMix",        "Mix",      "");

    // pitch / retune lives in the always-visible side panel
    buildPage = -1;
    addKnob (0, 7, "pitchSemi",   "Semitones", " st");
    addKnob (0, 8, "pitchFine",   "Fine",      " ct");
    addKnob (1, 7, "pitchMix",    "Mix",       "");
    buildPage = 1;

    auto setPitch = [this] (int semis)
    {
        auto setP = [this] (const char* id, float v)
        {
            if (auto* prm = processor.apvts.getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        setP ("pitchSemi", (float) semis);
        setP ("pitchFine", 0.f);
        setP ("pitchMix", 1.f);
    };
    dropDBtn.onClick = [setPitch] { setPitch (-2); };
    dropCBtn.onClick = [setPitch] { setPitch (-4); };
    octBtn.onClick   = [setPitch] { setPitch (-12); };
    resetPitchBtn.onClick = [setPitch] { setPitch (0); };
    for (auto* b : { &dropDBtn, &dropCBtn, &octBtn, &resetPitchBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
        addAndMakeVisible (b);
    }

    fxMasterBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    addAndMakeVisible (fxMasterBtn);
    fxMasterAtt = std::make_unique<BtnAtt> (processor.apvts, "fxOn", fxMasterBtn);
    wahOnBtn.setColour (juce::TextButton::buttonOnColourId, amber);
    addAndMakeVisible (wahOnBtn);
    wahOnAtt = std::make_unique<BtnAtt> (processor.apvts, "wahOn", wahOnBtn);
    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("wahMode")))
        for (int i = 0; i < ch->choices.size(); ++i)
            wahModeBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (wahModeBox);
    wahModeAtt = std::make_unique<BoxAtt> (processor.apvts, "wahMode", wahModeBox);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("delaySync")))
        for (int i = 0; i < ch->choices.size(); ++i)
            delaySyncBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (delaySyncBox);
    delaySyncAtt = std::make_unique<BoxAtt> (processor.apvts, "delaySync", delaySyncBox);
    syncLabel.setText ("SYNC", juce::dontSendNotification);
    syncLabel.setJustificationType (juce::Justification::centred);
    syncLabel.setColour (juce::Label::textColourId, vc::dim.brighter (0.4f));
    syncLabel.setFont (fnt (11.f, true, 0.12f));
    syncLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (syncLabel);

    tapBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    tapBtn.onClick = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! tapTimes.empty() && now - tapTimes.back() > 2000.0)
            tapTimes.clear();                       // pause too long: start a new tap series
        tapTimes.push_back (now);
        if (tapTimes.size() > 6)
            tapTimes.erase (tapTimes.begin());
        if (tapTimes.size() >= 2)
        {
            const double avgMs = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
            processor.manualBpm.store (juce::jlimit (30.0, 300.0, 60000.0 / avgMs));
        }
    };
    addAndMakeVisible (tapBtn);

    tunerMuteBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.b);
    addAndMakeVisible (tunerMuteBtn);
    tunerMuteAtt = std::make_unique<BtnAtt> (processor.apvts, "tunerMute", tunerMuteBtn);

    for (auto* c : std::initializer_list<juce::Component*> { &wahOnBtn, &wahModeBox, &delaySyncBox, &syncLabel, &fxMasterBtn })
        fxComps.push_back (c);
    buildPage = 0;


    // ================= CAB page =================
    buildPage = 2;
    addKnob (1, 5, "micPos",   "Position", "");
    addKnob (1, 6, "micDist",  "Distance", "");
    addKnob (2, 4, "cabMix",   "Cab Mix",  "");
    addKnob (2, 5, "cabLevel", "Cab Level", " dB");

    cabView = std::make_unique<CabView> (processor, laf.theme);
    addAndMakeVisible (*cabView);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("cabType")))
        for (int i = 0; i < ch->choices.size(); ++i)
            cabBox2.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (cabBox2);
    cabAtt2 = std::make_unique<BoxAtt> (processor.apvts, "cabType", cabBox2);
    cabBox2.onChange = [this]
    {
        if (cabBox2.getSelectedId() == 6 && processor.getUserIRName().isEmpty())
            chooseIR();
    };

    loadIrBtn2.onClick = [this] { chooseIR(); };
    addAndMakeVisible (loadIrBtn2);
    cabAllBtn2.setButtonText ("CAB ON GRIT ONLY");
    addAndMakeVisible (cabAllBtn2);
    cabAllAtt2 = std::make_unique<BtnAtt> (processor.apvts, "cabAll", cabAllBtn2);

    irLabel2.setFont (fnt (11.f));
    irLabel2.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (irLabel2);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("micType")))
        for (int i = 0; i < ch->choices.size(); ++i)
            micBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (micBox);
    micAtt = std::make_unique<BoxAtt> (processor.apvts, "micType", micBox);
    micTypeLabel.setText ("TYPE", juce::dontSendNotification);
    micTypeLabel.setJustificationType (juce::Justification::centred);
    micTypeLabel.setColour (juce::Label::textColourId, vc::dim.brighter (0.4f));
    micTypeLabel.setFont (fnt (11.f, true, 0.12f));
    micTypeLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (micTypeLabel);

    micDesc.setFont (fnt (12.f));
    micDesc.setJustificationType (juce::Justification::topLeft);
    micDesc.setColour (juce::Label::textColourId, vc::text.withAlpha (0.8f));
    addAndMakeVisible (micDesc);

    for (auto* c : std::initializer_list<juce::Component*> { cabView.get(), &cabBox2, &loadIrBtn2, &cabAllBtn2, &irLabel2,
                                                              &micBox, &micTypeLabel, &micDesc })
        cabComps.push_back (c);
    buildPage = 0;

    // page switch
    for (auto* b : { &pageMainBtn, &pageCabBtn, &pageFxBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
        b->setRadioGroupId (101);
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    pageMainBtn.setToggleState (true, juce::dontSendNotification);
    pageMainBtn.onClick = [this] { setPage (0); };
    pageFxBtn.onClick   = [this] { setPage (1); };
    pageCabBtn.onClick  = [this] { setPage (2); };

    // presets
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    prevBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    nextBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    // status bar
    statusLabel.setFont (fnt (12.f, true, 0.1f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    statusActivate.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    statusBuy.setColour (juce::TextButton::buttonOnColourId, laf.theme.b);
    statusActivate.onClick = [this] { activation.refresh(); activation.setVisible (true); activation.toFront (true); };
    statusBuy.onClick = [] { juce::URL (LicenseManager::purchaseUrl).launchInDefaultBrowser(); };
    addAndMakeVisible (statusActivate);
    addAndMakeVisible (statusBuy);

    // latency / quality + audio-device info
    infoLabel.setFont (fnt (11.f, true, 0.05f));
    infoLabel.setColour (juce::Label::textColourId, laf.theme.a.withAlpha (0.85f));
    infoLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (infoLabel);

    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("quality")))
        for (int i = 0; i < ch->choices.size(); ++i)
            qualityBox.addItem (ch->choices[i], i + 1);
    addAndMakeVisible (qualityBox);
    qualityAtt = std::make_unique<BoxAtt> (processor.apvts, "quality", qualityBox);

    audioBtn.onClick = [] { openAudioSettings(); };
    audioBtn.setVisible (isStandaloneApp());
    addChildComponent (audioBtn);
    audioBtn.setVisible (isStandaloneApp());

    // language selector (top right)
    for (int i = 0; i < loc::numLangs; ++i)
        langBox.addItem (juce::String::fromUTF8 (loc::nativeNames()[i]), i + 1);
    langBox.setSelectedId (loc::current() + 1, juce::dontSendNotification);
    langBox.setTooltip ("Language / Jezyk");
    langBox.onChange = [this]
    {
        loc::set (langBox.getSelectedId() - 1);
        applyLanguage();
    };
    addAndMakeVisible (langBox);

    gearBtn.setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    gearBtn.setTooltip ("Settings");
    gearBtn.onClick = [this] { settings.applyLanguage(); settings.setVisible (true); settings.toFront (true); };
    addAndMakeVisible (gearBtn);

    addChildComponent (activation);
    addChildComponent (settings);
    addChildComponent (notice);

    setSize (winW, winH);
    {
        const int pct = vcset::getInt ("scale", 100);
        if (pct != 100) setScaleFactor ((float) pct / 100.f);
    }
    updateStatusBar();
    updateAmpDescription();
    updateIrLabel();
    applySkin (juce::jmax (0, ampBox.getSelectedItemIndex()));
    setPage (0);
    updateDeviceInfo();

    if (processor.license.getState() == LicenseManager::Expired)
        activation.setVisible (true);

#if VC_FRIENDS_BUILD
    showNoticeForTesting();   // PL + EN thank-you / personal-use notice on every open
#endif

    for (auto* l : { &ampDesc, &irLabel, &irLabel2, &micDesc, &statusLabel, &infoLabel })
        l->setMinimumHorizontalScale (1.0f);   // consistent letter shapes everywhere (ellipsis instead of squeezing)
   #if VC_WITH_NAM
    namLabel.setMinimumHorizontalScale (1.0f);
   #endif

    applyLanguage();
    startTimerHz (30);
}

void MetalBassForgeAudioProcessorEditor::applyLanguage()
{
    for (auto& k : knobs)
        k->label.setText (loc::upper (loc::tr (k->key)), juce::dontSendNotification);

   #if VC_WITH_NAM
    loadNamBtn.setButtonText (loc::tr ("LOAD NAM..."));
    namOnBtn.setButtonText (loc::tr ("NAM ON"));
    updateNamLabel();
   #endif
    loadIrBtn.setButtonText (loc::tr ("LOAD IR..."));
    loadIrBtn2.setButtonText (loc::tr ("LOAD IR..."));
    resetPitchBtn.setButtonText (loc::tr ("RESET"));
    wahOnBtn.setButtonText (loc::tr ("WAH ON"));
    tunerMuteBtn.setButtonText (loc::tr ("MUTE OUTPUT"));
    syncLabel.setText (loc::tr ("SYNC"), juce::dontSendNotification);
    micTypeLabel.setText (loc::tr ("TYPE"), juce::dontSendNotification);
    tapBtn.setButtonText (loc::tr ("TAP"));
    pageMainBtn.setButtonText (loc::tr ("MAIN"));
    pageFxBtn.setButtonText (loc::tr ("FX"));
    pageCabBtn.setButtonText (loc::tr ("CAB"));
    audioBtn.setButtonText (loc::tr ("AUDIO"));
    statusActivate.setButtonText (loc::tr ("ACTIVATE"));
    statusBuy.setButtonText (loc::tr ("BUY"));

    activation.applyLanguage();
    notice.applyLanguage();
    settings.applyLanguage();
    updateStatusBar();
    updateSlowTexts();

    bgCache = {};   // section titles / labels are baked into the cached background
    repaint();
}

MetalBassForgeAudioProcessorEditor::~MetalBassForgeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MetalBassForgeAudioProcessorEditor::addKnob (int row, int col, const juce::String& paramId,
                                                  const juce::String& name, const juce::String& suffix)
{
    auto k = std::make_unique<Knob>();
    k->page = buildPage;
    k->row = row;
    k->col = col;

    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    k->slider.setTextValueSuffix (suffix);
    k->slider.setColour (juce::Slider::textBoxTextColourId, vc::text);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (k->slider);

    k->key = name;
    k->label.setText (loc::upper (loc::tr (name)), juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, vc::dim.brighter (0.4f));
    k->label.setFont (fnt (11.f, true, 0.12f));
    k->label.setMinimumHorizontalScale (1.0f);   // never squeeze the letters; long names are shortened in the translation table
    addAndMakeVisible (k->label);

    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k->slider);

    knobs.push_back (std::move (k));
}

void MetalBassForgeAudioProcessorEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    int id = presetBox.getSelectedId() - 1 + delta;
    id = (id % n + n) % n;
    presetBox.setSelectedId (id + 1);   // triggers onChange
}

//==============================================================================
juce::Rectangle<int> MetalBassForgeAudioProcessorEditor::sectionRect (const Section& s) const
{
    return { marginX + s.col0 * knobW, topY + s.row * rowH,
             (s.col1 - s.col0 + 1) * knobW, s.rows * rowH - 8 };
}

juce::Rectangle<int> MetalBassForgeAudioProcessorEditor::meterRect() const
{
    return { marginX + (cols + sideCols) * knobW + 18, topY + 18, meterW, rowH * rows - 26 };
}

void MetalBassForgeAudioProcessorEditor::renderBackground()
{
    bgCache = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (bgCache);
    const auto w = (float) getWidth(), h = (float) getHeight();

    g.setGradientFill (juce::ColourGradient (laf.theme.bgTop, 0.f, 0.f, laf.theme.bgBot, 0.f, h, false));
    g.fillAll();

    // glows
    g.setGradientFill (juce::ColourGradient (laf.theme.a.withAlpha (0.12f), 0.f, 0.f,
                                             juce::Colours::transparentBlack, w * 0.7f, h * 0.35f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (laf.theme.b.withAlpha (0.10f), w, h,
                                             juce::Colours::transparentBlack, w * 0.3f, h * 0.65f, true));
    g.fillAll();

    amp_art::hueShift = (page == 1 ? 0.5f : 0.f);
    amp_art::drawBackdrop (g, w, h, skinIdx < 0 ? 0 : skinIdx);
    amp_art::hueShift = 0.f;

    // grid
    g.setColour (laf.theme.a.withAlpha (0.04f));
    for (float x = 0.f; x < w; x += 24.f) g.drawVerticalLine ((int) x, 0.f, h);
    for (float y = 0.f; y < h; y += 24.f) g.drawHorizontalLine ((int) y, 0.f, w);

    // title with gradient
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (fnt (36.f, true, 0.22f), "VOIDCLANK", (float) marginX, 44.f);
        juce::Path p;
        ga.createPath (p);
        auto b = p.getBounds();
        g.setGradientFill (juce::ColourGradient (laf.theme.a, b.getX(), 0.f, laf.theme.b, b.getRight(), 0.f, false));
        g.fillPath (p);
        g.setColour (laf.theme.a.withAlpha (0.12f));
        g.strokePath (p, juce::PathStrokeType (3.f));

        g.setColour (vc::dim);
        g.setFont (fnt (11.f, true, 0.3f));
        g.drawText ("BASS ANNIHILATOR  //  v" + vcset::versionString(), (int) b.getRight() + 16, 34, 240, 14,
                    juce::Justification::centredLeft);
        g.setColour (vc::dim.withAlpha (0.8f));
        g.setFont (fnt (10.f, false, 0.2f));
        g.drawText ("FOMARDO SHOP", (int) b.getRight() + 16, 49, 240, 12, juce::Justification::centredLeft);
    }

    // header line
    g.setGradientFill (juce::ColourGradient (laf.theme.a, 0.f, 0.f, laf.theme.b, w, 0.f, false));
    g.fillRect (0.f, 62.f, w, 2.f);
    g.setColour (laf.theme.a.withAlpha (0.08f));
    g.fillRect (0.f, 64.f, w, 6.f);

    // preset label
    g.setColour (vc::dim);
    g.setFont (fnt (10.f, true, 0.25f));
    g.drawText (loc::tr ("PRESET"), marginX, 74, 60, 26, juce::Justification::centredLeft);

    // section panels
    for (auto& s : sections)
    {
        if (s.page >= 0 && s.page != page) continue;
        auto r = sectionRect (s).toFloat();
        if (s.page == 0 && s.title.startsWith ("AMP"))
        {
            amp_art::drawFace (g, r, skinIdx < 0 ? 0 : skinIdx);
        }
        else
        {
            g.setColour (laf.theme.panel.withAlpha (0.65f));
            g.fillRoundedRectangle (r, 8.f);
            g.setColour (laf.theme.a.withAlpha (0.22f));
            g.drawRoundedRectangle (r, 8.f, 1.f);
        }

        g.setColour (laf.theme.b.withAlpha (0.9f));
        g.setFont (fnt (10.f, true, 0.25f));
        g.drawText (loc::tr (s.title), (int) r.getX() + 12, (int) r.getY() + 5, (int) r.getWidth() - 16, 14,
                    juce::Justification::centredLeft);
    }

    if (page == 1)
        drawFxChain (g);

    // meter label
    g.setColour (vc::dim);
    g.setFont (fnt (10.f, true, 0.25f));
    g.drawText (loc::tr ("OUT"), meterRect().getX() - 10, topY, meterW + 20, 14, juce::Justification::centred);

    // status bar line
    g.setColour (laf.theme.a.withAlpha (0.2f));
    g.fillRect (0.f, h - (float) statusH, w, 1.f);
}

void MetalBassForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (bgCache.isNull() || bgCache.getWidth() != getWidth())
        renderBackground();
    g.drawImageAt (bgCache, 0, 0);
}

void MetalBassForgeAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (activation.isVisible() || notice.isVisible() || settings.isVisible())
        return;   // keep the dialog clean

    drawTuner (g);
    drawBpm (g);

    // output meter (LED segments)
    {
        auto r = meterRect();
        const int segs = 28;
        const float segH = (float) r.getHeight() / (float) segs;
        const float db = juce::Decibels::gainToDecibels (meter, -60.f);
        const float norm = juce::jlimit (0.f, 1.f, (db + 48.f) / 48.f);
        const int lit = (int) std::round (norm * (float) segs);

        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) i / (float) (segs - 1);
            auto col = laf.theme.a.interpolatedWith (laf.theme.b, t);
            auto seg = juce::Rectangle<float> ((float) r.getX(), (float) r.getBottom() - (float) (i + 1) * segH + 1.5f,
                                               (float) r.getWidth(), segH - 3.f);
            const bool on = i < lit;
            g.setColour (on ? col : col.withAlpha (0.08f));
            g.fillRoundedRectangle (seg, 1.5f);
            if (on)
            {
                g.setColour (col.withAlpha (0.18f));
                g.fillRoundedRectangle (seg.expanded (2.f), 3.f);
            }
        }
    }

    // slow scan line
    if (! animations)
        return;
    const float y = (float) lastScanY;
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.f, y - 18.f,
                                             laf.theme.a.withAlpha (0.07f), 0.f, y, false));
    g.fillRect (0.f, y - 18.f, (float) getWidth(), 18.f);
    g.setColour (laf.theme.a.withAlpha (0.10f));
    g.fillRect (0.f, y, (float) getWidth(), 1.f);
}

void MetalBassForgeAudioProcessorEditor::resized()
{
    renderBackground();

    for (auto& k : knobs)
    {
        const int x = marginX + k->col * knobW;
        const int y = topY + k->row * rowH + 24;
        k->label.setBounds (x, y, knobW, 14);
        k->slider.setBounds (x, y + 14, knobW, rowH - 24 - 14 - 8);
    }

    {
        auto place = [&] (int row, int dx, int dy, int w, int h)
        { return juce::Rectangle<int> (marginX + dx, topY + row * rowH + dy, w, h); };

        // SOLO pads (row 0, cols 5-6)
        const int sx = 5 * knobW + 12, sw = (2 * knobW - 24 - 12) / 3;
        soloLowBtn.setBounds  (place (0, sx,                 36, sw, 70));
        soloGritBtn.setBounds (place (0, sx + (sw + 6),      36, sw, 70));
        soloDryBtn.setBounds  (place (0, sx + 2 * (sw + 6),  36, sw, 70));

        // AMP (row 2, cols 0-2)
        ampBox.setBounds  (place (2, 14, 30, 3 * knobW - 28, 26));
       #if VC_WITH_NAM
        ampDesc.setBounds (place (2, 14, 60, 3 * knobW - 28, 34));
        namOnBtn.setBounds   (place (2, 10, 96, 84, 26));
        loadNamBtn.setBounds (place (2, 98, 96, 104, 26));
        namLabel.setBounds   (place (2, 206, 96, 3 * knobW - 28 - 196, 26));
       #else
        ampDesc.setBounds (place (2, 14, 64, 3 * knobW - 28, 54));
       #endif

        // CAB (row 3, cols 0-2)
        cabBox.setBounds     (place (3, 14, 28, 3 * knobW - 28, 24));
        loadIrBtn.setBounds  (place (3, 14, 58, 104, 26));
        cabAllBtn.setBounds  (place (3, 14 + 110, 58, 3 * knobW - 28 - 110, 26));
        irLabel.setBounds    (place (3, 14, 90, 3 * knobW - 28, 30));
    }

    {
        auto place = [&] (int row, int dx, int dy, int w, int h)
        { return juce::Rectangle<int> (marginX + dx, topY + row * rowH + dy, w, h); };

        // side panel (cols 7-8): pitch quick buttons stacked under the knobs' right column
        {
            const int bx = 8 * knobW + 10, bw = knobW - 20;
            dropDBtn.setBounds      (place (1, bx, 26, bw, 22));
            dropCBtn.setBounds      (place (1, bx, 50, bw, 22));
            octBtn.setBounds        (place (1, bx, 74, bw, 22));
            resetPitchBtn.setBounds (place (1, bx, 98, bw, 22));

            tunerMuteBtn.setBounds (place (2, 7 * knobW + 10, rowH - 8 - 30, 2 * knobW - 20, 24));
            tapBtn.setBounds       (place (3, 7 * knobW + 10, rowH - 8 - 32, 2 * knobW - 20, 26));
        }

        fxMasterBtn.setBounds (place (2, 2 * knobW + 12, 28, 5 * knobW - 24, rowH - 8 - 28 - 10));
        wahOnBtn.setBounds   (place (0, 10, 32, 176, 38));
        wahModeBox.setBounds (place (0, 14, 78, 168, 26));

        syncLabel.setBounds     (place (1, 0, 24, knobW, 14));
        delaySyncBox.setBounds  (place (1, 8, 62, knobW - 16, 26));

        pageMainBtn.setBounds (marginX + 388, 72, 96, 32);
        pageCabBtn.setBounds  (marginX + 482, 72, 96, 32);
        pageFxBtn.setBounds   (marginX + 576, 72, 96, 32);

        // CAB page
        {
            juce::Rectangle<int> cabRect;
            for (auto& sec : sections)
                if (sec.title.startsWith ("CABINET")) cabRect = sectionRect (sec);
            cabView->setBounds (cabRect.reduced (6, 0).withTrimmedTop (22).withTrimmedBottom (6));

            cabBox2.setBounds     (place (0, 4 * knobW + 14, 30, 3 * knobW - 28, 26));
            loadIrBtn2.setBounds  (place (0, 4 * knobW + 14, 62, 100, 26));
            cabAllBtn2.setBounds  (place (0, 4 * knobW + 14 + 106, 62, 3 * knobW - 28 - 106, 26));
            irLabel2.setBounds    (place (0, 4 * knobW + 14, 92, 3 * knobW - 28, 28));

            micTypeLabel.setBounds (place (1, 4 * knobW, 24, knobW, 14));
            micBox.setBounds       (place (1, 4 * knobW + 6, 62, knobW - 12, 26));

            micDesc.setBounds (place (3, 4 * knobW + 14, 30, 3 * knobW - 28, 86));
        }
    }

    presetBox.setBounds (marginX + 64, 76, 250, 24);
    prevBtn.setBounds (marginX + 64 + 258, 76, 30, 24);
    nextBtn.setBounds (marginX + 64 + 292, 76, 30, 24);

    auto bar = getLocalBounds().removeFromBottom (statusH).reduced (marginX, 8);
    statusBuy.setBounds (bar.removeFromRight (56));
    bar.removeFromRight (6);
    statusActivate.setBounds (bar.removeFromRight (84));
    statusLabel.setBounds (bar.removeFromLeft (236));
    bar.removeFromLeft (6);
    infoLabel.setBounds (bar.removeFromLeft (200));
    bar.removeFromLeft (6);
    qualityBox.setBounds (bar.removeFromLeft (118));
    bar.removeFromLeft (6);
    audioBtn.setBounds (bar.removeFromLeft (64));

    activation.setBounds (getLocalBounds());
    notice.setBounds (getLocalBounds());
    gearBtn.setBounds (getWidth() - marginX - 28, 32, 28, 28);
    langBox.setBounds (getWidth() - marginX - 28 - 8 - 124, 34, 124, 24);
    settings.setBounds (getLocalBounds());
}

//==============================================================================
void MetalBassForgeAudioProcessorEditor::updateStatusBar()
{
    const auto st = processor.license.getState();
    const int days = processor.license.getDaysLeft();
    lastShownState = (int) st;
    lastShownDays = days;

    juce::String t;
    juce::Colour c = vc::text;
    switch (st)
    {
        case LicenseManager::Licensed:
#if VC_FRIENDS_BUILD
            t = loc::tr ("FULL VERSION  //  TESTER");
#else
            t = loc::tr ("LICENSED") + "  //  " + processor.license.getLicensee();
#endif
            c = laf.theme.a;
            break;
        case LicenseManager::Trial:
            t = loc::tr ("TRIAL") + "  //  " + juce::String (days) + " " + loc::tr (days == 1 ? "DAY LEFT" : "DAYS LEFT");
            c = days <= 3 ? laf.theme.b : vc::text;
            break;
        case LicenseManager::Expired:
            t = loc::tr ("TRIAL EXPIRED  //  AUDIO MUTED");
            c = laf.theme.b;
            break;
    }
    statusLabel.setText (t, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, c);

    const bool licensed = (st == LicenseManager::Licensed);
    statusActivate.setVisible (! licensed);
    statusBuy.setVisible (! licensed);
}

void MetalBassForgeAudioProcessorEditor::updateSlowTexts()
{
    updateAmpDescription();
    updateIrLabel();
    cabAllBtn.setButtonText (loc::tr (cabAllBtn.getToggleState() ? "CAB ON FULL MIX" : "CAB ON GRIT ONLY"));
    cabAllBtn2.setButtonText (cabAllBtn.getButtonText());

    static const char* d[] = {
        "Dynamic - punchy upper mids and a tight low end. The workhorse for aggressive bass tones.",
        "Ribbon - smooth, dark and thick. Tames fizz and adds low-mid weight.",
        "Condenser - detailed and airy. Adds top-end clarity and definition." };
    const int mt = juce::jlimit (0, 2, micBox.getSelectedItemIndex());
    micDesc.setText (loc::tr (d[mt]) + "\n\n" + loc::tr ("Position: CAP = bright and aggressive, EDGE = dark and smooth. Distance: CLOSE = more low-end proximity."),
                     juce::dontSendNotification);
}

int MetalBassForgeAudioProcessorEditor::scanY() const
{
    return (int) std::fmod (phase * 90.f, (float) getHeight() + 40.f) - 20;
}

void MetalBassForgeAudioProcessorEditor::timerCallback()
{
    if (! isShowing())
        return;   // editor hidden / minimised: no drawing work at all

    phase += 1.f / 30.f;
    ++tunerTick;

    if (page == 1 && tunerTick % 4 == 0)
        repaint (chainRect().expanded (2));

    if (fxMasterBtn.getToggleState() != lastFxOn)
    {
        lastFxOn = fxMasterBtn.getToggleState();
        for (auto& k : knobs)
            if (k->page == 1) { k->slider.setAlpha (lastFxOn ? 1.f : 0.4f); k->label.setAlpha (lastFxOn ? 1.f : 0.4f); }
        for (auto* c : std::initializer_list<juce::Component*> { &wahOnBtn, &wahModeBox, &delaySyncBox, &syncLabel })
            c->setAlpha (lastFxOn ? 1.f : 0.4f);
    }

    const float target = processor.outLevel.load();
    meter = target > meter ? target : meter * 0.85f + target * 0.15f;

    if ((int) processor.license.getState() != lastShownState || processor.license.getDaysLeft() != lastShownDays)
        updateStatusBar();

    if (ampBox.getSelectedItemIndex() != skinIdx && ampBox.getSelectedItemIndex() >= 0)
        applySkin (ampBox.getSelectedItemIndex());

    if (tunerTick % 10 == 0)
        updateSlowTexts();
    if (tunerTick % 15 == 0)
        updateDeviceInfo();

    // ---- repaint only what actually moves --------------------------------------
    if (std::abs (meter - lastDrawnMeter) > 0.004f)
    {
        lastDrawnMeter = meter;
        repaint (meterRect().expanded (4));
    }

    if (tunerTick % 4 == 0)
    {
        updateTuner();
        repaint (tunerRect().expanded (4));
    }

    if (tunerTick % 5 == 0)
    {
        const double hb = processor.hostBpmShown.load();
        const double shown = hb > 0.0 ? hb : -processor.manualBpm.load();
        tapBtn.setEnabled (hb <= 0.0);
        if (std::abs (shown - lastShownBpm) > 0.01)
        {
            lastShownBpm = shown;
            repaint (bpmRect().expanded (4));
        }
    }

    if (page == 2 && cabView != nullptr)
    {
        cabView->setLevel (meter);
        if (tunerTick % 2 == 0)
            cabView->repaint();
    }

    if (animations && tunerTick % 2 == 0)   // decorative scan line at 15 fps: repaint only its strip
    {
        const int prev = lastScanY, y = scanY();
        const int y0 = juce::jmin (y, prev) - 20, y1 = juce::jmax (y, prev) + 3;
        lastScanY = y;
        repaint (0, y0, getWidth(), y1 - y0);
    }
}

//==============================================================================
void MetalBassForgeAudioProcessorEditor::updateAmpDescription()
{
    const int idx = ampBox.getSelectedItemIndex();
    if (idx == shownAmp)
        return;
    shownAmp = idx;
    ampDesc.setText (juce::isPositiveAndBelow (idx, ampDescriptions.size()) ? ampDescriptions[idx] : juce::String(),
                     juce::dontSendNotification);
}

void MetalBassForgeAudioProcessorEditor::updateIrLabel()
{
    const auto name = processor.getUserIRName();
    juce::String t;
    juce::Colour c = vc::dim.brighter (0.4f);

    if (irError.isNotEmpty())
    {
        t = irError;
        c = laf.theme.b;
    }
    else if (name.isNotEmpty())
    {
        t = "IR: " + name;
        c = laf.theme.a;
    }
    else
        t = loc::tr ("No IR loaded - click LOAD IR or drop a WAV / AIFF / FLAC file on the window.");

    irLabel.setText (t, juce::dontSendNotification);
    irLabel.setColour (juce::Label::textColourId, c);
    irLabel2.setText (t, juce::dontSendNotification);
    irLabel2.setColour (juce::Label::textColourId, c);
}

void MetalBassForgeAudioProcessorEditor::chooseIR()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load impulse response",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.wav;*.aif;*.aiff;*.flac");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<juce::Component> (this)] (const juce::FileChooser& fc)
        {
            if (auto* ed = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (safe.getComponent()))
                if (fc.getResult().existsAsFile())
                    ed->loadIRFile (fc.getResult());
        });
}

#if VC_WITH_NAM
void MetalBassForgeAudioProcessorEditor::chooseNam()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load Neural Amp Modeler model",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.nam");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<juce::Component> (this)] (const juce::FileChooser& fc)
        {
            auto* ed = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (safe.getComponent());
            if (ed == nullptr || ! fc.getResult().existsAsFile())
                return;
            juce::String err;
            ed->namError = ed->processor.loadNamModel (fc.getResult(), err) ? juce::String() : err;
            if (ed->namError.isEmpty())
                if (auto* p = ed->processor.apvts.getParameter ("namOn"))
                    p->setValueNotifyingHost (1.f);   // switch the model on straight away
            ed->updateNamLabel();
        });
}

void MetalBassForgeAudioProcessorEditor::updateNamLabel()
{
    juce::String t;
    juce::Colour c = vc::dim.brighter (0.4f);
    if (namError.isNotEmpty())                  { t = namError; c = laf.theme.b; }
    else if (processor.isNamLoaded())           { t = processor.getNamName(); c = laf.theme.a; }
    else                                        t = loc::tr ("No NAM model");
    namLabel.setText (t, juce::dontSendNotification);
    namLabel.setColour (juce::Label::textColourId, c);
}
#endif

void MetalBassForgeAudioProcessorEditor::loadIRFile (const juce::File& f)
{
    juce::String err;
    irError = processor.loadUserIR (f, err) ? juce::String() : err;
    updateIrLabel();
}

bool MetalBassForgeAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    return false;
}

void MetalBassForgeAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            loadIRFile (juce::File (f));
            return;
        }
}

//==============================================================================
juce::Rectangle<int> MetalBassForgeAudioProcessorEditor::chainRect() const
{
    for (auto& s : sections)
        if (s.page == 1 && s.title == "SIGNAL CHAIN")
            return sectionRect (s);
    return {};
}

// Signal-flow strip on the FX page: shows which stages are active right now.
void MetalBassForgeAudioProcessorEditor::drawFxChain (juce::Graphics& g)
{
    auto area = chainRect().reduced (14, 0).withTrimmedTop (30).withTrimmedBottom (12).toFloat();
    if (area.isEmpty()) return;

    auto val = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    const bool fx = val ("fxOn") > 0.5f;
    const float semis = val ("pitchSemi") + val ("pitchFine") / 100.f;

    struct Stage { const char* key; bool active; bool master; };
    const Stage st[] = {
        { "GATE",          fx && val ("gateThresh") > -79.5f, true  },
        { "PITCH",         std::fabs (semis) > 0.001f,        false },
        { "WAH",           fx && val ("wahOn") > 0.5f,        true  },
        { "AMP + CAB",     true,                              false },
        { "DELAY",         fx && val ("delayMix") > 0.001f,   true  },
        { "REVERB",        fx && val ("revMix") > 0.001f,     true  },
    };
    constexpr int n = 6;
    const float arrowW = 20.f;
    const float bw = (area.getWidth() - arrowW * (n - 1)) / (float) n;

    for (int i = 0; i < n; ++i)
    {
        const auto box = juce::Rectangle<float> (area.getX() + (float) i * (bw + arrowW), area.getY(), bw, area.getHeight()).withSizeKeepingCentre (bw, juce::jmin (area.getHeight(), 58.f));
        const bool on = st[i].active;
        const auto col = on ? laf.theme.a : vc::dim;

        if (on) { g.setColour (col.withAlpha (0.10f)); g.drawRoundedRectangle (box.expanded (2.f), 8.f, 3.f); }
        g.setColour (col.withAlpha (on ? 0.22f : 0.06f));
        g.fillRoundedRectangle (box, 7.f);
        g.setColour (col.withAlpha (on ? 0.95f : 0.45f));
        g.drawRoundedRectangle (box, 7.f, on ? 1.6f : 1.f);

        const float d = 9.f;
        g.setColour (on ? laf.theme.a.brighter (0.4f) : juce::Colour (0xff2a2f45));
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ box.getCentreX(), box.getY() + 12.f }));

        const auto txt = loc::upper (loc::tr (st[i].key));
        g.setColour (on ? juce::Colours::white : vc::text.withAlpha (0.55f));
        g.setFont (VoidLookAndFeel::fitButtonFont (txt, (int) box.getWidth() - 10, 12.f, 8.f));
        g.drawText (txt, box.withTrimmedTop (22.f).reduced (3.f, 0.f).toNearestInt(), juce::Justification::centred, false);

        if (i < n - 1)
        {
            const float ax = box.getRight() + 4.f, ay = box.getCentreY();
            g.setColour (laf.theme.b.withAlpha (0.8f));
            juce::Path p;
            p.startNewSubPath (ax, ay - 5.f); p.lineTo (ax + arrowW - 12.f, ay); p.lineTo (ax, ay + 5.f);
            g.strokePath (p, juce::PathStrokeType (2.f));
        }
    }
}

void MetalBassForgeAudioProcessorEditor::applySkin (int idx)
{
    skinIdx = idx;
    refreshTheme();
}

void MetalBassForgeAudioProcessorEditor::refreshTheme()
{
    const float shift = (page == 1 ? 0.5f : 0.f);       // EFFECTS page gets a rotated colour scheme
    laf.applyTheme (amp_art::themeFor (juce::jmax (0, skinIdx), shift));

    for (auto* b : { (juce::Component*) &soloLowBtn, (juce::Component*) &loadIrBtn, (juce::Component*) &pageMainBtn,
                     (juce::Component*) &pageFxBtn, (juce::Component*) &dropDBtn, (juce::Component*) &dropCBtn,
                     (juce::Component*) &octBtn, (juce::Component*) &resetPitchBtn, (juce::Component*) &prevBtn,
                     (juce::Component*) &nextBtn, (juce::Component*) &statusActivate, (juce::Component*) &pageCabBtn,
                     (juce::Component*) &loadIrBtn2, (juce::Component*) &tapBtn, (juce::Component*) &fxMasterBtn })
        b->setColour (juce::TextButton::buttonOnColourId, laf.theme.a);
    for (auto* b : { (juce::Component*) &soloGritBtn, (juce::Component*) &cabAllBtn,
                     (juce::Component*) &tunerMuteBtn, (juce::Component*) &statusBuy, (juce::Component*) &audioBtn, (juce::Component*) &cabAllBtn2 })
        b->setColour (juce::TextButton::buttonOnColourId, laf.theme.b);
    infoLabel.setColour (juce::Label::textColourId, laf.theme.a.withAlpha (0.85f));

    renderBackground();
    sendLookAndFeelChange();
    repaint();
}

void MetalBassForgeAudioProcessorEditor::updateDeviceInfo()
{
    const double sr = processor.getCurrentSampleRate();
    const int lat = processor.getLatencySamples();
    juce::String t;

    if (auto dev = getStandaloneDeviceInfo())
        t = dev->name.substring (0, 16) + " | " + juce::String (dev->bufferSize) + " smp | "
            + juce::String (dev->roundTripMs + 1000.0 * lat / juce::jmax (1.0, sr), 2) + " ms";
    else
        t = juce::String (sr / 1000.0, 1) + " kHz | buf " + juce::String (processor.getCurrentBlockSize())
            + " | +" + juce::String (1000.0 * lat / juce::jmax (1.0, sr), 2) + " ms";

    infoLabel.setText (t, juce::dontSendNotification);
}

void MetalBassForgeAudioProcessorEditor::setPage (int newPage)
{
    page = newPage;
    for (auto& k : knobs)
    {
        const bool vis = k->page < 0 || k->page == page;
        k->slider.setVisible (vis);
        k->label.setVisible (vis);
    }
    for (auto* c : mainComps) c->setVisible (page == 0);
    for (auto* c : fxComps)   c->setVisible (page == 1);
    for (auto* c : cabComps)  c->setVisible (page == 2);

    pageMainBtn.setToggleState (page == 0, juce::dontSendNotification);
    pageCabBtn.setToggleState (page == 2, juce::dontSendNotification);
    pageFxBtn.setToggleState (page == 1, juce::dontSendNotification);

    tunerHz = 0.f;
    refreshTheme();
}

juce::Rectangle<int> MetalBassForgeAudioProcessorEditor::tunerRect() const
{
    return { marginX + 7 * knobW + 10, topY + 2 * rowH + 22, 2 * knobW - 20, rowH - 8 - 22 - 32 };
}

juce::Rectangle<int> MetalBassForgeAudioProcessorEditor::bpmRect() const
{
    return { marginX + 7 * knobW + 10, topY + 3 * rowH + 22, 2 * knobW - 20, rowH - 8 - 22 - 36 };
}

void MetalBassForgeAudioProcessorEditor::updateTuner()
{
    processor.copyTunerSamples (tunerBuf.data(), (int) tunerBuf.size());
    const float f = MetalBassForgeAudioProcessor::detectPitch (tunerBuf.data(), (int) tunerBuf.size(),
                                                               processor.getCurrentSampleRate());
    if (f > 0.f)       { tunerHz = f; tunerHold = 6; }
    else if (tunerHold > 0) --tunerHold;
    else               tunerHz = 0.f;
}

void MetalBassForgeAudioProcessorEditor::drawTuner (juce::Graphics& g)
{
    if (activation.isVisible() || notice.isVisible())
        return;

    auto r = tunerRect();                       // ~172 x 70
    auto top = r.removeFromTop (38);
    auto scale = r;
    const float cx0 = (float) scale.getX() + 4.f, cw = (float) scale.getWidth() - 8.f;
    const float midY = (float) scale.getCentreY();

    for (int c = -50; c <= 50; c += 10)
    {
        const float x = cx0 + cw * (float) (c + 50) / 100.f;
        g.setColour ((c == 0 ? laf.theme.a : vc::dim).withAlpha (c == 0 ? 0.9f : 0.5f));
        g.drawLine (x, midY - (c == 0 ? 11.f : 6.f), x, midY + (c == 0 ? 11.f : 6.f), c == 0 ? 2.f : 1.f);
    }

    if (tunerHz <= 0.f)
    {
        g.setColour (vc::dim);
        g.setFont (fnt (11.f, true, 0.15f));
        g.drawText (loc::tr ("PLAY A NOTE"), top, juce::Justification::centred);
        return;
    }

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midi = 69.f + 12.f * std::log2 (tunerHz / 440.f);
    const int note = (int) std::round (midi);
    const float cents = (midi - (float) note) * 100.f;
    const bool inTune = std::fabs (cents) < 5.f;
    const auto col = inTune ? laf.theme.a : laf.theme.b.interpolatedWith (laf.theme.a, 1.f - juce::jmin (1.f, std::fabs (cents) / 50.f));

    auto noteBox = top.removeFromLeft (84);
    g.setColour (col);
    g.setFont (fnt (32.f, true, 0.03f));
    g.drawText (juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1), noteBox, juce::Justification::centred);

    g.setColour (vc::dim.brighter (0.4f));
    g.setFont (fnt (11.f, true, 0.05f));
    g.drawText (juce::String (tunerHz, 1) + " Hz", top.removeFromTop (19), juce::Justification::centredLeft);
    g.drawText ((cents >= 0 ? "+" : "") + juce::String ((int) std::round (cents)) + " ct", top, juce::Justification::centredLeft);

    const float nx = cx0 + cw * (juce::jlimit (-50.f, 50.f, cents) + 50.f) / 100.f;
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (col.withAlpha (0.08f * (float) (4 - i)));
        g.fillRoundedRectangle (nx - 2.f - (float) i * 1.5f, midY - 14.f, 4.f + (float) i * 3.f, 28.f, 3.f);
    }
    g.setColour (col);
    g.fillRoundedRectangle (nx - 2.f, midY - 14.f, 4.f, 28.f, 2.f);
}

void MetalBassForgeAudioProcessorEditor::drawBpm (juce::Graphics& g)
{
    if (activation.isVisible() || notice.isVisible())
        return;

    auto r = bpmRect();
    const bool host = processor.hostBpmShown.load() > 0.0;
    const double bpm = host ? processor.hostBpmShown.load() : processor.manualBpm.load();

    g.setColour (bpm > 0.0 ? laf.theme.a : vc::dim);
    g.setFont (fnt (30.f, true, 0.03f));
    g.drawText (bpm > 0.0 ? juce::String (bpm, 1) : juce::String ("---"), r.removeFromTop (38), juce::Justification::centred);
    g.setColour (vc::dim.brighter (0.4f));
    g.setFont (fnt (10.f, true, 0.15f));
    g.drawText (loc::tr (host ? "HOST SYNC" : "TAP TEMPO"), r, juce::Justification::centred);
}
