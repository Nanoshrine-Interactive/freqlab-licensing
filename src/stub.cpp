// Local-build implementation of the freqlab licensing API. The cloud
// build pipeline replaces this with the real implementation.

#include "freqlab_licensing.h"

#include <cstdlib>

namespace freqlab {
namespace licensing {

Status currentStatus() {
    // FREQLAB_LICENSING_DEV=1 returns NotActivated so sellers can exercise
    // their activate / status UI locally. Default is NoConfig (signal:
    // licensing not wired up in this build).
    if (std::getenv("FREQLAB_LICENSING_DEV") != nullptr) {
        return Status::NotActivated;
    }
    return Status::NoConfig;
}

LicenseInfo current() {
    LicenseInfo info{};
    info.status = currentStatus();
    return info;
}

void validateAndActivate(
    const std::string& /*key*/,
    std::function<void(LicenseInfo)> /*onSuccess*/,
    std::function<void(std::string)> onError) {
    // Synchronous in the stub; the cloud build uses a worker thread.
    // Sellers should already be calling this from a non-UI thread.
    if (onError) {
        onError(
            "freqlab licensing is inactive in this local build; "
            "submit the plugin through the freqlab cloud with licensing "
            "enabled to exercise the real activation flow");
    }
}

void deactivateThisMachine(std::function<void(bool)> done) {
    if (done) {
        done(false);
    }
}

void refreshAsync() {}

}} // namespace freqlab::licensing
