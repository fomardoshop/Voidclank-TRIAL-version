#pragma once

#include "PluginProcessor.h"
#include "VoidLookAndFeel.h"
#include "AmpArt.h"
#include "CabView.h"
#include "Loc.h"
#include "Settings.h"

//==============================================================================
// Activation / trial dialog shown on top of the plugin UI.
class ActivationPanel : public juce::Component
{
public:
    ActivationPanel (MetalBassForgeAudioProcessor& p, std::function<void()> onCloseFn);

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void applyLanguage();
    void closeNow() { if (onClose) onClose(); }

private:
    juce::Rectangle<int> card() const;

    MetalBassForgeAudioProcessor& proc;
    std::function<void()> onClose;

    juce::TextEditor emailEd, codeEd;
    juce::TextButton activateBtn { "ACTIVATE" }, buyBtn { "BUY" }, closeBtn { "CLOSE" };
    juce::String message;
    bool messageIsError = false;
};

//==============================================================================
// Round "gear" button (opens the settings panel)
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("settings") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const auto col = findColour (juce::TextButton::buttonOnColourId);
        g.setColour (col.withAlpha (down ? 0.38f : highlighted ? 0.24f : 0.10f));
        g.fillEllipse (r);
        g.setColour (col.withAlpha (0.9f));
        g.drawEllipse (r, 1.4f);

        juce::Path gear;
        const auto c = r.getCentre();
        gear.addStar (c, 8, r.getWidth() * 0.22f, r.getWidth() * 0.33f, 0.f);
        gear.addEllipse (c.x - r.getWidth() * 0.26f, c.y - r.getWidth() * 0.26f, r.getWidth() * 0.52f, r.getWidth() * 0.52f);
        gear.setUsingNonZeroWinding (true);
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 1.f : 0.85f));
        g.fillPath (gear);
        g.setColour (findColour (juce::TextButton::buttonOnColourId).darker (0.9f));
        g.fillEllipse (c.x - r.getWidth() * 0.11f, c.y - r.getWidth() * 0.11f, r.getWidth() * 0.22f, r.getWidth() * 0.22f);
    }
};

//==============================================================================
// Settings panel: AUDIO / INTERFACE / INFO tabs
class SettingsPanel : public juce::Component, private juce::Timer
{
public:
    SettingsPanel (MetalBassForgeAudioProcessor& p, std::function<void()> onCloseFn,
                   std::function<void (int)> onScaleFn, std::function<void (bool)> onAnimFn);
    ~SettingsPanel() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { if (isVisible()) { startTimerHz (2); showTab (tab); } else stopTimer(); }
    void applyLanguage();
    void showTab (int t);
    void mouseDown (const juce::MouseEvent&) override {}

private:
    juce::Rectangle<int> card() const;
    void timerCallback() override { repaint(); }

    MetalBassForgeAudioProcessor& proc;
    std::function<void()> onClose;
    std::function<void (int)> onScale;
    std::function<void (bool)> onAnim;
    int tab = 0;

    juce::ToggleButton tabAudio { "AUDIO" }, tabUi { "INTERFACE" }, tabInfo { "INFO" };
    juce::TextButton closeBtn { "CLOSE" }, deviceBtn { "OPEN AUDIO DEVICE SETTINGS" };
    juce::ComboBox qualityBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAtt;
    juce::ToggleButton scale75 { "75 %" }, scale100 { "100 %" }, scale125 { "125 %" }, scale150 { "150 %" }, animBtn { "ON" };
    juce::TextButton paypalBtn { "PayPal" }, mailBtn { "e-mail" };
};

//==============================================================================
// Bilingual (PL / EN) start-up notice for tester builds.
class NoticePanel : public juce::Component
{
public:
    explicit NoticePanel (std::function<void()> onCloseFn);
    void applyLanguage();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override {}   // swallow clicks behind the card

private:
    juce::Rectangle<int> card() const;

    std::function<void()> onClose;
    juce::TextButton okBtn { "OK  /  ROZUMIEM" };
};

//==============================================================================
// Big master kill switch for the whole EFFECTS page (wah, delay, reverb, noise gate).
class FxMasterButton : public juce::ToggleButton
{
public:
    FxMasterButton() { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const bool on = getToggleState();
        const auto col = findColour (juce::TextButton::buttonOnColourId);
        auto r = getLocalBounds().toFloat().reduced (5.f);

        if (on)
            for (int i = 5; i >= 1; --i)
            {
                g.setColour (col.withAlpha (0.045f * (float) (6 - i)));
                g.drawRoundedRectangle (r.expanded ((float) i * 1.4f), 12.f, 2.f);
            }

        g.setGradientFill (juce::ColourGradient (col.withAlpha (on ? (down ? 0.55f : 0.42f) : (highlighted ? 0.14f : 0.07f)), r.getX(), r.getY(),
                                                 col.withAlpha (on ? 0.20f : 0.03f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.f);
        g.setColour (col.withAlpha (on ? 1.f : 0.45f));
        g.drawRoundedRectangle (r, 10.f, on ? 2.4f : 1.4f);

        // LED
        const float d = juce::jmin (26.f, r.getHeight() * 0.32f);
        const auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + 34.f, r.getCentreY() });
        if (on)
        {
            g.setColour (col.withAlpha (0.25f));  g.fillEllipse (led.expanded (7.f));
            g.setColour (col.withAlpha (0.45f));  g.fillEllipse (led.expanded (3.5f));
        }
        g.setColour (on ? col.brighter (0.4f) : juce::Colour (0xff2a2f45));
        g.fillEllipse (led);
        g.setColour (col.withAlpha (on ? 1.f : 0.4f));
        g.drawEllipse (led, 1.5f);

        const auto txtArea = r.withTrimmedLeft (68.f).reduced (8.f, 6.f);
        const auto big = loc::upper (loc::tr (on ? "EFFECTS ON" : "EFFECTS OFF"));
        const auto small = loc::upper (loc::tr ("MASTER KILL SWITCH - CLICK TO TOGGLE"));

        g.setColour (on ? juce::Colours::white : vc::text.withAlpha (0.6f));
        g.setFont (VoidLookAndFeel::fitButtonFont (big, (int) txtArea.getWidth(), 30.f, 14.f));
        g.drawText (big, txtArea.withTrimmedBottom (txtArea.getHeight() * 0.32f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setColour (col.withAlpha (on ? 0.9f : 0.5f));
        g.setFont (VoidLookAndFeel::fitButtonFont (small, (int) txtArea.getWidth(), 10.f, 7.f));
        g.drawText (small, txtArea.withTrimmedTop (txtArea.getHeight() * 0.68f).toNearestInt(), juce::Justification::centredLeft, false);
    }
};

class MetalBassForgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::FileDragAndDropTarget,
                                           private juce::Timer
{
public:
    explicit MetalBassForgeAudioProcessorEditor (MetalBassForgeAudioProcessor&);
    ~MetalBassForgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void showPageForTesting (int p) { setPage (p); }
    void showSettingsForTesting (int tab) { settings.setVisible (true); settings.toFront (true); settings.showTab (tab); }
    void showNoticeForTesting() { notice.setVisible (true); notice.toFront (true); }
    void hideNoticeForTesting() { notice.setVisible (false); }

    // drag & drop an impulse response (.wav / .aif / .aiff / .flac) onto the window
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct Knob
    {
        int page = 0, row = 0, col = 0;
        juce::String key;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Section { juce::String title; int page, row, col0, col1; int rows = 1; };

    void addKnob (int row, int col, const juce::String& paramId, const juce::String& name,
                  const juce::String& suffix);
    void timerCallback() override;
    void updateStatusBar();
    void renderBackground();
    juce::Rectangle<int> sectionRect (const Section&) const;
    juce::Rectangle<int> meterRect() const;
    void stepPreset (int delta);
    void chooseIR();
    void loadIRFile (const juce::File&);
    void updateIrLabel();
    void updateSlowTexts();
   #if VC_WITH_NAM
    void chooseNam();
    void updateNamLabel();
   #endif
    void updateAmpDescription();
    void setPage (int newPage);
    void applySkin (int idx);
    juce::Rectangle<int> chainRect() const;
    void drawFxChain (juce::Graphics& g);
    void refreshTheme();
    int scanY() const;
    void applyLanguage();
    void updateDeviceInfo();
    void updateTuner();
    void drawTuner (juce::Graphics&);
    juce::Rectangle<int> tunerRect() const;
    juce::Rectangle<int> bpmRect() const;
    void drawBpm (juce::Graphics&);

    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using BoxAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    MetalBassForgeAudioProcessor& processor;
    VoidLookAndFeel laf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    // solo / amp / cab controls
    juce::ToggleButton soloLowBtn { "LOW" }, soloGritBtn { "GRIT" }, soloDryBtn { "DRY" }, cabAllBtn;
    juce::ComboBox ampBox, cabBox;
    juce::Label ampDesc, irLabel;
    juce::TextButton loadIrBtn { "LOAD IR..." };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray ampDescriptions;
    juce::String irError;
    int shownAmp = -1;
    std::unique_ptr<BtnAtt> soloLowAtt, soloGritAtt, soloDryAtt, cabAllAtt;
    std::unique_ptr<BoxAtt> ampAtt, cabAtt;

   #if VC_WITH_NAM
    juce::ToggleButton namOnBtn { "NAM ON" };
    juce::TextButton loadNamBtn { "LOAD NAM..." };
    juce::Label namLabel;
    juce::String namError;
    std::unique_ptr<BtnAtt> namAtt;
   #endif

    // CAB page (cabinet + microphone view)
    juce::ToggleButton pageCabBtn { "CAB" }, cabAllBtn2;
    juce::ComboBox cabBox2, micBox;
    juce::TextButton loadIrBtn2 { "LOAD IR..." };
    juce::Label irLabel2, micDesc, micTypeLabel;
    std::unique_ptr<BoxAtt> cabAtt2, micAtt;
    std::unique_ptr<BtnAtt> cabAllAtt2;
    std::vector<juce::Component*> cabComps;
    std::unique_ptr<CabView> cabView;

    // pages: 0 = MAIN, 1 = FX, 2 = CAB
    int page = 0;
    int buildPage = 0;
    juce::ToggleButton pageMainBtn { "MAIN" }, pageFxBtn { "FX" };
    std::vector<juce::Component*> mainComps, fxComps;

    // FX page controls
    juce::TextButton dropDBtn { "DROP D" }, dropCBtn { "DROP C" }, octBtn { "OCT -1" }, resetPitchBtn { "RESET" };
    juce::ToggleButton wahOnBtn { "WAH ON" }, tunerMuteBtn { "MUTE OUTPUT" };
    FxMasterButton fxMasterBtn;
    std::unique_ptr<BtnAtt> fxMasterAtt;
    bool lastFxOn = true;
    juce::ComboBox wahModeBox, delaySyncBox;
    juce::Label syncLabel;
    std::unique_ptr<BtnAtt> wahOnAtt, tunerMuteAtt;
    std::unique_ptr<BoxAtt> wahModeAtt, delaySyncAtt;

    // status-bar extras: audio info, latency mode
    juce::Label infoLabel;
    juce::ComboBox qualityBox;
    juce::TextButton audioBtn { "AUDIO" };
    std::unique_ptr<BoxAtt> qualityAtt;
    int skinIdx = -1;

    // tuner state
    std::vector<float> tunerBuf = std::vector<float> (8192);
    float tunerHz = 0.f;
    int tunerHold = 0, tunerTick = 0;

    juce::TextButton tapBtn { "TAP" };
    std::vector<double> tapTimes;
    double lastShownBpm = -1.0;
    juce::ComboBox langBox;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::Label statusLabel;
    juce::TextButton statusActivate { "ACTIVATE" }, statusBuy { "BUY" };
    ActivationPanel activation;
    NoticePanel notice;
    GearButton gearBtn;
    SettingsPanel settings;
    bool animations = true;

    juce::Image bgCache;
    float phase = 0.f, meter = 0.f, lastDrawnMeter = -1.f;
    int lastScanY = 0;
    int lastShownState = -1, lastShownDays = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalBassForgeAudioProcessorEditor)
};
