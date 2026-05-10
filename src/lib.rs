//! # freqlab-licensing
//!
//! Local development surface for the freqlab plugin licensing system.
//! Every fallible call returns [`Error::StubBuild`]; [`current_status`]
//! returns [`Status::NotActivated`]. Submit your plugin through the
//! freqlab cloud with licensing enabled to get the real implementation.
//!
//! See <https://github.com/Nanoshrine-Interactive/freqlab-licensing>.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use std::time::SystemTime;

/// Top-level state of the buyer's license. Always [`Status::NotActivated`]
/// in local builds.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Status {
    /// Valid, in good standing.
    Licensed,
    /// Trial active. Treat as licensed for plugin behavior.
    Trial,
    /// Expiring soon. Still valid; surface a re-activate prompt.
    GracePeriod,
    /// Past expiry.
    Expired,
    /// No license file found on this machine.
    NotActivated,
    /// Signature verification failed.
    Tampered,
    /// SDK was built without licensing enabled. Submit through the
    /// freqlab cloud with licensing turned on.
    NoConfig,
}

impl Default for Status {
    fn default() -> Self {
        Status::NotActivated
    }
}

/// Full snapshot returned by [`current`].
#[derive(Debug, Clone, Default)]
pub struct LicenseInfo {
    /// See [`Status`].
    pub status: Status,
    /// Entitlement features flagged on this license.
    pub features: Vec<String>,
    /// License expiry, if the policy declares one.
    pub expiry: Option<SystemTime>,
    /// The seller-visible license key.
    pub license_key: Option<String>,
    /// Whether the policy requires periodic check-in calls.
    pub check_in_required: bool,
    /// When the next check-in is due.
    pub next_check_in: Option<SystemTime>,
    /// Opaque license resource id.
    pub license_id: Option<String>,
    /// Whether the policy requires periodic machine heartbeats.
    pub heartbeat_required: bool,
    /// Heartbeat duration in seconds.
    pub heartbeat_duration_secs: u64,
    /// When the next heartbeat is due.
    pub next_heartbeat: Option<SystemTime>,
}

/// Errors returned by the activation / refresh / deactivation paths.
/// Local builds always return [`Error::StubBuild`].
#[derive(Debug)]
pub enum Error {
    /// SDK was built without licensing enabled.
    NotConfigured,
    /// Network failed (DNS, TCP, TLS, timeout).
    Network(String),
    /// HTTP-level failure from the licensing server.
    Server {
        /// HTTP status code.
        status: u16,
        /// Human-readable reason.
        reason: String,
    },
    /// License key didn't pass server-side validation.
    InvalidKey(String),
    /// Local file / disk error.
    Io(String),
    /// Couldn't read this machine's stable identifier.
    Fingerprint,
    /// License file written but on-disk verification failed.
    PostWriteVerify,
    /// Returned by every fallible call in a local (stub) build.
    StubBuild,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::NotConfigured => f.write_str(
                "freqlab licensing is not configured for this build",
            ),
            Error::Network(msg) => write!(f, "network error: {msg}"),
            Error::Server { status, reason } => write!(f, "server error {status}: {reason}"),
            Error::InvalidKey(msg) => write!(f, "invalid license key: {msg}"),
            Error::Io(msg) => write!(f, "i/o error: {msg}"),
            Error::Fingerprint => f.write_str("could not read this machine's identifier"),
            Error::PostWriteVerify => f.write_str(
                "license file written but verification failed; please \
                 rebuild the plugin in the freqlab cloud or contact support",
            ),
            Error::StubBuild => f.write_str(
                "freqlab licensing is inactive in this local build; submit \
                 the plugin through the freqlab cloud with licensing enabled \
                 to exercise the real activation flow",
            ),
        }
    }
}

impl std::error::Error for Error {}

/// Return the cached license state.
pub fn current() -> LicenseInfo {
    LicenseInfo::default()
}

/// Lock-free, alloc-free read of the [`Status`] enum. Audio-thread safe.
pub fn current_status() -> Status {
    Status::NotActivated
}

/// Activate this machine for the supplied license key. Blocking in cloud
/// builds; call from a non-realtime thread.
pub fn validate_and_activate(_key: &str) -> Result<LicenseInfo, Error> {
    Err(Error::StubBuild)
}

/// Deactivate this machine's slot for the currently-active license.
pub fn deactivate_this_machine() -> Result<(), Error> {
    Err(Error::StubBuild)
}

/// Re-read cached state and dispatch any due check-in / heartbeat. Call
/// from a non-realtime thread.
pub fn refresh() -> Result<(), Error> {
    Err(Error::StubBuild)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn current_status_is_not_activated() {
        assert_eq!(current_status(), Status::NotActivated);
    }

    #[test]
    fn current_returns_default() {
        let info = current();
        assert_eq!(info.status, Status::NotActivated);
        assert!(info.license_key.is_none());
        assert!(info.features.is_empty());
        assert!(!info.check_in_required);
        assert!(!info.heartbeat_required);
    }

    #[test]
    fn fallible_paths_return_stub_build() {
        assert!(matches!(
            validate_and_activate("ANY-KEY"),
            Err(Error::StubBuild)
        ));
        assert!(matches!(deactivate_this_machine(), Err(Error::StubBuild)));
        assert!(matches!(refresh(), Err(Error::StubBuild)));
    }

    #[test]
    fn error_display_is_informative() {
        let s = format!("{}", Error::StubBuild);
        assert!(s.contains("local build"));
    }
}
