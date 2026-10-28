#pragma once
// Thin wrapper around NeuralAmpModelerCore (MIT, https://github.com/sdatkinson/NeuralAmpModelerCore).
// No NAM / Eigen headers leak out of this file, so the plugin itself can stay C++17.
#include <memory>
#include <string>

class NamEngine
{
public:
    NamEngine();
    ~NamEngine();

    // Message thread.  Loads a .nam model (WaveNet / LSTM / A1 / A2 ...).  Returns false + error text on failure.
    bool load (const std::string& path, std::string& error);
    void unload();

    // Message thread (prepareToPlay).  Re-initialises the model for the host rate / block size.
    void prepare (double sampleRate, int maxBlockSize);

    // Audio thread.  True when a model is loaded, prepared and usable at the current host rate.
    bool isReady() const noexcept;

    // Audio thread.  In-place, mono model per channel.  inGainStart/End = linear gain ramp across the block.
    void process (float* const* channels, int numChannels, int numSamples, float inGainStart, float inGainEnd);

    std::string getName() const;
    double getModelSampleRate() const;   // -1 if unknown

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
