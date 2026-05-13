#pragma once

// Plugin identity
#define PLUG_NAME "Iplugtest"
#define PLUG_MFR "freqlab"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'AACF'
#define PLUG_MFR_ID 'FREQ'
#define PLUG_URL_STR ""
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2025 freqlab"
#define PLUG_CLASS_NAME Iplugtest

// Bundle configuration
#define BUNDLE_NAME "Iplugtest"
#define BUNDLE_MFR "freqlab"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "Iplugtest"

// Channel configuration
#define PLUG_CHANNEL_IO "2-2"

// Plugin characteristics
#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 420
#define PLUG_HEIGHT 340
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

// Graphics backend
#define IGRAPHICS_NANOVG

// Audio Unit configuration
#define AUV2_ENTRY Iplugtest_Entry
#define AUV2_ENTRY_STR "Iplugtest_Entry"
#define AUV2_FACTORY Iplugtest_Factory
#define AUV2_VIEW_CLASS Iplugtest_View
#define AUV2_VIEW_CLASS_STR "Iplugtest_View"

// AAX configuration (Pro Tools)
#define AAX_TYPE_IDS 'AACF'
#define AAX_TYPE_IDS_AUDIOSUITE 'AACF'
#define AAX_PLUG_MFR_STR "freqlab"
#define AAX_PLUG_NAME_STR "Iplugtest"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 0

// VST3 configuration
#define VST3_SUBCATEGORY "Fx"

// CLAP configuration
#define CLAP_MANUAL_URL ""
#define CLAP_SUPPORT_URL ""
#define CLAP_DESCRIPTION ""
#define CLAP_FEATURES "audio-effect", "stereo"

// Standalone app configuration
#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64
