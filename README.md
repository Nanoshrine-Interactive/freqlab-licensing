# freqlab-licensing

Local development surface for the [freqlab](https://freqlab.app) plugin licensing system. Seller plugin projects depend on this crate (Rust) or library (C++) so the licensing API resolves cleanly during ordinary local builds. When a plugin is submitted to the freqlab cloud build pipeline with licensing enabled, the build pipeline injects the real implementation at the same dependency name. Seller code is identical in both builds.

```
local build               →  stub returns Status::NoConfig
cloud build, no licensing →  stub returns Status::NoConfig
cloud build, licensed     →  real implementation (NotActivated → Licensed flow)
```

The `NoConfig` signal is your cue to hide the licensing UI entirely. In licensed cloud builds the SDK returns `NotActivated` (no `.lic` file yet) or `Licensed` (after activation), so your activate UI shows up in exactly the right place.

---

## Behavior in a local (stub) build

In default builds, `current_status()` returns `Status::NoConfig` (signal: licensing not wired up). All fallible calls return `Err(Error::StubBuild)` (Rust) / invoke `onError("...")` (C++). `refreshAsync()` is a no-op.

To test how your UI renders a specific status without doing a cloud build, enable a `dev-*` feature flag. Build-time, per-project, baked into the binary.

### Status options (pick one)

| State | Cargo feature | CMake option | What `current()` returns |
|---|---|---|---|
| `NotActivated` (fresh install) | `dev-not-activated` (alias: `dev-mode`) | `FREQLAB_LICENSING_DEV_NOT_ACTIVATED` | empty `LicenseInfo` with status |
| `Licensed` | `dev-licensed` | `FREQLAB_LICENSING_DEV_LICENSED` | fake key + id + features + future expiry |
| `Expired` | `dev-expired` | `FREQLAB_LICENSING_DEV_EXPIRED` | fake key + past expiry (2020) |
| `Tampered` | `dev-tampered` | `FREQLAB_LICENSING_DEV_TAMPERED` | empty `LicenseInfo` with status |
| `GracePeriod` | `dev-grace` | `FREQLAB_LICENSING_DEV_GRACE` | fake key + future expiry |
| `Trial` | `dev-trial` | `FREQLAB_LICENSING_DEV_TRIAL` | fake key + future expiry |

The fake `license_key` (`DEV-MODE-XXXX-YYYY-ZZZZ`), `license_id`, `expiry`, and `features` fields let your UI render every state with realistic data.

### Toggling

**Rust (Cargo):** edit your `Cargo.toml`, rebuild.
```toml
# default (licensing UI hidden):
freqlab-licensing = { git = "...", branch = "main" }

# pick one of:
freqlab-licensing = { git = "...", branch = "main", features = ["dev-not-activated"] }
freqlab-licensing = { git = "...", branch = "main", features = ["dev-licensed"] }
freqlab-licensing = { git = "...", branch = "main", features = ["dev-expired"] }
freqlab-licensing = { git = "...", branch = "main", features = ["dev-tampered"] }
freqlab-licensing = { git = "...", branch = "main", features = ["dev-grace"] }
freqlab-licensing = { git = "...", branch = "main", features = ["dev-trial"] }
```

**C++ (CMake):** pass the option at configure time.
```bash
cmake -S . -B build                                     # default
cmake -S . -B build -DFREQLAB_LICENSING_DEV_LICENSED=ON
cmake -S . -B build -DFREQLAB_LICENSING_DEV_EXPIRED=ON
cmake -S . -B build -DFREQLAB_LICENSING_DEV_TAMPERED=ON
# etc.
```

---

## Install

### Rust (Cargo)

```toml
[dependencies]
freqlab-licensing = { git = "https://github.com/Nanoshrine-Interactive/freqlab-licensing", tag = "v0.1.0" }
```

### C++ (CMake)

Vendor a copy under `vendor/freqlab-licensing/` (or use `FetchContent`), then:

```cmake
add_subdirectory(vendor/freqlab-licensing)
target_link_libraries(MyPlugin PRIVATE freqlab::licensing)
```

`FetchContent` form:

```cmake
include(FetchContent)
FetchContent_Declare(freqlab_licensing
    GIT_REPOSITORY https://github.com/Nanoshrine-Interactive/freqlab-licensing
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(freqlab_licensing)
target_link_libraries(MyPlugin PRIVATE freqlab::licensing)
```

---

## Initialize the cache at startup

The SDK's in-memory cache starts at `Status::NoConfig`. Call `refresh()` (Rust) / `refreshAsync()` (C++) once when your plugin loads so subsequent `current_status()` reads reflect the on-disk license state. Without this step, your licensing UI will think the build has no licensing wired up and stay hidden.

```rust
// In your plugin's initialize() / setup hook (worker thread, refresh blocks):
std::thread::spawn(|| { let _ = freqlab_licensing::refresh(); });
```

```cpp
// In your plugin's prepareToPlay / initialize / setup hook:
freqlab::licensing::refreshAsync();
```

In stub builds (with or without a `dev-*` feature) this is a no-op, so the same code is safe in both local and cloud builds.

---

## Tags

The cloud build pipeline runs a source-rewrite pass that transforms `// @freqlab:…` comment tags. Local builds ignore them (they are just comments).

| Tag | Status | Effect at build time |
|---|---|---|
| `// @freqlab:require-license` | shipping | Wraps the next function definition's body with a `currentStatus() == Licensed` gate. The behavior when invalid is configured per-product in freqlab Distro. |
| `// @freqlab:require-feature("X")` | reserved | Will gate a statement on entitlement to feature `"X"`. Today emits a warning into the build report. |
| `// @freqlab:integrity-check` | reserved | Anti-debug hook. Today a no-op. |
| `// @freqlab:status -> bool` | reserved | Will replace the marked declaration with one that returns `currentStatus() == Licensed`. Today a no-op. |

### Placement rules

1. The tag is a comment on its own line, immediately above the function definition you want to wrap.
2. Other comments between tag and target are tolerated.
3. Do not put a tag inside a function body. Definition sites only.

```rust
// @freqlab:require-license
fn process(&mut self, buffer: &mut Buffer, /* ... */) -> ProcessStatus {
    // your audio code, untouched in local builds
}
```

```cpp
// @freqlab:require-license
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
    // your audio code, untouched in local builds
}
```

---

## Status display in your UI

How your plugin behaves when the license is invalid is configured per-product in freqlab Distro (when you enable licensing on a product). The audio side is handled for you by the `// @freqlab:require-license` tag.

### Hide your licensing UI when the build has no licensing wired up

Local builds and cloud builds without licensing both return `Status::NoConfig`. Gate your indicator / activate modal on this:

```rust
let licensing_active = freqlab_licensing::current_status() != freqlab_licensing::Status::NoConfig;
// show indicator + modal only when licensing_active
```

```cpp
const bool licensingActive =
    freqlab::licensing::currentStatus() != freqlab::licensing::Status::NoConfig;
// show indicator + modal only when licensingActive
```

### Status banners

You can optionally surface license status in your UI by polling `current_status()` from a UI timer. Useful for "license expired" banners, re-activation prompts, etc.

```rust
// 1-2 Hz UI tick (Rust)
match freqlab_licensing::current_status() {
    Status::Licensed     => hide_banner(),
    Status::Expired      => show_banner("Your license has expired"),
    Status::NotActivated => show_banner("Enter a license key to activate"),
    Status::Tampered     => show_banner("License file invalid, please reactivate"),
    _                    => hide_banner(),
}
```

```cpp
// 1-2 Hz juce::Timer::timerCallback (C++)
const auto s = freqlab::licensing::currentStatus();
banner.setVisible(s != freqlab::licensing::Status::Licensed);
banner.setText(statusBannerText(s), juce::dontSendNotification);
```

---

## Public API

### Rust

```rust
pub enum Status {
    Licensed, Trial, GracePeriod, Expired,
    NotActivated, Tampered, NoConfig,
}

pub struct LicenseInfo {
    pub status: Status,
    pub features: Vec<String>,
    pub expiry: Option<SystemTime>,
    pub license_key: Option<String>,
    pub license_id: Option<String>,
    pub check_in_required: bool,
    pub next_check_in: Option<SystemTime>,
    pub heartbeat_required: bool,
    pub heartbeat_duration_secs: u64,
    pub next_heartbeat: Option<SystemTime>,
}

pub enum Error {
    NotConfigured, Network(String), Server { status: u16, reason: String },
    InvalidKey(String), Io(String), Fingerprint, PostWriteVerify,
    StubBuild,  // returned by every fallible call in this crate
}

pub fn current() -> LicenseInfo;
pub fn current_status() -> Status;                                       // audio-thread safe
pub fn validate_and_activate(key: &str) -> Result<LicenseInfo, Error>;   // blocking
pub fn deactivate_this_machine() -> Result<(), Error>;                   // blocking
pub fn refresh() -> Result<(), Error>;                                   // blocking
```

### C++

```cpp
namespace freqlab::licensing {

enum class Status { Licensed, Trial, GracePeriod, Expired,
                    NotActivated, Tampered, NoConfig };

struct LicenseInfo {
    Status status;
    std::vector<std::string> features;
    std::optional<std::chrono::system_clock::time_point> expiry;
    std::optional<std::string> licenseKey;
    bool checkInRequired;
    std::optional<std::chrono::system_clock::time_point> nextCheckIn;
    std::optional<std::string> licenseId;
    bool heartbeatRequired;
    std::uint64_t heartbeatDurationSecs;
    std::optional<std::chrono::system_clock::time_point> nextHeartbeat;
};

LicenseInfo current();             // ~5µs, allocates if features non-empty
Status      currentStatus();       // audio-thread safe
void        validateAndActivate(const std::string& key,
                                std::function<void(LicenseInfo)> onSuccess,
                                std::function<void(std::string)> onError);
void        deactivateThisMachine(std::function<void(bool)> done);
void        refreshAsync();

}
```

### Threading rules

| Function | Where to call |
|---|---|
| `current_status()` / `currentStatus()` | anywhere, including audio thread |
| `current()` | anywhere except hot audio loops (allocates on copy) |
| `validate_and_activate()` (Rust) | worker thread only (blocks) |
| `validateAndActivate()` (C++) | anywhere (spawns its own worker; callbacks fire on the worker) |
| `deactivate_this_machine()` (Rust) | worker thread only (blocks) |
| `deactivateThisMachine()` (C++) | anywhere (spawns its own worker) |
| `refresh()` (Rust) | worker thread only (blocks) |
| `refreshAsync()` (C++) | anywhere (spawns its own worker) |

---

## UI integration

The SDK is headless. You wire activate/status UI in your existing GUI framework. Patterns below are minimal. Adapt to your widget library.

### nih-plug (Rust)

```rust
// Activate (worker thread, never the audio callback):
fn on_activate_clicked(&self, key: String) {
    let key = key.trim().to_owned();
    std::thread::spawn(move || {
        match freqlab_licensing::validate_and_activate(&key) {
            Ok(_)  => /* signal UI to refresh */,
            Err(e) => /* signal UI: show e.to_string() */,
        }
    });
}

// Status text (poll at 1-2 Hz from your UI tick):
fn status_text() -> &'static str {
    match freqlab_licensing::current_status() {
        freqlab_licensing::Status::Licensed     => "Licensed",
        freqlab_licensing::Status::Expired      => "License expired",
        freqlab_licensing::Status::NotActivated => "Not activated",
        freqlab_licensing::Status::Tampered     => "License invalid",
        _                                        => "...",
    }
}
```

### JUCE (C++)

```cpp
class LicenseComponent : public juce::Component,
                         public juce::Button::Listener,
                         private juce::Timer {
    juce::TextEditor keyInput;
    juce::TextButton activateButton { "Activate" };
    juce::Label statusLabel;

public:
    LicenseComponent() {
        addAndMakeVisible(keyInput);
        addAndMakeVisible(activateButton);
        addAndMakeVisible(statusLabel);
        activateButton.addListener(this);
        startTimerHz(2);  // poll status
    }

    void buttonClicked(juce::Button* b) override {
        if (b != &activateButton) return;
        const auto key = keyInput.getText().toStdString();
        freqlab::licensing::validateAndActivate(
            key,
            [this](auto info) {
                juce::MessageManager::callAsync([this]{
                    statusLabel.setText("Licensed", juce::dontSendNotification);
                });
            },
            [this](std::string err) {
                juce::MessageManager::callAsync([this, err]{
                    statusLabel.setText(err, juce::dontSendNotification);
                });
            });
    }

    void timerCallback() override {
        const auto s = freqlab::licensing::currentStatus();
        statusLabel.setText(s == freqlab::licensing::Status::Licensed ? "Licensed"
                                                                      : "Not activated",
                            juce::dontSendNotification);
    }
};
```

### iPlug2 (C++)

```cpp
// In your IControl or main plugin class:
void OnActivateClicked(const char* key) {
    freqlab::licensing::validateAndActivate(
        std::string(key),
        [this](auto info) {
            GetUI()->SetAllControlsDirty();  // marshal to UI redraw
        },
        [this](std::string err) {
            mLastError = err;
            GetUI()->SetAllControlsDirty();
        });
}

// In your draw / status control:
const auto info = freqlab::licensing::current();
const char* text = (info.status == freqlab::licensing::Status::Licensed)
    ? "Licensed" : "Not activated";
g.DrawText(IText(14), text, IRECT(...));
```

### Custom Cargo (raw Rust audio plugin)

The SDK does not care about your framework. Wherever your UI lives:

```rust
// Activate from your UI event handler (never the audio callback):
std::thread::spawn(move || {
    let _ = freqlab_licensing::validate_and_activate(&key);
});

// Status from your audio callback (lock-free, alloc-free):
fn render(&mut self, output: &mut [f32]) {
    let _ = freqlab_licensing::current_status();
    // The `// @freqlab:require-license` tag handles the audio side
    // automatically. Manual checks here are only needed if you want
    // additional UX on top of what Distro configures.
}
```

### Custom CMake (raw C++ audio plugin)

Same pattern, framework-agnostic:

```cpp
// Activate from anywhere; validateAndActivate spawns its own worker.
freqlab::licensing::validateAndActivate(
    key,
    [](auto info) { /* refresh UI */ },
    [](std::string err) { /* show err */ });

// Status from your render callback (lock-free, alloc-free):
extern "C" void process(float** in, float** out, int frames) {
    auto _s = freqlab::licensing::currentStatus();
    // The `// @freqlab:require-license` tag handles the audio side
    // automatically. Manual checks here are only needed if you want
    // additional UX on top of what Distro configures.
}
```

The transformer rewrites `processBlock`, `process`, or whatever you tag, regardless of framework. UI integration is your call.

---

## Versioning

Releases are tagged `vMAJOR.MINOR.PATCH`. Pin to a tag when shipping production plugins so future stub releases do not change the API your plugin compiles against:

```toml
freqlab-licensing = { git = "https://github.com/Nanoshrine-Interactive/freqlab-licensing", tag = "v0.1.0" }
```

```cmake
FetchContent_Declare(freqlab_licensing
    GIT_REPOSITORY https://github.com/Nanoshrine-Interactive/freqlab-licensing
    GIT_TAG v0.1.0)
```

The freqlab cloud build pipeline tracks matching versions on its side.

---

## License

Dual-licensed under either:

- MIT License ([LICENSE-MIT](LICENSE-MIT))
- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))

at your option. Standard Rust-ecosystem dual license.
