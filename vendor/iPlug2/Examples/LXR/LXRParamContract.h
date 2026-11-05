#pragma once

#include <array>
#include <cstddef>

namespace lxr::vst
{

enum class ParamSemantic
{
  Reserved,
  Raw7Bit,
  Waveform,
  Frequency,
  Resonance,
  Envelope,
  Volume,
  Pan,
  Distortion,
  Decimation,
  LFO,
  Boolean,
  FilterType,
  Transient,
  OutputRouting,
  MidiNote,
  InternalOnly
};

struct ParamDescriptor
{
  const char* name;
  int defaultValue;
  int minValue;
  int maxValue;
  ParamSemantic semantic;
};

inline constexpr int kNativeNumParams = 228;
inline constexpr int kRouteNumParams = 12;
inline constexpr int kNumParams = kNativeNumParams + kRouteNumParams;
inline constexpr int kNumPresets = 1;

constexpr ParamDescriptor P(
  const char* name,
  int defaultValue = 0,
  ParamSemantic semantic = ParamSemantic::Raw7Bit)
{
  return {name, defaultValue, 0, 127, semantic};
}

inline constexpr std::array<ParamDescriptor, kNativeNumParams> kParamDescriptors {{
  P("RESERVED", 0, ParamSemantic::Reserved),                 // 0

  P("OSC_WAVE_DRUM1", 1, ParamSemantic::Waveform),           // 1
  P("OSC_WAVE_DRUM2", 1, ParamSemantic::Waveform),           // 2
  P("OSC_WAVE_DRUM3", 1, ParamSemantic::Waveform),           // 3
  P("OSC_WAVE_SNARE", 1, ParamSemantic::Waveform),           // 4
  P("NRPN_DATA_ENTRY_COARSE"),                               // 5
  P("WAVE1_CYM", 1, ParamSemantic::Waveform),                // 6
  P("WAVE1_HH", 1, ParamSemantic::Waveform),                 // 7

  P("COARSE1", 63),                                          // 8
  P("FINE1", 63),                                            // 9
  P("COARSE2", 63),                                          // 10
  P("FINE2", 63),                                            // 11
  P("COARSE3", 63),                                          // 12
  P("FINE3", 63),                                            // 13
  P("COARSE4", 63),                                          // 14
  P("FINE4", 63),                                            // 15
  P("COARSE5", 63),                                          // 16
  P("FINE5", 63),                                            // 17
  P("COARSE6", 63),                                          // 18
  P("FINE6", 63),                                            // 19

  P("MOD_WAVE_DRUM1", 1, ParamSemantic::Waveform),           // 20
  P("MOD_WAVE_DRUM2", 1, ParamSemantic::Waveform),           // 21
  P("MOD_WAVE_DRUM3", 1, ParamSemantic::Waveform),           // 22
  P("WAVE2_CYM", 1, ParamSemantic::Waveform),                // 23
  P("WAVE3_CYM", 1, ParamSemantic::Waveform),                // 24
  P("WAVE2_HH", 1, ParamSemantic::Waveform),                 // 25
  P("WAVE3_HH", 1, ParamSemantic::Waveform),                 // 26

  P("NOISE_FREQ1", 0, ParamSemantic::Frequency),             // 27
  P("MIX1"),                                                 // 28
  P("MOD_OSC_F1_CYM", 0, ParamSemantic::Frequency),          // 29
  P("MOD_OSC_F2_CYM", 0, ParamSemantic::Frequency),          // 30
  P("MOD_OSC_GAIN1_CYM"),                                    // 31
  P("MOD_OSC_GAIN2_CYM"),                                    // 32
  P("MOD_OSC_F1", 0, ParamSemantic::Frequency),              // 33
  P("MOD_OSC_F2", 0, ParamSemantic::Frequency),              // 34
  P("MOD_OSC_GAIN1"),                                        // 35
  P("MOD_OSC_GAIN2"),                                        // 36

  P("FILTER_FREQ_1", 64, ParamSemantic::Frequency),          // 37
  P("FILTER_FREQ_2", 64, ParamSemantic::Frequency),          // 38
  P("FILTER_FREQ_3", 64, ParamSemantic::Frequency),          // 39
  P("FILTER_FREQ_4", 64, ParamSemantic::Frequency),          // 40
  P("FILTER_FREQ_5", 64, ParamSemantic::Frequency),          // 41
  P("FILTER_FREQ_6", 64, ParamSemantic::Frequency),          // 42

  P("RESO_1", 64, ParamSemantic::Resonance),                 // 43
  P("RESO_2", 64, ParamSemantic::Resonance),                 // 44
  P("RESO_3", 64, ParamSemantic::Resonance),                 // 45
  P("RESO_4", 64, ParamSemantic::Resonance),                 // 46
  P("RESO_5", 64, ParamSemantic::Resonance),                 // 47
  P("RESO_6", 64, ParamSemantic::Resonance),                 // 48

  // Initial patch policy: instant attack. Restored values, including 127,
  // remain valid and must be reapplied unchanged by ResetEngineState().
  P("VELOA1", 0, ParamSemantic::Envelope),                   // 49
  P("VELOD1", 64, ParamSemantic::Envelope),                  // 50
  P("VELOA2", 0, ParamSemantic::Envelope),                   // 51
  P("VELOD2", 64, ParamSemantic::Envelope),                  // 52
  P("VELOA3", 0, ParamSemantic::Envelope),                   // 53
  P("VELOD3", 64, ParamSemantic::Envelope),                  // 54
  P("VELOA4", 0, ParamSemantic::Envelope),                   // 55
  P("VELOD4", 64, ParamSemantic::Envelope),                  // 56
  P("VELOA5", 0, ParamSemantic::Envelope),                   // 57
  P("VELOD5", 64, ParamSemantic::Envelope),                  // 58
  P("VELOA6", 0, ParamSemantic::Envelope),                   // 59
  P("VELOD6_CLOSED", 64, ParamSemantic::Envelope),           // 60
  P("VELOD6_OPEN", 64, ParamSemantic::Envelope),             // 61

  P("VOL_SLOPE1", 0, ParamSemantic::Envelope),               // 62
  P("VOL_SLOPE2", 0, ParamSemantic::Envelope),               // 63
  P("VOL_SLOPE3", 0, ParamSemantic::Envelope),               // 64
  P("VOL_SLOPE4", 0, ParamSemantic::Envelope),               // 65
  P("VOL_SLOPE5", 0, ParamSemantic::Envelope),               // 66
  P("VOL_SLOPE6", 0, ParamSemantic::Envelope),               // 67

  P("REPEAT4"),                                              // 68
  P("REPEAT5"),                                              // 69
  P("MOD_EG1", 0, ParamSemantic::Envelope),                  // 70
  P("MOD_EG2", 0, ParamSemantic::Envelope),                  // 71
  P("MOD_EG3", 0, ParamSemantic::Envelope),                  // 72
  P("MOD_EG4", 0, ParamSemantic::Envelope),                  // 73
  P("MODAMNT1"),                                             // 74
  P("MODAMNT2"),                                             // 75
  P("MODAMNT3"),                                             // 76
  P("MODAMNT4"),                                             // 77
  P("PITCH_SLOPE1", 0, ParamSemantic::Envelope),             // 78
  P("PITCH_SLOPE2", 0, ParamSemantic::Envelope),             // 79
  P("PITCH_SLOPE3", 0, ParamSemantic::Envelope),             // 80
  P("PITCH_SLOPE4", 0, ParamSemantic::Envelope),             // 81

  P("FMAMNT1"),                                              // 82
  P("FM_FREQ1", 0, ParamSemantic::Frequency),                // 83
  P("FMAMNT2"),                                              // 84
  P("FM_FREQ2", 0, ParamSemantic::Frequency),                // 85
  P("FMAMNT3"),                                              // 86
  P("FM_FREQ3", 0, ParamSemantic::Frequency),                // 87

  P("VOL1", 100, ParamSemantic::Volume),                     // 88
  P("VOL2", 100, ParamSemantic::Volume),                     // 89
  P("VOL3", 100, ParamSemantic::Volume),                     // 90
  P("VOL4", 100, ParamSemantic::Volume),                     // 91
  P("VOL5", 100, ParamSemantic::Volume),                     // 92
  P("VOL6", 100, ParamSemantic::Volume),                     // 93

  P("PAN1", 63, ParamSemantic::Pan),                         // 94
  P("PAN2", 63, ParamSemantic::Pan),                         // 95
  P("PAN3", 63, ParamSemantic::Pan),                         // 96
  P("NRPN_FINE"),                                            // 97
  P("NRPN_COARSE"),                                          // 98
  P("PAN4", 63, ParamSemantic::Pan),                         // 99
  P("PAN5", 63, ParamSemantic::Pan),                         // 100
  P("PAN6", 63, ParamSemantic::Pan),                         // 101

  P("DRIVE1", 0, ParamSemantic::Distortion),                 // 102
  P("DRIVE2", 0, ParamSemantic::Distortion),                 // 103
  P("DRIVE3", 0, ParamSemantic::Distortion),                 // 104
  P("SNARE_DISTORTION", 0, ParamSemantic::Distortion),       // 105
  P("CYMBAL_DISTORTION", 0, ParamSemantic::Distortion),      // 106
  P("HAT_DISTORTION", 0, ParamSemantic::Distortion),         // 107

  // Full native rate; zero intentionally freezes a decimator's held sample.
  P("VOICE_DECIMATION1", 127, ParamSemantic::Decimation),    // 108
  P("VOICE_DECIMATION2", 127, ParamSemantic::Decimation),    // 109
  P("VOICE_DECIMATION3", 127, ParamSemantic::Decimation),    // 110
  P("VOICE_DECIMATION4", 127, ParamSemantic::Decimation),    // 111
  P("VOICE_DECIMATION5", 127, ParamSemantic::Decimation),    // 112
  P("VOICE_DECIMATION6", 127, ParamSemantic::Decimation),    // 113
  P("VOICE_DECIMATION_ALL", 127, ParamSemantic::Decimation), // 114

  P("FREQ_LFO1", 64, ParamSemantic::LFO),                    // 115
  P("FREQ_LFO2", 64, ParamSemantic::LFO),                    // 116
  P("FREQ_LFO3", 64, ParamSemantic::LFO),                    // 117
  P("FREQ_LFO4", 64, ParamSemantic::LFO),                    // 118
  P("FREQ_LFO5", 64, ParamSemantic::LFO),                    // 119
  P("FREQ_LFO6", 64, ParamSemantic::LFO),                    // 120

  P("AMOUNT_LFO1", 0, ParamSemantic::LFO),                   // 121
  P("AMOUNT_LFO2", 0, ParamSemantic::LFO),                   // 122
  P("AMOUNT_LFO3", 0, ParamSemantic::LFO),                   // 123
  P("AMOUNT_LFO4", 0, ParamSemantic::LFO),                   // 124
  P("AMOUNT_LFO5", 0, ParamSemantic::LFO),                   // 125
  P("AMOUNT_LFO6", 0, ParamSemantic::LFO),                   // 126

  P("RESERVED4", 0, ParamSemantic::Reserved),                // 127

  P("FILTER_DRIVE_1", 0, ParamSemantic::Distortion),         // 128
  P("FILTER_DRIVE_2", 0, ParamSemantic::Distortion),         // 129
  P("FILTER_DRIVE_3", 0, ParamSemantic::Distortion),         // 130
  P("FILTER_DRIVE_4", 0, ParamSemantic::Distortion),         // 131
  P("FILTER_DRIVE_5", 0, ParamSemantic::Distortion),         // 132
  P("FILTER_DRIVE_6", 0, ParamSemantic::Distortion),         // 133

  P("MIX_MOD_1", 1, ParamSemantic::Boolean),                 // 134
  P("MIX_MOD_2", 1, ParamSemantic::Boolean),                 // 135
  P("MIX_MOD_3", 1, ParamSemantic::Boolean),                 // 136

  P("VOLUME_MOD_ON_OFF1", 1, ParamSemantic::Boolean),        // 137
  P("VOLUME_MOD_ON_OFF2", 1, ParamSemantic::Boolean),        // 138
  P("VOLUME_MOD_ON_OFF3", 1, ParamSemantic::Boolean),        // 139
  P("VOLUME_MOD_ON_OFF4", 1, ParamSemantic::Boolean),        // 140
  P("VOLUME_MOD_ON_OFF5", 1, ParamSemantic::Boolean),        // 141
  P("VOLUME_MOD_ON_OFF6", 1, ParamSemantic::Boolean),        // 142

  P("VELO_MOD_AMT_1"),                                       // 143
  P("VELO_MOD_AMT_2"),                                       // 144
  P("VELO_MOD_AMT_3"),                                       // 145
  P("VELO_MOD_AMT_4"),                                       // 146
  P("VELO_MOD_AMT_5"),                                       // 147
  P("VELO_MOD_AMT_6"),                                       // 148

  P("VEL_DEST_1", 0, ParamSemantic::InternalOnly),           // 149
  P("VEL_DEST_2", 0, ParamSemantic::InternalOnly),           // 150
  P("VEL_DEST_3", 0, ParamSemantic::InternalOnly),           // 151
  P("VEL_DEST_4", 0, ParamSemantic::InternalOnly),           // 152
  P("VEL_DEST_5", 0, ParamSemantic::InternalOnly),           // 153
  P("VEL_DEST_6", 0, ParamSemantic::InternalOnly),           // 154

  P("WAVE_LFO1", 0, ParamSemantic::Waveform),                // 155
  P("WAVE_LFO2", 0, ParamSemantic::Waveform),                // 156
  P("WAVE_LFO3", 0, ParamSemantic::Waveform),                // 157
  P("WAVE_LFO4", 0, ParamSemantic::Waveform),                // 158
  P("WAVE_LFO5", 0, ParamSemantic::Waveform),                // 159
  P("WAVE_LFO6", 0, ParamSemantic::Waveform),                // 160

  P("VOICE_LFO1", 0, ParamSemantic::InternalOnly),           // 161
  P("VOICE_LFO2", 0, ParamSemantic::InternalOnly),           // 162
  P("VOICE_LFO3", 0, ParamSemantic::InternalOnly),           // 163
  P("VOICE_LFO4", 0, ParamSemantic::InternalOnly),           // 164
  P("VOICE_LFO5", 0, ParamSemantic::InternalOnly),           // 165
  P("VOICE_LFO6", 0, ParamSemantic::InternalOnly),           // 166

  P("TARGET_LFO1", 0, ParamSemantic::InternalOnly),          // 167
  P("TARGET_LFO2", 0, ParamSemantic::InternalOnly),          // 168
  P("TARGET_LFO3", 0, ParamSemantic::InternalOnly),          // 169
  P("TARGET_LFO4", 0, ParamSemantic::InternalOnly),          // 170
  P("TARGET_LFO5", 0, ParamSemantic::InternalOnly),          // 171
  P("TARGET_LFO6", 0, ParamSemantic::InternalOnly),          // 172

  P("RETRIGGER_LFO1", 0, ParamSemantic::LFO),                // 173
  P("RETRIGGER_LFO2", 0, ParamSemantic::LFO),                // 174
  P("RETRIGGER_LFO3", 0, ParamSemantic::LFO),                // 175
  P("RETRIGGER_LFO4", 0, ParamSemantic::LFO),                // 176
  P("RETRIGGER_LFO5", 0, ParamSemantic::LFO),                // 177
  P("RETRIGGER_LFO6", 0, ParamSemantic::LFO),                // 178

  P("SYNC_LFO1", 0, ParamSemantic::LFO),                     // 179
  P("SYNC_LFO2", 0, ParamSemantic::LFO),                     // 180
  P("SYNC_LFO3", 0, ParamSemantic::LFO),                     // 181
  P("SYNC_LFO4", 0, ParamSemantic::LFO),                     // 182
  P("SYNC_LFO5", 0, ParamSemantic::LFO),                     // 183
  P("SYNC_LFO6", 0, ParamSemantic::LFO),                     // 184

  P("OFFSET_LFO1", 0, ParamSemantic::LFO),                   // 185
  P("OFFSET_LFO2", 0, ParamSemantic::LFO),                   // 186
  P("OFFSET_LFO3", 0, ParamSemantic::LFO),                   // 187
  P("OFFSET_LFO4", 0, ParamSemantic::LFO),                   // 188
  P("OFFSET_LFO5", 0, ParamSemantic::LFO),                   // 189
  P("OFFSET_LFO6", 0, ParamSemantic::LFO),                   // 190

  P("FILTER_TYPE_1", 0, ParamSemantic::FilterType),          // 191
  P("FILTER_TYPE_2", 0, ParamSemantic::FilterType),          // 192
  P("FILTER_TYPE_3", 0, ParamSemantic::FilterType),          // 193
  P("FILTER_TYPE_4", 0, ParamSemantic::FilterType),          // 194
  P("FILTER_TYPE_5", 0, ParamSemantic::FilterType),          // 195
  P("FILTER_TYPE_6", 0, ParamSemantic::FilterType),          // 196

  P("TRANS1_VOL", 0, ParamSemantic::Transient),              // 197
  P("TRANS2_VOL", 0, ParamSemantic::Transient),              // 198
  P("TRANS3_VOL", 0, ParamSemantic::Transient),              // 199
  P("TRANS4_VOL", 0, ParamSemantic::Transient),              // 200
  P("TRANS5_VOL", 0, ParamSemantic::Transient),              // 201
  P("TRANS6_VOL", 0, ParamSemantic::Transient),              // 202

  P("TRANS1_WAVE", 0, ParamSemantic::Transient),             // 203
  P("TRANS2_WAVE", 0, ParamSemantic::Transient),             // 204
  P("TRANS3_WAVE", 0, ParamSemantic::Transient),             // 205
  P("TRANS4_WAVE", 0, ParamSemantic::Transient),             // 206
  P("TRANS5_WAVE", 0, ParamSemantic::Transient),             // 207
  P("TRANS6_WAVE", 0, ParamSemantic::Transient),             // 208

  P("TRANS1_FREQ", 0, ParamSemantic::Transient),             // 209
  P("TRANS2_FREQ", 0, ParamSemantic::Transient),             // 210
  P("TRANS3_FREQ", 0, ParamSemantic::Transient),             // 211
  P("TRANS4_FREQ", 0, ParamSemantic::Transient),             // 212
  P("TRANS5_FREQ", 0, ParamSemantic::Transient),             // 213
  P("TRANS6_FREQ", 0, ParamSemantic::Transient),             // 214

  P("AUDIO_OUT1", 0, ParamSemantic::OutputRouting),          // 215
  P("AUDIO_OUT2", 0, ParamSemantic::OutputRouting),          // 216
  P("AUDIO_OUT3", 0, ParamSemantic::OutputRouting),          // 217
  P("AUDIO_OUT4", 0, ParamSemantic::OutputRouting),          // 218
  P("AUDIO_OUT5", 0, ParamSemantic::OutputRouting),          // 219
  P("AUDIO_OUT6", 0, ParamSemantic::OutputRouting),          // 220

  P("MIDI_NOTE1", 0, ParamSemantic::MidiNote),               // 221
  P("MIDI_NOTE2", 0, ParamSemantic::MidiNote),               // 222
  P("MIDI_NOTE3", 0, ParamSemantic::MidiNote),               // 223
  P("MIDI_NOTE4", 0, ParamSemantic::MidiNote),               // 224
  P("MIDI_NOTE5", 0, ParamSemantic::MidiNote),               // 225
  P("MIDI_NOTE6", 0, ParamSemantic::MidiNote),               // 226
  P("MIDI_NOTE7", 0, ParamSemantic::MidiNote)                // 227
}};

static_assert(
  kParamDescriptors.size() == static_cast<std::size_t>(kNativeNumParams),
  "LXR parameter descriptor table must contain exactly 228 entries"
);

constexpr bool ValidateParamDescriptors()
{
  for (const auto& descriptor : kParamDescriptors)
  {
    if (descriptor.name == nullptr)
      return false;

    if (descriptor.name[0] == '\0')
      return false;

    if (descriptor.minValue > descriptor.maxValue)
      return false;

    if (descriptor.defaultValue < descriptor.minValue)
      return false;

    if (descriptor.defaultValue > descriptor.maxValue)
      return false;
  }

  return true;
}

static_assert(
  ValidateParamDescriptors(),
  "LXR parameter descriptor table contains an invalid descriptor"
);

} // namespace lxr::vst
