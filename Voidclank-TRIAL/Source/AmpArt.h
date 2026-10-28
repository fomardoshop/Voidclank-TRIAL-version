#pragma once

#include <JuceHeader.h>
#include "VoidLookAndFeel.h"

// Per-amp visual identity: palette + procedurally drawn artwork (original designs).
namespace amp_art
{
    struct Skin
    {
        const char* name;
        const char* tagline;
        juce::uint32 a, b, bgTop, bgBot, panel, plate;
        int motif;
    };

    inline const Skin skins[] = {
        { "MODERN ETHER",   "AIRY  /  POLISHED",     0xff7af0d0, 0xff9ab8ff, 0xff09161c, 0xff04090e, 0xff0d1b22, 0xff15303a, 0 },
        { "METALCORE EDGE", "TIGHT  /  CLANKY",      0xff4aa8ff, 0xffff4d6d, 0xff0b1220, 0xff04070d, 0xff0e1426, 0xff1b2a46, 1 },
        { "THRASH STEEL",   "CRUNCH  /  DYNAMIC",    0xffffb02e, 0xffff6a1a, 0xff17120b, 0xff070504, 0xff1b150e, 0xff36291a, 2 },
        { "RAW GRIND",      "ABRASIVE  /  RAW",      0xffff3b3b, 0xffffa07a, 0xff170808, 0xff060202, 0xff1e0c0c, 0xff3a1414, 3 },
        { "SLAM ABYSS",     "SUBBY  /  HEAVY",       0xffa45cff, 0xff3d5bff, 0xff0f0a1e, 0xff05030d, 0xff150e2a, 0xff271a4a, 4 },
        { "CRYPT DEATH",    "GRINDING  /  OLD-SCHOOL", 0xff9bff4a, 0xffe4e0b8, 0xff0c140a, 0xff040703, 0xff111b0e, 0xff22341a, 5 },
    };
    constexpr int numSkins = (int) (sizeof (skins) / sizeof (skins[0]));

    inline const Skin& skin (int i) { return skins[juce::jlimit (0, numSkins - 1, i)]; }

    // Hue rotation applied on the EFFECTS page so it never looks like the main page.
    // Set by the editor right before it draws (UI thread only).
    inline float hueShift = 0.f;
    inline juce::Colour shiftHue (juce::Colour c) { return hueShift == 0.f ? c : c.withRotatedHue (hueShift); }

    inline Theme themeFor (int i, float shift = 0.f)
    {
        const auto& s = skin (i);
        auto sh = [shift] (juce::uint32 v) { juce::Colour c (v); return shift == 0.f ? c : c.withRotatedHue (shift); };
        Theme t;
        t.a = sh (s.a);  t.b = sh (s.b);
        t.bgTop = sh (s.bgTop);  t.bgBot = sh (s.bgBot);
        t.panel = sh (s.panel);
        return t;
    }

    //==========================================================================
    // Big, low-contrast artwork behind the whole window.
    inline void drawBackdrop (juce::Graphics& g, float w, float h, int idx)
    {
        const auto& s = skin (idx);
        const juce::Colour A = shiftHue (juce::Colour (s.a)), B = shiftHue (juce::Colour (s.b));
        juce::Random rnd (1234 + idx);

        switch (s.motif)
        {
            case 0:   // ETHER: soft concentric rings + drifting waves + particles
            {
                const float cx = w * 0.82f, cy = h * 0.34f;
                for (int i = 0; i < 16; ++i)
                {
                    const float r = 50.f + (float) i * 42.f;
                    g.setColour (A.withAlpha (juce::jmax (0.015f, 0.14f - (float) i * 0.008f)));
                    g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 1.4f);
                }
                for (int k = 0; k < 6; ++k)
                {
                    juce::Path p;
                    const float y0 = h * (0.25f + 0.11f * (float) k);
                    p.startNewSubPath (0.f, y0);
                    for (float x = 0.f; x <= w; x += 6.f)
                        p.lineTo (x, y0 + std::sin (x * 0.012f + (float) k * 0.9f) * (14.f + (float) k * 3.f));
                    g.setColour ((k % 2 ? B : A).withAlpha (0.08f));
                    g.strokePath (p, juce::PathStrokeType (1.6f));
                }
                for (int i = 0; i < 70; ++i)
                {
                    g.setColour (A.withAlpha (rnd.nextFloat() * 0.25f));
                    g.fillEllipse (rnd.nextFloat() * w, rnd.nextFloat() * h, 1.5f + rnd.nextFloat() * 2.5f, 1.5f + rnd.nextFloat() * 2.5f);
                }
                break;
            }
            case 1:   // EDGE: sharp angular shards
            {
                for (int i = 0; i < 26; ++i)
                {
                    const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
                    const float sz = 50.f + rnd.nextFloat() * 170.f;
                    const float ang = -0.9f + rnd.nextFloat() * 0.5f;
                    juce::Path p;
                    p.startNewSubPath (x, y);
                    p.lineTo (x + std::cos (ang) * sz, y + std::sin (ang) * sz);
                    p.lineTo (x + std::cos (ang + 1.1f) * sz * 0.5f, y + std::sin (ang + 1.1f) * sz * 0.5f);
                    p.closeSubPath();
                    g.setColour ((i % 5 == 0 ? B : A).withAlpha (0.045f));
                    g.fillPath (p);
                    g.setColour ((i % 5 == 0 ? B : A).withAlpha (0.16f));
                    g.strokePath (p, juce::PathStrokeType (1.f));
                }
                break;
            }
            case 2:   // STEEL: brushed metal lines + lightning bolt + rivets
            {
                g.setColour (A.withAlpha (0.035f));
                for (float y = 0.f; y < h; y += 3.f) g.drawHorizontalLine ((int) y, 0.f, w);
                juce::Path bolt;
                float x = w * 0.78f, y = 0.f;
                bolt.startNewSubPath (x, y);
                while (y < h)
                {
                    y += 38.f + rnd.nextFloat() * 30.f;
                    x += (rnd.nextBool() ? 1.f : -1.f) * (26.f + rnd.nextFloat() * 36.f);
                    bolt.lineTo (x, y);
                }
                g.setColour (A.withAlpha (0.07f));
                g.strokePath (bolt, juce::PathStrokeType (10.f));
                g.setColour (B.withAlpha (0.28f));
                g.strokePath (bolt, juce::PathStrokeType (1.6f));
                for (float rx = 14.f; rx < w; rx += 52.f)
                    for (float ry : { 8.f, h - 8.f })
                    {
                        g.setColour (A.withAlpha (0.2f));
                        g.fillEllipse (rx - 2.5f, ry - 2.5f, 5.f, 5.f);
                    }
                break;
            }
            case 3:   // GRIND: barbed wire, drips, saw teeth
            {
                for (int k = 0; k < 2; ++k)
                {
                    const float y0 = k == 0 ? h * 0.30f : h * 0.74f;
                    juce::Path wire;
                    wire.startNewSubPath (0.f, y0);
                    for (float x = 0.f; x <= w; x += 5.f)
                        wire.lineTo (x, y0 + std::sin (x * 0.03f + (float) k) * 9.f);
                    g.setColour (A.withAlpha (0.20f));
                    g.strokePath (wire, juce::PathStrokeType (1.8f));
                    for (float x = 30.f; x < w; x += 70.f)
                    {
                        const float yy = y0 + std::sin (x * 0.03f + (float) k) * 9.f;
                        g.drawLine (x - 7.f, yy - 7.f, x + 7.f, yy + 7.f, 1.6f);
                        g.drawLine (x - 7.f, yy + 7.f, x + 7.f, yy - 7.f, 1.6f);
                    }
                }
                for (int i = 0; i < 16; ++i)    // drips from the header
                {
                    const float x = 20.f + rnd.nextFloat() * (w - 40.f);
                    const float len = 24.f + rnd.nextFloat() * 110.f;
                    juce::Path d;
                    d.addRoundedRectangle (x - 2.5f, 66.f, 5.f, len, 2.5f);
                    d.addEllipse (x - 5.f, 66.f + len - 4.f, 10.f, 12.f);
                    g.setColour (A.withAlpha (0.16f));
                    g.fillPath (d);
                }
                juce::Path saw;
                saw.startNewSubPath (0.f, h - 40.f);
                for (float x = 0.f; x < w; x += 22.f)
                {
                    saw.lineTo (x + 11.f, h - 58.f);
                    saw.lineTo (x + 22.f, h - 40.f);
                }
                g.setColour (A.withAlpha (0.18f));
                g.strokePath (saw, juce::PathStrokeType (1.4f));
                break;
            }
            case 4:   // ABYSS: vortex spirals + falling lines
            {
                const float cx = w * 0.5f, cy = h * 0.52f;
                for (int arm = 0; arm < 4; ++arm)
                {
                    juce::Path sp;
                    for (float t = 0.f; t < 22.f; t += 0.08f)
                    {
                        const float r = 8.f + t * 16.f;
                        const float ang = t + (float) arm * juce::MathConstants<float>::halfPi;
                        const float px = cx + std::cos (ang) * r * 1.5f, py = cy + std::sin (ang) * r;
                        if (t == 0.f) sp.startNewSubPath (px, py); else sp.lineTo (px, py);
                    }
                    g.setColour ((arm % 2 ? B : A).withAlpha (0.11f));
                    g.strokePath (sp, juce::PathStrokeType (1.6f));
                }
                for (int i = 0; i < 40; ++i)
                {
                    const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h, l = 14.f + rnd.nextFloat() * 50.f;
                    g.setColour (A.withAlpha (0.05f + rnd.nextFloat() * 0.1f));
                    g.drawLine (x, y, x, y + l, 1.f);
                }
                break;
            }
            default:  // CRYPT: gothic arches + fog
            {
                for (int row = 0; row < 3; ++row)
                    for (float x = -20.f + (row % 2) * 45.f; x < w + 40.f; x += 90.f)
                    {
                        const float base = h * (0.35f + 0.22f * (float) row), aw = 70.f - (float) row * 6.f, ah = 120.f;
                        juce::Path arch;
                        arch.startNewSubPath (x, base);
                        arch.lineTo (x, base - ah * 0.55f);
                        arch.quadraticTo (x, base - ah * 0.9f, x + aw * 0.5f, base - ah);
                        arch.quadraticTo (x + aw, base - ah * 0.9f, x + aw, base - ah * 0.55f);
                        arch.lineTo (x + aw, base);
                        g.setColour (A.withAlpha (0.09f - (float) row * 0.02f));
                        g.strokePath (arch, juce::PathStrokeType (1.4f));
                    }
                g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.f, h * 0.55f,
                                                         B.withAlpha (0.10f), 0.f, h, false));
                g.fillRect (0.f, h * 0.55f, w, h * 0.45f);
                break;
            }
        }
    }

    //==========================================================================
    // Amp-head faceplate drawn inside the AMP section (r = section rect).
    inline void drawFace (juce::Graphics& g, juce::Rectangle<float> r, int idx)
    {
        const auto& s = skin (idx);
        const juce::Colour A (s.a), B (s.b), plate (s.plate);

        // faceplate
        g.setGradientFill (juce::ColourGradient (plate.brighter (0.15f), r.getX(), r.getY(),
                                                 plate.darker (0.55f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (juce::Colours::white.withAlpha (0.03f));       // brushed texture
        for (float y = r.getY() + 2.f; y < r.getBottom(); y += 2.f)
            g.drawHorizontalLine ((int) y, r.getX() + 6.f, r.getRight() - 6.f);

        for (int i = 3; i >= 1; --i)
        {
            g.setColour (A.withAlpha (0.045f * (float) (4 - i)));
            g.drawRoundedRectangle (r.expanded ((float) i), 8.f, 1.6f);
        }
        g.setColour (A.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 8.f, 1.2f);

        // screws
        for (auto c : { r.getTopRight() + juce::Point<float> (-9.f, 9.f),
                        r.getBottomLeft() + juce::Point<float> (9.f, -9.f), r.getBottomRight() + juce::Point<float> (-9.f, -9.f) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7.f, 7.f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawEllipse (c.x - 3.5f, c.y - 3.5f, 7.f, 7.f, 0.8f);
            g.drawLine (c.x - 2.f, c.y + 1.f, c.x + 2.f, c.y - 1.f, 0.9f);
        }

        // pilot lamp
        const auto lamp = juce::Point<float> (r.getX() + 222.f, r.getY() + 12.f);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (B.withAlpha (0.08f * (float) (5 - i)));
            g.fillEllipse (lamp.x - (float) i * 2.f - 3.f, lamp.y - (float) i * 2.f - 3.f, 6.f + (float) i * 4.f, 6.f + (float) i * 4.f);
        }
        g.setColour (B.brighter (0.5f));
        g.fillEllipse (lamp.x - 3.f, lamp.y - 3.f, 6.f, 6.f);

        // grille behind the description text
        {
            auto gr = juce::Rectangle<float> (r.getX() + 12.f, r.getY() + 60.f, 3.f * 96.f - 24.f, r.getHeight() - 72.f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (gr, 5.f);
            g.setColour (A.withAlpha (0.10f));
            for (float x = gr.getX() + 4.f; x < gr.getRight(); x += 5.f)
                g.drawVerticalLine ((int) x, gr.getY() + 3.f, gr.getBottom() - 3.f);
            g.setColour (A.withAlpha (0.25f));
            g.drawRoundedRectangle (gr, 5.f, 1.f);
        }

        // name plate + ornament (top right)
        auto plateR = juce::Rectangle<float> (r.getRight() - 300.f, r.getY() + 4.f, 280.f, 18.f);
        g.setColour (A);
        g.setFont (juce::Font (juce::FontOptions (13.f, juce::Font::bold).withKerningFactor (0.28f)));
        g.drawText (s.name, plateR.removeFromRight (190.f), juce::Justification::centredRight);

        // ornament
        const float ox = r.getRight() - 300.f + 6.f, oy = r.getY() + 13.f;
        g.setColour (A.withAlpha (0.85f));
        switch (s.motif)
        {
            case 0: for (int k = 0; k < 3; ++k) { juce::Path p; p.startNewSubPath (ox, oy + (float) (k - 1) * 4.f);
                        for (float x = 0; x <= 60.f; x += 2.f) p.lineTo (ox + x, oy + (float) (k - 1) * 4.f + std::sin (x * 0.2f + (float) k) * 2.f);
                        g.strokePath (p, juce::PathStrokeType (1.f)); } break;
            case 1: for (int k = 0; k < 4; ++k) { juce::Path p; p.startNewSubPath (ox + (float) k * 12.f, oy - 6.f);
                        p.lineTo (ox + (float) k * 12.f + 8.f, oy); p.lineTo (ox + (float) k * 12.f, oy + 6.f);
                        g.strokePath (p, juce::PathStrokeType (1.6f)); } break;
            case 2: { juce::Path p; p.startNewSubPath (ox, oy - 6.f); p.lineTo (ox + 14.f, oy + 1.f); p.lineTo (ox + 6.f, oy + 1.f);
                      p.lineTo (ox + 22.f, oy + 7.f); p.lineTo (ox + 12.f, oy - 2.f); p.lineTo (ox + 20.f, oy - 2.f); p.closeSubPath();
                      g.fillPath (p); } break;
            case 3: for (int k = 0; k < 7; ++k) { juce::Path p; p.addTriangle (ox + (float) k * 9.f, oy + 6.f, ox + (float) k * 9.f + 4.5f, oy - 7.f, ox + (float) k * 9.f + 9.f, oy + 6.f);
                        g.fillPath (p); } break;
            case 4: for (int k = 1; k <= 4; ++k) g.drawEllipse (ox + 20.f - (float) k * 4.f, oy - (float) k * 1.8f, (float) k * 8.f, (float) k * 3.6f, 1.f); break;
            default: for (int k = 0; k < 4; ++k) { juce::Path p; const float x = ox + (float) k * 14.f;
                        p.startNewSubPath (x, oy + 7.f); p.lineTo (x, oy - 1.f); p.quadraticTo (x, oy - 7.f, x + 5.f, oy - 8.f);
                        p.quadraticTo (x + 10.f, oy - 7.f, x + 10.f, oy - 1.f); p.lineTo (x + 10.f, oy + 7.f);
                        g.strokePath (p, juce::PathStrokeType (1.3f)); } break;
        }
    }
}
