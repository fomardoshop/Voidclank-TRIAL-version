#pragma once

#include <JuceHeader.h>
#include "Loc.h"

namespace vc
{
    inline const juce::Colour cyan    { 0xff00e5ff };
    inline const juce::Colour magenta { 0xffff2bd6 };
    inline const juce::Colour bgTop   { 0xff0b0d1c };
    inline const juce::Colour bgBot   { 0xff04040a };
    inline const juce::Colour panel   { 0xff0e1226 };
    inline const juce::Colour text    { 0xffc8d4ff };
    inline const juce::Colour dim     { 0xff6b77a8 };
}

// Neon / sci-fi look: glowing arc knobs, outlined buttons.
struct Theme
{
    juce::Colour a { vc::cyan }, b { vc::magenta };
    juce::Colour bgTop { vc::bgTop }, bgBot { vc::bgBot }, panel { vc::panel };
};

class VoidLookAndFeel : public juce::LookAndFeel_V4
{
public:
    Theme theme;

    void applyTheme (const Theme& t)
    {
        theme = t;
        setColour (juce::ComboBox::outlineColourId, t.a.withAlpha (0.55f));
        setColour (juce::ComboBox::arrowColourId, t.a);
        setColour (juce::ComboBox::backgroundColourId, t.panel);
        setColour (juce::PopupMenu::backgroundColourId, t.bgTop);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, t.a.withAlpha (0.25f));
        setColour (juce::TextEditor::outlineColourId, t.a.withAlpha (0.35f));
        setColour (juce::TextEditor::focusedOutlineColourId, t.a);
    }

    VoidLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, vc::panel);
        setColour (juce::ComboBox::outlineColourId, vc::cyan.withAlpha (0.55f));
        setColour (juce::ComboBox::textColourId, vc::text);
        setColour (juce::ComboBox::arrowColourId, vc::cyan);

        setColour (juce::PopupMenu::backgroundColourId, vc::bgTop);
        setColour (juce::PopupMenu::textColourId, vc::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, vc::cyan.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff070815));
        setColour (juce::TextEditor::textColourId, vc::text);
        setColour (juce::TextEditor::outlineColourId, vc::cyan.withAlpha (0.35f));
        setColour (juce::TextEditor::focusedOutlineColourId, vc::cyan);
        setColour (juce::TextEditor::highlightColourId, vc::magenta.withAlpha (0.4f));

        setColour (juce::TextButton::textColourOffId, vc::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, vc::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float arcR = r - 2.f;
        const float ang = start + pos * (end - start);
        const auto col = theme.a.interpolatedWith (theme.b, pos);

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, start, end, true);
        g.setColour (juce::Colour (0xff1b2040));
        g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        if (pos > 0.002f)
        {
            juce::Path v;
            v.addCentredArc (c.x, c.y, arcR, arcR, 0.f, start, ang, true);

            for (int i = 4; i >= 1; --i)   // glow
            {
                g.setColour (col.withAlpha (0.05f * (float) (5 - i)));
                g.strokePath (v, juce::PathStrokeType (4.f + (float) i * 3.f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            }
            g.setColour (col);
            g.strokePath (v, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }

        const float ir = arcR - 9.f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2f45), c.x - ir * 0.4f, c.y - ir * 0.6f,
                                                 juce::Colour (0xff090a12), c.x + ir * 0.5f, c.y + ir * 0.7f, true));
        g.fillEllipse (c.x - ir, c.y - ir, ir * 2.f, ir * 2.f);
        g.setColour (col.withAlpha (0.35f));
        g.drawEllipse (c.x - ir, c.y - ir, ir * 2.f, ir * 2.f, 1.f);

        const float s = std::sin (ang), co = -std::cos (ang);
        g.setColour (col.brighter (0.4f));
        g.drawLine (c.x + s * ir * 0.35f, c.y + co * ir * 0.35f,
                    c.x + s * ir * 0.92f, c.y + co * ir * 0.92f, 2.4f);
    }

    // ---- one typographic style for every button: bold 12 pt, tracked caps; shrinks (never stretches) to fit
    static juce::Font buttonFont (float h = 12.f)
    {
        return juce::Font (juce::FontOptions (h, juce::Font::bold).withKerningFactor (0.12f));
    }

    static juce::Font fitButtonFont (const juce::String& text, int maxWidth, float maxH = 12.f, float minH = 8.f)
    {
        for (float h = maxH; h > minH; h -= 0.5f)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (buttonFont (h), text, 0.f, 0.f);
            if (ga.getBoundingBox (0, -1, false).getWidth() <= (float) maxWidth)
                return buttonFont (h);
        }
        return buttonFont (minH);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return buttonFont(); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().reduced (6, 2);
        const auto txt = loc::upper (b.getButtonText());
        g.setFont (fitButtonFont (txt, r.getWidth()));
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId
                                                      : juce::TextButton::textColourOffId)
                         .withMultipliedAlpha (b.isEnabled() ? 1.f : 0.5f));
        g.drawText (txt, r, juce::Justification::centred, false);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (14.f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.f)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const auto col = b.findColour (juce::TextButton::buttonOnColourId);
        g.setColour (col.withAlpha (down ? 0.38f : highlighted ? 0.24f : 0.10f));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (col.withAlpha (0.9f));
        g.drawRoundedRectangle (r, 6.f, 1.4f);
    }

    // Toggle buttons are drawn as neon "pads" that light up when on (accent = buttonOnColourId)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (4.f);
        const auto col = b.findColour (juce::TextButton::buttonOnColourId);
        const bool on = b.getToggleState();

        if (on)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (col.withAlpha (0.07f * (float) (4 - i)));
                g.drawRoundedRectangle (r.expanded ((float) i * 1.2f), 7.f, 2.f);
            }

        g.setColour (col.withAlpha (on ? (down ? 0.55f : 0.40f) : highlighted ? 0.18f : 0.06f));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (col.withAlpha (on ? 1.f : 0.5f));
        g.drawRoundedRectangle (r, 6.f, on ? 1.8f : 1.2f);

        g.setColour (on ? juce::Colours::white : vc::text.withAlpha (0.75f));
        const auto txt = loc::upper (b.getButtonText());
        const auto area = b.getLocalBounds().reduced (6);
        const auto f = fitButtonFont (txt, area.getWidth(), 12.f, 9.f);
        g.setFont (f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, txt, 0.f, 0.f);
        if (ga.getBoundingBox (0, -1, false).getWidth() <= (float) area.getWidth())
            g.drawText (txt, area, juce::Justification::centred, false);
        else   // still too long at the smallest size: wrap on two lines instead of squeezing the letters
            g.drawFittedText (txt, area, juce::Justification::centred, 2, 1.0f);
    }
};
