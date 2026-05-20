#include "PluginProcessor.h"
#include "IPlug_include_in_plug_src.h"

#include "freqlab_licensing.h"
#include "freqlab_response.h"
#include "LicenseUI.h"

using namespace iplug;
using namespace igraphics;

Iplugtest::Iplugtest(const InstanceInfo& info)
    : iplug::Plugin(info, MakeConfig(kNumParams, 1))
{
    GetParam(kGain)->InitDouble("Gain", 0., -70., 12., 0.1, "dB");
    mGain = DBToAmp(GetParam(kGain)->Value());

    // Sync the SDK cache against the server when the plugin loads.
    // refreshAsync() returns immediately and does the blocking HTTPS
    // call on the SDK's worker thread.
    freqlab::licensing::refreshAsync();

#if IPLUG_EDITOR
    mMakeGraphicsFunc = [&]() { return CreateGraphics(); };
    mLayoutFunc = [&](IGraphics* pGraphics) { LayoutUI(pGraphics); };
#endif
}

Iplugtest::~Iplugtest() {
    // Block until any in-flight SDK worker has exited so the host can
    // safely unload the plugin dylib.
    freqlab::licensing::shutdown();
}

#if IPLUG_EDITOR
IGraphics* Iplugtest::CreateGraphics() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, 1.0f);
}

void Iplugtest::LayoutUI(IGraphics* pGraphics) {
    pGraphics->LoadFont("Roboto-Regular", "Arial", ETextStyle::Normal);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_DARK_GRAY);

    const IRECT bounds = pGraphics->GetBounds();

    // Title label
    const IText titleText(24.f, COLOR_WHITE);
    pGraphics->AttachControl(new ITextControl(
        bounds.GetFromTop(50.f),
        "Iplugtest",
        titleText
    ));

    // Gain knob
    const IRECT knobArea = bounds.GetCentredInside(100.f, 100.f);
    pGraphics->AttachControl(new IVKnobControl(knobArea, kGain));

    // Gain label
    const IText labelText(14.f, COLOR_WHITE);
    pGraphics->AttachControl(new ITextControl(
        knobArea.GetFromBottom(20.f).GetVShifted(30.f),
        "Gain",
        labelText
    ));

    // freqlab licensing UI. Banner is attached AFTER the pill so it
    // paints on top and fully covers the pill row when shown.
    auto* modal = new freqlab_ui::LicenseModalControl(bounds);

    const float pillW = 120.f, pillH = 18.f;
    const IRECT pillArea = IRECT(bounds.R - pillW - 8.f, 4.f,
                                 bounds.R - 8.f,         4.f + pillH);
    pGraphics->AttachControl(new freqlab_ui::LicenseStatusPill(pillArea, modal));

    const IRECT bannerArea = bounds.GetFromTop(24.f);
    pGraphics->AttachControl(new freqlab_ui::LicenseBanner(bannerArea, modal));

    pGraphics->AttachControl(modal);
}
#endif

void Iplugtest::OnReset() {
}

void Iplugtest::OnParamChange(int paramIdx) {
    switch (paramIdx) {
        case kGain:
            mGain = DBToAmp(GetParam(kGain)->Value());
            break;
    }
}

// The `@freqlab:require-license` marker tells our cloud transformer to
// wrap this function with a license gate at build time. The inline
// `freqlab_should_run_dsp(...)` call below makes the same gate run
// during local development too, so the example silences correctly
// without a cloud build.
//
// @freqlab:require-license
void Iplugtest::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {
    const int nChans = NOutChansConnected();
    if (!freqlab_should_run_dsp(outputs, nChans, nFrames)) return;

    const double gain = mGain;

    for (int s = 0; s < nFrames; s++) {
        for (int c = 0; c < nChans; c++) {
            double samp = inputs[c][s];
            samp *= gain;

            if (!std::isfinite(samp)) {
                samp = 0.0;
            }

            outputs[c][s] = samp;
        }
    }
}
