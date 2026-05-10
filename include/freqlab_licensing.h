// freqlab_licensing.h
//
// Public C++ API for the freqlab plugin licensing system. This header is
// the local-build surface; the cloud build pipeline injects the real
// implementation at the same target name. Seller code is identical in
// both builds.
//
// Threading:
//   currentStatus() is audio-thread safe (lock-free atomic read).
//   current() may allocate when copying a populated LicenseInfo; prefer
//   currentStatus() on the audio thread.
//   validateAndActivate(), refreshAsync(), deactivateThisMachine() do
//   blocking I/O in cloud builds (no-op in local). Callbacks fire on a
//   worker thread; marshal back to your UI thread before touching UI.
//
// Local-build behavior:
//   currentStatus() returns Status::NotActivated.
//   validateAndActivate(...) invokes onError synchronously.
//   refreshAsync() is a no-op. deactivateThisMachine(done) calls done(false).

#ifndef FREQLAB_LICENSING_H
#define FREQLAB_LICENSING_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace freqlab {
namespace licensing {

/// Top-level state of the buyer's license. Always NotActivated in local builds.
enum class Status {
    Licensed,       ///< Valid, in good standing.
    Trial,          ///< Trial active. Treat as licensed for plugin behavior.
    GracePeriod,    ///< Expiring soon. Still valid; surface a re-activate prompt.
    Expired,        ///< Past expiry.
    NotActivated,   ///< No license file found on this machine.
    Tampered,       ///< Signature verification failed.
    NoConfig,       ///< SDK was built without licensing enabled.
};

/// Full snapshot returned by current().
struct LicenseInfo {
    Status status = Status::NotActivated;
    std::vector<std::string> features;
    std::optional<std::chrono::system_clock::time_point> expiry;
    std::optional<std::string> licenseKey;
    bool checkInRequired = false;
    std::optional<std::chrono::system_clock::time_point> nextCheckIn;
    std::optional<std::string> licenseId;
    bool heartbeatRequired = false;
    std::uint64_t heartbeatDurationSecs = 0;
    std::optional<std::chrono::system_clock::time_point> nextHeartbeat;
};

/// Return the cached license state.
LicenseInfo current();

/// Lock-free read of the Status enum. Audio-thread safe.
Status currentStatus();

/// Activate this machine against the supplied license key. Callbacks fire
/// on a worker thread; marshal back to your UI thread before touching UI.
void validateAndActivate(
    const std::string& key,
    std::function<void(LicenseInfo)> onSuccess,
    std::function<void(std::string)> onError);

/// Deactivate this machine's slot. done(true) on success, done(false) otherwise.
void deactivateThisMachine(std::function<void(bool)> done);

/// Re-read cached state and dispatch any due check-in / heartbeat. Spawns
/// a worker; returns immediately.
void refreshAsync();

}} // namespace freqlab::licensing

#endif // FREQLAB_LICENSING_H
