//! # freqlab-licensing
//!
//! Local development surface for the freqlab plugin licensing system.
//! [`current_status`] returns [`Status::NoConfig`] (signal: this build
//! has no licensing wired up). Every fallible call returns
//! [`Error::StubBuild`]. Submit your plugin through the freqlab cloud
//! with licensing enabled to get the real implementation.
//!
//! Sellers should hide their licensing UI when `current_status()` returns
//! [`Status::NoConfig`]; that way local builds and unlicensed cloud builds
//! both show no licensing surface, while licensed cloud builds show the
//! activate flow.
//!
//! ## Developing your licensing UI locally
//!
//! Enable a `dev-*` Cargo feature on the dep in your plugin's
//! `Cargo.toml` to bake a fake status into the stub:
//!
//! ```toml
//! # any one of:
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-not-activated"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-licensed"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-expired"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-tampered"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-grace"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-overdue"] }
//! freqlab-licensing = { git = "...", branch = "main", features = ["dev-trial"] }
//! ```
//!
//! Rebuild. [`current_status`] now returns the matching variant and
//! [`current`] returns a [`LicenseInfo`] with realistic fake fields
//! (key, expiry, features), so you can test how your UI renders each
//! state. Remove the `features = [...]` line and rebuild to flip back
//! to default ([`Status::NoConfig`]).
//!
//! See <https://github.com/Nanoshrine-Interactive/freqlab-licensing>.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use std::time::SystemTime;

/// Top-level state of the buyer's license. Defaults to [`Status::NoConfig`]
/// in local builds. Enable a `dev-*` Cargo feature to bake a different
/// status for local UI testing (see crate-level docs).
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub enum Status {
    /// Valid, in good standing.
    Licensed,
    /// Trial active. Treat as licensed for plugin behavior.
    Trial,
    /// Expiring soon. Still valid; surface a re-activate prompt.
    GracePeriod,
    /// Missed server-side check-in window. Recoverable via the SDK's
    /// background check-in; do NOT prompt re-activation.
    Overdue,
    /// Past expiry.
    Expired,
    /// No license file found on this machine.
    NotActivated,
    /// Signature verification failed.
    Tampered,
    /// SDK was built without licensing enabled. Submit through the
    /// freqlab cloud with licensing turned on.
    #[default]
    NoConfig,
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
    /// Buyer display name from license metadata. Absent for bulk-issued.
    /// Fall back to [`buyer_email`](LicenseInfo::buyer_email).
    pub buyer_name: Option<String>,
    /// Buyer email from license metadata. Absent for bulk-issued.
    pub buyer_email: Option<String>,
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
            Error::NotConfigured => {
                f.write_str("freqlab licensing is not configured for this build")
            }
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

/// Return the cached license state. In default builds the [`LicenseInfo`]
/// is empty (status = [`Status::NoConfig`]). With a `dev-*` Cargo feature
/// enabled, returns realistic fake fields matching the chosen status so
/// your UI can render licensed / expired / etc. states without a cloud
/// round-trip.
pub fn current() -> LicenseInfo {
    let status = current_status();
    let mut info = LicenseInfo {
        status,
        ..LicenseInfo::default()
    };
    if matches!(
        status,
        Status::Licensed | Status::Expired | Status::GracePeriod | Status::Overdue | Status::Trial
    ) {
        info.license_key = Some("DEV-MODE-XXXX-YYYY-ZZZZ".to_owned());
        info.license_id = Some("dev_license_00000000".to_owned());
        info.features = vec!["DEV_FEATURE".to_owned()];
        info.buyer_name = Some("Dev Tester".to_owned());
        info.buyer_email = Some("dev@example.com".to_owned());
    }
    if matches!(status, Status::Expired) {
        // Year 2020.
        info.expiry = Some(SystemTime::UNIX_EPOCH + std::time::Duration::from_secs(1_577_836_800));
    } else if matches!(
        status,
        Status::Licensed | Status::GracePeriod | Status::Overdue | Status::Trial
    ) {
        info.expiry = Some(SystemTime::now() + std::time::Duration::from_secs(365 * 86400));
    }
    info
}

/// Lock-free, alloc-free read of the [`Status`] enum. Audio-thread safe.
/// Defaults to [`Status::NoConfig`]; enable a `dev-*` Cargo feature to
/// override (see crate docs).
pub fn current_status() -> Status {
    if cfg!(feature = "dev-licensed") {
        Status::Licensed
    } else if cfg!(feature = "dev-expired") {
        Status::Expired
    } else if cfg!(feature = "dev-tampered") {
        Status::Tampered
    } else if cfg!(feature = "dev-grace") {
        Status::GracePeriod
    } else if cfg!(feature = "dev-overdue") {
        Status::Overdue
    } else if cfg!(feature = "dev-trial") {
        Status::Trial
    } else if cfg!(any(feature = "dev-mode", feature = "dev-not-activated")) {
        Status::NotActivated
    } else {
        Status::NoConfig
    }
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

/// Signal in-flight SDK workers to cancel at the next checkpoint. Call
/// from your plugin's `Drop` impl. No-op in the stub.
pub fn shutdown() {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn current_status_default() {
        // Default build (no dev-* feature) is NoConfig.
        // Test runs against whatever feature set the test build picked up.
        let s = current_status();
        if cfg!(feature = "dev-licensed") {
            assert_eq!(s, Status::Licensed);
        } else if cfg!(feature = "dev-expired") {
            assert_eq!(s, Status::Expired);
        } else if cfg!(feature = "dev-tampered") {
            assert_eq!(s, Status::Tampered);
        } else if cfg!(feature = "dev-grace") {
            assert_eq!(s, Status::GracePeriod);
        } else if cfg!(feature = "dev-overdue") {
            assert_eq!(s, Status::Overdue);
        } else if cfg!(feature = "dev-trial") {
            assert_eq!(s, Status::Trial);
        } else if cfg!(any(feature = "dev-mode", feature = "dev-not-activated")) {
            assert_eq!(s, Status::NotActivated);
        } else {
            assert_eq!(s, Status::NoConfig);
        }
    }

    #[test]
    fn current_matches_status() {
        let info = current();
        assert_eq!(info.status, current_status());
    }

    #[test]
    fn licensed_states_get_fake_fields() {
        let info = current();
        match info.status {
            Status::Licensed
            | Status::Expired
            | Status::GracePeriod
            | Status::Overdue
            | Status::Trial => {
                assert!(info.license_key.is_some(), "should have a fake key");
                assert!(info.license_id.is_some(), "should have a fake id");
                assert!(!info.features.is_empty(), "should have fake features");
                assert!(info.buyer_name.is_some(), "should have a fake buyer_name");
                assert!(info.buyer_email.is_some(), "should have a fake buyer_email");
            }
            _ => {
                assert!(info.license_key.is_none());
                assert!(info.license_id.is_none());
                assert!(info.features.is_empty());
                assert!(info.buyer_name.is_none());
                assert!(info.buyer_email.is_none());
            }
        }
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
