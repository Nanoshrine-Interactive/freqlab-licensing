#pragma once

// freqlab licensing UI controls for iPlug2 / IGraphics.
//   LicenseStatusPill - top-right colored pill + status label.
//   LicenseBanner     - full-width sticky banner shown on bad states.
//   LicenseModalControl - overlay with status pill, key input, buttons.

#include "IControl.h"
#include "IGraphics.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "freqlab_licensing.h"

namespace freqlab_ui {

using namespace iplug;
using namespace iplug::igraphics;

/// Shared state between the modal and SDK worker-thread callbacks.
///
/// validateAndActivate / deactivateThisMachine fire their callbacks on
/// the SDK's worker thread, but the editor (and thus the modal) can be
/// destroyed while the request is still in flight (host closes the
/// plugin window). iPlug2 doesn't have JUCE's SafePointer, so we route
/// the result through a shared_ptr<ModalState>: both the modal and the
/// callback hold a strong reference, the callback writes into it
/// unconditionally, and the modal reads from it on every redraw.
/// Worst case the callback writes into orphaned state and the writes
/// are simply ignored.
struct ModalState {
    std::mutex mu;
    std::string errorMsg;
    std::atomic<bool> busy{false};
};

inline IColor ColorForStatus(freqlab::licensing::Status s) {
    using S = freqlab::licensing::Status;
    switch (s) {
        case S::Licensed:     return IColor(255,  68, 255, 136);  // green
        case S::Trial:        return IColor(255, 102, 204, 255);  // blue
        case S::GracePeriod:  return IColor(255, 255, 204,  68);  // amber
        case S::Expired:      return IColor(255, 255, 102,  85);  // red
        case S::NotActivated: return IColor(255, 255, 170,  68);  // orange
        case S::Tampered:     return IColor(255, 187,  85, 255);  // purple
        case S::NoConfig:     return IColor(120, 110, 110, 110);
    }
    return IColor(120, 110, 110, 110);
}

inline const char* PillLabel(freqlab::licensing::Status s) {
    using S = freqlab::licensing::Status;
    switch (s) {
        case S::Licensed:     return "Licensed";
        case S::Trial:        return "Trial";
        case S::GracePeriod:  return "Expiring soon";
        case S::Expired:      return "Expired";
        case S::NotActivated: return "Not activated";
        case S::Tampered:     return "License invalid";
        case S::NoConfig:     return "";
    }
    return "";
}

inline const char* ModalLabel(freqlab::licensing::Status s) {
    using S = freqlab::licensing::Status;
    switch (s) {
        case S::Licensed:     return "LICENSED";
        case S::Trial:        return "TRIAL";
        case S::GracePeriod:  return "EXPIRING SOON";
        case S::Expired:      return "EXPIRED";
        case S::NotActivated: return "NOT ACTIVATED";
        case S::Tampered:     return "LICENSE INVALID";
        case S::NoConfig:     return "";
    }
    return "";
}

inline bool StatusNeedsBanner(freqlab::licensing::Status s) {
    using S = freqlab::licensing::Status;
    return s == S::Expired || s == S::NotActivated || s == S::Tampered;
}

inline const char* BannerCopy(freqlab::licensing::Status s) {
    using S = freqlab::licensing::Status;
    if (s == S::NotActivated) return "This plugin is not activated. Click to enter your license key.";
    if (s == S::Expired)      return "Your license has expired. Renew to continue using this plugin.";
    if (s == S::Tampered)     return "License could not be verified. Please reinstall the plugin.";
    return "";
}

class LicenseModalControl;

/// Clickable status pill in the editor's top-right corner - colored dot
/// plus status label ("Licensed", "Expired", etc.).
class LicenseStatusPill : public IControl {
public:
    LicenseStatusPill(const IRECT& bounds, LicenseModalControl* modal)
        : IControl(bounds), mModal(modal) {}

    // SetAnimation runs the lambda on every frame. We use it to poll
    // currentStatus() and redraw when it changes. currentStatus() is a
    // lock-free atomic read so this is effectively free per frame.
    void OnInit() override {
        SetAnimation([this](IControl*) {
            const auto s = freqlab::licensing::currentStatus();
            if (s != mLastStatus) {
                mLastStatus = s;
                SetDirty(false);
            }
        });
        mLastStatus = freqlab::licensing::currentStatus();
    }

    void Draw(IGraphics& g) override {
        if (mLastStatus == freqlab::licensing::Status::NoConfig) return;

        const IColor c = ColorForStatus(mLastStatus);
        const IRECT bg = mRECT;
        const float radius = bg.H() / 2.f;

        // Pill background.
        g.FillRoundRect(IColor(136, 0, 0, 0), bg, radius);
        IColor border = c; border.A = 100;
        g.DrawRoundRect(border, bg, radius, nullptr, 1.f);

        // Status dot on the left.
        const float dotR = bg.H() * 0.28f;
        const float dotX = bg.L + dotR + 6.f;
        const float dotY = bg.MH();
        g.FillCircle(c, dotX, dotY, dotR);

        // Status text.
        IRECT textArea = bg;
        textArea.L = dotX + dotR + 6.f;
        textArea.R -= 8.f;
        g.DrawText(IText(10.f, EAlign::Near, c), PillLabel(mLastStatus), textArea);
    }

    void OnMouseDown(float x, float y, const IMouseMod& mod) override;

private:
    LicenseModalControl* mModal;
    freqlab::licensing::Status mLastStatus = freqlab::licensing::Status::NoConfig;
};

/// Full-bounds modal overlay. Hidden by default; the dot shows it.
class LicenseModalControl : public IControl {
public:
    explicit LicenseModalControl(const IRECT& bounds)
        : IControl(bounds), mState(std::make_shared<ModalState>())
    {
        Hide(true);
    }

    void OnInit() override {
        SetAnimation([this](IControl*) {
            if (!IsHidden()) SetDirty(false);
        });
    }

    void Draw(IGraphics& g) override {
        g.FillRect(IColor(180, 0, 0, 0), mRECT);

        // Compact panel when Licensed (just the status + Deactivate).
        const bool licensedNow = (freqlab::licensing::currentStatus()
            == freqlab::licensing::Status::Licensed);
        const float panelW = 360.f;
        const float panelH = licensedNow ? 180.f : 280.f;
        const IRECT panel = mRECT.GetCentredInside(panelW, panelH);
        g.FillRoundRect(IColor(255, 28, 30, 38), panel, 12.f);
        g.DrawRoundRect(IColor(255, 70, 75, 90), panel, 12.f, nullptr, 1.f);

        IRECT inner = panel.GetPadded(-20.f);

        // Header row: title + close X share the same baseline.
        IRECT headerRow = inner.GetFromTop(28.f);
        g.DrawText(IText(20.f, EAlign::Near, COLOR_WHITE),
                   "Licensing", headerRow);
        mCloseRect = headerRow.GetFromRight(28.f);
        g.DrawText(IText(16.f, EAlign::Center, IColor(255, 200, 200, 210)),
                   "X", mCloseRect);

        // Divider line.
        const float dividerY = inner.T + 36.f;
        g.DrawLine(IColor(255, 60, 65, 78), inner.L, dividerY, inner.R, dividerY, nullptr, 1.f);

        // Status pill - sits directly below the divider.
        const auto status = freqlab::licensing::currentStatus();
        const bool isLicensed = (status == freqlab::licensing::Status::Licensed);
        const IColor pillBg = ColorForStatus(status);
        IRECT pillRect = IRECT(inner.L, dividerY + 12.f, inner.L + 140.f, dividerY + 40.f);
        g.FillRoundRect(pillBg, pillRect, 14.f);
        // Dark text on the bright pill bg - much better contrast than white.
        g.DrawText(IText(13.f, IColor(255, 10, 14, 22)), ModalLabel(status), pillRect);

        // Hide the key input + Activate when Licensed.
        if (!isLicensed) {
            const float keyLabelY = pillRect.B + 16.f;
            IRECT keyLabel = IRECT(inner.L, keyLabelY, inner.R, keyLabelY + 14.f);
            g.DrawText(IText(12.f, EAlign::Near, IColor(255, 180, 180, 190)),
                       "License key", keyLabel);
            mKeyRect = IRECT(inner.L, keyLabel.B + 4.f, inner.R, keyLabel.B + 40.f);
            g.FillRoundRect(IColor(255, 18, 20, 26), mKeyRect, 6.f);
            g.DrawRoundRect(IColor(255, 70, 75, 90), mKeyRect, 6.f, nullptr, 1.f);

            // Middle-ellipsis the key - signed JWTs are too long to ever fit.
            std::string displayKey;
            if (mKeyInput.GetLength() > 0) {
                const std::string raw(mKeyInput.Get());
                constexpr std::size_t kMax = 34;
                if (raw.size() > kMax) {
                    const std::size_t head = (kMax - 3) / 2;
                    const std::size_t tail = (kMax - 3) - head;
                    displayKey = raw.substr(0, head) + "..." + raw.substr(raw.size() - tail);
                } else {
                    displayKey = raw;
                }
            }
            const char* keyDisplay = !displayKey.empty() ? displayKey.c_str() : "Click to enter key";
            const IColor keyTextColor = !displayKey.empty()
                ? COLOR_WHITE
                : IColor(255, 120, 120, 130);
            g.DrawText(IText(13.f, EAlign::Near, keyTextColor),
                       keyDisplay, mKeyRect.GetPadded(-8.f));
        } else {
            mKeyRect = IRECT();  // hit-test disabled
        }

        // Buttons row. When Licensed we anchor right under the pill so the
        // compact panel has no empty middle; otherwise anchor to the bottom
        // so the key input + label sit between the pill and the buttons.
        IRECT btnRow;
        if (isLicensed) {
            btnRow = IRECT(inner.L, pillRect.B + 20.f, inner.R, pillRect.B + 50.f);
        } else {
            btnRow = inner.GetFromBottom(70.f).GetReducedFromBottom(30.f);
        }
        const bool canDeactivate = !mState->busy.load() && (
            status == freqlab::licensing::Status::Licensed ||
            status == freqlab::licensing::Status::Trial ||
            status == freqlab::licensing::Status::GracePeriod ||
            status == freqlab::licensing::Status::Expired);

        if (isLicensed) {
            mActivateRect = IRECT();
            mDeactivateRect = btnRow;
            g.FillRoundRect(IColor(255, 90, 95, 110), mDeactivateRect, 8.f);
            g.DrawText(IText(14.f, COLOR_WHITE), "Deactivate", mDeactivateRect);
        } else {
            const float gap = 10.f;
            const float btnW = (btnRow.W() - gap) / 2.f;
            mActivateRect    = btnRow.GetFromLeft(btnW);
            mDeactivateRect  = btnRow.GetFromRight(btnW);

            const bool canActivate = !mState->busy.load() && mKeyInput.GetLength() > 0;

            g.FillRoundRect(canActivate ? IColor(255, 45, 168, 110) : IColor(255, 60, 60, 70),
                            mActivateRect, 8.f);
            g.DrawText(IText(14.f, COLOR_WHITE), "Activate", mActivateRect);

            g.FillRoundRect(canDeactivate ? IColor(255, 90, 95, 110) : IColor(255, 50, 50, 60),
                            mDeactivateRect, 8.f);
            g.DrawText(IText(14.f, COLOR_WHITE), "Deactivate", mDeactivateRect);
        }

        // Error label.
        std::string err;
        {
            std::lock_guard<std::mutex> lk(mState->mu);
            err = mState->errorMsg;
        }
        if (!err.empty()) {
            IRECT errRect = inner.GetFromBottom(20.f);
            g.DrawText(IText(11.f, EAlign::Near, IColor(255, 220, 90, 90)),
                       err.c_str(), errRect);
        }
    }

    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        if (IsHidden()) return;

        const bool licensedNow = (freqlab::licensing::currentStatus()
            == freqlab::licensing::Status::Licensed);
        const float panelH = licensedNow ? 180.f : 280.f;
        const IRECT panel = mRECT.GetCentredInside(360.f, panelH);
        if (!panel.Contains(x, y)) { Close(); return; }
        if (mCloseRect.Contains(x, y)) { Close(); return; }

        if (mKeyRect.Contains(x, y)) {
            GetUI()->CreateTextEntry(*this, IText(13.f, COLOR_WHITE), mKeyRect,
                                     mKeyInput.Get());
            return;
        }

        const auto status = freqlab::licensing::currentStatus();
        if (mActivateRect.Contains(x, y) && !mState->busy.load() && mKeyInput.GetLength() > 0) {
            DoActivate();
            return;
        }
        const bool canDeactivate =
            status == freqlab::licensing::Status::Licensed ||
            status == freqlab::licensing::Status::Trial ||
            status == freqlab::licensing::Status::GracePeriod ||
            status == freqlab::licensing::Status::Expired;
        if (mDeactivateRect.Contains(x, y) && !mState->busy.load() && canDeactivate) {
            DoDeactivate();
            return;
        }
    }

    void OnTextEntryCompletion(const char* str, int valIdx) override {
        if (str) mKeyInput.Set(str);
        // The platform text edit leaves the OS cursor as an I-beam on
        // some macOS hosts after dismissal. Force it back to ARROW.
        if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
        SetDirty(false);
    }

    void Open() {
        Hide(false);
        SetDirty(false);
    }

private:
    void Close() {
        Hide(true);
        if (auto* ui = GetUI()) ui->SetAllControlsDirty();
    }

    void DoActivate() {
        mState->busy.store(true);
        {
            std::lock_guard<std::mutex> lk(mState->mu);
            mState->errorMsg.clear();
        }
        // Capture the shared_ptr by value so the callbacks always have
        // valid state to write into, even if the modal is destroyed
        // before the request completes. The callbacks fire on the SDK's
        // worker thread; the next animation tick reads the result and
        // redraws on the UI thread.
        auto state = mState;
        std::string key(mKeyInput.Get());
        freqlab::licensing::validateAndActivate(
            key,
            [state](freqlab::licensing::LicenseInfo) {
                state->busy.store(false);
                std::lock_guard<std::mutex> lk(state->mu);
                state->errorMsg.clear();
            },
            [state](std::string err) {
                state->busy.store(false);
                std::lock_guard<std::mutex> lk(state->mu);
                state->errorMsg = std::move(err);
            });
    }

    void DoDeactivate() {
        mState->busy.store(true);
        {
            std::lock_guard<std::mutex> lk(mState->mu);
            mState->errorMsg.clear();
        }
        auto state = mState;
        freqlab::licensing::deactivateThisMachine([state](bool ok) {
            state->busy.store(false);
            std::lock_guard<std::mutex> lk(state->mu);
            if (!ok) state->errorMsg = "Deactivation failed.";
        });
    }

    WDL_String mKeyInput;
    std::shared_ptr<ModalState> mState;
    IRECT mCloseRect, mKeyRect, mActivateRect, mDeactivateRect;
};

inline void LicenseStatusPill::OnMouseDown(float x, float y, const IMouseMod& mod) {
    if (mModal && mLastStatus != freqlab::licensing::Status::NoConfig) {
        mModal->Open();
    }
}

/// Sticky banner across the top of the editor for bad license states
/// (NotActivated / Expired / Tampered). Hidden otherwise. Click opens
/// the license modal.
class LicenseBanner : public IControl {
public:
    LicenseBanner(const IRECT& bounds, LicenseModalControl* modal)
        : IControl(bounds), mModal(modal) {}

    void OnInit() override {
        SetAnimation([this](IControl*) {
            const auto s = freqlab::licensing::currentStatus();
            if (s != mLastStatus) {
                mLastStatus = s;
                Hide(!StatusNeedsBanner(s));
                SetDirty(false);
            }
        });
        mLastStatus = freqlab::licensing::currentStatus();
        Hide(!StatusNeedsBanner(mLastStatus));
    }

    void Draw(IGraphics& g) override {
        if (!StatusNeedsBanner(mLastStatus)) return;
        const IColor c = ColorForStatus(mLastStatus);
        // Dark solid background derived from the status color so the
        // banner has real opacity over whatever's below.
        const IColor bg = IColor(255, c.R / 4, c.G / 4, c.B / 4);
        g.FillRect(bg, mRECT);
        IColor border = c; border.A = 180;
        g.DrawHorizontalLine(border, mRECT.B - 1.f, mRECT.L, mRECT.R);
        g.DrawText(IText(11.f, c), BannerCopy(mLastStatus), mRECT);
    }

    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        if (mModal && StatusNeedsBanner(mLastStatus)) mModal->Open();
    }

private:
    LicenseModalControl* mModal;
    freqlab::licensing::Status mLastStatus = freqlab::licensing::Status::NoConfig;
};

}  // namespace freqlab_ui
