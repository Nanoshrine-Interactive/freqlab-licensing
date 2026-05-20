// Local-build implementation of the freqlab licensing API. The cloud
// build pipeline replaces this with the real implementation.

#include "freqlab_licensing.h"

#include <chrono>

namespace freqlab {
namespace licensing {

Status currentStatus() {
    // Build-time dev-mode flags set via CMake options of the same name.
    // Precedence: LICENSED > EXPIRED > TAMPERED > GRACE > TRIAL >
    // NOT_ACTIVATED > MODE (alias). Default is NoConfig (UI hidden).
#if defined(FREQLAB_LICENSING_DEV_LICENSED)
    return Status::Licensed;
#elif defined(FREQLAB_LICENSING_DEV_EXPIRED)
    return Status::Expired;
#elif defined(FREQLAB_LICENSING_DEV_TAMPERED)
    return Status::Tampered;
#elif defined(FREQLAB_LICENSING_DEV_GRACE)
    return Status::GracePeriod;
#elif defined(FREQLAB_LICENSING_DEV_TRIAL)
    return Status::Trial;
#elif defined(FREQLAB_LICENSING_DEV_MODE) || defined(FREQLAB_LICENSING_DEV_NOT_ACTIVATED)
    return Status::NotActivated;
#else
    return Status::NoConfig;
#endif
}

LicenseInfo current() {
    LicenseInfo info{};
    info.status = currentStatus();
    // For statuses where a real cloud build would have a license attached,
    // populate fake fields so the seller's UI can render them locally.
    switch (info.status) {
        case Status::Licensed:
        case Status::Expired:
        case Status::GracePeriod:
        case Status::Trial:
            info.licenseKey = std::string("DEV-MODE-XXXX-YYYY-ZZZZ");
            info.licenseId = std::string("dev_license_00000000");
            info.features = {std::string("DEV_FEATURE")};
            break;
        default:
            break;
    }
    if (info.status == Status::Expired) {
        // Year 2020.
        info.expiry = std::chrono::system_clock::time_point{} +
                      std::chrono::seconds(1577836800);
    } else if (info.status == Status::Licensed ||
               info.status == Status::GracePeriod ||
               info.status == Status::Trial) {
        info.expiry = std::chrono::system_clock::now() +
                      std::chrono::hours(24 * 365);
    }
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

void shutdown() {}

}} // namespace freqlab::licensing
