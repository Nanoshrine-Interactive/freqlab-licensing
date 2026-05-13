#pragma once

// freqlab_response.h - audio-thread response to license state.
//
// This file is the seller's behavioral policy. Your plugin's
// ProcessBlock calls `freqlab_should_run_dsp(outputs, nChans, nFrames)`
// at the top; if it returns false, ProcessBlock returns early without
// running DSP. When false is returned, this helper is responsible for
// leaving the output buffer in a safe state (the default impl zeros it).
//
// Edit the switch arms below to customize behavior per status:
//   - want a watermark on Expired? leave the arm returning true and
//     mix a noise burst into `outputs` here.
//   - want Expired to keep playing with banner-only nag? return true.
//   - want stricter Tampered handling? corrupt `outputs` instead of
//     zeroing it.
//
// Only currentStatus() is called here. It's a lock-free atomic read
// and safe to call from the audio thread.

#include "freqlab_licensing.h"
#include "IPlug_include_in_plug_hdr.h"

inline bool freqlab_should_run_dsp(iplug::sample** outputs, int nChans, int nFrames)
{
    using S = ::freqlab::licensing::Status;
    switch (::freqlab::licensing::currentStatus())
    {
        // Fully licensed, paid trial, grace period, or unconfigured
        // build (NoConfig = local dev / stub). Run DSP normally.
        case S::Licensed:
        case S::Trial:
        case S::GracePeriod:
        case S::NoConfig:
            return true;

        // License is invalid. Default: silent. memset zeros every
        // sample so the prior block's last sample doesn't loop as a
        // DC glitch.
        case S::Expired:
        case S::NotActivated:
        case S::Tampered:
            for (int c = 0; c < nChans; ++c)
                std::memset(outputs[c], 0, sizeof(iplug::sample) * static_cast<size_t>(nFrames));
            return false;
    }
    return true;
}
