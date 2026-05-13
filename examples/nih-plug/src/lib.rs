use nih_plug::prelude::*;
use nih_plug_webview::{WebViewEditor, HTMLSource};
use serde::Deserialize;
use serde_json::json;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};

use freqlab_licensing as licensing;

mod freqlab_response;
use freqlab_response::freqlab_should_run_dsp;

/// Messages from the WebView UI
#[derive(Deserialize)]
#[serde(tag = "type")]
enum UIMessage {
    Init,
    SetGain { value: f32 },
    /// Activate the entered license key. Runs on a worker thread; result
    /// flows back via the `license_state` event.
    ActivateLicense { key: String },
    DeactivateLicense,
    QueryLicense,
}

struct Nihplugtest {
    params: Arc<NihplugtestParams>,
}

#[derive(Params)]
struct NihplugtestParams {
    #[id = "gain"]
    pub gain: FloatParam,
    #[persist = "gain-dirty"]
    gain_changed: Arc<AtomicBool>,
}

impl Default for Nihplugtest {
    fn default() -> Self {
        Self {
            params: Arc::new(NihplugtestParams::default()),
        }
    }
}

impl Default for NihplugtestParams {
    fn default() -> Self {
        let gain_changed = Arc::new(AtomicBool::new(false));
        let gain_changed_clone = gain_changed.clone();

        Self {
            gain: FloatParam::new(
                "Gain",
                util::db_to_gain(0.0),
                FloatRange::Skewed {
                    min: util::db_to_gain(-30.0),
                    max: util::db_to_gain(30.0),
                    factor: FloatRange::gain_skew_factor(-30.0, 30.0),
                },
            )
            .with_smoother(SmoothingStyle::Logarithmic(50.0))
            .with_unit(" dB")
            .with_value_to_string(formatters::v2s_f32_gain_to_db(2))
            .with_string_to_value(formatters::s2v_f32_gain_to_db())
            .with_callback(Arc::new(move |_| {
                gain_changed_clone.store(true, Ordering::Relaxed);
            })),
            gain_changed,
        }
    }
}

impl Plugin for Nihplugtest {
    const NAME: &'static str = "Nihplugtest";
    const VENDOR: &'static str = "freqlab";
    const URL: &'static str = "";
    const EMAIL: &'static str = "";
    const VERSION: &'static str = env!("CARGO_PKG_VERSION");

    const AUDIO_IO_LAYOUTS: &'static [AudioIOLayout] = &[AudioIOLayout {
        main_input_channels: NonZeroU32::new(2),
        main_output_channels: NonZeroU32::new(2),
        ..AudioIOLayout::const_default()
    }];

    const MIDI_INPUT: MidiConfig = MidiConfig::None;
    const MIDI_OUTPUT: MidiConfig = MidiConfig::None;

    type SysExMessage = ();
    type BackgroundTask = ();

    fn params(&self) -> Arc<dyn Params> {
        self.params.clone()
    }

    fn editor(&mut self, _async_executor: AsyncExecutor<Self>) -> Option<Box<dyn Editor>> {
        let params = self.params.clone();
        let gain_changed = self.params.gain_changed.clone();

        // The licensing SDK's activate / deactivate / refresh calls are
        // blocking (network + disk I/O), so they run on worker threads.
        // Their results land via these flags: the worker sets
        // `license_changed = true`, and on the next event_loop tick we
        // read fresh state and send_json() to the WebView. send_json
        // is GUI-thread-bound, which is why we can't just call it from
        // the worker directly. `last_license_error` carries the most
        // recent activation/deactivation failure string back to the UI.
        let license_changed = Arc::new(AtomicBool::new(true));
        let last_license_error: Arc<Mutex<Option<String>>> = Arc::new(Mutex::new(None));

        // Refresh the SDK cache when the editor opens so the UI sees
        // the latest server-confirmed state rather than the post-launch
        // initial value (NoConfig until first refresh()).
        {
            let changed = license_changed.clone();
            std::thread::spawn(move || {
                let _ = licensing::refresh();
                changed.store(true, Ordering::Relaxed);
            });
        }

        let editor = WebViewEditor::new(HTMLSource::String(include_str!("ui.html")), (400, 320))
            .with_background_color((26, 26, 46, 255))
            .with_developer_mode(true)
            .with_event_loop(move |ctx, setter, _window| {
                while let Ok(msg) = ctx.next_event() {
                    if let Ok(ui_msg) = serde_json::from_value::<UIMessage>(msg) {
                        match ui_msg {
                            UIMessage::Init => {
                                ctx.send_json(json!({
                                    "type": "param_change",
                                    "param": "gain",
                                    "value": params.gain.unmodulated_normalized_value(),
                                    "text": params.gain.to_string()
                                }));
                                license_changed.store(true, Ordering::Relaxed);
                            }
                            UIMessage::SetGain { value } => {
                                setter.begin_set_parameter(&params.gain);
                                setter.set_parameter_normalized(&params.gain, value);
                                setter.end_set_parameter(&params.gain);
                            }
                            UIMessage::ActivateLicense { key } => {
                                // validate_and_activate is blocking
                                // (network + disk). Run it on a worker;
                                // the event loop will pick up the new
                                // license_state on the next tick.
                                let changed = license_changed.clone();
                                let err_slot = last_license_error.clone();
                                std::thread::spawn(move || {
                                    let result = licensing::validate_and_activate(&key);
                                    if let Err(e) = result {
                                        if let Ok(mut slot) = err_slot.lock() {
                                            *slot = Some(format!("{}", e));
                                        }
                                    } else if let Ok(mut slot) = err_slot.lock() {
                                        *slot = None;
                                    }
                                    changed.store(true, Ordering::Relaxed);
                                });
                            }
                            UIMessage::DeactivateLicense => {
                                let changed = license_changed.clone();
                                let err_slot = last_license_error.clone();
                                std::thread::spawn(move || {
                                    let result = licensing::deactivate_this_machine();
                                    if let Err(e) = result {
                                        if let Ok(mut slot) = err_slot.lock() {
                                            *slot = Some(format!("{}", e));
                                        }
                                    } else if let Ok(mut slot) = err_slot.lock() {
                                        *slot = None;
                                    }
                                    changed.store(true, Ordering::Relaxed);
                                });
                            }
                            UIMessage::QueryLicense => {
                                license_changed.store(true, Ordering::Relaxed);
                            }
                        }
                    }
                }

                if gain_changed.swap(false, Ordering::Relaxed) {
                    ctx.send_json(json!({
                        "type": "param_change",
                        "param": "gain",
                        "value": params.gain.unmodulated_normalized_value(),
                        "text": params.gain.to_string()
                    }));
                }

                if license_changed.swap(false, Ordering::Relaxed) {
                    let info = licensing::current();
                    let status_str = match info.status {
                        licensing::Status::Licensed => "Licensed",
                        licensing::Status::Trial => "Trial",
                        licensing::Status::GracePeriod => "GracePeriod",
                        licensing::Status::Expired => "Expired",
                        licensing::Status::NotActivated => "NotActivated",
                        licensing::Status::Tampered => "Tampered",
                        licensing::Status::NoConfig => "NoConfig",
                    };
                    let licensing_active = info.status != licensing::Status::NoConfig;
                    let key_preview = info.license_key.as_deref().map(|k| {
                        let prefix_len = k
                            .find('-')
                            .map(|i: usize| i + 5)
                            .unwrap_or(8)
                            .min(k.len());
                        &k[..prefix_len]
                    });
                    let error = last_license_error.lock().ok().and_then(|s| s.clone());
                    ctx.send_json(json!({
                        "type": "license_state",
                        "active": licensing_active,
                        "status": status_str,
                        "key_preview": key_preview,
                        "features": info.features,
                        "error": error,
                    }));
                }
            });

        Some(Box::new(editor))
    }

    fn initialize(
        &mut self,
        _audio_io_layout: &AudioIOLayout,
        _buffer_config: &BufferConfig,
        _context: &mut impl InitContext<Self>,
    ) -> bool {
        // Sync the SDK cache against the server when the plugin loads.
        // refresh() is blocking (disk + HTTP) so it runs on a worker.
        std::thread::spawn(|| { let _ = licensing::refresh(); });
        true
    }

    // The `@freqlab:require-license` marker tells our cloud transformer
    // to wrap this function with a license gate at build time. The
    // inline `freqlab_should_run_dsp(buffer)` call below makes the same
    // gate run during local development too, so the example silences
    // correctly without a cloud build.
    //
    // @freqlab:require-license
    fn process(
        &mut self,
        buffer: &mut Buffer,
        _aux: &mut AuxiliaryBuffers,
        _context: &mut impl ProcessContext<Self>,
    ) -> ProcessStatus {
        if !freqlab_should_run_dsp(buffer) {
            return ProcessStatus::Normal;
        }

        for channel_samples in buffer.iter_samples() {
            let gain = self.params.gain.smoothed.next();
            for sample in channel_samples {
                *sample *= gain;
                if !sample.is_finite() {
                    *sample = 0.0;
                }
            }
        }
        ProcessStatus::Normal
    }
}

impl ClapPlugin for Nihplugtest {
    const CLAP_ID: &'static str = "com.freqlab.nihplugtest";
    const CLAP_DESCRIPTION: Option<&'static str> = Some("");
    const CLAP_MANUAL_URL: Option<&'static str> = None;
    const CLAP_SUPPORT_URL: Option<&'static str> = None;
    const CLAP_FEATURES: &'static [ClapFeature] = &[ClapFeature::AudioEffect, ClapFeature::Stereo];
}

impl Vst3Plugin for Nihplugtest {
    const VST3_CLASS_ID: [u8; 16] = *b"Freqlab284899914";
    const VST3_SUBCATEGORIES: &'static [Vst3SubCategory] = &[Vst3SubCategory::Fx];
}

nih_export_clap!(Nihplugtest);
nih_export_vst3!(Nihplugtest);
