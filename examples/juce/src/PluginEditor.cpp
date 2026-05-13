#include "PluginEditor.h"
#include "freqlab_licensing.h"

namespace
{
    constexpr int kStatusLicensed     = static_cast<int>(freqlab::licensing::Status::Licensed);
    constexpr int kStatusTrial        = static_cast<int>(freqlab::licensing::Status::Trial);
    constexpr int kStatusGracePeriod  = static_cast<int>(freqlab::licensing::Status::GracePeriod);
    constexpr int kStatusExpired      = static_cast<int>(freqlab::licensing::Status::Expired);
    constexpr int kStatusNotActivated = static_cast<int>(freqlab::licensing::Status::NotActivated);
    constexpr int kStatusTampered     = static_cast<int>(freqlab::licensing::Status::Tampered);
    constexpr int kStatusNoConfig     = static_cast<int>(freqlab::licensing::Status::NoConfig);

    juce::Colour colourForStatus(int status)
    {
        if (status == kStatusLicensed)     return juce::Colour(0xFF44FF88);  // green
        if (status == kStatusTrial)        return juce::Colour(0xFF66CCFF);  // blue
        if (status == kStatusGracePeriod)  return juce::Colour(0xFFFFCC44);  // amber
        if (status == kStatusExpired)      return juce::Colour(0xFFFF6655);  // red
        if (status == kStatusNotActivated) return juce::Colour(0xFFFFAA44);  // orange
        if (status == kStatusTampered)     return juce::Colour(0xFFBB55FF);  // purple
        return juce::Colours::transparentBlack;
    }

    juce::String pillLabelForStatus(int status)
    {
        if (status == kStatusLicensed)     return "Licensed";
        if (status == kStatusTrial)        return "Trial";
        if (status == kStatusGracePeriod)  return "Expiring soon";
        if (status == kStatusExpired)      return "Expired";
        if (status == kStatusNotActivated) return "Not activated";
        if (status == kStatusTampered)     return "License invalid";
        return "Unknown";
    }

    juce::String modalLabelForStatus(int status) { return pillLabelForStatus(status).toUpperCase(); }

    bool statusNeedsBanner(int status)
    {
        return status == kStatusNotActivated
            || status == kStatusExpired
            || status == kStatusTampered;
    }

    juce::String bannerCopyForStatus(int status)
    {
        if (status == kStatusNotActivated) return "This plugin is not activated. Click to enter your license key.";
        if (status == kStatusExpired)      return "Your license has expired. Renew to continue using this plugin.";
        if (status == kStatusTampered)     return "License could not be verified. Please reinstall the plugin.";
        return {};
    }
}

// ═══════════════════════════════════════════════════════════════════
//  LicenseStatusPill
// ═══════════════════════════════════════════════════════════════════

// Polls currentStatus() at 250ms cadence and redraws when it flips.
// We don't get notified by the SDK on state changes, so the UI is
// poll-driven. currentStatus() is a cheap atomic read so the timer is
// effectively free.
LicenseStatusPill::LicenseStatusPill()
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimer(250);
}

void LicenseStatusPill::paint(juce::Graphics& g)
{
    if (currentStatusInt == kStatusNoConfig || currentStatusInt < 0)
        return;

    const auto colour = colourForStatus(currentStatusInt);
    const auto bounds = getLocalBounds().toFloat();

    // Pill background.
    g.setColour(juce::Colour(0x88000000));
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    g.setColour(colour.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.5f, 1.0f);

    // Status dot on the left of the pill.
    const float dotR = bounds.getHeight() * 0.28f;
    const float dotCx = bounds.getX() + dotR + 6.0f;
    const float dotCy = bounds.getCentreY();
    g.setColour(colour);
    g.fillEllipse(dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);

    // Status text label.
    auto textBounds = bounds.withTrimmedLeft(dotR * 2.0f + 12.0f).withTrimmedRight(8.0f);
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(pillLabelForStatus(currentStatusInt), textBounds.toNearestInt(),
               juce::Justification::centredLeft, false);
}

void LicenseStatusPill::timerCallback()
{
    const auto next = static_cast<int>(freqlab::licensing::currentStatus());
    if (next != currentStatusInt)
    {
        currentStatusInt = next;
        if (auto* parent = getParentComponent())
            parent->resized();
        repaint();
    }
}

void LicenseStatusPill::mouseDown(const juce::MouseEvent&)
{
    if (onClicked) onClicked();
}

// ═══════════════════════════════════════════════════════════════════
//  LicenseBanner
// ═══════════════════════════════════════════════════════════════════

LicenseBanner::LicenseBanner()
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimer(250);
}

void LicenseBanner::paint(juce::Graphics& g)
{
    if (! statusNeedsBanner(currentStatusInt)) return;

    const auto colour = colourForStatus(currentStatusInt);
    const auto bounds = getLocalBounds().toFloat();

    // Solid dark-tinted background so nothing underneath shows through.
    g.setColour(colour.withMultipliedBrightness(0.2f).withAlpha(1.0f));
    g.fillRect(bounds);
    g.setColour(colour);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    g.setColour(colour.brighter(0.35f));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(bannerCopyForStatus(currentStatusInt), bounds.toNearestInt(),
               juce::Justification::centred, false);
}

void LicenseBanner::timerCallback()
{
    const auto next = static_cast<int>(freqlab::licensing::currentStatus());
    if (next != currentStatusInt)
    {
        currentStatusInt = next;
        setVisible(statusNeedsBanner(next));
        if (auto* parent = getParentComponent())
            parent->resized();
        repaint();
    }
}

void LicenseBanner::mouseDown(const juce::MouseEvent&)
{
    if (onClicked) onClicked();
}

// ═══════════════════════════════════════════════════════════════════
//  LicenseModalComponent
// ═══════════════════════════════════════════════════════════════════

LicenseModalComponent::LicenseModalComponent()
{
    statusPill.setText("UNKNOWN", juce::dontSendNotification);
    statusPill.setJustificationType(juce::Justification::centred);
    statusPill.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    statusPill.setColour(juce::Label::backgroundColourId, juce::Colour(0xff060612));
    addAndMakeVisible(statusPill);

    keyInput.setMultiLine(false);
    keyInput.setTextToShowWhenEmpty("XXXX-XXXX-XXXX-XXXX", juce::Colour(0xff555555));
    // Use the default JUCE font at 12pt - matches the_3d_switch's
    // Courier-style key field visually. Avoid `withTypefaceStyle` which
    // is not on juce::FontOptions; pre-JUCE-8 used a different API.
    keyInput.setFont(juce::Font(juce::FontOptions(12.0f)));
    keyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050510));
    keyInput.setColour(juce::TextEditor::textColourId, juce::Colour(0xffdddddd));
    keyInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1a1a66));
    keyInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00aaff));
    addAndMakeVisible(keyInput);

    activateBtn.setButtonText("ACTIVATE");
    activateBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    activateBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff66ccff));
    activateBtn.onClick = [this] { onActivate(); };
    addAndMakeVisible(activateBtn);

    deactivateBtn.setButtonText("DEACTIVATE");
    deactivateBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    deactivateBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffff6688));
    deactivateBtn.onClick = [this] { onDeactivate(); };
    addAndMakeVisible(deactivateBtn);

    closeBtn.setButtonText("X");
    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff888888));
    closeBtn.onClick = [this] { setVisible(false); };
    addAndMakeVisible(closeBtn);

    errorLabel.setJustificationType(juce::Justification::centred);
    errorLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    errorLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6688));
    addAndMakeVisible(errorLabel);

    refreshDisplay();
}

void LicenseModalComponent::visibilityChanged()
{
    // Poll only while visible. Initial refresh fires immediately so the
    // status pill isn't 100ms-stale on open.
    if (isVisible())
    {
        refreshDisplay();
        startTimer(100);
        // Re-validate against the server in case the seller flipped
        // suspend / unsuspend after the plugin launched.
        freqlab::licensing::refreshAsync();
    }
    else
    {
        stopTimer();
    }
}

void LicenseModalComponent::timerCallback()
{
    refreshDisplay();
}

void LicenseModalComponent::mouseDown(const juce::MouseEvent& e)
{
    // Click on the dimmed backdrop (outside the panel) closes the modal.
    auto modalRect = getLocalBounds().reduced(15);
    if (! modalRect.contains(e.getPosition()))
        setVisible(false);
}

void LicenseModalComponent::paint(juce::Graphics& g)
{
    // Dim the editor behind the modal.
    g.fillAll(juce::Colour(0xee05050f));

    const bool isLicensed =
        static_cast<int>(freqlab::licensing::currentStatus()) == kStatusLicensed;

    // Compact panel when fully Licensed (no key input / Activate); full
    // panel otherwise so the key input has room.
    const int panelH = isLicensed ? 130 : 200;
    auto modal = getLocalBounds().withSizeKeepingCentre(getWidth() - 30, panelH);
    g.setColour(juce::Colour(0xff060612));
    g.fillRect(modal);
    g.setColour(juce::Colour(0xff00aaff));
    g.drawRect(modal, 3);
    g.setColour(juce::Colour(0xff1a1a66));
    g.drawRect(modal.expanded(2), 2);

    g.setColour(juce::Colour(0xff66ccff));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    auto titleArea = modal.removeFromTop(24).reduced(8, 4);
    g.drawText("LICENSE", titleArea, juce::Justification::centred);
}

void LicenseModalComponent::resized()
{
    const bool isLicensed =
        static_cast<int>(freqlab::licensing::currentStatus()) == kStatusLicensed;

    const int panelH = isLicensed ? 130 : 200;
    auto modal = getLocalBounds().withSizeKeepingCentre(getWidth() - 30, panelH);
    auto interior = modal.reduced(10);
    interior.removeFromTop(24);  // title area inside panel

    closeBtn.setBounds(modal.getRight() - 24, modal.getY() + 6, 16, 16);

    statusPill.setBounds(interior.removeFromTop(24));
    interior.removeFromTop(8);

    keyInput.setVisible(! isLicensed);
    activateBtn.setVisible(! isLicensed);

    if (! isLicensed)
    {
        keyInput.setBounds(interior.removeFromTop(28));
        interior.removeFromTop(8);
        auto btnRow = interior.removeFromTop(28);
        activateBtn.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2 - 4));
        btnRow.removeFromLeft(8);
        deactivateBtn.setBounds(btnRow);
    }
    else
    {
        deactivateBtn.setBounds(interior.removeFromTop(28));
    }

    interior.removeFromTop(6);
    errorLabel.setBounds(interior.removeFromTop(16));
}

void LicenseModalComponent::refreshDisplay()
{
    const auto status = static_cast<int>(freqlab::licensing::currentStatus());
    auto colour = colourForStatus(status);
    statusPill.setText(busy.load() ? "WORKING..." : modalLabelForStatus(status),
                       juce::dontSendNotification);
    statusPill.setColour(juce::Label::textColourId, colour);
    statusPill.setColour(juce::Label::outlineColourId, colour.withAlpha(0.5f));

    activateBtn.setEnabled(! busy.load() && status != kStatusLicensed);
    deactivateBtn.setEnabled(! busy.load() && status != kStatusNotActivated && status != kStatusNoConfig);

    // Re-flow on Licensed transition so the key input + Activate button
    // hide. repaint() is needed: resized() only moves children, and
    // without a full paint the previous panel border ghosts the new size.
    const bool isLicensed = (status == kStatusLicensed);
    if (keyInput.isVisible() == isLicensed)
    {
        resized();
        repaint();
    }

    errorLabel.setText(pendingError, juce::dontSendNotification);
}

void LicenseModalComponent::onActivate()
{
    auto key = keyInput.getText().trim().toStdString();
    if (key.empty())
    {
        pendingError = "Enter a license key first";
        refreshDisplay();
        return;
    }

    busy.store(true);
    pendingError.clear();
    refreshDisplay();

    // validateAndActivate is non-blocking; it kicks the call off to an
    // SDK worker thread and fires the success or error callback when
    // done. The callbacks fire on that worker thread, so we must marshal
    // back to the JUCE message thread before touching any UI state.
    // SafePointer protects against the modal being destroyed while the
    // request is in flight (user closes the plugin window mid-request).
    juce::Component::SafePointer<LicenseModalComponent> safeThis(this);

    freqlab::licensing::validateAndActivate(
        key,
        [safeThis](freqlab::licensing::LicenseInfo /*info*/) {
            juce::MessageManager::callAsync([safeThis] {
                if (auto* self = safeThis.getComponent())
                {
                    self->busy.store(false);
                    self->pendingError.clear();
                    self->refreshDisplay();
                }
            });
        },
        [safeThis](std::string err) {
            juce::MessageManager::callAsync([safeThis, err] {
                if (auto* self = safeThis.getComponent())
                {
                    self->busy.store(false);
                    self->pendingError = juce::String(err);
                    self->refreshDisplay();
                }
            });
        });
}

void LicenseModalComponent::onDeactivate()
{
    busy.store(true);
    pendingError.clear();
    refreshDisplay();

    juce::Component::SafePointer<LicenseModalComponent> safeThis(this);
    freqlab::licensing::deactivateThisMachine(
        [safeThis](bool ok) {
            juce::MessageManager::callAsync([safeThis, ok] {
                if (auto* self = safeThis.getComponent())
                {
                    self->busy.store(false);
                    if (! ok) self->pendingError = "Deactivation failed";
                    self->refreshDisplay();
                }
            });
        });
}

// ═══════════════════════════════════════════════════════════════════
//  JucerguyEditor
// ═══════════════════════════════════════════════════════════════════

JucerguyEditor::JucerguyEditor(JucerguyProcessor& p)
    : AudioProcessorEditor(p), processorRef(p)
{
    setLookAndFeel(&lookAndFeel);

    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(gainSlider);

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    addAndMakeVisible(gainLabel);

    // License UI: pill top-right, sticky banner on top (bad states only),
    // modal that opens on click of either.
    addAndMakeVisible(licensePill);
    licensePill.onClicked = [this] { licenseModal.setVisible(true); };

    addChildComponent(licenseBanner);
    licenseBanner.onClicked = [this] { licenseModal.setVisible(true); };

    addChildComponent(licenseModal);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "gain", gainSlider);

    setSize(380, 260);
}

JucerguyEditor::~JucerguyEditor()
{
    // Remove the LookAndFeel before child components destruct.
    setLookAndFeel(nullptr);
}

void JucerguyEditor::paint(juce::Graphics& g)
{
    // Dark gradient background
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff1a1a2e), bounds.getCentreX(), 0,
        juce::Colour(0xff0f0f1a), bounds.getCentreX(), bounds.getHeight(),
        false));
    g.fillAll();

    // Plugin name
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("Jucerguy", getLocalBounds().removeFromTop(40), juce::Justification::centred);
}

void JucerguyEditor::resized()
{
    auto full = getLocalBounds();

    // Banner across the top (24px), sticky while a bad status is active.
    if (licenseBanner.isVisible())
        licenseBanner.setBounds(full.removeFromTop(24));

    auto bounds = full.reduced(20);
    bounds.removeFromTop(40);

    auto sliderBounds = bounds.withSizeKeepingCentre(120, 120);
    gainLabel.setBounds(sliderBounds.removeFromTop(20));
    gainSlider.setBounds(sliderBounds);

    // Pill width grows with the label; cap at 120px.
    const int pillH = 18;
    const int pillW = 120;
    const int pillTop = licenseBanner.isVisible() ? 28 : 6;
    licensePill.setBounds(getWidth() - pillW - 8, pillTop, pillW, pillH);

    licenseModal.setBounds(getLocalBounds());
}
