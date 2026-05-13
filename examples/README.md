# Examples

Three reference plugins showing how to wire freqlab licensing into a
plugin across the major audio frameworks. Each one is a minimal gain
effect plus the full licensing UI surface. Copy the framework you use
into your own project as a starting point.

## What's wired up

Every example demonstrates the same three integration touch-points:

1. **SDK calls.** The plugin reads `freqlab::licensing::currentStatus()`
   to know the state, calls `refreshAsync()` to ask the SDK to sync
   against the server, and calls `validateAndActivate()` /
   `deactivateThisMachine()` from the modal's buttons. The SDK is a
   pure data source: it never mutes audio, shows dialogs, or otherwise
   forces behavior on you. The plugin decides what to do with each
   status.

2. **Audio-thread enforcement.** `src/freqlab_response.{h,rs}` is the
   helper that decides what happens on each status. The default impl
   zeros the output buffer for `Expired` / `NotActivated` / `Tampered`
   and runs DSP normally for everything else. Edit this file to swap
   in a watermark, keep playing on expired with banner-only nag, or
   disable enforcement entirely. The plugin's audio function calls
   `freqlab_should_run_dsp(buffer)` at the top and returns early if it
   returns `false`.

3. **License UI.** Three components, identical pattern across all three
   examples:
   - **Status pill** in the top-right corner. Color + label
     (`Licensed`, `Expired`, etc.). Click opens the modal.
   - **Sticky banner** across the top. Only shown for `NotActivated`,
     `Expired`, and `Tampered`. Click also opens the modal.
   - **Modal overlay** with the status, license-key input, Activate /
     Deactivate buttons, and an error line. Collapses to a single
     Deactivate button when already Licensed.

## Examples by framework

| Folder | Framework | Plugin type | UI tech |
| --- | --- | --- | --- |
| [`juce/`](./juce) | JUCE 8 | Stereo gain effect | Native `juce::Component` |
| [`iplug2/`](./iplug2) | iPlug2 (IGraphics) | Stereo gain effect | Native `IControl` |
| [`nih-plug/`](./nih-plug) | nih-plug + nih-plug-webview | Stereo gain effect | HTML/CSS via WebView |

## How the @freqlab:require-license marker works

Each plugin marks its audio function with `// @freqlab:require-license`
(C++) or `// @freqlab:require-license` above `fn process` (Rust). At
cloud-build time our pipeline reads the marker and produces a hardened
licensed release. For local builds the examples already call the
helper inline so the silence behavior works during development too.

Apply the marker to every audio function you want covered.

## Local preview of each license state

The licensing stub returns `Status::NoConfig` by default, which keeps
the licensing UI hidden during local development. To preview each
state, set a dev flag:

**Rust (nih-plug)** - set a feature in `Cargo.toml`:
```toml
freqlab-licensing = { ..., features = ["dev-licensed"] }
# or "dev-expired" / "dev-not-activated" / "dev-tampered" / "dev-grace" / "dev-trial"
```

**C++ (juce, iplug2)** - set a CMake flag at configure time:
```bash
cmake -DFREQLAB_LICENSING_DEV_LICENSED=ON ...
# or DEV_EXPIRED / DEV_NOT_ACTIVATED / DEV_TAMPERED
```

Cloud licensed builds inject the real SDK with real credentials, so
none of these dev flags affect the shipped binary.

## Threading rules to remember

- `currentStatus()` is **audio-thread safe** (lock-free atomic read).
- `validateAndActivate()`, `deactivateThisMachine()`, and
  `refreshAsync()` do blocking HTTPS / file I/O. They MUST NOT be
  called from the audio thread. Their callbacks fire on a worker
  thread, so marshal back to the UI thread before touching UI state
  (the examples use `juce::MessageManager::callAsync` for JUCE,
  `SharedPointer` for iPlug2, and atomic flags polled from the event
  loop for nih-plug-webview).
