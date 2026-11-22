#pragma once

// P19-C source-grounded Sound editor contract. Native parameters retain their
// original 7-bit host/storage identity. Selector domains below describe only
// values that the firmware UI allowed a user to choose; restored values outside
// a selector domain remain readable and are never silently coerced.

#include "LXRParamContract.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Included by LXRGUIFoundation.h after VoiceOwner is defined, inside
// namespace lxr::gui. Keep this contract header namespace-neutral.

enum class SoundSection : std::uint8_t
{
    Source = 0,
    PitchEnvelope,
    AmpEnvelope,
    Transient,
    Filter,
    Output,
    Count
};

enum class SoundControlKind : std::uint8_t
{
    IntegerKnob = 0,
    IntegerSlider,
    Enumeration,
    Boolean,
    Note
};

enum class EnumerationDomain : std::uint8_t
{
    None = 0,
    Waveform,
    FilterType,
    Transient,
    AudioOutput,
    MixMode,
    Boolean
};

struct SoundParameterBinding final
{
    int parameter = -1;
    const char* controlId = nullptr;
    const char* label = nullptr;
    VoiceOwner owner;
    SoundSection section = SoundSection::Source;
    SoundControlKind kind = SoundControlKind::IntegerKnob;
    EnumerationDomain enumeration = EnumerationDomain::None;
};

inline constexpr std::array<const char*, 6> kSoundSectionNames {{
    "SOURCE", "PITCH ENVELOPE", "AMPLITUDE ENVELOPE",
    "TRANSIENT", "FILTER", "OUTPUT"
}};

// The ordering is also the visible focus order. It follows the firmware voice
// pages, with shared concepts kept in the same section across owners.
inline constexpr std::array<SoundParameterBinding, 162> kSoundBindings {{
    // Drum 1
    {1, "lxr.param.001", "Main Wave", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {8, "lxr.param.008", "Coarse", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {9, "lxr.param.009", "Fine", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {20, "lxr.param.020", "Mod Wave", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {83, "lxr.param.083", "FM Frequency", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {82, "lxr.param.082", "FM Amount", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {134, "lxr.param.134", "Mix / FM", VoiceOwner::Drum1, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::MixMode},
    {70, "lxr.param.070", "Decay", VoiceOwner::Drum1, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {74, "lxr.param.074", "Amount", VoiceOwner::Drum1, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {78, "lxr.param.078", "Slope", VoiceOwner::Drum1, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {49, "lxr.param.049", "Attack", VoiceOwner::Drum1, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {50, "lxr.param.050", "Decay", VoiceOwner::Drum1, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {62, "lxr.param.062", "Slope", VoiceOwner::Drum1, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {137, "lxr.param.137", "Velocity to Amp", VoiceOwner::Drum1, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {203, "lxr.param.203", "Wave", VoiceOwner::Drum1, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {197, "lxr.param.197", "Level", VoiceOwner::Drum1, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {209, "lxr.param.209", "Pitch", VoiceOwner::Drum1, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {191, "lxr.param.191", "Type", VoiceOwner::Drum1, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {37, "lxr.param.037", "Cutoff", VoiceOwner::Drum1, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {43, "lxr.param.043", "Resonance", VoiceOwner::Drum1, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {128, "lxr.param.128", "Drive", VoiceOwner::Drum1, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {88, "lxr.param.088", "Level", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {94, "lxr.param.094", "Pan", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {102, "lxr.param.102", "Distortion", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {108, "lxr.param.108", "Decimate", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {215, "lxr.param.215", "Audio Output", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {221, "lxr.param.221", "MIDI Note", VoiceOwner::Drum1, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},

    // Drum 2
    {2, "lxr.param.002", "Main Wave", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {10, "lxr.param.010", "Coarse", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {11, "lxr.param.011", "Fine", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {21, "lxr.param.021", "Mod Wave", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {85, "lxr.param.085", "FM Frequency", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {84, "lxr.param.084", "FM Amount", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {135, "lxr.param.135", "Mix / FM", VoiceOwner::Drum2, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::MixMode},
    {71, "lxr.param.071", "Decay", VoiceOwner::Drum2, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {75, "lxr.param.075", "Amount", VoiceOwner::Drum2, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {79, "lxr.param.079", "Slope", VoiceOwner::Drum2, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {51, "lxr.param.051", "Attack", VoiceOwner::Drum2, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {52, "lxr.param.052", "Decay", VoiceOwner::Drum2, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {63, "lxr.param.063", "Slope", VoiceOwner::Drum2, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {138, "lxr.param.138", "Velocity to Amp", VoiceOwner::Drum2, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {204, "lxr.param.204", "Wave", VoiceOwner::Drum2, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {198, "lxr.param.198", "Level", VoiceOwner::Drum2, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {210, "lxr.param.210", "Pitch", VoiceOwner::Drum2, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {192, "lxr.param.192", "Type", VoiceOwner::Drum2, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {38, "lxr.param.038", "Cutoff", VoiceOwner::Drum2, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {44, "lxr.param.044", "Resonance", VoiceOwner::Drum2, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {129, "lxr.param.129", "Drive", VoiceOwner::Drum2, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {89, "lxr.param.089", "Level", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {95, "lxr.param.095", "Pan", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {103, "lxr.param.103", "Distortion", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {109, "lxr.param.109", "Decimate", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {216, "lxr.param.216", "Audio Output", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {222, "lxr.param.222", "MIDI Note", VoiceOwner::Drum2, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},

    // Drum 3
    {3, "lxr.param.003", "Main Wave", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {12, "lxr.param.012", "Coarse", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {13, "lxr.param.013", "Fine", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {22, "lxr.param.022", "Mod Wave", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {87, "lxr.param.087", "FM Frequency", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {86, "lxr.param.086", "FM Amount", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {136, "lxr.param.136", "Mix / FM", VoiceOwner::Drum3, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::MixMode},
    {72, "lxr.param.072", "Decay", VoiceOwner::Drum3, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {76, "lxr.param.076", "Amount", VoiceOwner::Drum3, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {80, "lxr.param.080", "Slope", VoiceOwner::Drum3, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {53, "lxr.param.053", "Attack", VoiceOwner::Drum3, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {54, "lxr.param.054", "Decay", VoiceOwner::Drum3, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {64, "lxr.param.064", "Slope", VoiceOwner::Drum3, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {139, "lxr.param.139", "Velocity to Amp", VoiceOwner::Drum3, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {205, "lxr.param.205", "Wave", VoiceOwner::Drum3, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {199, "lxr.param.199", "Level", VoiceOwner::Drum3, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {211, "lxr.param.211", "Pitch", VoiceOwner::Drum3, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {193, "lxr.param.193", "Type", VoiceOwner::Drum3, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {39, "lxr.param.039", "Cutoff", VoiceOwner::Drum3, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {45, "lxr.param.045", "Resonance", VoiceOwner::Drum3, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {130, "lxr.param.130", "Drive", VoiceOwner::Drum3, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {90, "lxr.param.090", "Level", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {96, "lxr.param.096", "Pan", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {104, "lxr.param.104", "Distortion", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {110, "lxr.param.110", "Decimate", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {217, "lxr.param.217", "Audio Output", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {223, "lxr.param.223", "MIDI Note", VoiceOwner::Drum3, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},

    // Snare
    {4, "lxr.param.004", "Tone Wave", VoiceOwner::Snare, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {14, "lxr.param.014", "Coarse", VoiceOwner::Snare, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {15, "lxr.param.015", "Fine", VoiceOwner::Snare, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {27, "lxr.param.027", "Noise Frequency", VoiceOwner::Snare, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {28, "lxr.param.028", "Noise Mix", VoiceOwner::Snare, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {73, "lxr.param.073", "Decay", VoiceOwner::Snare, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {77, "lxr.param.077", "Amount", VoiceOwner::Snare, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {81, "lxr.param.081", "Slope", VoiceOwner::Snare, SoundSection::PitchEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {55, "lxr.param.055", "Attack", VoiceOwner::Snare, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {56, "lxr.param.056", "Decay", VoiceOwner::Snare, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {65, "lxr.param.065", "Slope", VoiceOwner::Snare, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {68, "lxr.param.068", "Repeat", VoiceOwner::Snare, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {140, "lxr.param.140", "Velocity to Amp", VoiceOwner::Snare, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {206, "lxr.param.206", "Wave", VoiceOwner::Snare, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {200, "lxr.param.200", "Level", VoiceOwner::Snare, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {212, "lxr.param.212", "Pitch", VoiceOwner::Snare, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {194, "lxr.param.194", "Type", VoiceOwner::Snare, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {40, "lxr.param.040", "Cutoff", VoiceOwner::Snare, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {46, "lxr.param.046", "Resonance", VoiceOwner::Snare, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {131, "lxr.param.131", "Drive", VoiceOwner::Snare, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {91, "lxr.param.091", "Level", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {99, "lxr.param.099", "Pan", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {105, "lxr.param.105", "Distortion", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {111, "lxr.param.111", "Decimate", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {218, "lxr.param.218", "Audio Output", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {224, "lxr.param.224", "MIDI Note", VoiceOwner::Snare, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},

    // Cymbal
    {6, "lxr.param.006", "Main Wave", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {16, "lxr.param.016", "Coarse", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {17, "lxr.param.017", "Fine", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {23, "lxr.param.023", "Mod 1 Wave", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {29, "lxr.param.029", "Mod 1 Pitch", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {31, "lxr.param.031", "Mod 1 Gain", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {24, "lxr.param.024", "Mod 2 Wave", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {30, "lxr.param.030", "Mod 2 Pitch", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {32, "lxr.param.032", "Mod 2 Gain", VoiceOwner::Cymbal, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {57, "lxr.param.057", "Attack", VoiceOwner::Cymbal, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {58, "lxr.param.058", "Decay", VoiceOwner::Cymbal, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {66, "lxr.param.066", "Slope", VoiceOwner::Cymbal, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {69, "lxr.param.069", "Repeat", VoiceOwner::Cymbal, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {141, "lxr.param.141", "Velocity to Amp", VoiceOwner::Cymbal, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {207, "lxr.param.207", "Wave", VoiceOwner::Cymbal, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {201, "lxr.param.201", "Level", VoiceOwner::Cymbal, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {213, "lxr.param.213", "Pitch", VoiceOwner::Cymbal, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {195, "lxr.param.195", "Type", VoiceOwner::Cymbal, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {41, "lxr.param.041", "Cutoff", VoiceOwner::Cymbal, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {47, "lxr.param.047", "Resonance", VoiceOwner::Cymbal, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {132, "lxr.param.132", "Drive", VoiceOwner::Cymbal, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {92, "lxr.param.092", "Level", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {100, "lxr.param.100", "Pan", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {106, "lxr.param.106", "Distortion", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {112, "lxr.param.112", "Decimate", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {219, "lxr.param.219", "Audio Output", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {225, "lxr.param.225", "MIDI Note", VoiceOwner::Cymbal, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},

    // Shared hi-hat synthesis owner. IDs 226/227 are articulation trigger notes,
    // not a seventh physical synthesis engine.
    {7, "lxr.param.007", "Main Wave", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {18, "lxr.param.018", "Coarse", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {19, "lxr.param.019", "Fine", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {25, "lxr.param.025", "Mod 1 Wave", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {33, "lxr.param.033", "Mod 1 Pitch", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {35, "lxr.param.035", "Mod 1 Gain", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {26, "lxr.param.026", "Mod 2 Wave", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::Enumeration, EnumerationDomain::Waveform},
    {34, "lxr.param.034", "Mod 2 Pitch", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {36, "lxr.param.036", "Mod 2 Gain", VoiceOwner::HiHat, SoundSection::Source, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {59, "lxr.param.059", "Attack", VoiceOwner::HiHat, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {60, "lxr.param.060", "Closed Decay", VoiceOwner::HiHat, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {61, "lxr.param.061", "Open Decay", VoiceOwner::HiHat, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {67, "lxr.param.067", "Slope", VoiceOwner::HiHat, SoundSection::AmpEnvelope, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {142, "lxr.param.142", "Velocity to Amp", VoiceOwner::HiHat, SoundSection::AmpEnvelope, SoundControlKind::Boolean, EnumerationDomain::Boolean},
    {208, "lxr.param.208", "Wave", VoiceOwner::HiHat, SoundSection::Transient, SoundControlKind::Enumeration, EnumerationDomain::Transient},
    {202, "lxr.param.202", "Level", VoiceOwner::HiHat, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {214, "lxr.param.214", "Pitch", VoiceOwner::HiHat, SoundSection::Transient, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {196, "lxr.param.196", "Type", VoiceOwner::HiHat, SoundSection::Filter, SoundControlKind::Enumeration, EnumerationDomain::FilterType},
    {42, "lxr.param.042", "Cutoff", VoiceOwner::HiHat, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {48, "lxr.param.048", "Resonance", VoiceOwner::HiHat, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {133, "lxr.param.133", "Drive", VoiceOwner::HiHat, SoundSection::Filter, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {93, "lxr.param.093", "Level", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::IntegerSlider, EnumerationDomain::None},
    {101, "lxr.param.101", "Pan", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {107, "lxr.param.107", "Distortion", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {113, "lxr.param.113", "Decimate", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::IntegerKnob, EnumerationDomain::None},
    {220, "lxr.param.220", "Audio Output", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::Enumeration, EnumerationDomain::AudioOutput},
    {226, "lxr.param.226", "Closed Note", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None},
    {227, "lxr.param.227", "Open Note", VoiceOwner::HiHat, SoundSection::Output, SoundControlKind::Note, EnumerationDomain::None}
}};

struct EnumerationOption final
{
    int value = 0;
    const char* label = nullptr;
};

inline constexpr std::array<EnumerationOption, 6> kWaveformOptions {{
    {0, "Sin"}, {1, "Tri"}, {2, "Saw"}, {3, "Rec"},
    {4, "Noi"}, {5, "Cym"}
}};
inline constexpr std::array<EnumerationOption, 8> kFilterOptions {{
    {0, "LP"}, {1, "HP"}, {2, "BP"}, {3, "UBP"},
    {4, "Nch"}, {5, "Pek"}, {6, "LP2"}, {7, "off"}
}};
inline constexpr std::array<EnumerationOption, 14> kTransientOptions {{
    {0, "Snp"}, {1, "Ofs"}, {2, "Clk"}, {3, "Ck2"},
    {4, "Tik"}, {5, "Kik"}, {6, "Rim"}, {7, "Drp"},
    {8, "Hat"}, {9, "Clp"}, {10, "Kk2"}, {11, "Snr"},
    {12, "Tom"}, {13, "Sp2"}
}};
inline constexpr std::array<EnumerationOption, 6> kAudioOutputOptions {{
    {0, "St1"}, {1, "St2"}, {2, "L1"},
    {3, "R1"}, {4, "L2"}, {5, "R2"}
}};
inline constexpr std::array<EnumerationOption, 2> kMixModeOptions {{
    {0, "FM"}, {1, "Mix"}
}};
inline constexpr std::array<EnumerationOption, 2> kBooleanOptions {{
    {0, "Off"}, {1, "On"}
}};

constexpr std::size_t soundBindingCount(VoiceOwner owner) noexcept
{
    std::size_t count = 0u;
    for (const auto& binding : kSoundBindings)
        if (binding.owner == owner)
            ++count;
    return count;
}

constexpr const SoundParameterBinding* soundBindingForParameter(int parameter) noexcept
{
    for (const auto& binding : kSoundBindings)
        if (binding.parameter == parameter)
            return &binding;
    return nullptr;
}

constexpr int enumerationMinimum(EnumerationDomain domain) noexcept
{
    return domain == EnumerationDomain::None ? -1 : 0;
}

constexpr int enumerationMaximum(EnumerationDomain domain) noexcept
{
    switch (domain)
    {
        case EnumerationDomain::Waveform: return 5;
        case EnumerationDomain::FilterType: return 7;
        case EnumerationDomain::Transient: return 13;
        case EnumerationDomain::AudioOutput: return 5;
        case EnumerationDomain::MixMode:
        case EnumerationDomain::Boolean: return 1;
        case EnumerationDomain::None: break;
    }
    return -1;
}

constexpr bool enumerationValueSupported(EnumerationDomain domain, int value) noexcept
{
    return domain != EnumerationDomain::None &&
        value >= enumerationMinimum(domain) && value <= enumerationMaximum(domain);
}

constexpr const char* enumerationLabel(EnumerationDomain domain, int value) noexcept
{
    if (!enumerationValueSupported(domain, value))
        return nullptr;
    switch (domain)
    {
        case EnumerationDomain::Waveform: return kWaveformOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::FilterType: return kFilterOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::Transient: return kTransientOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::AudioOutput: return kAudioOutputOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::MixMode: return kMixModeOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::Boolean: return kBooleanOptions[static_cast<std::size_t>(value)].label;
        case EnumerationDomain::None: break;
    }
    return nullptr;
}

constexpr int adjacentEnumerationValue(EnumerationDomain domain, int current,
                                       int direction) noexcept
{
    const int maximum = enumerationMaximum(domain);
    if (maximum < 0)
        return current;
    if (!enumerationValueSupported(domain, current))
        return direction < 0 ? maximum : 0;
    return direction < 0
        ? (current == 0 ? maximum : current - 1)
        : (current == maximum ? 0 : current + 1);
}

constexpr int soundEndpoint(const SoundParameterBinding& binding,
                            bool maximum) noexcept
{
    if (!maximum)
        return 0;
    return binding.enumeration == EnumerationDomain::None
        ? 127 : enumerationMaximum(binding.enumeration);
}

inline bool normalizeNativeInteger(int value, double& normalized) noexcept
{
    if (value < 0 || value > 127)
        return false;
    normalized = static_cast<double>(value) / 127.0;
    return true;
}

inline bool denormalizeNativeInteger(double normalized, int& value) noexcept
{
    if (!std::isfinite(normalized) || normalized < 0.0 || normalized > 1.0)
        return false;
    value = static_cast<int>(std::floor(normalized * 127.0 + 0.5));
    return value >= 0 && value <= 127;
}

constexpr bool validateSoundBindings() noexcept
{
    for (std::size_t i = 0; i < kSoundBindings.size(); ++i)
    {
        const auto& row = kSoundBindings[i];
        if (row.parameter < 0 || row.parameter >= vst::kNativeNumParams ||
            row.controlId == nullptr || row.controlId[0] == '\0' ||
            row.label == nullptr || row.label[0] == '\0' ||
            static_cast<std::size_t>(row.section) >= kSoundSectionNames.size())
            return false;
        const auto& descriptor = vst::kParamDescriptors[static_cast<std::size_t>(row.parameter)];
        if (descriptor.minValue != 0 || descriptor.maxValue != 127 ||
            descriptor.defaultValue < 0 || descriptor.defaultValue > 127)
            return false;
        if ((row.kind == SoundControlKind::Enumeration ||
             row.kind == SoundControlKind::Boolean) !=
            (row.enumeration != EnumerationDomain::None))
            return false;
        for (std::size_t j = 0; j < i; ++j)
            if (row.parameter == kSoundBindings[j].parameter)
                return false;
    }
    return soundBindingCount(VoiceOwner::Drum1) == 27u &&
        soundBindingCount(VoiceOwner::Drum2) == 27u &&
        soundBindingCount(VoiceOwner::Drum3) == 27u &&
        soundBindingCount(VoiceOwner::Snare) == 26u &&
        soundBindingCount(VoiceOwner::Cymbal) == 27u &&
        soundBindingCount(VoiceOwner::HiHat) == 28u;
}

static_assert(validateSoundBindings(),
              "P19-C Sound bindings must be complete, unique and source-valid");

struct SoundBounds final
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    constexpr int width() const noexcept { return right - left; }
    constexpr int height() const noexcept { return bottom - top; }
    constexpr bool insideCanvas() const noexcept
    {
        return left >= 0 && top >= 0 && right <= 1280 && bottom <= 800 &&
            left < right && top < bottom;
    }
};

inline constexpr std::array<SoundBounds, 6> kSoundSectionBounds {{
    {24, 198, 590, 414},   // source
    {606, 198, 806, 414},  // pitch envelope
    {822, 198, 1256, 414}, // amplitude envelope
    {24, 428, 374, 684},   // transient
    {390, 428, 776, 684},  // filter
    {792, 428, 1256, 684}  // output
}};

inline constexpr std::array<int, 6> kSoundSectionColumns {{5, 2, 3, 3, 2, 4}};

constexpr std::size_t soundSectionCount(VoiceOwner owner,
                                        SoundSection section) noexcept
{
    std::size_t count = 0u;
    for (const auto& binding : kSoundBindings)
        if (binding.owner == owner && binding.section == section)
            ++count;
    return count;
}

constexpr SoundBounds soundControlBounds(const SoundParameterBinding& target) noexcept
{
    const auto sectionIndex = static_cast<std::size_t>(target.section);
    if (sectionIndex >= kSoundSectionBounds.size())
        return {};
    const auto panel = kSoundSectionBounds[sectionIndex];
    const int columns = kSoundSectionColumns[sectionIndex];
    std::size_t index = 0u;
    bool found = false;
    for (const auto& binding : kSoundBindings)
    {
        if (binding.owner != target.owner || binding.section != target.section)
            continue;
        if (binding.parameter == target.parameter)
        {
            found = true;
            break;
        }
        ++index;
    }
    if (!found)
        return {};
    const int column = static_cast<int>(index % static_cast<std::size_t>(columns));
    const int row = static_cast<int>(index / static_cast<std::size_t>(columns));
    const int cellWidth = (panel.width() - 12) / columns;
    constexpr int controlHeight = 78;
    constexpr int firstTop = 31;
    constexpr int rowStride = 86;
    return {
        panel.left + 6 + column * cellWidth,
        panel.top + firstTop + row * rowStride,
        panel.left + 6 + (column + 1) * cellWidth - 5,
        panel.top + firstTop + row * rowStride + controlHeight
    };
}

constexpr bool validateSoundGeometry() noexcept
{
    for (const auto& panel : kSoundSectionBounds)
        if (!panel.insideCanvas() || panel.top < 190 || panel.bottom > 700)
            return false;
    for (const auto& binding : kSoundBindings)
    {
        const auto bounds = soundControlBounds(binding);
        const auto panel = kSoundSectionBounds[static_cast<std::size_t>(binding.section)];
        if (!bounds.insideCanvas() || bounds.left < panel.left ||
            bounds.top < panel.top || bounds.right > panel.right ||
            bounds.bottom > panel.bottom)
            return false;
    }
    return true;
}

static_assert(validateSoundGeometry(),
              "P19-C Sound controls must fit the fixed 1280 x 800 canvas");
