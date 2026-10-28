#include "NamEngine.h"

#include "NAM/get_dsp.h"
#include "NAM/dsp.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <vector>

struct NamEngine::Impl
{
    struct Model
    {
        std::unique_ptr<nam::DSP> dsp[2];   // one instance per channel (models are stateful, mono)
        double modelRate = -1.0;
        float outGain = 1.f;                // loudness normalisation
        std::string name;
    };

    std::mutex m;                           // audio thread only ever try_locks
    std::unique_ptr<Model> model;
    double sr = 48000.0;
    int maxBlock = 512;
    std::atomic<bool> ready { false };

    std::vector<NAM_SAMPLE> in[2], out[2];

    void resetModel (Model& mdl)
    {
        for (auto& d : mdl.dsp)
            if (d) d->Reset (sr, maxBlock);
        for (int c = 0; c < 2; ++c) { in[c].assign ((size_t) maxBlock, 0); out[c].assign ((size_t) maxBlock, 0); }
    }

    bool rateUsable (const Model& mdl) const
    {
        if (mdl.modelRate <= 0.0) return true;                              // unknown: assume ok
        if (mdl.dsp[0] && mdl.dsp[0]->SupportsArbitrarySampleRate()) return true;
        return std::abs (mdl.modelRate - sr) < 1.0;                         // otherwise host rate must match
    }
};

NamEngine::NamEngine() : impl (std::make_unique<Impl>()) {}
NamEngine::~NamEngine() = default;

bool NamEngine::load (const std::string& path, std::string& error)
{
    try
    {
        auto mdl = std::make_unique<Impl::Model>();
        mdl->dsp[0] = nam::get_dsp (std::filesystem::path (path));
        mdl->dsp[1] = nam::get_dsp (std::filesystem::path (path));
        if (! mdl->dsp[0] || ! mdl->dsp[1]) { error = "Could not create the model."; return false; }

        mdl->modelRate = mdl->dsp[0]->GetExpectedSampleRate();
        mdl->name = std::filesystem::path (path).filename().string();
        if (mdl->dsp[0]->HasLoudness())   // bring every model to a comparable level (-18 dB target)
            mdl->outGain = (float) std::pow (10.0, (-18.0 - mdl->dsp[0]->GetLoudness()) / 20.0);

        impl->resetModel (*mdl);          // prewarm happens here, off the audio thread

        const bool ok = impl->rateUsable (*mdl);
        {
            std::lock_guard<std::mutex> lk (impl->m);
            impl->model = std::move (mdl);
            impl->ready.store (ok);
        }
        if (! ok)
            error = "Model was trained at " + std::to_string ((int) impl->model->modelRate)
                    + " Hz and does not support the current host rate.";
        return ok;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
    catch (...)
    {
        error = "Unknown error while loading the model.";
        return false;
    }
}

void NamEngine::unload()
{
    std::lock_guard<std::mutex> lk (impl->m);
    impl->ready.store (false);
    impl->model.reset();
}

void NamEngine::prepare (double sampleRate, int maxBlockSize)
{
    std::lock_guard<std::mutex> lk (impl->m);
    impl->sr = sampleRate;
    impl->maxBlock = std::max (16, maxBlockSize);
    for (int c = 0; c < 2; ++c) { impl->in[c].assign ((size_t) impl->maxBlock, 0); impl->out[c].assign ((size_t) impl->maxBlock, 0); }
    if (impl->model)
    {
        try { impl->resetModel (*impl->model); } catch (...) { impl->ready.store (false); return; }
        impl->ready.store (impl->rateUsable (*impl->model));
    }
}

bool NamEngine::isReady() const noexcept { return impl->ready.load(); }

void NamEngine::process (float* const* ch, int numCh, int n, float g0, float g1)
{
    std::unique_lock<std::mutex> lk (impl->m, std::try_to_lock);
    if (! lk.owns_lock() || ! impl->model || ! impl->ready.load())
        return;   // model is being swapped: pass the signal through for this block

    auto& mdl = *impl->model;
    int chans = std::min (numCh, 2);

    // A mono bass DI usually arrives as identical L/R: run the model once and copy (halves the CPU load)
    const bool dualMono = chans == 2 && std::equal (ch[0], ch[0] + n, ch[1]);
    if (dualMono) chans = 1;

    for (int start = 0; start < n; start += impl->maxBlock)
    {
        const int len = std::min (impl->maxBlock, n - start);
        for (int c = 0; c < chans; ++c)
        {
            auto& ib = impl->in[c];
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) (start + i) / (float) std::max (1, n);
                ib[(size_t) i] = (NAM_SAMPLE) (ch[c][start + i] * (g0 + (g1 - g0) * t));
            }
            NAM_SAMPLE* pin = ib.data();
            NAM_SAMPLE* pout = impl->out[c].data();
            mdl.dsp[c]->process (&pin, &pout, len);
            for (int i = 0; i < len; ++i)
                ch[c][start + i] = (float) pout[i] * mdl.outGain;
            if (dualMono)
                std::copy (ch[0] + start, ch[0] + start + len, ch[1] + start);
        }
    }
}

std::string NamEngine::getName() const
{
    std::lock_guard<std::mutex> lk (impl->m);
    return impl->model ? impl->model->name : std::string();
}

double NamEngine::getModelSampleRate() const
{
    std::lock_guard<std::mutex> lk (impl->m);
    return impl->model ? impl->model->modelRate : -1.0;
}
