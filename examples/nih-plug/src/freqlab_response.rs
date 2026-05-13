// freqlab_response.rs - audio-thread response to license state.
//
// This file is the seller's behavioral policy. Your plugin's `process`
// calls `freqlab_should_run_dsp(buffer)` at the top; if it returns
// false, `process` returns early without running DSP. When false is
// returned, this helper is responsible for leaving the buffer in a
// safe state (the default impl zeros it).
//
// Edit the match arms below to customize behavior per status:
//   - want a watermark on Expired? leave the arm returning true and
//     mix a noise burst into `buffer` here.
//   - want Expired to keep playing with banner-only nag? return true.
//   - want stricter Tampered handling? scramble `buffer` instead of
//     zeroing it.
//
// Only `currentStatus()` is called here - it's a lock-free atomic
// read and safe to call from the audio thread.

use freqlab_licensing as licensing;
use nih_plug::prelude::Buffer;

pub fn freqlab_should_run_dsp(buffer: &mut Buffer) -> bool {
    use licensing::Status;
    match licensing::current_status() {
        // Buyer is fully licensed, in a paid trial, in grace period, or
        // running an unconfigured build (NoConfig = local dev / stub).
        // Run DSP normally.
        Status::Licensed
        | Status::Trial
        | Status::GracePeriod
        | Status::NoConfig => true,

        // License is invalid. Default: zero the output buffer (silence)
        // and skip DSP. The buffer.iter_samples() loop sets every sample
        // to 0.0 - including any leftover values from the prior block,
        // which prevents DC glitches.
        Status::Expired | Status::NotActivated | Status::Tampered => {
            for channel_samples in buffer.iter_samples() {
                for sample in channel_samples {
                    *sample = 0.0;
                }
            }
            false
        }
    }
}
