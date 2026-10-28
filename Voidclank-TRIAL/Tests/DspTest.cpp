#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (! (c)) { ++failures; std::printf ("  FAIL: %s (line %d)\n", #c, __LINE__); } } while (0)

static void licenceTests()
{
    std::printf ("--- licence ---\n");

    juce::RSAKey pub, priv;
    juce::RSAKey::createKeyPair (pub, priv, 1024);

    const auto code = LicenseManager::sign ("Buyer@Example.com ", priv);
    CHECK (LicenseManager::verify ("buyer@example.com", code, pub));
    CHECK (! LicenseManager::verify ("other@example.com", code, pub));
    CHECK (! LicenseManager::verify ("buyer@example.com", code.replaceSection (10, 1, "Z"), pub));
    CHECK (! LicenseManager::verify ("buyer@example.com", "garbage", pub));

    // trial behaviour with a fake clock + temp storage
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vc_test_" + juce::String (juce::Random::getSystemRandom().nextInt()));
    juce::Array<juce::File> store { dir.getChildFile ("a/vc.dat"), dir.getChildFile ("b/vc.dat") };
    const juce::int64 day = 86400000LL;
    juce::int64 now = 1800000000000LL;

    {
        LicenseManager lm ([&] { return now; }, store, false);
        CHECK (lm.getState() == LicenseManager::Trial);
        CHECK (lm.getDaysLeft() == 14);
        now += 13 * day + 1000; lm.refresh();
        CHECK (lm.getState() == LicenseManager::Trial);
        CHECK (lm.getDaysLeft() == 1);
    }
    {   // restart: first-run date persists
        LicenseManager lm ([&] { return now; }, store, false);
        CHECK (lm.getState() == LicenseManager::Trial);
        now += day; lm.refresh();
        CHECK (lm.getState() == LicenseManager::Expired);
        std::printf ("  expired after 14 days: ok\n");
    }
    {   // one copy deleted: still expired
        store[0].deleteFile();
        LicenseManager lm ([&] { return now; }, store, false);
        CHECK (lm.getState() == LicenseManager::Expired);
    }
    {   // clock rolled back
        auto s2 = juce::Array<juce::File> { dir.getChildFile("c/vc.dat") };
        now = 1900000000000LL;
        LicenseManager lm ([&] { return now; }, s2, false);
        CHECK (lm.getState() == LicenseManager::Trial);
        now -= 5 * day; lm.refresh();
        CHECK (lm.getState() == LicenseManager::Expired);
        now += 5 * day; lm.refresh();
        CHECK (lm.getState() == LicenseManager::Trial);
    }
    {   // tampered file
        auto f = dir.getChildFile ("d/vc.dat");
        f.getParentDirectory().createDirectory();
        f.replaceWithText ("first=1900000000000\nlast=1900000000000\nemail=\ncode=\nsum=deadbeef\n");
        LicenseManager lm ([&] { return now; }, { f }, false);
        CHECK (lm.getState() == LicenseManager::Expired);
    }
    dir.deleteRecursively();
}

// builds a small WAV in memory: decaying noise burst = fake cabinet IR
static juce::MemoryBlock makeWav (double sr, int len, int channels = 1)
{
    juce::MemoryBlock mb;
    {
        juce::WavAudioFormat wav;
        auto os = std::make_unique<juce::MemoryOutputStream> (mb, false);
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) channels, 16, {}, 0));
        if (w != nullptr)
        {
            os.release();   // writer owns the stream now
            juce::AudioBuffer<float> b (channels, len);
            juce::Random r (7);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < len; ++i)
                    b.setSample (ch, i, (i == 0 ? 1.f : (r.nextFloat() - 0.5f) * 0.4f) * std::exp (-(float) i / (len * 0.15f)));
            w->writeFromAudioSampleBuffer (b, 0, len);
        }
    }
    return mb;
}

#include "../Source/PluginEditor.h"

static void snapshot (const juce::File& out, const juce::String& mode, int ampIdx)
{
    MetalBassForgeAudioProcessor p;
    p.setPlayConfigDetails (1, 1, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);

    auto setParam = [&] (const char* id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    // each amp is shown with the factory preset that is voiced for it
    static const int presetForAmp[6] = { 2, 1, 5, 6, 3, 4 };
    p.setCurrentProgram (mode == "amp" || mode == "cab" ? presetForAmp[juce::jlimit (0, 5, ampIdx)] : 1);
    p.outLevel.store (0.45f);

    {
        juce::String err;
        p.loadUserIRFromMemory (makeWav (48000.0, 4800), "Mesa_4x12_SM57", err);
    }
    if (mode == "cab")
    {
        // a typical cab + mic pairing for each amp model
        static const int cabFor[6]  = { 4, 2, 2, 1, 3, 1 };
        static const int micFor[6]  = { 2, 0, 0, 0, 1, 1 };
        static const float posFor[6]  = { 0.20f, 0.15f, 0.30f, 0.65f, 0.40f, 0.50f };
        static const float distFor[6] = { 0.45f, 0.10f, 0.20f, 0.12f, 0.55f, 0.35f };
        const int a = juce::jlimit (0, 5, ampIdx);
        auto setP = [&] (const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        setP ("cabType", (float) cabFor[a]); setP ("micType", (float) micFor[a]);
        setP ("micPos", posFor[a]); setP ("micDist", distFor[a]);
        if (ampIdx == 6) setP ("cabType", 5.f);
        if (std::getenv ("VC_CAB")) setP ("cabType", (float) std::atoi (std::getenv ("VC_CAB")));   // extra shot with the user IR loaded
    }
    if (mode == "fx")
    {
        setParam ("wahOn", 1); setParam ("wahMode", 1); setParam ("wahDepth", 0.8f);
        setParam ("pitchSemi", -2); setParam ("gateThresh", -48.f);
        if (std::getenv ("VC_FX_OFF")) setParam ("fxOn", 0);
        setParam ("delayMix", 0.25f); setParam ("delayFeedback", 0.4f); setParam ("revMix", 0.2f);

        // feed an E1 (41.2 Hz) so the tuner has something to show
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        long t = 0;
        for (int blk = 0; blk < 40; ++blk)
        {
            for (int i = 0; i < 512; ++i, ++t)
            {
                const float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 41.5 * t / 48000.0);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            p.processBlock (b, m);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    if (mode != "notice")
        if (auto* e = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (ed.get())) e->hideNoticeForTesting();
    if (mode == "activation")
        for (auto* c : ed->getChildren())
            if (dynamic_cast<ActivationPanel*> (c)) c->setVisible (true);
    if (mode == "settings")
        if (auto* e = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (ed.get())) e->showSettingsForTesting (ampIdx);   // ampIdx = tab
    if (mode == "notice")
        if (auto* e = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (ed.get())) e->showNoticeForTesting();
    if (mode == "fx" || mode == "cab")
        if (auto* e = dynamic_cast<MetalBassForgeAudioProcessorEditor*> (ed.get())) e->showPageForTesting (mode == "fx" ? 1 : 2);

    // let the meter ease up and the scan line move
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::PNGImageFormat png;
    juce::FileOutputStream fos (out);
    fos.setPosition (0); fos.truncate();
    png.writeImageToStream (img, fos);
    std::printf ("snapshot written: %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}


// ---------------------------------------------------------------------------
// CPU benchmark:  VoidclankTest --bench
//   real-time load = processing time / audio time, one instance, 48 kHz stereo
static void bench()
{
    std::printf ("=== CPU benchmark (single core, 48 kHz, stereo) ===\n");
    const double sr = 48000.0;
    const char* qn[] = { "Live 0 ms (no oversampling)", "Standard 2x", "HQ 4x" };

    for (int heavy = 0; heavy < 2; ++heavy)
    {
        for (int block : { 64, 512 })
        {
            for (int q = 0; q < 3; ++q)
            {
                MetalBassForgeAudioProcessor p;
                p.setPlayConfigDetails (2, 2, sr, block);
                auto set = [&] (const char* id, float v)
                {
                    auto* prm = p.apvts.getParameter (id);
                    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                };
                set ("quality", (float) q);
                if (heavy)
                {
                    set ("cabType", 2.f); set ("cabAll", 1.f);
                    set ("wahOn", 1.f); set ("pitchSemi", -2.f); set ("pitchMix", 1.f);
                    set ("delayMix", 0.3f); set ("revMix", 0.3f);
                }
                p.prepareToPlay (sr, block);

                juce::AudioBuffer<float> b (2, block);
                juce::MidiBuffer m;
                const int blocks = (int) (sr * 8.0 / block);
                long t = 0;
                auto fill = [&]
                {
                    for (int i = 0; i < block; ++i, ++t)
                    {
                        const float v = 0.35f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * (double) t / sr)
                                      + 0.1f  * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 880.0 * (double) t / sr);
                        b.setSample (0, i, v); b.setSample (1, i, v);
                    }
                };
                for (int i = 0; i < 200; ++i) { fill(); p.processBlock (b, m); }   // warm-up

                double worst = 0.0, total = 0.0;
                for (int i = 0; i < blocks; ++i)
                {
                    fill();
                    const auto t0 = juce::Time::getHighResolutionTicks();
                    p.processBlock (b, m);
                    const double dt = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                    total += dt; worst = juce::jmax (worst, dt);
                }
                const double audioSec = (double) blocks * block / sr;
                std::printf ("%s  block %3d  %-28s  avg %5.2f %%   worst block %5.1f %% of its deadline   (~%d instances/core)\n",
                             heavy ? "ALL ON " : "DEFAULT", block, qn[q], 100.0 * total / audioSec,
                             100.0 * worst / ((double) block / sr), (int) (0.7 * audioSec / total));
            }
        }
    }

    // GUI: cost of one full-window paint vs. what the timer now repaints
    MetalBassForgeAudioProcessor p;
    p.setPlayConfigDetails (2, 2, sr, 512);
    p.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setBounds (0, 0, ed->getWidth(), ed->getHeight());
    auto timeIt = [&] (juce::Rectangle<int> r)
    {
        const int n = 60;
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < n; ++i) { auto img = ed->createComponentSnapshot (r, false, 1.0f); }
        return 1000.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / n;
    };
    const auto full = ed->getLocalBounds();
    const double fullMs = timeIt (full);
    const double stripMs = timeIt ({ 0, 300, full.getWidth(), 24 });
    const double meterMs = timeIt ({ full.getRight() - 60, 100, 50, 540 });
    std::printf ("GUI paint (software renderer, includes snapshot alloc):  full window %.2f ms | scan-line strip %.2f ms | meter %.2f ms\n",
                 fullMs, stripMs, meterMs);
    {   // tuner pitch detection (runs on the message thread ~7x per second while the editor is open)
        std::vector<float> buf (8192);
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 41.2 * (double) i / sr);
        const auto t0 = juce::Time::getHighResolutionTicks();
        float f = 0.f;
        for (int i = 0; i < 50; ++i) f = MetalBassForgeAudioProcessor::detectPitch (buf.data(), (int) buf.size(), sr);
        std::printf ("tuner detectPitch: %.2f ms per call (detected %.1f Hz)\n",
                     1000.0 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / 50.0, f);
    }
    std::printf ("  old: full repaint x 30 fps = %.1f ms/s   new: strip x 15 + meter x 30 = %.1f ms/s (only while the editor is open)\n",
                 fullMs * 30.0, stripMs * 15.0 + meterMs * 30.0);
}


#if VC_WITH_NAM
// VoidclankTest --nam model1.nam [model2.nam ...] : loads each model into the GRIT stage, checks the output and times it
static int namTest (const juce::StringArray& files)
{
    int bad = 0;
    const double sr = 48000.0;
    const int block = 128;
    for (auto& f : files)
    {
        MetalBassForgeAudioProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        auto set = [&] (const char* id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };

        juce::String err;
        const bool ok = p.loadNamModel (juce::File (f), err);
        std::printf ("NAM  %-34s load: %s %s\n", juce::File (f).getFileName().toRawUTF8(), ok ? "ok" : "FAILED", err.toRawUTF8());
        if (! ok) { ++bad; continue; }

        for (int q = 0; q < 3; ++q)
        {
            set ("quality", (float) q);
            set ("soloGrit", 1.f);   // listen to the GRIT band only, where the NAM model sits
            set ("gritLevel", 0.f);
            for (int useNam = 0; useNam < 2; ++useNam)
            {
                set ("namOn", (float) useNam);
                p.prepareToPlay (sr, block);
                juce::AudioBuffer<float> b (2, block);
                juce::MidiBuffer m;
                long t = 0;
                double peak = 0, total = 0, sq = 0; long cnt = 0;
                const int blocks = (int) (sr * 6.0 / block);
                bool finite = true;
                for (int i = 0; i < blocks; ++i)
                {
                    for (int k = 0; k < block; ++k, ++t)
                    {
                        const float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * (double) t / sr);
                        b.setSample (0, k, v); b.setSample (1, k, v);
                    }
                    const auto t0 = juce::Time::getHighResolutionTicks();
                    p.processBlock (b, m);
                    if (i > 100) total += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                    for (int k = 0; k < block; ++k)
                    {
                        const float v = b.getSample (0, k);
                        if (! std::isfinite (v)) finite = false;
                        peak = std::max (peak, (double) std::abs (v));
                        if (i > 100) { sq += (double) v * v; ++cnt; }
                    }
                }
                const double audioSec = (blocks - 101) * (double) block / sr;
                std::printf ("     %-12s %-14s peak %.3f  rms %.1f dBFS  finite=%d  CPU %.2f %%\n",
                             q == 0 ? "Live" : q == 1 ? "Standard 2x" : "HQ 4x", useNam ? "NAM" : "built-in drive",
                             peak, 10.0 * std::log10 (sq / (double) std::max (1L, cnt) + 1e-12), (int) finite, 100.0 * total / audioSec);
                if (! finite || peak < 1e-4) ++bad;
            }
        }
    }
    std::printf (bad == 0 ? "NAM OK\n" : "NAM PROBLEMS: %d\n", bad);
    return bad;
}
#endif

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

   #if VC_WITH_NAM
    if (argc >= 3 && juce::String (argv[1]) == "--nam")
    {
        juce::StringArray files;
        for (int i = 2; i < argc; ++i) files.add (argv[i]);
        return namTest (files) == 0 ? 0 : 1;
    }
   #endif

    if (argc >= 2 && juce::String (argv[1]) == "--bench")
    {
        bench();
        return 0;
    }

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
    {
        if (argc >= 6) loc::langRef().store (juce::String (argv[5]).getIntValue());   // 0=PL 1=EN 2=ES 3=IT 4=DE 5=JA 6=ZH
        snapshot (juce::File (argv[2]), argc >= 4 ? juce::String (argv[3]) : juce::String(),
                  argc >= 5 ? juce::String (argv[4]).getIntValue() : 0);
        return 0;
    }

    licenceTests();

    std::printf ("--- dsp ---\n");
    MetalBassForgeAudioProcessor p;
    const double sr = 48000.0;
    const int bs = 512;
    p.setPlayConfigDetails (1, 1, sr, bs);
    p.prepareToPlay (sr, bs);

    auto run = [&] (float freq, float amp, int blocks, const char* tag)
    {
        double peak = 0, rms = 0; long cnt = 0; bool bad = false;
        long t = 0;
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < bs; ++i, ++t)
            {
                float s = amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * t / sr);
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            p.processBlock (buf, midi);
            if (b > blocks / 2)
                for (int i = 0; i < bs; ++i)
                {
                    float v = buf.getSample (0, i);
                    if (! std::isfinite (v)) bad = true;
                    peak = std::max (peak, (double) std::fabs (v));
                    rms += v * v; ++cnt;
                }
        }
        std::printf ("  %-14s f=%5.0f Hz in=%.2f -> peak=%.3f rms=%.3f nan=%d\n", tag, freq, amp, peak, std::sqrt (rms / cnt), (int) bad);
        CHECK (! bad);
        CHECK (peak < 1.0);
        CHECK (peak > 0.01);
    };

    std::printf ("  latency = %d samples, presets = %d\n", p.getLatencySamples(), p.getNumPrograms());
    CHECK (p.getNumPrograms() == 7);

    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        p.setCurrentProgram (i);
        run (82.f, 0.3f, 120, p.getProgramName (i).toRawUTF8());
        run (700.f, 0.3f, 120, p.getProgramName (i).toRawUTF8());
    }
    p.setCurrentProgram (0);
    run (82.f, 1.0f, 120, "hot input");


    // ---------------- new feature tests ----------------
    std::printf("--- amps / solo / IR ---\n");

    auto measure = [&] (float freq, float amp, int blocks)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        double rms = 0; long cnt = 0; bool bad = false; long t = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < bs; ++i, ++t)
            {
                const float v = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * t / sr);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            if (b > blocks / 2)
                for (int i = 0; i < bs; ++i)
                {
                    const float v = buf.getSample (0, i);
                    if (! std::isfinite (v)) bad = true;
                    rms += v * v; ++cnt;
                }
        }
        CHECK (! bad);
        return std::sqrt (rms / (double) cnt);
    };
    auto setParam = [&] (const char* id, float v)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    // wait for async IR loading (convolution loads on a background thread)
    auto settle = [&]
    {
        for (int i = 0; i < 40; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (25);
            juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; b.clear();
            p.processBlock (b, m);
        }
    };

    p.setCurrentProgram (0);
    setParam ("cabType", 0);   // cab off for amp comparison

    double rmsAmp[6];
    for (int a = 0; a < 6; ++a)
    {
        setParam ("amp", (float) a);
        for (int i = 0; i < 20; ++i) measure (110.f, 0.3f, 4);
        rmsAmp[a] = measure (110.f, 0.3f, 60);
        std::printf ("  amp %d rms=%.4f\n", a, rmsAmp[a]);
        CHECK (rmsAmp[a] > 0.005);
    }
    int distinct = 0;
    for (int a = 1; a < 6; ++a) if (std::fabs (rmsAmp[a] - rmsAmp[0]) > 1e-4) ++distinct;
    CHECK (distinct >= 4);   // models really sound different

    // solo
    setParam ("amp", 0);
    const double full = measure (110.f, 0.3f, 80);
    setParam ("soloDry", 1);
    const double dryOnly = measure (110.f, 0.3f, 80);
    setParam ("soloDry", 0);
    setParam ("soloLow", 1);
    const double lowOnly = measure (110.f, 0.3f, 80);
    setParam ("soloLow", 0);
    setParam ("soloGrit", 1);
    const double gritOnly = measure (110.f, 0.3f, 80);
    setParam ("soloGrit", 0);
    std::printf ("  solo: full=%.4f dry=%.4f low=%.4f grit=%.4f\n", full, dryOnly, lowOnly, gritOnly);
    CHECK (dryOnly > 0.01 && lowOnly > 0.01 && gritOnly > 0.001);
    CHECK (std::fabs (dryOnly - full) > 1e-4 || std::fabs (lowOnly - full) > 1e-4);
    // soloing the DI must equal the input level (unity, 0.3/sqrt2 = 0.212)
    CHECK (std::fabs (dryOnly - 0.212) < 0.02);
    setParam ("soloGrit", 1);
    const double gritHigh = measure (3000.f, 0.3f, 80);
    setParam ("soloGrit", 0);
    setParam ("soloLow", 1);
    const double lowHigh = measure (3000.f, 0.3f, 80);
    setParam ("soloLow", 0);
    std::printf ("  3 kHz: grit-solo=%.4f low-solo=%.5f\n", gritHigh, lowHigh);
    CHECK (lowHigh < gritHigh * 0.1);   // the LOW path really is only lows

    // built-in cabs
    for (int c : { 1, 2, 3, 4, 6, 7, 8, 9, 10, 11 })   // 5 = user IR (tested below)
    {
        setParam ("cabType", (float) c);
        settle();
        CHECK (p.isCabReady());
        const double r = measure (110.f, 0.3f, 60);
        std::printf ("  cab %d rms=%.4f\n", c, r);
        CHECK (r > 0.003);
    }

    // user IR: invalid input is rejected, valid WAV loads and changes the sound
    {
        juce::String err;
        CHECK (! p.loadUserIRFromMemory (juce::MemoryBlock ("not audio", 9), "bad", err));
        CHECK (err.isNotEmpty());

        setParam ("cabType", 1);
        settle();
        const double before = measure (700.f, 0.3f, 60);

        const auto wav = makeWav (44100.0, 4410, 2);   // different sample rate + stereo: must be resampled/handled
        CHECK (p.loadUserIRFromMemory (wav, "TestCab", err));
        CHECK (p.getUserIRName() == "TestCab");
        settle();
        CHECK ((int) p.apvts.getRawParameterValue ("cabType")->load() == 5);   // switched to User IR automatically
        const double after = measure (700.f, 0.3f, 60);
        std::printf ("  user IR: before=%.4f after=%.4f\n", before, after);
        CHECK (after > 0.0005);
        CHECK (std::fabs (after - before) > 1e-4);

        // 7 s IR must be refused
        CHECK (! p.loadUserIRFromMemory (makeWav (44100.0, 44100 * 7, 1), "toolong", err));
        CHECK (p.getUserIRName() == "TestCab");

        // cab on full mix
        setParam ("cabAll", 1);
        const double allMix = measure (700.f, 0.3f, 60);
        CHECK (allMix > 0.0005);
        setParam ("cabAll", 0);

        // session save / restore keeps the IR
        juce::MemoryBlock state;
        p.getStateInformation (state);
        MetalBassForgeAudioProcessor p2;
        p2.setPlayConfigDetails (1, 1, sr, bs);
        p2.prepareToPlay (sr, bs);
        p2.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (p2.getUserIRName() == "TestCab");
        int iters = 0;
        for (; iters < 200 && ! p2.isCabReady(); ++iters)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (25);
            juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; b.clear();
            p2.processBlock (b, m);
        }
        std::printf ("  p2 ready after %d iterations\n", iters);
        std::printf ("  p2 cabType=%d requested=%d\n", (int) p2.apvts.getRawParameterValue ("cabType")->load(), (int) p2.isCabLoadRequested());
        CHECK (p2.isCabReady());
        std::printf ("  state restore: IR '%s' ready=%d\n", p2.getUserIRName().toRawUTF8(), (int) p2.isCabReady());
    }

    // presets must not touch the cab
    setParam ("cabType", 3);
    for (int i = 0; i < p.getNumPrograms(); ++i)
    {
        p.setCurrentProgram (i);
        CHECK ((int) p.apvts.getRawParameterValue ("cabType")->load() == 3);
    }
    CHECK ((int) p.apvts.getRawParameterValue ("amp")->load() == 3);   // Extreme Metal -> Raw Grind

    // ---------------- extra FX / tuner / latency ----------------
    std::printf("--- fx / tuner / latency ---\n");
    setParam ("cabType", 0);
    setParam ("soloDry", 0); setParam ("soloLow", 0); setParam ("soloGrit", 0);
    p.setCurrentProgram (0);

    auto feed = [&] (std::function<float (long)> gen, int blocks, std::vector<float>* capture = nullptr, bool settleFirst = false)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        long t = 0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < bs; ++i, ++t)
            {
                const float v = gen (t);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            if (capture)
                for (int i = 0; i < bs; ++i) capture->push_back (buf.getSample (0, i));
        }
        (void) settleFirst;
    };
    auto sine = [&] (float f, float a) { return [=] (long t) { return a * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * (double) t / 48000.0); }; };
    auto rmsOf = [] (const std::vector<float>& v, size_t from)
    {
        double e = 0; for (size_t i = from; i < v.size(); ++i) e += (double) v[i] * v[i];
        return std::sqrt (e / (double) (v.size() - from));
    };

    // tuner pitch detector
    for (float f : { 30.87f, 41.2f, 82.41f, 110.f, 196.f })
    {
        std::vector<float> cap;
        setParam ("soloDry", 1);
        feed (sine (f, 0.3f), 40, &cap);
        setParam ("soloDry", 0);
        std::vector<float> ring (8192);
        p.copyTunerSamples (ring.data(), 8192);
        const float det = MetalBassForgeAudioProcessor::detectPitch (ring.data(), 8192, 48000.0);
        std::printf ("  tuner: %.2f Hz -> %.2f Hz\n", f, det);
        CHECK (std::fabs (det - f) < 0.5f);
    }
    {
        std::vector<float> silence (8192, 0.f);
        CHECK (MetalBassForgeAudioProcessor::detectPitch (silence.data(), 8192, 48000.0) == 0.f);
    }

    // latency modes
    {
        int lat[3];
        for (int q = 0; q < 3; ++q)
        {
            setParam ("quality", (float) q);
            feed (sine (110.f, 0.2f), 4);
            settle();
            lat[q] = p.getLatencySamples();
            std::printf ("  quality %d latency = %d samples (%.2f ms)\n", q, lat[q], 1000.0 * lat[q] / sr);
        }
        CHECK (lat[0] == 0);
        CHECK (lat[1] > 0 && lat[1] < lat[2]);
        CHECK (lat[2] > 0);
        for (int q = 0; q < 3; ++q)
        {
            setParam ("quality", (float) q);
            setParam ("amp", 1);
            std::vector<float> cap;
            feed (sine (110.f, 0.3f), 80, &cap);
            const double r = rmsOf (cap, cap.size() / 2);
            std::printf ("  quality %d rms=%.4f\n", q, r);
            CHECK (r > 0.01 && std::isfinite (r));
        }
        setParam ("quality", 1);
        feed (sine (110.f, 0.2f), 4);
        settle();
    }

    // noise gate (soloDry isolates the gate effect)
    {
        setParam ("soloDry", 1);
        std::vector<float> a, b;
        setParam ("gateThresh", -80.f);
        feed (sine (110.f, 0.004f), 60, &a);
        setParam ("gateThresh", -35.f);
        feed (sine (110.f, 0.004f), 60, &b);
        const double ra = rmsOf (a, a.size() / 2), rb = rmsOf (b, b.size() / 2);
        std::printf ("  gate: open=%.5f closed=%.8f\n", ra, rb);
        CHECK (rb < ra * 0.1);
        std::vector<float> c;
        feed (sine (110.f, 0.3f), 60, &c);   // loud signal must pass
        CHECK (rmsOf (c, c.size() / 2) > 0.15);
        setParam ("gateThresh", -80.f);
        setParam ("soloDry", 0);
    }

    // pitch shifter: one octave down (soloDry isolates it)
    for (int semi : { -12, -2, 7 })
    {
        setParam ("soloDry", 1);
        setParam ("pitchSemi", (float) semi);
        setParam ("pitchMix", 1.f);
        std::vector<float> cap;
        for (int blk = 0; blk < 220; ++blk)   // real-time-ish pacing so the background pitch tracker can lock on
        {
            std::vector<float> part;
            feed ([&] (long t) { return 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * (double) (t + blk * 512L) / 48000.0); }, 1, &part);
            cap.insert (cap.end(), part.begin(), part.end());
            juce::Thread::sleep (3);
        }
        const float expect = 110.f * std::pow (2.f, (float) semi / 12.f);
        auto goertzel = [&] (float f)
        {
            const int N = 16384;
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i)
            {
                const double v = cap[cap.size() - (size_t) N + (size_t) i];
                re += v * std::cos (2 * juce::MathConstants<double>::pi * f * i / 48000.0);
                im += v * std::sin (2 * juce::MathConstants<double>::pi * f * i / 48000.0);
            }
            return 2.0 * std::sqrt (re * re + im * im) / N;
        };
        const double aNew = goertzel (expect), aOld = goertzel (110.f);
        std::printf ("  pitch %+d st: level at %.1f Hz = %.3f, leftover 110 Hz = %.3f\n", semi, expect, aNew, aOld);
        CHECK (aNew > 0.12 && aNew > aOld * 4.0);
        for (float v : cap) CHECK (std::isfinite (v));
        setParam ("pitchSemi", 0);
        setParam ("soloDry", 0);
        feed (sine (110.f, 0.1f), 20);
    }

    // wah changes the grit path
    {
        setParam ("soloGrit", 1);
        std::vector<float> off, on;
        feed (sine (110.f, 0.3f), 60, &off);
        setParam ("wahOn", 1); setParam ("wahMode", 0); setParam ("wahPos", 0.8f);
        feed (sine (110.f, 0.3f), 60, &on);
        const double ro = rmsOf (off, off.size() / 2), rw = rmsOf (on, on.size() / 2);
        std::printf ("  wah: off=%.4f on=%.4f\n", ro, rw);
        CHECK (std::fabs (ro - rw) > 0.002);
        for (int mode = 1; mode <= 2; ++mode)
        {
            setParam ("wahMode", (float) mode);
            std::vector<float> m;
            feed (sine (110.f, 0.3f), 60, &m);
            for (float v : m) CHECK (std::isfinite (v));
        }
        setParam ("wahOn", 0); setParam ("soloGrit", 0);
        feed (sine (110.f, 0.1f), 20);
    }

    // delay: impulse -> echo 100 ms later
    {
        setParam ("quality", 0);
        feed (sine (110.f, 0.0f), 8);
        setParam ("soloDry", 1);
        setParam ("delaySync", 0); setParam ("delayTime", 100.f);
        setParam ("delayFeedback", 0.f); setParam ("delayMix", 1.f); setParam ("delayTone", 12000.f);
        feed (sine (110.f, 0.f), 80);   // let smoothers settle
        std::vector<float> cap;
        feed ([] (long t) { return t == 10 ? 0.8f : 0.f; }, 30, &cap);
        size_t pk = 0; float pv = 0;
        for (size_t i = 0; i < cap.size() / 2; ++i) if (std::fabs (cap[i]) > pv) { pv = std::fabs (cap[i]); pk = i; }
        float echo = 0;
        for (size_t i = pk + 4700; i < pk + 4900 && i < cap.size(); ++i) echo = std::max (echo, std::fabs (cap[i]));
        std::printf ("  delay: peak %.3f @%zu, echo %.3f\n", pv, pk, echo);
        CHECK (pv > 0.1f && echo > pv * 0.3f);
        setParam ("delayMix", 0.f); setParam ("soloDry", 0); setParam ("quality", 1);
        feed (sine (110.f, 0.f), 40);
    }

    // reverb: tail after an impulse
    {
        setParam ("revMix", 1.f); setParam ("revSize", 0.8f);
        feed (sine (110.f, 0.f), 40);
        std::vector<float> cap;
        feed ([] (long t) { return t == 10 ? 0.8f : 0.f; }, 60, &cap);
        const double tail = rmsOf (cap, 20000);
        std::printf ("  reverb tail rms = %.6f\n", tail);
        CHECK (tail > 1e-4);
        for (float v : cap) CHECK (std::isfinite (v));
        setParam ("revMix", 0.f);
    }

    // presets must not touch the FX
    {
        setParam ("gateThresh", -30.f); setParam ("pitchSemi", -2); setParam ("revMix", 0.3f);
        setParam ("delayMix", 0.2f); setParam ("wahOn", 1); setParam ("quality", 2);
        for (int i = 0; i < p.getNumPrograms(); ++i) p.setCurrentProgram (i);
        CHECK (std::fabs (p.apvts.getRawParameterValue ("gateThresh")->load() + 30.f) < 0.01f);
        CHECK ((int) p.apvts.getRawParameterValue ("pitchSemi")->load() == -2);
        CHECK (std::fabs (p.apvts.getRawParameterValue ("revMix")->load() - 0.3f) < 0.01f);
        CHECK (p.apvts.getRawParameterValue ("wahOn")->load() > 0.5f);
        CHECK ((int) p.apvts.getRawParameterValue ("quality")->load() == 2);
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
