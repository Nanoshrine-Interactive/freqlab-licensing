#pragma once

// freqlab_response.h - audio-thread response to license state.
//
// This file is the seller's behavioral policy. Your plugin's
// processBlock calls `freqlab_should_run_dsp(buffer)` at the top; if
// it returns false, processBlock returns early without running DSP.
// When false is returned, this helper is responsible for leaving the
// buffer in a safe state (the default impl clears it).
//
// Edit the switch arms below to customize behavior per status:
//   - want a watermark on Expired? leave the arm returning true and
//     mix a noise burst into `buffer` here.
//   - want Expired to keep playing with banner-only nag? return true.
//   - want stricter Tampered handling? corrupt `buffer` instead of
//     clearing it.
//
// Only currentStatus() is called here. It's a lock-free atomic read
// and safe to call from the audio thread.

#include "freqlab_licensing.h"
#include <juce_audio_basics/juce_audio_basics.h>

inline bool freqlab_should_run_dsp(juce::AudioBuffer<float>& buffer)
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

        // License is invalid. Default: silent. buffer.clear() zeros
        // every sample so the prior block's last sample doesn't loop
        // as a DC glitch.
        case S::Expired:
        case S::NotActivated:
        case S::Tampered:
            buffer.clear();
            return false;
    }
    return true;
}
