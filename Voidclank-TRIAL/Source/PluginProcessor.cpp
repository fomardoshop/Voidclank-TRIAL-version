#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Amp models.  Each one voices the drive stage (bias / hardness / pre-EQ) and the
// post tone stack.  Names are descriptive on purpose; the README lists the tones
// they were voiced towards.
//==============================================================================
namespace
{
    struct AmpModel
    {
        const char* name;
        const char* description;
        float driveScale, bias, hard, charBias;     // drive stage
        float preFreq, preDb, preQ;                 // pre-drive mid push/cut
        float lowDb, midFreq, midDb, midQ, highDb;  // post voicing
    };

    const AmpModel amps[] = {
        { "Modern Ether",   "Polished, wide and airy. Smooth drive, fat low-mids, silky top.",
          0.85f, 0.10f, 1.6f,  0.10f,   700.f, -3.f, 0.8f,   2.f,  650.f, -3.f, 0.7f,  2.f },
        { "Metalcore Edge", "Tight, bright and clanky. Hard compression for modern metalcore.",
          1.00f, 0.12f, 2.4f,  0.20f,  1200.f,  3.f, 1.0f,   0.f,  400.f, -4.f, 0.8f,  4.f },
        { "Thrash Steel",   "Classic crunch. Mid-forward, dynamic, moderate gain.",
          0.70f, 0.20f, 1.2f, -0.25f,   800.f,  4.f, 0.9f,   1.f,  800.f,  3.f, 0.8f,  1.f },
        { "Raw Grind",      "Abrasive and raw. Upper-mid bite, gritty asymmetric fuzz.",
          1.10f, 0.28f, 1.8f, -0.10f,  1500.f,  4.f, 1.2f,  -1.f, 1200.f,  4.f, 1.0f,  3.f },
        { "Slam Abyss",     "Huge and subby. Round, heavy low end with scooped mids.",
          0.90f, 0.08f, 1.4f,  0.10f,   250.f,  3.f, 0.8f,   5.f,  500.f, -5.f, 0.8f, -1.f },
        { "Crypt Death",    "Grinding, mid-rich saturation for old-school death metal.",
          1.20f, 0.22f, 2.0f, -0.20f,   600.f,  2.f, 0.7f,   2.f,  900.f,  2.f, 0.7f,  2.f },
    };
    constexpr int numAmps = (int) (sizeof (amps) / sizeof (amps[0]));

    const char* const cabNames[] = { "Off", "8x10 Modern", "4x12 Closed", "2x15 Vintage", "1x15 Tight", "User IR",
                                    // genre-voiced built-in cabs (synthetic EQ/comb models, not captures of any real rig)
                                    "4x10 Atmospheric", "4x12 Modern Clank", "2x12 Melodic", "4x12 Classic Thrash", "8x10 Heavy Classic", "4x12 Blackened" };
}

juce::StringArray MetalBassForgeAudioProcessor::getAmpDescriptions()
{
    juce::StringArray s;
    for (auto& a : amps) s.add (a.description);
    return s;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MetalBassForgeAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    using A = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&] (const char* id, const char* name, R range, float def, const char* unit)
    {
        layout.add (std::make_unique<P> (juce::ParameterID { id, 1 }, name, range, def,
                                         A().withLabel (unit)));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    add ("inGain",    "Input",        R (-12.f, 12.f, 0.1f),        0.f,    "dB");
    add ("tight",     "Tight",        R (20.f, 120.f, 1.f, 0.6f),   30.f,   "Hz");
    add ("xover",     "Crossover",    R (80.f, 500.f, 1.f, 0.6f),   200.f,  "Hz");
    add ("dry",       "Dry DI",       R (0.f, 1.f, 0.01f),          0.15f,  "");
    add ("outGain",   "Output",       R (-24.f, 12.f, 0.1f),        0.f,    "dB");

    add ("lowComp",   "Low Comp",     R (0.f, 1.f, 0.01f),          0.6f,   "");
    add ("lowLevel",  "Low Level",    R (-12.f, 6.f, 0.1f),         0.f,    "dB");

    add ("drive",     "Drive",        R (0.f, 1.f, 0.01f),          0.5f,   "");
    add ("character", "BOD <> Dark",  R (0.f, 1.f, 0.01f),          0.5f,   "");
    add ("scoop",     "Mid Scoop",    R (0.f, 12.f, 0.1f),          4.f,    "dB");
    add ("gritLevel", "Grit Level",   R (-24.f, 6.f, 0.1f),         -6.f,   "dB");

    add ("clank",     "Clank",        R (0.f, 12.f, 0.1f),          4.f,    "dB");
    add ("clankFreq", "Clank Freq",   R (800.f, 4000.f, 1.f, 0.5f), 1800.f, "Hz");
    add ("fizz",      "Hi-Cut",       R (2000.f, 12000.f, 1.f, 0.5f), 6000.f, "Hz");

    // solo per path
    addBool ("soloLow",  "Solo Low",  false);
    addBool ("soloGrit", "Solo Grit", false);
    addBool ("soloDry",  "Solo Dry",  false);

    // amp model + tone stack
    juce::StringArray ampNames;
    for (auto& a : amps) ampNames.add (a.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "amp", 1 }, "Amp Model", ampNames, 0));
    add ("ampBass",     "Bass",     R (-12.f, 12.f, 0.1f), 0.f, "dB");
    add ("ampMid",      "Mid",      R (-12.f, 12.f, 0.1f), 0.f, "dB");
    add ("ampTreble",   "Treble",   R (-12.f, 12.f, 0.1f), 0.f, "dB");
    add ("ampPresence", "Presence", R (-12.f, 12.f, 0.1f), 0.f, "dB");

    // cabinet
    juce::StringArray cabs;
    for (auto* c : cabNames) cabs.add (c);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "cabType", 1 }, "Cab Model", cabs, 1));
    add ("cabMix",   "Cab Mix",   R (0.f, 1.f, 0.01f),   1.f, "");
    add ("cabLevel", "Cab Level", R (-12.f, 12.f, 0.1f), 0.f, "dB");
    addBool ("cabAll", "Cab On Full Mix", false);

    // ---- extra FX (not part of the factory presets)
    add ("gateThresh",  "Gate Threshold", R (-80.f, -20.f, 0.1f),  -80.f, "dB");   // -80 = gate off
    add ("gateRelease", "Gate Release",   R (10.f, 500.f, 1.f, 0.5f), 100.f, "ms");

    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pitchSemi", 1 }, "Pitch", -12, 12, 0));
    add ("pitchFine",   "Pitch Fine",     R (-50.f, 50.f, 1.f),    0.f,   "ct");
    add ("pitchMix",    "Pitch Mix",      R (0.f, 1.f, 0.01f),     1.f,   "");

    addBool ("wahOn", "Wah On", false);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "wahMode", 1 }, "Wah Mode",
                                                              juce::StringArray { "Pedal", "Auto LFO", "Envelope" }, 0));
    add ("wahPos",   "Wah Pedal",  R (0.f, 1.f, 0.001f),         0.5f,  "");
    add ("wahRate",  "Wah Rate",   R (0.1f, 10.f, 0.01f, 0.5f),  1.5f,  "Hz");
    add ("wahDepth", "Wah Depth",  R (0.f, 1.f, 0.01f),          0.7f,  "");
    add ("wahQ",     "Wah Q",      R (1.f, 12.f, 0.1f),          4.f,   "");
    add ("wahMix",   "Wah Mix",    R (0.f, 1.f, 0.01f),          1.f,   "");

    add ("delayTime",     "Delay Time",     R (20.f, 1000.f, 1.f, 0.5f), 350.f,  "ms");
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "delaySync", 1 }, "Delay Sync",
                                                              juce::StringArray { "Free", "1/4", "1/8", "1/8 dotted", "1/16" }, 0));
    add ("delayFeedback", "Delay Feedback", R (0.f, 0.9f, 0.01f),        0.35f,  "");
    add ("delayTone",     "Delay Tone",     R (500.f, 12000.f, 1.f, 0.5f), 4000.f, "Hz");
    add ("delayMix",      "Delay Mix",      R (0.f, 1.f, 0.01f),         0.f,    "");

    add ("revSize", "Reverb Size", R (0.f, 1.f, 0.01f), 0.5f, "");
    add ("revMix",  "Reverb Mix",  R (0.f, 1.f, 0.01f), 0.f,  "");

    addBool ("tunerMute", "Tuner Mute", false);
   #if VC_WITH_NAM
    addBool ("namOn", "NAM On", false);
   #endif

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "micType", 1 }, "Mic Type",
                                                              juce::StringArray { "Dynamic", "Ribbon", "Condenser" }, 0));
    add ("micPos",  "Mic Position", R (0.f, 1.f, 0.01f), 0.3f, "");   // 0 = cone centre (bright), 1 = edge (dark)
    add ("micDist", "Mic Distance", R (0.f, 1.f, 0.01f), 0.2f, "");   // 0 = close (proximity), 1 = far

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quality", 1 }, "Quality",
                                                              juce::StringArray { "Live 0 ms", "Standard 2x", "HQ 4x" }, 1));

    addBool ("fxOn", "Effects On", true);        // master kill switch: wah, delay, reverb, noise gate
    return layout;
}

//==============================================================================
// Factory presets.  Order of values must match presetParamIds.
namespace
{
    constexpr int numParams = 19;
    const char* const presetParamIds[numParams] = {
        "inGain", "tight", "xover", "dry", "outGain",
        "lowComp", "lowLevel",
        "drive", "character", "scoop", "gritLevel",
        "clank", "clankFreq", "fizz",
        "amp", "ampBass", "ampMid", "ampTreble", "ampPresence" };

    struct Preset { const char* name; float v[numParams]; };

    // (cabinet settings are deliberately not part of presets - the cab is "your rig")
    const Preset presets[] = {
        //                  in tight xover  dry   out | comp  lvl | drive char  scoop grit | clank freq  fizz | amp  bass mid tre pres
        { "Init",          { 0, 30, 200, 0.15f, 0,   0.6f,  0,    0.5f,  0.5f,  4, -6,      4, 1800, 6000,   0,  0,  0,  0,  0 } },
        { "Djent",         { 0, 35, 180, 0.10f, 0,   0.8f,  0,    0.65f, 0.85f, 3, -5,      6, 2200, 7000,   1,  0, -1,  1,  2 } },
        { "Metalcore",     { 0, 35, 200, 0.12f, 0,   0.7f,  0,    0.6f,  0.7f,  5, -5,      5, 1900, 6500,   0,  0,  0,  1,  1 } },
        { "Deathcore",     { 0, 30, 150, 0.05f, 0,   0.85f, 1,    0.8f,  0.9f,  6, -4,      4, 1500, 5500,   4,  2, -1,  0,  0 } },
        { "Death Metal",   { 0, 28, 220, 0.20f, 0,   0.6f,  0,    0.75f, 0.35f, 2, -6,      6, 1200, 5000,   5,  1,  1,  0,  0 } },
        { "Thrash Metal",  { 0, 40, 250, 0.30f, 0,   0.4f,  0,    0.55f, 0.2f,  0, -8,      7, 1600, 7500,   2,  0,  1,  1,  1 } },
        { "Extreme Metal", { 0, 30, 200, 0.10f, 0,   0.9f,  1,    0.95f, 0.6f,  5, -4,      8, 2000, 8000,   3,  0,  0,  2,  2 } },
    };
    constexpr int numPresetsTotal = (int) (sizeof (presets) / sizeof (presets[0]));
}

int MetalBassForgeAudioProcessor::getNumPrograms() { return numPresetsTotal; }

const juce::String MetalBassForgeAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresetsTotal) ? presets[index].name : "";
}

void MetalBassForgeAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresetsTotal))
        return;

    currentPreset = index;
    for (int i = 0; i < numParams; ++i)
        if (auto* p = apvts.getParameter (presetParamIds[i]))
            p->setValueNotifyingHost (p->convertTo0to1 (presets[index].v[i]));
}

//==============================================================================
MetalBassForgeAudioProcessor::MetalBassForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInGain    = apvts.getRawParameterValue ("inGain");
    pTight     = apvts.getRawParameterValue ("tight");
    pXover     = apvts.getRawParameterValue ("xover");
    pLowComp   = apvts.getRawParameterValue ("lowComp");
    pLowLevel  = apvts.getRawParameterValue ("lowLevel");
    pDrive     = apvts.getRawParameterValue ("drive");
   #if VC_WITH_NAM
    pNamOn     = apvts.getRawParameterValue ("namOn");
   #endif
    pCharacter = apvts.getRawParameterValue ("character");
    pScoop     = apvts.getRawParameterValue ("scoop");
    pClank     = apvts.getRawParameterValue ("clank");
    pClankFreq = apvts.getRawParameterValue ("clankFreq");
    pFizz      = apvts.getRawParameterValue ("fizz");
    pGritLevel = apvts.getRawParameterValue ("gritLevel");
    pDry       = apvts.getRawParameterValue ("dry");
    pOutGain   = apvts.getRawParameterValue ("outGain");
    pSoloLow   = apvts.getRawParameterValue ("soloLow");
    pSoloGrit  = apvts.getRawParameterValue ("soloGrit");
    pSoloDry   = apvts.getRawParameterValue ("soloDry");
    pAmp       = apvts.getRawParameterValue ("amp");
    pBass      = apvts.getRawParameterValue ("ampBass");
    pMid       = apvts.getRawParameterValue ("ampMid");
    pTreble    = apvts.getRawParameterValue ("ampTreble");
    pPres      = apvts.getRawParameterValue ("ampPresence");
    pCabType   = apvts.getRawParameterValue ("cabType");
    pCabMix    = apvts.getRawParameterValue ("cabMix");
    pCabLevel  = apvts.getRawParameterValue ("cabLevel");
    pCabAll    = apvts.getRawParameterValue ("cabAll");

    pGateThresh = apvts.getRawParameterValue ("gateThresh");
    pGateRel    = apvts.getRawParameterValue ("gateRelease");
    pPitchSemi  = apvts.getRawParameterValue ("pitchSemi");
    pPitchFine  = apvts.getRawParameterValue ("pitchFine");
    pPitchMix   = apvts.getRawParameterValue ("pitchMix");
    pWahOn      = apvts.getRawParameterValue ("wahOn");
    pFxOn       = apvts.getRawParameterValue ("fxOn");
    pWahMode    = apvts.getRawParameterValue ("wahMode");
    pWahPos     = apvts.getRawParameterValue ("wahPos");
    pWahRate    = apvts.getRawParameterValue ("wahRate");
    pWahDepth   = apvts.getRawParameterValue ("wahDepth");
    pWahQ       = apvts.getRawParameterValue ("wahQ");
    pWahMix     = apvts.getRawParameterValue ("wahMix");
    pDelayTime  = apvts.getRawParameterValue ("delayTime");
    pDelaySync  = apvts.getRawParameterValue ("delaySync");
    pDelayFb    = apvts.getRawParameterValue ("delayFeedback");
    pDelayTone  = apvts.getRawParameterValue ("delayTone");
    pDelayMix   = apvts.getRawParameterValue ("delayMix");
    pRevSize    = apvts.getRawParameterValue ("revSize");
    pRevMix     = apvts.getRawParameterValue ("revMix");
    pTunerMute  = apvts.getRawParameterValue ("tunerMute");
    pQuality    = apvts.getRawParameterValue ("quality");
    pMicType    = apvts.getRawParameterValue ("micType");
    pMicPos     = apvts.getRawParameterValue ("micPos");
    pMicDist    = apvts.getRawParameterValue ("micDist");

    apvts.addParameterListener ("cabType", this);
    apvts.addParameterListener ("quality", this);
}

// Background pitch tracker for the pitch shifter (only works while the shifter is active)
namespace
{
    struct PitchTrackerThread : public juce::Thread
    {
        PitchTrackerThread (MetalBassForgeAudioProcessor& p, std::atomic<bool>& on, std::atomic<float>& out)
            : juce::Thread ("VoidclankPitchTracker"), proc (p), active (on), period (out) {}

        void run() override
        {
            std::vector<float> raw, dec;
            while (! threadShouldExit())
            {
                wait (20);
                if (! active.load()) { period.store (0.f); continue; }

                const double sr = proc.getCurrentSampleRate();
                const int d = juce::jmax (1, (int) std::round (sr / 12000.0));
                const int n = 2048;
                raw.resize ((size_t) (n * d));
                dec.resize ((size_t) n);
                proc.copyTunerSamples (raw.data(), n * d);
                for (int i = 0; i < n; ++i)
                {
                    float sum = 0.f;
                    for (int k = 0; k < d; ++k) sum += raw[(size_t) (i * d + k)];
                    dec[(size_t) i] = sum / (float) d;
                }
                const float f = MetalBassForgeAudioProcessor::detectPitch (dec.data(), n, sr / d);
                period.store (f > 0.f ? (float) (sr / f) : 0.f);
            }
        }

        MetalBassForgeAudioProcessor& proc;
        std::atomic<bool>& active;
        std::atomic<float>& period;
    };
}

MetalBassForgeAudioProcessor::~MetalBassForgeAudioProcessor()
{
    if (pitchTracker) pitchTracker->stopThread (2000);
    apvts.removeParameterListener ("cabType", this);
    apvts.removeParameterListener ("quality", this);
    cancelPendingUpdate();
}

bool MetalBassForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // mono->mono, mono->stereo (bass is mono), stereo->stereo
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
juce::AudioBuffer<float> MetalBassForgeAudioProcessor::makeBuiltInIR (int type, double sr)
{
    struct Band { char kind; float f, q, db; };
    std::vector<Band> bands;

    switch (type)
    {
        case 0:  // 8x10: big low-mid thump, scooped 400, 2.4k presence, steep top
            bands = { { 'H', 45, 0.7f, 0 }, { 'P', 100, 1.0f, 4 }, { 'P', 400, 0.9f, -3 },
                      { 'P', 2400, 1.2f, 4 }, { 'L', 5500, 0.7f, 0 }, { 'L', 5500, 0.7f, 0 } };
            break;
        case 1:  // 4x12 closed back: tighter low end, upper-mid bite
            bands = { { 'H', 70, 0.7f, 0 }, { 'P', 120, 1.0f, 3 }, { 'P', 700, 1.0f, 2 },
                      { 'P', 2200, 1.3f, 5 }, { 'L', 4800, 0.7f, 0 }, { 'L', 4800, 0.7f, 0 } };
            break;
        case 2:  // 2x15 vintage: warm, big sub, dark top
            bands = { { 'H', 35, 0.7f, 0 }, { 'P', 80, 1.4f, 5 }, { 'P', 500, 0.9f, -2 },
                      { 'P', 1400, 1.0f, 3 }, { 'L', 3200, 0.7f, 0 }, { 'L', 3200, 0.7f, 0 } };
            break;
        case 4:  // 4x10 atmospheric: open and airy, extended top, gentle mid scoop
            bands = { { 'H', 50, 0.7f, 0 }, { 'P', 110, 1.0f, 2 }, { 'P', 380, 0.9f, -2 },
                      { 'P', 1800, 1.0f, 1 }, { 'P', 3500, 1.0f, 2 }, { 'L', 7500, 0.7f, 0 }, { 'L', 7500, 0.7f, 0 } };
            break;
        case 5:  // 4x12 modern clank: tight low end, strong 2.8k bite
            bands = { { 'H', 60, 0.7f, 0 }, { 'P', 130, 1.0f, 2.5f }, { 'P', 600, 1.0f, -1 },
                      { 'P', 2800, 1.2f, 6 }, { 'P', 4500, 1.0f, 2 }, { 'L', 6500, 0.7f, 0 }, { 'L', 6500, 0.7f, 0 } };
            break;
        case 6:  // 2x12 melodic: balanced, smooth upper mids
            bands = { { 'H', 50, 0.7f, 0 }, { 'P', 120, 1.0f, 3 }, { 'P', 800, 0.9f, 1 },
                      { 'P', 2000, 1.0f, 3 }, { 'L', 6000, 0.7f, 0 }, { 'L', 6000, 0.7f, 0 } };
            break;
        case 7:  // 4x12 classic thrash: mid-forward, rolled-off top
            bands = { { 'H', 65, 0.7f, 0 }, { 'P', 100, 1.0f, 3 }, { 'P', 800, 0.9f, 4 },
                      { 'P', 1600, 1.0f, 3 }, { 'L', 4200, 0.7f, 0 }, { 'L', 4200, 0.7f, 0 } };
            break;
        case 8:  // 8x10 heavy classic: big, full, solid presence
            bands = { { 'H', 40, 0.7f, 0 }, { 'P', 90, 1.0f, 4 }, { 'P', 500, 0.9f, -2 },
                      { 'P', 1200, 1.0f, 2 }, { 'P', 3000, 1.1f, 3 }, { 'L', 5500, 0.7f, 0 }, { 'L', 5500, 0.7f, 0 } };
            break;
        case 9:  // 4x12 blackened: dense, aggressive 1.2-2k, dark top
            bands = { { 'H', 70, 0.7f, 0 }, { 'P', 150, 1.0f, 2 }, { 'P', 1200, 1.0f, 4 },
                      { 'P', 2000, 1.0f, 3 }, { 'P', 3500, 1.0f, -2 }, { 'L', 4500, 0.7f, 0 }, { 'L', 4500, 0.7f, 0 } };
            break;
        default: // 1x15 tight: focused and smooth
            bands = { { 'H', 55, 0.7f, 0 }, { 'P', 140, 1.0f, 3 }, { 'P', 900, 0.8f, 1 },
                      { 'P', 3000, 1.0f, 2 }, { 'L', 6000, 0.7f, 0 }, { 'L', 6000, 0.7f, 0 } };
            break;
    }

    using C = juce::dsp::IIR::Coefficients<float>;
    const int len = (int) (sr * 0.04);
    juce::AudioBuffer<float> ir (1, len);
    ir.clear();
    float* d = ir.getWritePointer (0);
    d[0] = 1.f;

    for (auto& b : bands)
    {
        C::Ptr c;
        if (b.kind == 'H')      c = C::makeHighPass (sr, b.f, b.q);
        else if (b.kind == 'L') c = C::makeLowPass (sr, b.f, b.q);
        else                    c = C::makePeakFilter (sr, b.f, b.q, juce::Decibels::decibelsToGain (b.db));

        juce::dsp::IIR::Filter<float> f (c);
        for (int i = 0; i < len; ++i)
            d[i] = f.processSample (d[i]);
    }

    // two early reflections -> the slightly boxy comb colour of a real cabinet
    std::vector<float> orig (d, d + len);
    const int r1 = (int) (sr * 0.0004), r2 = (int) (sr * 0.0011);
    for (int i = r1; i < len; ++i) d[i] += 0.25f * orig[(size_t) (i - r1)];
    for (int i = r2; i < len; ++i) d[i] -= 0.15f * orig[(size_t) (i - r2)];

    // fade out the tail
    const int fade = len / 4;
    for (int i = 0; i < fade; ++i)
    {
        const float t = (float) i / (float) fade;
        d[len - fade + i] *= 0.5f * (1.f + std::cos (juce::MathConstants<float>::pi * t));
    }
    return ir;
}

//==============================================================================
void MetalBassForgeAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
   #if VC_WITH_NAM
    nam.prepare (sr, samplesPerBlock);
   #endif

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    crossover.prepare (spec);
    crossover.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    lowComp.prepare (spec);
    lowComp.setAttack (12.f);
    lowComp.setRelease (140.f);

    limGain = 1.f;

    convolution.prepare (spec);

    // oversampling modes: [0] = Live (none, zero latency), [1] = 2x polyphase IIR, [2] = 4x linear-phase FIR
    oversampling[0].reset();
    oversampling[1] = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling[2] = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampling[1]->initProcessing ((size_t) samplesPerBlock);
    oversampling[2]->initProcessing ((size_t) samplesPerBlock);
    modeLatency[0] = 0;
    modeLatency[1] = (int) std::ceil (oversampling[1]->getLatencyInSamples());
    modeLatency[2] = (int) std::round (oversampling[2]->getLatencyInSamples());
    blockSize = samplesPerBlock;

    activeMode = juce::jlimit (0, 2, (int) pQuality->load());
    latencySamples = modeLatency[activeMode];
    setLatencySamples (latencySamples);

    lowDelay.prepare (spec);
    dryDelay.prepare (spec);
    lowDelay.setDelay ((float) latencySamples);
    dryDelay.setDelay ((float) latencySamples);

    lowBuf.setSize (2, samplesPerBlock);
    highBuf.setSize (2, samplesPerBlock);
    dryBuf.setSize (2, samplesPerBlock);
    cabDry.setSize (2, samplesPerBlock);

    for (int i = 0; i < numBuiltInIRs; ++i)
        builtInIR[i] = makeBuiltInIR (i, sr);

    // extra FX
    if (! pitchTracker)
    {
        pitchTracker = std::make_unique<PitchTrackerThread> (*this, pitchOn, trackedPeriod);
        pitchTracker->startThread (juce::Thread::Priority::low);
    }
    gate.prepare (spec);
    gate.setRatio (20.f);
    gate.setAttack (1.f);
    wahFilter.prepare (spec);
    wahFilter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    wahFilter.reset();
    wahPhase = 0.f; wahEnv = 0.f;
    for (auto& ps : pitchShifter) ps.prepare (sr);
    for (auto& d : delayLine) d.prepare ((int) (sr * 1.5) + 8);
    for (auto& f : delayLP) f.reset();
    reverb.prepare (spec);
    reverb.reset();
    revBuf.setSize (2, samplesPerBlock);
    revHP.reset();
    revHP.set (juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 160.f, 0.707f));
    tunerRing.fill (0.f);
    tunerPos.store (0);
    lastDelayTone = lastRevSize = -1.f;

    for (auto* b : { &micLow, &micMid, &micHigh, &inHP, &scoopEq, &preMid, &postHP, &toneLow, &toneMid, &toneHigh, &tonePres, &clankEq, &fizzLP })
        b->reset();

    crossover.reset();
    lowComp.reset();
    convolution.reset();
    for (auto& o : oversampling) if (o) o->reset();
    lowDelay.reset();
    dryDelay.reset();

    const double ramp = 0.03;
    for (auto* s : { &inSm, &lowSm, &gritSm, &drySm, &outSm, &charSm, &cabMixSm, &cabGainSm })
        s->reset (sr, ramp);
    driveSm.reset (sr * 4.0, ramp);
    for (auto* sm : { &pitchMixSm, &wahPosSm, &wahMixSm, &delayFbSm, &delayMixSm, &revMixSm })
        sm->reset (sr, ramp);
    delayTimeSm.reset (sr, 0.15);   // slow glide -> tape-style pitch bend when the time changes
    pitchMixSm.setCurrentAndTargetValue (0.f);
    wahPosSm.setCurrentAndTargetValue (pWahPos->load());
    wahMixSm.setCurrentAndTargetValue (0.f);
    delayTimeSm.setCurrentAndTargetValue ((float) (sr * pDelayTime->load() / 1000.0));
    delayFbSm.setCurrentAndTargetValue (pDelayFb->load());
    delayMixSm.setCurrentAndTargetValue (pDelayMix->load());
    revMixSm.setCurrentAndTargetValue (pRevMix->load());

    inSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    drySm.setCurrentAndTargetValue (pDry->load());
    gritSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGritLevel->load()));
    lowSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pLowLevel->load()));
    charSm.setCurrentAndTargetValue (pCharacter->load());
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pDrive->load() * 40.f));
    cabMixSm.setCurrentAndTargetValue (pCabMix->load());
    cabGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pCabLevel->load()));

    lastTight = lastXover = lastScoop = lastClank = lastClankFreq = lastFizz = lastComp = -1.f;
    lastMicType = lastMicPos = lastMicDist = -1.f;
    lastAmp = -1.f; lastBass = lastMid = lastTreble = lastPres = -99.f;
    updateParameters();
    applyCabSelection();
}

//==============================================================================
void MetalBassForgeAudioProcessor::updateParameters()
{
    const auto sr = sampleRate;
    using C = juce::dsp::IIR::Coefficients<float>;

    // ---- amp model + tone stack
    const int ampIdx = juce::jlimit (0, numAmps - 1, (int) pAmp->load());
    const auto& am = amps[ampIdx];
    const float bass = pBass->load(), mid = pMid->load(), treble = pTreble->load(), pres = pPres->load();

    ampBias = am.bias;
    ampHard = am.hard;
    ampCharBias = am.charBias;
    biasOff = std::tanh (ampBias);

    if ((float) ampIdx != lastAmp || bass != lastBass || mid != lastMid || treble != lastTreble || pres != lastPres)
    {
        preMid.set (C::makePeakFilter (sr, am.preFreq, am.preQ, juce::Decibels::decibelsToGain (am.preDb)));
        toneLow.set (C::makeLowShelf (sr, 100.f, 0.7f, juce::Decibels::decibelsToGain (am.lowDb + bass)));
        toneMid.set (C::makePeakFilter (sr, am.midFreq, am.midQ, juce::Decibels::decibelsToGain (am.midDb + mid)));
        toneHigh.set (C::makeHighShelf (sr, 3000.f, 0.7f, juce::Decibels::decibelsToGain (am.highDb + treble)));
        tonePres.set (C::makeHighShelf (sr, juce::jmin (6500.f, (float) (sr * 0.4)), 0.7f,
                                        juce::Decibels::decibelsToGain (pres)));
        lastAmp = (float) ampIdx; lastBass = bass; lastMid = mid; lastTreble = treble; lastPres = pres;
    }

    // ---- microphone model (tone shaping applied to the cabinet output)
    {
        const int mt = juce::jlimit (0, 2, (int) pMicType->load());
        const float mp = pMicPos->load(), md = pMicDist->load();
        if ((float) mt != lastMicType || mp != lastMicPos || md != lastMicDist)
        {
            static const float typeHigh[3] = { 0.f, -3.5f, 2.5f };
            static const float typeLow[3]  = { 0.f,  2.0f, -0.5f };
            static const float typePeak[3] = { 2.5f, 0.5f,  0.8f };
            const float hi = 3.f - 9.f * mp - 3.f * md + typeHigh[mt];
            const float pk = typePeak[mt] * (1.f - mp);
            const float lo = 2.5f * (1.f - md) + 1.5f * mp + typeLow[mt];
            micLow.set  (C::makeLowShelf  (sr, 140.f, 0.7f, juce::Decibels::decibelsToGain (lo)));
            micMid.set  (C::makePeakFilter (sr, 4500.f, 1.0f, juce::Decibels::decibelsToGain (pk)));
            micHigh.set (C::makeHighShelf (sr, juce::jmin (3500.f, (float) (sr * 0.45)), 0.7f, juce::Decibels::decibelsToGain (hi)));
            lastMicType = (float) mt; lastMicPos = mp; lastMicDist = md;
        }
    }

    // ---- solo logic
    const bool sl = pSoloLow->load() > 0.5f, sg = pSoloGrit->load() > 0.5f, sd = pSoloDry->load() > 0.5f;
    const bool anySolo = sl || sg || sd;

    inSm.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    drySm.setTargetValue (anySolo ? (sd ? 1.f : 0.f) : pDry->load());   // soloed DI is heard at unity
    gritSm.setTargetValue (anySolo && ! sg ? 0.f : juce::Decibels::decibelsToGain (pGritLevel->load()));
    charSm.setTargetValue (pCharacter->load());
    driveSm.setTargetValue (juce::Decibels::decibelsToGain (pDrive->load() * 40.f * am.driveScale));
    cabMixSm.setTargetValue (pCabMix->load());
    cabGainSm.setTargetValue (juce::Decibels::decibelsToGain (pCabLevel->load()));

    const float tight = pTight->load();
    if (tight != lastTight)
    {
        inHP.set (C::makeHighPass (sr, tight, 0.707f));
        lastTight = tight;
    }

    const float xo = pXover->load();
    if (xo != lastXover)
    {
        crossover.setCutoffFrequency (xo);
        // post-drive high-pass keeps distortion products out of the low end (and removes DC)
        postHP.set (C::makeHighPass (sr, xo * 0.7f, 0.707f));
        lastXover = xo;
    }

    const float scoop = pScoop->load();
    if (scoop != lastScoop)
    {
        scoopEq.set (C::makePeakFilter (sr, 500.f, 0.8f, juce::Decibels::decibelsToGain (-scoop)));
        lastScoop = scoop;
    }

    const float clank = pClank->load();
    const float clankF = pClankFreq->load();
    if (clank != lastClank || clankF != lastClankFreq)
    {
        clankEq.set (C::makePeakFilter (sr, clankF, 1.2f, juce::Decibels::decibelsToGain (clank)));
        lastClank = clank;
        lastClankFreq = clankF;
    }

    const float fizz = juce::jmin (pFizz->load(), (float) (sr * 0.45));
    if (fizz != lastFizz)
    {
        fizzLP.set (C::makeLowPass (sr, fizz, 0.707f));
        lastFizz = fizz;
    }

    // low band: parallel-ish compression with automatic make-up gain
    const float amt = pLowComp->load();
    const float thresholdDb = -6.f - amt * 24.f;
    const float ratio = 1.5f + amt * 6.5f;
    lowComp.setThreshold (thresholdDb);
    lowComp.setRatio (ratio);
    const float makeupDb = juce::jmin (12.f, 0.4f * (-thresholdDb) * (1.f - 1.f / ratio));
    lowSm.setTargetValue (anySolo && ! sl ? 0.f : juce::Decibels::decibelsToGain (pLowLevel->load() + makeupDb));
    lastComp = amt;

    // ---- extra FX
    gate.setThreshold (pGateThresh->load());
    gate.setRelease (pGateRel->load());

    const float semis = pPitchSemi->load() + pPitchFine->load() / 100.f;
    pitchActive = std::fabs (semis) > 0.001f;
    pitchOn.store (pitchActive);
    pitchRatio = std::pow (2.f, semis / 12.f);
    pitchMixSm.setTargetValue (pitchActive ? pPitchMix->load() : 0.f);

    {
        static const float beatMul[] = { 0.f, 1.f, 0.5f, 0.75f, 0.25f };   // of a quarter note
        const int sync = juce::jlimit (0, 4, (int) pDelaySync->load());
        const double ms = sync == 0 ? (double) pDelayTime->load()
                                    : 60000.0 / juce::jmax (30.0, hostBpm) * beatMul[sync];
        delayTimeSm.setTargetValue ((float) juce::jlimit (8.0, sr * 1.4, sr * ms / 1000.0));
        delayFbSm.setTargetValue (pDelayFb->load());
        delayMixSm.setTargetValue (pFxOn->load() > 0.5f ? pDelayMix->load() : 0.f);

        const float tone = juce::jmin (pDelayTone->load(), (float) (sr * 0.45));
        if (tone != lastDelayTone)
        {
            auto c = C::makeLowPass (sr, tone, 0.707f);
            for (auto& f : delayLP) f.coefficients = c;
            lastDelayTone = tone;
        }
    }

    revMixSm.setTargetValue (pFxOn->load() > 0.5f ? pRevMix->load() : 0.f);
    const float rs = pRevSize->load();
    if (rs != lastRevSize)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.25f + 0.7f * rs;
        rp.damping = 0.55f;
        rp.wetLevel = 1.f;
        rp.dryLevel = 0.f;
        rp.width = 1.f;
        rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        lastRevSize = rs;
    }
}

//==============================================================================
void MetalBassForgeAudioProcessor::processWah (juce::AudioBuffer<float>& buf, int numCh, int n)
{
    const bool on = pWahOn->load() > 0.5f && pFxOn->load() > 0.5f;
    wahMixSm.setTargetValue (on ? pWahMix->load() : 0.f);
    if (! on && ! wahMixSm.isSmoothing() && wahMixSm.getCurrentValue() < 0.0005f)
        return;

    const int mode = (int) pWahMode->load();
    const float base = pWahPos->load(), depth = pWahDepth->load();
    const float q = pWahQ->load();
    const float inc = pWahRate->load() / (float) sampleRate;
    wahFilter.setResonance (q);

    for (int i = 0; i < n; ++i)
    {
        const float mix = wahMixSm.getNextValue();

        const float a = std::abs (buf.getReadPointer (0)[i]);
        wahEnv += (a > wahEnv ? 0.02f : 0.0008f) * (a - wahEnv);

        float pos = base;
        if (mode == 1)
        {
            wahPhase += inc;
            wahPhase -= std::floor (wahPhase);
            pos = base + depth * 0.5f * std::sin (juce::MathConstants<float>::twoPi * wahPhase);
        }
        else if (mode == 2)
            pos = base * 0.5f + depth * juce::jlimit (0.f, 1.f, wahEnv * 5.f);

        wahPosSm.setTargetValue (juce::jlimit (0.f, 1.f, pos));
        const float ps = wahPosSm.getNextValue();

        if ((i & 15) == 0)
            wahFilter.setCutoffFrequency (juce::jmin (300.f * std::pow (8.f, ps), (float) sampleRate * 0.45f));

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* w = buf.getWritePointer (ch);
            const float x = w[i];
            const float wet = wahFilter.processSample (ch, x) * (1.f / q) * 1.8f;
            w[i] = x * (1.f - mix) + wet * mix;
        }
    }
}

//==============================================================================
void MetalBassForgeAudioProcessor::copyTunerSamples (float* dest, int n) const
{
    n = juce::jmin (n, tunerRingSize);
    int start = tunerPos.load() - n;
    for (int i = 0; i < n; ++i)
        dest[i] = tunerRing[(size_t) ((start + i) & (tunerRingSize - 1))];
}

float MetalBassForgeAudioProcessor::detectPitch (const float* x, int n, double sr)
{
    const int W = n / 2;
    double e = 0.0;
    for (int i = 0; i < n; ++i) e += (double) x[i] * x[i];
    if (std::sqrt (e / n) < 0.004) return 0.f;

    const int tauMax = juce::jmin (W - 2, (int) (sr / 28.0));
    const int tauMin = juce::jmax (2, (int) (sr / 400.0));
    if (tauMax <= tauMin + 2) return 0.f;

    std::vector<float> cm ((size_t) tauMax + 1, 1.f);
    double run = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < W; ++j)
        {
            const float df = x[j] - x[j + tau];
            sum += (double) df * df;
        }
        run += sum;
        cm[(size_t) tau] = run > 0.0 ? (float) (sum * tau / run) : 1.f;
    }

    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
        if (cm[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    if (best < 1) return 0.f;

    const float s0 = cm[(size_t) best - 1], s1 = cm[(size_t) best], s2 = cm[(size_t) best + 1];
    const float denom = 2.f * (s0 - 2.f * s1 + s2);
    const float shift = denom != 0.f ? (s0 - s2) / denom : 0.f;
    return (float) (sr / ((double) best + shift));
}

//==============================================================================
float MetalBassForgeAudioProcessor::driveSample (float x, float g, float character) const noexcept
{
    // BOD-ish: bias before tanh -> even harmonics, then a bit of level trim
    const float a = (std::tanh (g * x + ampBias) - biasOff) * 0.8f;

    // Darkglass-ish: hard first stage, slight asymmetry, second clipper
    float z = std::tanh (g * ampHard * x);
    z = z + 0.15f * z * z;
    const float b = std::tanh (2.0f * z) * 0.7f;

    return a * (1.f - character) + b * character;
}

//==============================================================================
void MetalBassForgeAudioProcessor::applyCab (juce::AudioBuffer<float>& buf, int numCh, int n)
{
    if ((int) pCabType->load() == 0 || ! cabRequested.load())
        return;

    for (int ch = 0; ch < numCh; ++ch)
        cabDry.copyFrom (ch, 0, buf, ch, 0, n);

    juce::dsp::AudioBlock<float> blk (buf.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    convolution.process (juce::dsp::ProcessContextReplacing<float> (blk));
    micLow.process (buf);
    micMid.process (buf);
    micHigh.process (buf);

    for (int i = 0; i < n; ++i)
    {
        const float m = cabMixSm.getNextValue();
        const float gn = cabGainSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* w = buf.getWritePointer (ch);
            w[i] = cabDry.getReadPointer (ch)[i] * (1.f - m) + w[i] * m * gn;
        }
    }
}

//==============================================================================
void MetalBassForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // trial over and no valid licence: output is muted
    if (license.getState() == LicenseManager::Expired)
    {
        buffer.clear();
        outLevel.store (0.f);
        return;
    }

    // mono in -> stereo out: duplicate so both channels carry the signal
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    // tuner: mirror the raw input (channel 0) into a ring buffer
    {
        const float* in0 = buffer.getReadPointer (0);
        int pos = tunerPos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            tunerRing[(size_t) ((pos + i) & (tunerRingSize - 1))] = in0[i];
        tunerPos.store (pos + n, std::memory_order_release);
    }

    if (pTunerMute->load() > 0.5f)
    {
        buffer.clear();
        outLevel.store (0.f);
        return;
    }

    {
        bool haveHostBpm = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpm = pos->getBpm())
                {
                    hostBpm = *bpm;
                    haveHostBpm = true;
                }
        hostBpmShown.store (haveHostBpm ? hostBpm : 0.0);
        if (! haveHostBpm && manualBpm.load() > 0.0)
            hostBpm = manualBpm.load();
    }

    if (n > lowBuf.getNumSamples())
    {
        lowBuf.setSize (2, n, false, false, true);
        highBuf.setSize (2, n, false, false, true);
        dryBuf.setSize (2, n, false, false, true);
        cabDry.setSize (2, n, false, false, true);
    }
    if (n > revBuf.getNumSamples())
        revBuf.setSize (2, n, false, false, true);

    // quality / latency mode
    const int mode = juce::jlimit (0, 2, (int) pQuality->load());
    if (mode != activeMode)
    {
        activeMode = mode;
        if (oversampling[mode]) oversampling[mode]->reset();
        lowDelay.setDelay ((float) modeLatency[mode]);
        dryDelay.setDelay ((float) modeLatency[mode]);
        triggerAsyncUpdate();     // tell the host about the new latency from the message thread
    }

    updateParameters();
    const bool cabAll = pCabAll->load() > 0.5f;

    // --- input gain
    for (int i = 0; i < n; ++i)
    {
        const float g = inSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }

    // --- tight high-pass (removes sub-rumble that would eat headroom)
    inHP.process (buffer);

    // --- noise gate (threshold at -80 dB = off)
    if (pFxOn->load() > 0.5f && pGateThresh->load() > -79.5f)
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        gate.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // --- pitch shift / retune (sits before the split, so the whole signal is retuned)
    {
        const bool compute = pitchActive || pitchMixSm.isSmoothing() || pitchMixSm.getCurrentValue() > 0.0005f;
        const float period = trackedPeriod.load();
        for (int i = 0; i < n; ++i)
        {
            const float m = pitchMixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* w = buffer.getWritePointer (ch);
                const float x = w[i];
                const float y = pitchShifter[ch].process (x, pitchRatio, compute, period);
                if (compute) w[i] = x * (1.f - m) + y * m;
            }
        }
    }

    // --- crossover (LR4): low / high / phase-coherent dry
    for (auto* b : { &lowBuf, &highBuf, &dryBuf })
        b->setSize (numCh, n, false, false, true);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        float* lo = lowBuf.getWritePointer (ch);
        float* hi = highBuf.getWritePointer (ch);
        float* dr = dryBuf.getWritePointer (ch);

        for (int i = 0; i < n; ++i)
        {
            float l, h;
            crossover.processSample (ch, in[i], l, h);
            lo[i] = l;
            hi[i] = h;
            dr[i] = l + h;
        }
    }

    // --- LOW: clean + compressed
    {
        juce::dsp::AudioBlock<float> blk (lowBuf);
        lowComp.process (juce::dsp::ProcessContextReplacing<float> (blk));
        lowDelay.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // --- DRY: delay-match to the oversampled path
    {
        juce::dsp::AudioBlock<float> blk (dryBuf);
        dryDelay.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    // --- HIGH: scoop -> amp pre-EQ -> drive (4x oversampled) -> HPF -> tone stack -> clank -> hi-cut -> [cab]
    processWah (highBuf, numCh, n);
    scoopEq.process (highBuf);
    preMid.process (highBuf);
   #if VC_WITH_NAM
    const bool useNam = pNamOn != nullptr && pNamOn->load() > 0.5f && nam.isReady();
    if (useNam)
    {
        // keep the same latency as the built-in drive path so LOW / DRY stay phase-aligned
        if (mode != 0)
        {
            auto& os = *oversampling[mode];
            juce::dsp::AudioBlock<float> hb (highBuf);
            os.processSamplesUp (hb);
            os.processSamplesDown (hb);
        }
        // DRIVE knob = input level into the model (-12 .. +12 dB)
        const float gNew = juce::Decibels::decibelsToGain ((pDrive->load() - 0.5f) * 24.f);
        nam.process (highBuf.getArrayOfWritePointers(), numCh, n, namGainPrev, gNew);
        namGainPrev = gNew;
        charSm.skip (n);
        driveSm.skip (n * (mode == 0 ? 1 : mode == 1 ? 2 : 4));
    }
    else
   #endif
    {
    if (mode == 0)
    {
        // Live mode: no oversampling, zero added latency
        for (int i = 0; i < n; ++i)
        {
            const float g = driveSm.getNextValue();
            const float c = juce::jlimit (0.f, 1.f, charSm.getNextValue() + ampCharBias);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* p = highBuf.getWritePointer (ch);
                p[i] = driveSample (p[i], g, c);
            }
        }
    }
    else
    {
        auto& os = *oversampling[mode];
        juce::dsp::AudioBlock<float> hb (highBuf);
        auto up = os.processSamplesUp (hb);
        const size_t upN = up.getNumSamples();
        const size_t upCh = up.getNumChannels();

        for (size_t i = 0; i < upN; ++i)
        {
            const float g = driveSm.getNextValue();
            const float c = juce::jlimit (0.f, 1.f, charSm.getCurrentValue() + ampCharBias);
            for (size_t ch = 0; ch < upCh; ++ch)
            {
                float* p = up.getChannelPointer (ch);
                p[i] = driveSample (p[i], g, c);
            }
        }
        charSm.skip (n);   // charSm advances at the base rate

        os.processSamplesDown (hb);
    }
    }
    postHP.process (highBuf);
    toneLow.process (highBuf);
    toneMid.process (highBuf);
    toneHigh.process (highBuf);
    tonePres.process (highBuf);
    clankEq.process (highBuf);
    fizzLP.process (highBuf);

    if (! cabAll)
        applyCab (highBuf, numCh, n);

    // --- sum
    for (int i = 0; i < n; ++i)
    {
        const float lg = lowSm.getNextValue();
        const float gg = gritSm.getNextValue();
        const float dg = drySm.getNextValue();
        const float og = outSm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float s = lowBuf.getReadPointer (ch)[i] * lg
                          + highBuf.getReadPointer (ch)[i] * gg
                          + dryBuf.getReadPointer (ch)[i] * dg;
            buffer.getWritePointer (ch)[i] = s * og;
        }
    }

    if (cabAll)
        applyCab (buffer, numCh, n);

    // --- delay
    for (int i = 0; i < n; ++i)
    {
        const float t = delayTimeSm.getNextValue();
        const float fb = delayFbSm.getNextValue();
        const float mx = delayMixSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* w = buffer.getWritePointer (ch);
            const float x = w[i];
            const float d = delayLine[ch].read (t);
            delayLine[ch].write (x + delayLP[ch].processSample (d) * fb);
            w[i] = x + d * mx;
        }
    }

    // --- reverb (high-passed send, wet only)
    if (revMixSm.getTargetValue() > 0.0005f || revMixSm.isSmoothing())
    {
        for (int ch = 0; ch < numCh; ++ch)
            revBuf.copyFrom (ch, 0, buffer, ch, 0, n);
        revBuf.setSize (numCh, n, true, false, true);
        revHP.process (revBuf);
        {
            juce::dsp::AudioBlock<float> blk (revBuf.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
            reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = revMixSm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] += revBuf.getReadPointer (ch)[i] * m * 0.8f;
        }
    }

    // --- safety limiter (instant attack, smooth release; unity gain below the ceiling)
    {
        const float ceiling = 0.944f;   // -0.5 dBFS
        const float rel = 1.f - std::exp (-1.f / (0.06f * (float) sampleRate));
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getReadPointer (ch)[i]));
            const float target = peak > ceiling ? ceiling / peak : 1.f;
            if (target < limGain) limGain = target;
            else                  limGain += (target - limGain) * rel;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= limGain;
        }
    }

    const float pk = buffer.getMagnitude (0, n);
    const float prev = outLevel.load();
    outLevel.store (pk > prev ? pk : prev * 0.92f);
}

//==============================================================================
// Cabinet / IR handling
//==============================================================================
void MetalBassForgeAudioProcessor::applyCabSelection()
{
    const int t = (int) pCabType->load();

    // cab menu: 0 off, 1-4 classic cabs, 5 user IR, 6-11 genre-voiced cabs
    const int bi = (t >= 1 && t <= 4) ? t - 1 : (t >= 6 && t <= 11) ? t - 2 : -1;
    if (bi >= 0)
    {
        if (builtInIR[bi].getNumSamples() > 0)
        {
            cabRequested.store (true);
            juce::AudioBuffer<float> copy (builtInIR[bi]);
            convolution.loadImpulseResponse (std::move (copy), sampleRate,
                                             juce::dsp::Convolution::Stereo::no,
                                             juce::dsp::Convolution::Trim::no,
                                             juce::dsp::Convolution::Normalise::yes);
        }
    }
    else if (t == 5)
    {
        juce::MemoryBlock data;
        {
            const juce::ScopedLock sl (irLock);
            data = userIRData;
        }
        if (data.getSize() > 0)
        {
            cabRequested.store (true);
            convolution.loadImpulseResponse (data.getData(), data.getSize(),
                                             juce::dsp::Convolution::Stereo::no,
                                             juce::dsp::Convolution::Trim::yes, 0,
                                             juce::dsp::Convolution::Normalise::yes);
        }
    }
}

juce::String MetalBassForgeAudioProcessor::getUserIRName() const
{
    const juce::ScopedLock sl (irLock);
    return userIRName;
}

bool MetalBassForgeAudioProcessor::loadUserIR (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "File not found.";
        return false;
    }
    if (file.getSize() > 4 * 1024 * 1024)
    {
        error = "IR file is too large (max 4 MB).";
        return false;
    }

    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
    {
        error = "Could not read the file.";
        return false;
    }
    return loadUserIRFromMemory (mb, file.getFileNameWithoutExtension(), error);
}

bool MetalBassForgeAudioProcessor::loadUserIRFromMemory (const juce::MemoryBlock& data, const juce::String& name,
                                                         juce::String& error)
{
    if (data.getSize() == 0 || data.getSize() > 4 * 1024 * 1024)
    {
        error = "IR file is empty or too large (max 4 MB).";
        return false;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (
        fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (data.getData(), data.getSize(), false)));

    if (reader == nullptr)
    {
        error = "Unsupported or corrupt audio file. Use WAV, AIFF or FLAC.";
        return false;
    }
    if (reader->lengthInSamples <= 0 || (double) reader->lengthInSamples / reader->sampleRate > 6.0)
    {
        error = "The impulse response must be shorter than 6 seconds.";
        return false;
    }
    reader.reset();

    {
        const juce::ScopedLock sl (irLock);
        userIRData = data;
        userIRName = name;
    }
    apvts.state.setProperty ("irName", name, nullptr);
    apvts.state.setProperty ("irData", data.toBase64Encoding(), nullptr);

    if (auto* p = apvts.getParameter ("cabType"))
        p->setValueNotifyingHost (p->convertTo0to1 (5.f));

    triggerAsyncUpdate();
    return true;
}

void MetalBassForgeAudioProcessor::restoreUserIRFromState()
{
    const auto name = apvts.state.getProperty ("irName").toString();
    const auto b64 = apvts.state.getProperty ("irData").toString();

    juce::MemoryBlock mb;
    if (b64.isNotEmpty())
        mb.fromBase64Encoding (b64);

    {
        const juce::ScopedLock sl (irLock);
        userIRData = mb;
        userIRName = mb.getSize() > 0 ? name : juce::String();
    }
    triggerAsyncUpdate();
}

#if VC_WITH_NAM
bool MetalBassForgeAudioProcessor::loadNamModel (const juce::File& file, juce::String& error)
{
    std::string err;
    const bool ok = nam.load (file.getFullPathName().toStdString(), err);
    error = juce::String (err);
    if (nam.isReady() || ! nam.getName().empty())
        apvts.state.setProperty ("namPath", file.getFullPathName(), nullptr);
    return ok;
}

void MetalBassForgeAudioProcessor::restoreNamFromState()
{
    const juce::File f (apvts.state.getProperty ("namPath").toString());
    if (f.existsAsFile())
    {
        juce::String err;
        loadNamModel (f, err);
    }
}
#endif

//==============================================================================
juce::AudioProcessorEditor* MetalBassForgeAudioProcessor::createEditor()
{
    return new MetalBassForgeAudioProcessorEditor (*this);
}

void MetalBassForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MetalBassForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            restoreUserIRFromState();
           #if VC_WITH_NAM
            restoreNamFromState();
           #endif
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetalBassForgeAudioProcessor();
}
