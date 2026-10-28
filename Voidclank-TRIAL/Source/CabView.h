#pragma once

#include "PluginProcessor.h"
#include "VoidLookAndFeel.h"

// Animated cabinet + microphone visualisation (front view of the speakers, side view of the mic).
class CabView : public juce::Component
{
public:
    CabView (MetalBassForgeAudioProcessor& p, const Theme& t) : proc (p), theme (t) {}

    void setLevel (float l) { level = juce::jlimit (0.f, 1.f, l); }

    void paint (juce::Graphics& g) override
    {
        refreshIrIfNeeded();

        const int cab = (int) proc.apvts.getRawParameterValue ("cabType")->load();
        const int mic = (int) proc.apvts.getRawParameterValue ("micType")->load();
        const float pos  = proc.apvts.getRawParameterValue ("micPos")->load();
        const float dist = proc.apvts.getRawParameterValue ("micDist")->load();

        auto area = getLocalBounds().toFloat().reduced (6.f);
        auto sideR = area.removeFromBottom (128.f);
        auto frontR = area.withTrimmedBottom (6.f);

        drawFront (g, frontR, cab, pos);
        drawSide (g, sideR, cab, mic, pos, dist);
    }

private:
    MetalBassForgeAudioProcessor& proc;
    const Theme& theme;
    float level = 0.f;
    juce::String irShown;
    std::vector<float> irEnv;

    static const char* cabName (int c)
    {
        static const char* n[] = { "CAB OFF", "8x10 MODERN", "4x12 CLOSED", "2x15 VINTAGE", "1x15 TIGHT", "USER IR",
                                   "4x10 ATMOSPHERIC", "4x12 MODERN CLANK", "2x12 MELODIC", "4x12 CLASSIC THRASH", "8x10 HEAVY CLASSIC", "4x12 BLACKENED" };
        return n[juce::jlimit (0, 11, c)];
    }

    void refreshIrIfNeeded()
    {
        const auto name = proc.getUserIRName();
        if (name == irShown) return;
        irShown = name;
        irEnv.clear();
        if (name.isEmpty()) return;

        const auto data = proc.getUserIRData();
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (
            fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data.getData(), data.getSize(), false)));
        if (r == nullptr || r->lengthInSamples <= 0) return;

        const int len = (int) juce::jmin ((juce::int64) 400000, r->lengthInSamples);
        juce::AudioBuffer<float> b ((int) juce::jmax (1u, r->numChannels), len);
        r->read (&b, 0, len, 0, true, true);
        const int bins = 180;
        irEnv.assign ((size_t) bins, 0.f);
        for (int i = 0; i < len; ++i)
        {
            const int k = juce::jmin (bins - 1, (int) ((juce::int64) i * bins / len));
            irEnv[(size_t) k] = juce::jmax (irEnv[(size_t) k], std::abs (b.getSample (0, i)));
        }
        const float mx = *std::max_element (irEnv.begin(), irEnv.end());
        if (mx > 0.f) for (auto& v : irEnv) v /= mx;
    }

    //======================================================================
    void drawSpeaker (juce::Graphics& g, juce::Point<float> c, float r, bool active) const
    {
        const float lv = active ? level : 0.f;
        const auto A = theme.a, B = theme.b;

        for (int i = 3; i >= 1; --i)                      // glow follows the output level
        {
            g.setColour (A.withAlpha ((0.04f + lv * 0.10f) * (float) (4 - i)));
            g.drawEllipse (c.x - r - (float) i * 2.f, c.y - r - (float) i * 2.f, (r + (float) i * 2.f) * 2.f, (r + (float) i * 2.f) * 2.f, 2.f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f4c), c.x - r, c.y - r, juce::Colour (0xff0d0e14), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);                 // frame
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - r * 0.92f, c.y - r * 0.92f, r * 1.84f, r * 1.84f);
        g.setColour (juce::Colour (0xff1c1f29));                            // rubber surround
        g.drawEllipse (c.x - r * 0.88f, c.y - r * 0.88f, r * 1.76f, r * 1.76f, r * 0.08f);

        const float cr = r * 0.78f;                                         // cone
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c303c), c.x - cr * 0.5f, c.y - cr * 0.6f,
                                                 juce::Colour (0xff07080c), c.x + cr * 0.7f, c.y + cr * 0.8f, true));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f);
        for (int i = 1; i <= 6; ++i)
        {
            const float rr = cr * (float) i / 6.5f;
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f, 1.f);
        }
        g.setColour ((active ? B : juce::Colours::grey).withAlpha (0.25f + lv * 0.5f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.f, cr * 2.f, 1.4f);

        const float dr = r * 0.26f * (1.f + lv * 0.10f);                    // dust cap
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a7080), c.x - dr * 0.4f, c.y - dr * 0.5f,
                                                 juce::Colour (0xff14161d), c.x + dr, c.y + dr, true));
        g.fillEllipse (c.x - dr, c.y - dr, dr * 2.f, dr * 2.f);
        g.setColour (A.withAlpha (0.5f));
        g.drawEllipse (c.x - dr, c.y - dr, dr * 2.f, dr * 2.f, 1.f);
    }

    void drawFront (juce::Graphics& g, juce::Rectangle<float> r, int cab, float pos) const
    {
        int cols = 2, rows = 2; float aspect = 1.f;
        switch (cab)
        {
            case 1: cols = 2; rows = 4; aspect = 0.62f; break;   // 8x10
            case 2: cols = 2; rows = 2; aspect = 1.0f;  break;   // 4x12
            case 3: cols = 1; rows = 2; aspect = 0.60f; break;   // 2x15
            case 4: cols = 1; rows = 1; aspect = 1.0f;  break;   // 1x15
            case 5: cols = 1; rows = 1; aspect = 1.15f; break;   // user IR
            case 6: cols = 2; rows = 2; aspect = 0.80f; break;   // 4x10
            case 7: cols = 2; rows = 2; aspect = 1.0f;  break;   // 4x12
            case 8: cols = 2; rows = 1; aspect = 1.6f;  break;   // 2x12
            case 9: cols = 2; rows = 2; aspect = 1.0f;  break;   // 4x12
            case 10: cols = 2; rows = 4; aspect = 0.62f; break;  // 8x10
            case 11: cols = 2; rows = 2; aspect = 1.0f; break;   // 4x12
            default: break;
        }

        auto avail = r.withTrimmedBottom (cab == 5 ? 52.f : 24.f);
        float h = juce::jmin (avail.getHeight(), avail.getWidth() / aspect);
        auto body = juce::Rectangle<float> (h * aspect, h).withCentre (avail.getCentre());

        // cabinet body + grille cloth
        g.setGradientFill (juce::ColourGradient (theme.panel.brighter (0.25f), body.getX(), body.getY(),
                                                 theme.panel.darker (0.6f), body.getX(), body.getBottom(), false));
        g.fillRoundedRectangle (body, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        for (float x = body.getX() + 3.f; x < body.getRight(); x += 3.f) g.drawVerticalLine ((int) x, body.getY() + 4.f, body.getBottom() - 4.f);
        for (float y = body.getY() + 3.f; y < body.getBottom(); y += 3.f) g.drawHorizontalLine ((int) y, body.getX() + 4.f, body.getRight() - 4.f);
        g.setColour (theme.a.withAlpha (cab == 0 ? 0.25f : 0.8f));
        g.drawRoundedRectangle (body, 10.f, 1.4f);

        const bool active = cab != 0;
        auto cells = body.reduced (10.f);
        const float cw = cells.getWidth() / (float) cols, ch = cells.getHeight() / (float) rows;
        const float rad = juce::jmin (cw, ch) * 0.5f * 0.9f;

        juce::Point<float> first;
        for (int ry = 0; ry < rows; ++ry)
            for (int cx = 0; cx < cols; ++cx)
            {
                const juce::Point<float> c (cells.getX() + cw * ((float) cx + 0.5f), cells.getY() + ch * ((float) ry + 0.5f));
                if (cx == 0 && ry == 0) first = c;
                drawSpeaker (g, c, rad, active);
            }

        if (active)   // capture point of the microphone on the first speaker
        {
            const float rr = pos * rad * 0.78f;
            juce::Path ring;
            ring.addEllipse (first.x - rr, first.y - rr, rr * 2.f, rr * 2.f);
            juce::Path dashed;
            const float dl[] = { 4.f, 4.f };
            juce::PathStrokeType (1.4f).createDashedStroke (dashed, ring, dl, 2);
            g.setColour (theme.b.withAlpha (0.9f));
            if (rr > 3.f) g.fillPath (dashed);
            const juce::Point<float> m (first.x + rr, first.y);
            g.setColour (theme.b);
            g.drawEllipse (m.x - 6.f, m.y - 6.f, 12.f, 12.f, 1.6f);
            g.drawLine (m.x - 10.f, m.y, m.x + 10.f, m.y, 1.2f);
            g.drawLine (m.x, m.y - 10.f, m.x, m.y + 10.f, 1.2f);
        }
        else
        {
            g.setColour (theme.a.withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions (16.f, juce::Font::bold).withKerningFactor (0.2f)));
            g.drawText ("CAB OFF", body, juce::Justification::centred);
        }

        auto label = juce::Rectangle<float> (r.getX(), body.getBottom() + 3.f, r.getWidth(), 16.f);
        g.setColour (theme.a);
        g.setFont (juce::Font (juce::FontOptions (11.f, juce::Font::bold).withKerningFactor (0.25f)));
        g.drawText (cabName (cab), label, juce::Justification::centred);

        if (cab == 5)   // impulse-response waveform
        {
            auto w = juce::Rectangle<float> (r.getX() + 20.f, label.getBottom() + 2.f, r.getWidth() - 40.f, 30.f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (w, 4.f);
            if (! irEnv.empty())
            {
                juce::Path p;
                for (size_t i = 0; i < irEnv.size(); ++i)
                {
                    const float x = w.getX() + w.getWidth() * (float) i / (float) (irEnv.size() - 1);
                    const float hh = irEnv[i] * w.getHeight() * 0.48f;
                    p.addLineSegment ({ x, w.getCentreY() - hh, x, w.getCentreY() + hh }, 0.6f);
                }
                g.setColour (theme.b.withAlpha (0.9f));
                g.fillPath (p);
            }
            else
            {
                g.setColour (theme.a.withAlpha (0.5f));
                g.setFont (juce::Font (juce::FontOptions (10.f)));
                g.drawText ("no IR loaded", w, juce::Justification::centred);
            }
        }
    }

    //======================================================================
    void drawMicBody (juce::Graphics& g, int type, float tx, float ty) const
    {
        auto fillMetal = [&] (juce::Rectangle<float> rc, float rad)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6d7482), rc.getX(), rc.getY(),
                                                     juce::Colour (0xff1a1d26), rc.getX(), rc.getBottom(), false));
            g.fillRoundedRectangle (rc, rad);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawLine (rc.getX() + rad, rc.getY() + 2.f, rc.getRight() - rad, rc.getY() + 2.f, 1.f);
        };

        if (type == 0)          // dynamic: stubby mesh head + body
        {
            fillMetal ({ tx + 60.f, ty - 8.f, 56.f, 16.f }, 4.f);
            juce::Path neck;
            neck.addQuadrilateral (tx + 36.f, ty - 13.f, tx + 60.f, ty - 8.f, tx + 60.f, ty + 8.f, tx + 36.f, ty + 13.f);
            g.setColour (juce::Colour (0xff2a2f3a)); g.fillPath (neck);
            fillMetal ({ tx, ty - 13.f, 38.f, 26.f }, 9.f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            for (float x = tx + 4.f; x < tx + 36.f; x += 3.f) g.drawLine (x, ty - 10.f, x, ty + 10.f, 0.8f);
            g.setColour (theme.a);
            g.fillRect (tx + 37.f, ty - 13.f, 3.f, 26.f);
        }
        else if (type == 1)     // ribbon: tall slotted head
        {
            fillMetal ({ tx + 86.f, ty - 6.f, 36.f, 12.f }, 4.f);
            fillMetal ({ tx + 66.f, ty - 8.f, 24.f, 16.f }, 4.f);
            fillMetal ({ tx, ty - 14.f, 70.f, 28.f }, 7.f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            for (float y = ty - 10.f; y <= ty + 10.f; y += 4.f) g.drawLine (tx + 6.f, y, tx + 62.f, y, 1.2f);
            g.setColour (theme.a);
            g.fillRect (tx + 69.f, ty - 14.f, 3.f, 28.f);
        }
        else                    // condenser: round head in a shock-mount ring
        {
            fillMetal ({ tx + 58.f, ty - 7.f, 58.f, 14.f }, 4.f);
            fillMetal ({ tx + 30.f, ty - 6.f, 32.f, 12.f }, 3.f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a909e), tx + 6.f, ty - 16.f, juce::Colour (0xff1c1f29), tx + 30.f, ty + 16.f, false));
            g.fillEllipse (tx, ty - 17.f, 34.f, 34.f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            for (int i = -3; i <= 3; ++i) g.drawLine (tx + 17.f + (float) i * 4.f, ty - 14.f, tx + 17.f + (float) i * 4.f, ty + 14.f, 0.7f);
            for (int i = -3; i <= 3; ++i) g.drawLine (tx + 3.f, ty + (float) i * 4.f, tx + 31.f, ty + (float) i * 4.f, 0.7f);
            g.setColour (theme.a);
            g.drawEllipse (tx - 5.f, ty - 22.f, 44.f, 44.f, 1.6f);
        }
    }

    void drawSide (juce::Graphics& g, juce::Rectangle<float> r, int cab, int mic, float pos, float dist) const
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (theme.a.withAlpha (0.25f));
        g.drawRoundedRectangle (r, 8.f, 1.f);

        const float cy = r.getCentreY() + 6.f, R = 40.f;
        const float bx = r.getX() + 46.f;

        // speaker profile + baffle
        g.setColour (juce::Colour (0xff1a1d26));
        g.fillRect (bx - 8.f, r.getY() + 8.f, 14.f, r.getHeight() - 16.f);
        g.setColour (theme.a.withAlpha (0.5f));
        g.drawRect (bx - 8.f, r.getY() + 8.f, 14.f, r.getHeight() - 16.f, 1.f);

        juce::Path cone;
        cone.startNewSubPath (bx - 6.f, cy - R);
        cone.lineTo (bx - 34.f, cy - R * 0.22f);
        cone.lineTo (bx - 34.f, cy + R * 0.22f);
        cone.lineTo (bx - 6.f, cy + R);
        g.setColour (theme.a.withAlpha (cab == 0 ? 0.04f : 0.14f + level * 0.3f));
        g.fillPath (cone);
        g.setColour (theme.a.withAlpha (cab == 0 ? 0.3f : 0.9f));
        g.strokePath (cone, juce::PathStrokeType (1.4f));
        g.fillEllipse (bx - 38.f, cy - 6.f, 8.f, 12.f);

        if (cab == 0)
            return;

        const float ty = cy - pos * R * 0.9f;
        const float maxGap = r.getWidth() - 46.f - 140.f;
        const float tx = bx + 12.f + dist * juce::jmax (10.f, maxGap);

        // aim line
        juce::Path line;
        line.startNewSubPath (tx, ty);
        line.lineTo (bx + 2.f, ty);
        juce::Path dashed;
        const float dl[] = { 4.f, 3.f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, line, dl, 2);
        g.setColour (theme.b.withAlpha (0.9f));
        g.fillPath (dashed);
        g.fillEllipse (bx - 2.f, ty - 3.f, 6.f, 6.f);

        drawMicBody (g, mic, tx, ty);

        g.setColour (theme.a.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions (9.f, juce::Font::bold).withKerningFactor (0.2f)));
        static const char* micNames[] = { "DYNAMIC", "RIBBON", "CONDENSER" };
        g.drawText (micNames[juce::jlimit (0, 2, mic)], juce::Rectangle<float> (r.getRight() - 120.f, r.getY() + 6.f, 112.f, 12.f),
                    juce::Justification::centredRight);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("CLOSE  >  FAR", juce::Rectangle<float> (bx + 10.f, r.getBottom() - 16.f, 140.f, 12.f), juce::Justification::centredLeft);
        g.drawText ("CAP  >  EDGE", juce::Rectangle<float> (r.getX() + 6.f, r.getY() + 6.f, 90.f, 12.f), juce::Justification::centredLeft);
    }
};
