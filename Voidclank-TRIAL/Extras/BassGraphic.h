#pragma once

#include <JuceHeader.h>
#include "VoidLookAndFeel.h"

//==============================================================================
// Hero graphic: an original multi-scale ("fanned fret") 4-string bass drawn with vector
// graphics - headstock on the right, body on the left.  The strings vibrate with the
// plugin's output level.
//==============================================================================
namespace bass
{
    using Pt = juce::Point<float>;

    constexpr float heroTop = 108.f, heroH = 130.f;
    constexpr float cy = heroTop + heroH * 0.5f;
    constexpr float nutX = 506.f;
    constexpr float winW = 582.f;
    constexpr int numFrets = 21;

    inline juce::Font font (float h, bool bold = false, float kern = 0.f)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)
                               .withKerningFactor (kern));
    }

    inline Pt startPt (int i) { return { nutX, cy - 12.f + 8.f * (float) i }; }
    // bass side (i = 0) has the longer scale, treble side the shorter -> frets fan out
    inline Pt endPt (int i)   { return { 70.f + 6.f * (float) i, cy - 20.f + 13.33f * (float) i }; }
    inline Pt pt (int i, float t) { auto s = startPt (i), e = endPt (i); return s + (e - s) * t; }
    inline float fretT (int n) { return 1.f - std::pow (2.f, -(float) n / 12.f); }

    inline void edge (float t, Pt& top, Pt& bot, float ext = 0.9f)
    {
        const auto p0 = pt (0, t), p3 = pt (3, t);
        const auto d = (p3 - p0) / 3.f;
        top = p0 - d * ext;
        bot = p3 + d * ext;
    }

    inline juce::Path quad (Pt a, Pt b, Pt c, Pt d)
    {
        juce::Path p;
        p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath();
        return p;
    }

    inline void chrome (juce::Graphics& g, Pt c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f6fb), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 juce::Colour (0xff2c3040), c.x + r * 0.7f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r, 0.8f);
    }

    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (c.withAlpha (0.05f * (float) (4 - i)));
            g.strokePath (p, juce::PathStrokeType (w + (float) i * 3.f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline juce::ColourGradient maple (float y0, float y1, float x = 0.f)
    {
        juce::ColourGradient gr (juce::Colour (0xffe2bb7e), x, y0, juce::Colour (0xff8f5f2c), x, y1, false);
        gr.addColour (0.45, juce::Colour (0xffcfa264));
        return gr;
    }

    //==========================================================================
    inline void drawStatic (juce::Graphics& g)
    {
        // soft aura behind the instrument
        g.setGradientFill (juce::ColourGradient (vc::magenta.withAlpha (0.16f), 150.f, cy,
                                                 juce::Colours::transparentBlack, 150.f, cy - 150.f, true));
        g.fillRect (juce::Rectangle<float> (0.f, heroTop - 6.f, 330.f, heroH + 12.f));
        g.setGradientFill (juce::ColourGradient (vc::cyan.withAlpha (0.08f), 460.f, cy,
                                                 juce::Colours::transparentBlack, 460.f, cy - 120.f, true));
        g.fillRect (juce::Rectangle<float> (330.f, heroTop - 6.f, 252.f, heroH + 12.f));

        // ---------------- body (mirrored contour, rounded for an organic look)
        const float src[][2] = { { 296, -18 }, { 340, -30 }, { 366, -56 }, { 392, -58 }, { 420, -40 },
                                 { 468, -34 }, { 520, -46 }, { 554, -30 }, { 558, 8 }, { 548, 44 },
                                 { 512, 58 }, { 450, 56 }, { 404, 46 }, { 372, 62 }, { 344, 60 },
                                 { 322, 32 }, { 296, 20 } };
        juce::Path poly;
        for (size_t i = 0; i < sizeof (src) / sizeof (src[0]); ++i)
        {
            const Pt p { winW - src[i][0], cy + src[i][1] };
            if (i == 0) poly.startNewSubPath (p); else poly.lineTo (p);
        }
        poly.closeSubPath();
        const auto body = poly.createPathWithRoundedCorners (22.f);

        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 12, { 2, 5 }).drawForPath (g, body);

        // glossy black/purple finish
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35255e), 150.f, cy - 62.f,
                                                 juce::Colour (0xff09080f), 150.f, cy + 62.f, false));
        g.fillPath (body);
        {
            juce::ColourGradient sheen (juce::Colours::transparentWhite, 24.f, cy - 60.f,
                                        juce::Colours::transparentWhite, 290.f, cy + 60.f, false);
            sheen.addColour (0.42, juce::Colours::white.withAlpha (0.10f));
            sheen.addColour (0.55, juce::Colours::transparentWhite);
            g.setGradientFill (sheen);
            g.fillPath (body);
        }

        // bevelled edge (inner shadow + highlight), clipped to the body
        g.saveState();
        g.reduceClipRegion (body, {});
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (body, juce::PathStrokeType (10.f));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        auto hl = body;
        hl.applyTransform (juce::AffineTransform::translation (1.5f, 2.f));
        g.strokePath (hl, juce::PathStrokeType (2.f));
        g.restoreState();

        // contour line + neon rim light
        auto inner = body;
        inner.applyTransform (juce::AffineTransform::scale (0.88f, 0.80f, 150.f, cy));
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.strokePath (inner, juce::PathStrokeType (1.f));

        g.setGradientFill (juce::ColourGradient (vc::magenta, 24.f, 0.f, vc::cyan, 290.f, 0.f, false));
        for (int i = 3; i >= 1; --i)
        {
            g.setOpacity (0.05f * (float) (4 - i));
            g.strokePath (body, juce::PathStrokeType (1.4f + (float) i * 3.f));
        }
        g.setOpacity (0.8f);
        g.strokePath (body, juce::PathStrokeType (1.4f));
        g.setOpacity (1.f);

        // ---------------- neck (maple) with ebony fretboard
        Pt wt0, wb0, wt1, wb1;
        edge (0.f, wt0, wb0, 0.78f);
        edge (fretT (numFrets) + 0.012f, wt1, wb1, 0.78f);
        const auto neck = quad (wt0, wt1, wb1, wb0);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 3 }).drawForPath (g, neck);
        g.setGradientFill (maple (cy - 22.f, cy + 22.f));
        g.fillPath (neck);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (neck, juce::PathStrokeType (1.f));

        Pt t0, b0, t1, b1;
        edge (0.f, t0, b0, 0.55f);
        edge (fretT (numFrets), t1, b1, 0.55f);
        const auto board = quad (t0, t1, b1, b0);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff231a14), nutX, cy - 20.f,
                                                 juce::Colour (0xff0a0807), nutX, cy + 20.f, false));
        g.fillPath (board);
        g.setColour (juce::Colours::white.withAlpha (0.035f));   // faint grain
        for (int k = 1; k < 7; ++k)
        {
            const float f = (float) k / 7.f;
            g.drawLine (juce::Line<float> (t0 + (b0 - t0) * f, t1 + (b1 - t1) * f), 0.8f);
        }

        // frets (fanned), with a tiny shadow
        for (int n = 1; n <= numFrets; ++n)
        {
            Pt a, b;
            edge (fretT (n), a, b, 0.55f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawLine (a.x - 1.2f, a.y, b.x - 1.2f, b.y, 1.4f);
            g.setColour (juce::Colour (0xffdadee8).withAlpha (0.9f));
            g.drawLine (a.x, a.y, b.x, b.y, 1.1f);
        }

        // pearl inlays
        for (int n : { 3, 5, 7, 9, 12, 15, 17, 19 })
        {
            const float t = (fretT (n - 1) + fretT (n)) * 0.5f;
            Pt top, bot;
            edge (t, top, bot, 0.f);
            const auto c = (top + bot) * 0.5f;
            auto dot = [&] (float oy)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - 1.f, c.y + oy - 1.f,
                                                         juce::Colour (0xff9fb0d8), c.x + 2.f, c.y + oy + 2.f, true));
                g.fillEllipse (c.x - 2.4f, c.y + oy - 2.4f, 4.8f, 4.8f);
            };
            if (n == 12) { dot (-6.f); dot (6.f); } else dot (0.f);
        }

        // nut
        {
            Pt a, b; edge (0.f, a, b, 0.55f);
            g.setColour (juce::Colour (0xfff2e9d4));
            g.drawLine (a.x, a.y, b.x, b.y, 3.4f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawLine (a.x + 1.8f, a.y, b.x + 1.8f, b.y, 1.f);
        }

        // ---------------- headstock (right)
        {
            juce::Path hs;
            hs.startNewSubPath (nutX, cy - 14.f);
            hs.lineTo (winW - 58.f, cy - 22.f);
            hs.lineTo (winW - 22.f, cy - 17.f);
            hs.lineTo (winW - 22.f, cy + 17.f);
            hs.lineTo (winW - 58.f, cy + 22.f);
            hs.lineTo (nutX, cy + 14.f);
            hs.closeSubPath();
            const auto h = hs.createPathWithRoundedCorners (6.f);

            juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 8, { 0, 3 }).drawForPath (g, h);
            g.setGradientFill (maple (cy - 22.f, cy + 22.f));
            g.fillPath (h);

            auto face = h;
            face.applyTransform (juce::AffineTransform::scale (0.86f, 0.80f, winW - 38.f, cy));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2040), winW - 50.f, cy - 16.f,
                                                     juce::Colour (0xff09080f), winW - 30.f, cy + 16.f, false));
            g.fillPath (face);
            g.setColour (vc::magenta.withAlpha (0.55f));
            g.strokePath (face, juce::PathStrokeType (0.8f));

            g.setColour (vc::cyan.withAlpha (0.9f));
            g.setFont (font (7.f, true, 0.3f));
            g.drawText ("BP", (int) (winW - 50.f), (int) (cy - 5.f), 24, 10, juce::Justification::centred);
        }

        // tuners (2 + 2) and the string run from nut to posts
        {
            const Pt posts[4]   = { { 520.f, cy - 14.f }, { 540.f, cy - 16.f }, { 540.f, cy + 16.f }, { 520.f, cy + 14.f } };
            const Pt buttons[4] = { { 520.f, cy - 28.f }, { 540.f, cy - 29.f }, { 540.f, cy + 29.f }, { 520.f, cy + 28.f } };
            for (int i = 0; i < 4; ++i)
            {
                const auto s = startPt (i);
                g.setColour (juce::Colour (0xffb9bfcc).withAlpha (0.8f));
                g.drawLine (s.x, s.y, posts[i].x, posts[i].y, 0.9f + 0.2f * (float) (3 - i));
                g.setColour (juce::Colour (0xff8a90a0));
                g.drawLine (posts[i].x, posts[i].y, buttons[i].x, buttons[i].y, 2.f);
                chrome (g, posts[i], 2.4f);
                chrome (g, buttons[i], 4.6f);
            }
        }

        // ---------------- pickups
        for (auto range : { std::pair<float, float> { 0.795f, 0.835f }, std::pair<float, float> { 0.875f, 0.915f } })
        {
            Pt at, ab, bt, bb;
            edge (range.first, at, ab, 1.0f);
            edge (range.second, bt, bb, 1.0f);
            const auto pk = quad (at, bt, bb, ab);
            juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 4, { 1, 2 }).drawForPath (g, pk);
            g.setColour (juce::Colour (0xff050509));
            g.fillPath (pk);
            g.setColour (juce::Colour (0xffc7ccd8).withAlpha (0.65f));
            g.strokePath (pk, juce::PathStrokeType (1.f));
            for (int i = 0; i < 4; ++i)
                chrome (g, pt (i, (range.first + range.second) * 0.5f), 2.f);
        }

        // ---------------- bridge (chrome)
        {
            Pt at, ab, bt, bb;
            edge (0.975f, at, ab, 0.95f);
            edge (1.03f, bt, bb, 0.95f);
            const auto br = quad (at, bt, bb, ab);
            juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 4, { 1, 2 }).drawForPath (g, br);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef1f8), 0.f, cy - 28.f,
                                                     juce::Colour (0xff3b4052), 0.f, cy + 28.f, false));
            g.fillPath (br);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (br, juce::PathStrokeType (0.8f));
            for (auto c : { at, bt, bb, ab })
                chrome (g, (c + (at + bt + bb + ab) * 0.25f * 1.f) * 0.5f, 1.5f);
            for (int i = 0; i < 4; ++i)
            {
                const auto c = pt (i, 1.f);
                g.setColour (juce::Colour (0xff1a1c26));
                g.fillRoundedRectangle (c.x - 4.f, c.y - 3.2f, 8.f, 6.4f, 1.5f);
                chrome (g, c, 2.4f);
            }
        }

        // ---------------- controls, jack, strap buttons
        for (auto c : { Pt (112.f, cy + 40.f), Pt (88.f, cy + 44.f), Pt (64.f, cy + 40.f) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (c.x - 7.f, c.y - 5.f, 14.f, 14.f);
            chrome (g, c, 6.f);
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.drawLine (c.x, c.y, c.x + 2.5f, c.y - 4.5f, 1.4f);
        }
        chrome (g, { 52.f, cy + 52.f }, 4.2f);
        chrome (g, { 196.f, cy - 50.f }, 3.2f);
        chrome (g, { 30.f, cy + 2.f }, 3.2f);

        // logo on the lower bout
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.setFont (font (10.5f, true, 0.3f));
        g.drawText ("BASSPHEMY", 153, (int) (cy + 27.f) + 1, 140, 16, juce::Justification::centredLeft);
        g.setColour (vc::magenta.withAlpha (0.95f));
        g.drawText ("BASSPHEMY", 152, (int) (cy + 27.f), 140, 16, juce::Justification::centredLeft);
    }

    //==========================================================================
    inline void drawStrings (juce::Graphics& g, float level, float phase)
    {
        const float amp = juce::jmin (6.f, level * 20.f);
        const float thick[4] = { 2.5f, 2.1f, 1.7f, 1.2f };

        for (int i = 0; i < 4; ++i)
        {
            const auto s = startPt (i), e = endPt (i);
            const auto dir = e - s;
            const float len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
            const Pt nrm { -dir.y / len, dir.x / len };
            const float freq = 2.f + 1.15f * (float) i;
            const float a = amp * (1.f - 0.08f * (float) i);

            juce::Path p;
            constexpr int N = 56;
            for (int k = 0; k <= N; ++k)
            {
                const float u = (float) k / (float) N;
                const float disp = a * std::sin (juce::MathConstants<float>::pi * u)
                                     * std::sin (phase * juce::MathConstants<float>::twoPi * freq);
                const Pt q = s + dir * u + nrm * disp;
                if (k == 0) p.startNewSubPath (q); else p.lineTo (q);
            }

            // shadow on the board
            auto sh = p;
            sh.applyTransform (juce::AffineTransform::translation (0.f, 2.4f));
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.strokePath (sh, juce::PathStrokeType (thick[i]));

            // neon glow while it rings
            if (a > 0.4f)
            {
                g.setColour (vc::cyan.interpolatedWith (vc::magenta, (float) i / 3.f).withAlpha (0.14f));
                g.strokePath (p, juce::PathStrokeType (thick[i] + 5.f));
            }

            // steel core
            g.setColour (juce::Colour (i < 3 ? 0xffc3c9d6 : 0xffe9edf7));
            g.strokePath (p, juce::PathStrokeType (thick[i]));

            // windings (wound strings only)
            if (i < 3)
            {
                const float dash[] = { 1.2f, 0.9f };
                juce::Path windings;
                juce::PathStrokeType (thick[i]).createDashedStroke (windings, p, dash, 2);
                g.setColour (juce::Colours::black.withAlpha (0.28f));
                g.fillPath (windings);
            }

            // highlight
            auto hi = p;
            hi.applyTransform (juce::AffineTransform::translation (0.f, -thick[i] * 0.25f));
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.strokePath (hi, juce::PathStrokeType (juce::jmax (0.6f, thick[i] * 0.3f)));
        }
    }
}
