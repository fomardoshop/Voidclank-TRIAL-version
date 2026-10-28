#pragma once

#include <JuceHeader.h>
#include "License.h"

#ifndef VC_WITH_NAM
 #define VC_WITH_NAM 0
#endif
#if VC_WITH_NAM
 #include "NamEngine.h"
#endif

//==============================================================================
// Voidclank
//
//  input -> gain -> tight HPF -> LR4 crossover
//     LOW  (clean sub, parallel compression)                      ----\
//     HIGH (scoop -> amp pre-EQ -> drive 4x OS -> post HPF ->          \
//           amp tone stack -> clank EQ -> fizz LPF -> [CAB])      ------>-- sum -> [CAB] -> limiter
//     DRY  (phase-coherent low+high before processing)            ----/
//
//  Solo buttons for LOW / GRIT / DRY.  Cab = built-in cabinets or a user IR (convolution),
//  applied either to the GRIT path only or to the whole mix.
//==============================================================================
class MetalBassForgeAudioProcessor : public juce::AudioProcessor,
                                     private juce::AsyncUpdater,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    MetalBassForgeAudioProcessor();
    ~MetalBassForgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- cabinet / IR loader (message thread)
    bool loadUserIR (const juce::File& file, juce::String& error);
    bool loadUserIRFromMemory (const juce::MemoryBlock& data, const juce::String& name, juce::String& error);
    juce::String getUserIRName() const;

   #if VC_WITH_NAM
    // optional Neural Amp Modeler stage (replaces the built-in drive stage in the GRIT band)
    bool loadNamModel (const juce::File& file, juce::String& error);
    juce::String getNamName() const { return juce::String (nam.getName()); }
    bool isNamActive() const noexcept { return pNamOn != nullptr && pNamOn->load() > 0.5f && nam.isReady(); }
    bool isNamLoaded() const { return ! nam.getName().empty(); }
   #endif
    juce::MemoryBlock getUserIRData() const { const juce::ScopedLock sl (irLock); return userIRData; }
    void applyCabSelection();                       // (re)loads the IR for the current cab choice
    bool isCabReady() const noexcept { return convolution.getCurrentIRSize() > 0; }
    bool isCabLoadRequested() const noexcept { return cabRequested.load(); }

    // ---- tuner (input is mirrored into a ring buffer; analysis runs on the GUI thread)
    void copyTunerSamples (float* dest, int n) const;
    double getCurrentSampleRate() const noexcept { return sampleRate; }
    int getCurrentBlockSize() const noexcept { return blockSize; }
    static float detectPitch (const float* x, int n, double sr);   // YIN, returns Hz or 0

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getAmpDescriptions();

    LicenseManager license;
    std::atomic<float> outLevel { 0.f };   // decaying output peak for the GUI meter
    juce::AudioProcessorValueTreeState apvts;

private:
    // Small stereo wrapper around dsp::IIR::Filter
    struct Biquad
    {
        using Coeffs = juce::dsp::IIR::Coefficients<float>;
        juce::dsp::IIR::Filter<float> f[2];

        void set (Coeffs::Ptr c) { for (auto& x : f) x.coefficients = c; }
        void reset() { for (auto& x : f) x.reset(); }
        void process (juce::AudioBuffer<float>& b)
        {
            const auto n = (size_t) b.getNumSamples();
            for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
            {
                float* ptr = b.getWritePointer (ch);
                juce::dsp::AudioBlock<float> blk (&ptr, 1, n);
                f[ch].process (juce::dsp::ProcessContextReplacing<float> (blk));
            }
        }
    };

    void handleAsyncUpdate() override
    {
        applyCabSelection();
        if (modeLatency[activeMode] != getLatencySamples())
            setLatencySamples (modeLatency[activeMode]);
    }
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }

    void updateParameters();
    void applyCab (juce::AudioBuffer<float>& buf, int numCh, int n);
    void restoreUserIRFromState();
   #if VC_WITH_NAM
    mutable NamEngine nam;
    std::atomic<float>* pNamOn = nullptr;
    float namGainPrev = 1.f;
    void restoreNamFromState();
   #endif
    float driveSample (float x, float gain, float character) const noexcept;
    static juce::AudioBuffer<float> makeBuiltInIR (int type, double sr);

    double sampleRate = 44100.0;
    int latencySamples = 0, blockSize = 0;
    int activeMode = 1;
    int modeLatency[3] { 0, 0, 0 };
    int currentPreset = 0;

    // parameter pointers
    std::atomic<float> *pInGain, *pTight, *pXover, *pLowComp, *pLowLevel, *pDrive, *pCharacter,
        *pScoop, *pClank, *pClankFreq, *pFizz, *pGritLevel, *pDry, *pOutGain,
        *pSoloLow, *pSoloGrit, *pSoloDry,
        *pAmp, *pBass, *pMid, *pTreble, *pPres,
        *pCabType, *pCabMix, *pCabLevel, *pCabAll,
        *pGateThresh, *pGateRel, *pPitchSemi, *pPitchFine, *pPitchMix,
        *pFxOn, *pWahOn, *pWahMode, *pWahPos, *pWahRate, *pWahDepth, *pWahQ, *pWahMix,
        *pDelayTime, *pDelaySync, *pDelayFb, *pDelayTone, *pDelayMix,
        *pRevSize, *pRevMix, *pTunerMute, *pQuality, *pMicType, *pMicPos, *pMicDist;

    // DSP
    juce::dsp::LinkwitzRileyFilter<float> crossover;
    juce::dsp::Compressor<float> lowComp;
    float limGain = 1.f;
    std::atomic<bool> cabRequested { false };
    juce::dsp::Convolution convolution;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling[3];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lowDelay { 2048 },
        dryDelay { 2048 };

    Biquad micLow, micMid, micHigh;
    float lastMicType = -1.f, lastMicPos = -1.f, lastMicDist = -1.f;
    Biquad inHP, scoopEq, preMid, postHP, toneLow, toneMid, toneHigh, tonePres, clankEq, fizzLP;

    juce::AudioBuffer<float> lowBuf, highBuf, dryBuf, cabDry;
    static constexpr int numBuiltInIRs = 10;      // 4 classic cabs + 6 genre-voiced cabs
    juce::AudioBuffer<float> builtInIR[numBuiltInIRs];

    juce::SmoothedValue<float> inSm, lowSm, gritSm, drySm, outSm, driveSm, charSm, cabMixSm, cabGainSm;

    // current amp model (set from updateParameters)
    float ampBias = 0.18f, ampHard = 2.2f, biasOff = 0.f, ampCharBias = 0.f;

    // user IR kept as raw file bytes so it is saved with the session
    mutable juce::CriticalSection irLock;
    juce::MemoryBlock userIRData;
    juce::String userIRName;

    // ---- extra FX --------------------------------------------------------
    // tuner ring buffer
    static constexpr int tunerRingSize = 32768;
    std::array<float, tunerRingSize> tunerRing {};
    std::atomic<int> tunerPos { 0 };

    // noise gate
    juce::dsp::NoiseGate<float> gate;

    // delay-line pitch shifter (two crossfaded taps, ~80 ms window)
    struct PitchShifter
    {
        std::vector<float> buf;
        int mask = 0, wp = 0, win = 1;
        float phase = 0.f, spacing = 0.f;

        void prepare (double sr)
        {
            win = juce::jmax (64, (int) (sr * 0.08));
            int n = 1;
            while (n < win * 2 + 8) n <<= 1;
            buf.assign ((size_t) n, 0.f);
            mask = n - 1; wp = 0; phase = 0.f;
            spacing = (float) win * 0.5f;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.f); wp = 0; phase = 0.f; spacing = (float) win * 0.5f; }

        // distance between the two read taps: a whole number of input periods near half a window,
        // so both taps carry the same waveform and the crossfade stays coherent (mono bass is periodic)
        float chooseSpacing (float period) const
        {
            const float half = (float) win * 0.5f;
            if (period < 8.f) return half;
            const float k = juce::jmax (1.f, std::round (half / period));
            const float s = k * period;
            return (s < (float) win * 0.3f || s > (float) win * 0.7f) ? half : s;
        }

        float tap (float d) const
        {
            const float rp = (float) wp - (d + 2.f);
            const int i0 = (int) std::floor (rp);
            const float fr = rp - (float) i0;
            return buf[(size_t) (i0 & mask)] * (1.f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
        }
        static float hann (float ph) { return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * ph); }

        float process (float x, float ratio, bool compute, float period)
        {
            buf[(size_t) wp] = x;
            float out = 0.f;
            if (compute)
            {
                phase += (1.f - ratio) / (float) win;
                phase -= std::floor (phase);
                spacing += juce::jlimit (-0.05f, 0.05f, chooseSpacing (period) - spacing);

                const float dA = phase * (float) win;
                float dB = dA + spacing;
                if (dB >= (float) win) dB -= (float) win;
                const float gA = hann (phase), gB = hann (dB / (float) win);
                out = (tap (dA) * gA + tap (dB) * gB) / juce::jmax (0.05f, gA + gB);
            }
            wp = (wp + 1) & mask;
            return out;
        }
    };
    PitchShifter pitchShifter[2];
    float pitchRatio = 1.f;
    bool pitchActive = false;
    std::atomic<bool> pitchOn { false };
    std::atomic<float> trackedPeriod { 0.f };
    std::unique_ptr<juce::Thread> pitchTracker;
    juce::SmoothedValue<float> pitchMixSm;

    // wah (on the GRIT path, before the drive)
    juce::dsp::StateVariableTPTFilter<float> wahFilter;
    float wahPhase = 0.f, wahEnv = 0.f;
    juce::SmoothedValue<float> wahPosSm, wahMixSm;
    void processWah (juce::AudioBuffer<float>& buf, int numCh, int n);

    // delay (own circular buffer, linear interpolation)
    struct SimpleDelay
    {
        std::vector<float> buf;
        int mask = 0, wp = 0;
        void prepare (int maxSamples)
        {
            int n = 1;
            while (n < maxSamples + 4) n <<= 1;
            buf.assign ((size_t) n, 0.f);
            mask = n - 1; wp = 0;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.f); wp = 0; }
        float read (float d) const
        {
            const float rp = (float) wp - d;
            const int i0 = (int) std::floor (rp);
            const float fr = rp - (float) i0;
            return buf[(size_t) (i0 & mask)] * (1.f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
        }
        void write (float x) { buf[(size_t) wp] = x; wp = (wp + 1) & mask; }
    };
    SimpleDelay delayLine[2];
    juce::dsp::IIR::Filter<float> delayLP[2];
    juce::SmoothedValue<float> delayTimeSm, delayFbSm, delayMixSm;
    float lastDelayTone = -1.f;
    double hostBpm = 120.0;

public:
    std::atomic<double> hostBpmShown { 0.0 };   // BPM reported by the host (0 = host gives none)
    std::atomic<double> manualBpm { 0.0 };      // tap-tempo value, used for delay sync when the host gives none
private:

    // reverb (high-passed send, wet only)
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> revBuf;
    Biquad revHP;
    juce::SmoothedValue<float> revMixSm;
    float lastRevSize = -1.f;

    // change tracking so coefficients are only rebuilt when needed
    float lastTight = -1.f, lastXover = -1.f, lastScoop = -1.f, lastClank = -1.f,
          lastClankFreq = -1.f, lastFizz = -1.f, lastComp = -1.f,
          lastAmp = -1.f, lastBass = -99.f, lastMid = -99.f, lastTreble = -99.f, lastPres = -99.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalBassForgeAudioProcessor)
};
