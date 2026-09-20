# param_map.md — LXR 0.37 parameter map

Source: SonicPotions/LXR @ dee4968 (fw 0.37). Files: `mainboard/LxrStm32/src/MIDI/{ParameterArray.h,ParameterArray.c,MidiParser.c,MidiMessages.h}`, `front/LxrAvr/Menu/menu.c` (dtype).
NOT the brendanclarke/Catalyst fork (its DSP differs).

## Addressing
- `PAR` = internal parameter index (0-227). Voices: D1-D3 drum, SN snare, CY cymbal, HH hi-hat (closed+open share one voice).
- `PAR` 1..127: MIDI **CC = PAR+1** (CC 0 = bank select, CC 1 = mod wheel, unused). Received on the *global* channel only.
- `PAR` 128..227: **NRPN number = PAR-128**. Send NRPN MSB on CC99, LSB on CC98, value on CC6 (Data Entry MSB). Value is 7-bit.
- NRPN 200..206 = track mutes 1-7 (0 = unmute, else mute). No PAR index.
- Notes: each of 7 tracks has its own MIDI channel (+ global channel). Optional per-track note override (`MIDI_NOTE`): 0 = any note plays voice with that note as pitch; else only that note triggers, at default note 63.
- `VEL_DEST`, `VOICE_LFO`, `TARGET_LFO` are **not settable over MIDI** in the original (ignored in `midiParser_ccHandler`); front panel sets them via an internal message. Destination value = a `PAR` index.
- PAR 1 is aliased by `PAR_MOD_WHEEL`; row omitted. PAR 5, 97, 98, 127 are placeholders (no engine target).

## Scale tags (value v = 0..127)
| tag | engine value (from source) |
|---|---|
| wave | raw int. 0 Sin,1 Tri,2 Saw,3 Rec,4 Noise,5 Crash sample; >=6 = SD user sample (not ported) |
| coarse | high byte of `midiFreq`. note = clamp(coarse + baseNote - 63, 0, 127); freq = MidiNoteFrequencies[note] x detune. Default base note 63 |
| fine | low byte of `midiFreq`. detune = 1 + (v/127 - 0.5) x 0.0594631 (linear, about +-50 cent) |
| noisef | snare noise S&H rate Hz = v/127 x 22000 |
| lin | v/127 |
| cutoff | f = shaper(v/127, -0.9) x 0.45 (cycles/sample); shaper(x,s): k=2s/(1.0001-s); (1+k)x/(1+k abs(x)). g = fastTan(pi f) |
| reso | q = 1 - v/127; if q < 0.1 then q = 0.02 |
| egA | attack step/tick = 1 - (1+K)x/(1+K abs(x)), x=v/127, K = 2x0.99f/(1.f-0.99f) computed in float = 198.000198 (v=127 -> step 0) |
| egD | same with K = 2x0.999f/(1.f-0.999f) computed in float = 1998.02576 (v=127 -> step 0, never decays) |
| slopeA | amount = (v/127 - 0.5) x 1.999; slope = 2a/(1-a); invSlope uses -a |
| slopeP | pitch EG: amount = (v/127 - 0.5) x 2 (**v=127 divides by zero**); slope = 2a/(1-a) |
| pDecay | pitch EG decay step/tick: as egA (same K = 198.000198) |
| pAmt | (v/127)^2 x 32 |
| pan | raw 0..127. L = sqrtLut[127-pan], R = sqrtLut[pan] (sqrtLut[i] = sqrt(i/127)); handled in the mixer phase (P11), not in P2 |
| dist | s = v/128; shape = 2s/(1-s); y = (1+shape)x/(1+shape abs(x)) |
| fdrive | 0.4 + (v/127)^2 x 6 (pre-filter gain into softclip) |
| decim | rate = shaper(v/127, -0.7); S&H counter += rate x rate_ALL per sample; ALL = index 6 |
| lfoF | ((v+1)/128)^3 x 200 Hz (free-running); if sync != 0 tempo-synced |
| lfoOfs | phaseOffset = v/127.f x 0xffffffff (float product, then to uint32; v=127 gives 2^32, out of range: ARM saturates to 0xFFFFFFFF, port does the same) |
| ftype | engine type = v+1: 1 LP,2 HP,3 BP,4 unity-BP,5 notch,6 peak,7 LP2 (naive 2-pole),8 off (passthrough) |
| trF | transient pitch = 1 + (v/33.9 - 0.75) (about 0.25..4.0) |
| trW | 0 snap EG, 1 offset (start-phase shift), 2..13 = 12 samples (Clk,Ck2,Tik,Kik,Rim,Drp,Hat,Clp,Kk2,Snr,Tom,Sp2); >=14 -> 0 |
| raw | stored as-is. REPEAT: repeat count (SN, CY only). MIX_MOD: 0 FM, 1 mix. VOLUME_MOD: velocity->amp on/off. WAVE_LFO: 0 sin,1 tri,2 saw up,3 saw dn,4 sqr,5 rnd,6 exp up,7 exp dn. RETRIGGER: 0 off, 1..6 = voice. SYNC: 0 off, 1..11 = 4/1,2/1,1/1,1/2,1/3,1/4,1/6,1/8,1/12,1/16,1/32 (scaler 0.25,0.5,1,2,3,4,6,8,12,16,32 x bar rate). AUDIO_OUT: 0 St1,1 St2,2 L1,3 R1,4 L2,5 R2 |
| dest | PAR index of modulation target (front->mainboard message) |

`dtype` = front-panel type from `parameter_dtypes[]` (0B127 = 0..127, PM63 = shown -63..+63, MENU:x = name list).

## Table
| PAR | name | MIDI | voice | dtype | engine target | scale |
|---|---|---|---|---|---|---|
| 1 | OSC_WAVE_DRUM1 | CC 2 | D1 | MENU:waveform | `drum[0].osc.waveform` | wave |
| 2 | OSC_WAVE_DRUM2 | CC 3 | D2 | MENU:waveform | `drum[1].osc.waveform` | wave |
| 3 | OSC_WAVE_DRUM3 | CC 4 | D3 | MENU:waveform | `drum[2].osc.waveform` | wave |
| 4 | OSC_WAVE_SNARE | CC 5 | SN | MENU:waveform | `snareVoice.osc.waveform` | wave |
| 5 | NRPN_DATA_ENTRY_COARSE | CC 6 | - | ? | `-` | placeholder |
| 6 | WAVE1_CYM | CC 7 | CY | MENU:waveform | `cymbalVoice.osc.waveform` | wave |
| 7 | WAVE1_HH | CC 8 | HH | MENU:waveform | `hatVoice.osc.waveform` | wave |
| 8 | COARSE1 | CC 9 | D1 | 0B127 | `drum[0].osc.modNodeValue` | coarse |
| 9 | FINE1 | CC 10 | D1 | PM63 | `drum[0].osc.modNodeValue` | fine |
| 10 | COARSE2 | CC 11 | D2 | 0B127 | `drum[1].osc.modNodeValue` | coarse |
| 11 | FINE2 | CC 12 | D2 | PM63 | `drum[1].osc.modNodeValue` | fine |
| 12 | COARSE3 | CC 13 | D3 | 0B127 | `drum[2].osc.modNodeValue` | coarse |
| 13 | FINE3 | CC 14 | D3 | PM63 | `drum[2].osc.modNodeValue` | fine |
| 14 | COARSE4 | CC 15 | SN | 0B127 | `snareVoice.osc.modNodeValue` | coarse |
| 15 | FINE4 | CC 16 | SN | PM63 | `snareVoice.osc.modNodeValue` | fine |
| 16 | COARSE5 | CC 17 | CY | 0B127 | `cymbalVoice.osc.modNodeValue` | coarse |
| 17 | FINE5 | CC 18 | CY | PM63 | `cymbalVoice.osc.modNodeValue` | fine |
| 18 | COARSE6 | CC 19 | HH | 0B127 | `hatVoice.osc.modNodeValue` | coarse |
| 19 | FINE6 | CC 20 | HH | PM63 | `hatVoice.osc.modNodeValue` | fine |
| 20 | MOD_WAVE_DRUM1 | CC 21 | D1 | MENU:waveform | `drum[0].modOsc.waveform` | wave |
| 21 | MOD_WAVE_DRUM2 | CC 22 | D2 | MENU:waveform | `drum[1].modOsc.waveform` | wave |
| 22 | MOD_WAVE_DRUM3 | CC 23 | D3 | MENU:waveform | `drum[2].modOsc.waveform` | wave |
| 23 | WAVE2_CYM | CC 24 | CY | MENU:waveform | `cymbalVoice.modOsc.waveform` | wave |
| 24 | WAVE3_CYM | CC 25 | CY | MENU:waveform | `cymbalVoice.modOsc2.waveform` | wave |
| 25 | WAVE2_HH | CC 26 | HH | MENU:waveform | `hatVoice.modOsc.waveform` | wave |
| 26 | WAVE3_HH | CC 27 | HH | MENU:waveform | `hatVoice.modOsc2.waveform` | wave |
| 27 | NOISE_FREQ1 | CC 28 | SN | 0B127 | `snareVoice.noiseOsc.modNodeValue` | noisef |
| 28 | MIX1 | CC 29 | SN | 0B127 | `snareVoice.mix` | lin |
| 29 | MOD_OSC_F1_CYM | CC 30 | CY | 0B127 | `cymbalVoice.modOsc.modNodeValue` | coarse |
| 30 | MOD_OSC_F2_CYM | CC 31 | CY | 0B127 | `cymbalVoice.modOsc2.modNodeValue` | coarse |
| 31 | MOD_OSC_GAIN1_CYM | CC 32 | CY | 0B127 | `cymbalVoice.fmModAmount1` | lin |
| 32 | MOD_OSC_GAIN2_CYM | CC 33 | CY | 0B127 | `cymbalVoice.fmModAmount2` | lin |
| 33 | MOD_OSC_F1 | CC 34 | HH | 0B127 | `hatVoice.modOsc.modNodeValue` | coarse |
| 34 | MOD_OSC_F2 | CC 35 | HH | 0B127 | `hatVoice.modOsc2.modNodeValue` | coarse |
| 35 | MOD_OSC_GAIN1 | CC 36 | HH | 0B127 | `hatVoice.fmModAmount1` | lin |
| 36 | MOD_OSC_GAIN2 | CC 37 | HH | 0B127 | `hatVoice.fmModAmount2` | lin |
| 37 | FILTER_FREQ_1 | CC 38 | D1 | 0B127 | `drum[0].filter.f` | cutoff |
| 38 | FILTER_FREQ_2 | CC 39 | D2 | 0B127 | `drum[1].filter.f` | cutoff |
| 39 | FILTER_FREQ_3 | CC 40 | D3 | 0B127 | `drum[2].filter.f` | cutoff |
| 40 | FILTER_FREQ_4 | CC 41 | SN | 0B127 | `snareVoice.filter.f` | cutoff |
| 41 | FILTER_FREQ_5 | CC 42 | CY | 0B127 | `cymbalVoice.filter.f` | cutoff |
| 42 | FILTER_FREQ_6 | CC 43 | HH | 0B127 | `hatVoice.filter.f` | cutoff |
| 43 | RESO_1 | CC 44 | D1 | 0B127 | `drum[0].filter.q` | reso |
| 44 | RESO_2 | CC 45 | D2 | 0B127 | `drum[1].filter.q` | reso |
| 45 | RESO_3 | CC 46 | D3 | 0B127 | `drum[2].filter.q` | reso |
| 46 | RESO_4 | CC 47 | SN | 0B127 | `snareVoice.filter.q` | reso |
| 47 | RESO_5 | CC 48 | CY | 0B127 | `cymbalVoice.filter.q` | reso |
| 48 | RESO_6 | CC 49 | HH | 0B127 | `hatVoice.filter.q` | reso |
| 49 | VELOA1 | CC 50 | D1 | 0B127 | `drum[0].oscVolEg.attack` | egA |
| 50 | VELOD1 | CC 51 | D1 | 0B127 | `drum[0].oscVolEg.decay` | egD |
| 51 | VELOA2 | CC 52 | D2 | 0B127 | `drum[1].oscVolEg.attack` | egA |
| 52 | VELOD2 | CC 53 | D2 | 0B127 | `drum[1].oscVolEg.decay` | egD |
| 53 | VELOA3 | CC 54 | D3 | 0B127 | `drum[2].oscVolEg.attack` | egA |
| 54 | VELOD3 | CC 55 | D3 | 0B127 | `drum[2].oscVolEg.decay` | egD |
| 55 | VELOA4 | CC 56 | SN | 0B127 | `snareVoice.oscVolEg.attack` | egA |
| 56 | VELOD4 | CC 57 | SN | 0B127 | `snareVoice.oscVolEg.decay` | egD |
| 57 | VELOA5 | CC 58 | CY | 0B127 | `cymbalVoice.oscVolEg.attack` | egA |
| 58 | VELOD5 | CC 59 | CY | 0B127 | `cymbalVoice.oscVolEg.decay` | egD |
| 59 | VELOA6 | CC 60 | HH | 0B127 | `hatVoice.oscVolEg.attack` | egA |
| 60 | VELOD6_CLOSED | CC 61 | HH | 0B127 | `hatVoice.decayClosed` | egD |
| 61 | VELOD6_OPEN | CC 62 | HH | 0B127 | `hatVoice.decayOpen` | egD |
| 62 | VOL_SLOPE1 | CC 63 | D1 | 0B127 | `drum[0].oscVolEg.slope` | slopeA |
| 63 | VOL_SLOPE2 | CC 64 | D2 | 0B127 | `drum[1].oscVolEg.slope` | slopeA |
| 64 | VOL_SLOPE3 | CC 65 | D3 | 0B127 | `drum[2].oscVolEg.slope` | slopeA |
| 65 | VOL_SLOPE4 | CC 66 | SN | 0B127 | `snareVoice.oscVolEg.slope` | slopeA |
| 66 | VOL_SLOPE5 | CC 67 | CY | 0B127 | `cymbalVoice.oscVolEg.slope` | slopeA |
| 67 | VOL_SLOPE6 | CC 68 | HH | 0B127 | `hatVoice.oscVolEg.slope` | slopeA |
| 68 | REPEAT4 | CC 69 | SN | 0B127 | `snareVoice.oscVolEg.repeat` | raw |
| 69 | REPEAT5 | CC 70 | CY | 0B127 | `cymbalVoice.oscVolEg.repeat` | raw |
| 70 | MOD_EG1 | CC 71 | D1 | 0B127 | `drum[0].oscPitchEg.decay` | pDecay |
| 71 | MOD_EG2 | CC 72 | D2 | 0B127 | `drum[1].oscPitchEg.decay` | pDecay |
| 72 | MOD_EG3 | CC 73 | D3 | 0B127 | `drum[2].oscPitchEg.decay` | pDecay |
| 73 | MOD_EG4 | CC 74 | SN | 0B127 | `snareVoice.oscPitchEg.decay` | pDecay |
| 74 | MODAMNT1 | CC 75 | D1 | 0B127 | `drum[0].egPitchModAmount` | pAmt |
| 75 | MODAMNT2 | CC 76 | D2 | 0B127 | `drum[1].egPitchModAmount` | pAmt |
| 76 | MODAMNT3 | CC 77 | D3 | 0B127 | `drum[2].egPitchModAmount` | pAmt |
| 77 | MODAMNT4 | CC 78 | SN | 0B127 | `snareVoice.egPitchModAmount` | pAmt |
| 78 | PITCH_SLOPE1 | CC 79 | D1 | 0B127 | `drum[0].oscPitchEg.slope` | slopeP |
| 79 | PITCH_SLOPE2 | CC 80 | D2 | 0B127 | `drum[1].oscPitchEg.slope` | slopeP |
| 80 | PITCH_SLOPE3 | CC 81 | D3 | 0B127 | `drum[2].oscPitchEg.slope` | slopeP |
| 81 | PITCH_SLOPE4 | CC 82 | SN | 0B127 | `snareVoice.oscPitchEg.slope` | slopeP |
| 82 | FMAMNT1 | CC 83 | D1 | 0B127 | `drum[0].fmModAmount` | lin |
| 83 | FM_FREQ1 | CC 84 | D1 | 0B127 | `drum[0].modOsc.modNodeValue` | coarse |
| 84 | FMAMNT2 | CC 85 | D2 | 0B127 | `drum[1].fmModAmount` | lin |
| 85 | FM_FREQ2 | CC 86 | D2 | 0B127 | `drum[1].modOsc.modNodeValue` | coarse |
| 86 | FMAMNT3 | CC 87 | D3 | 0B127 | `drum[2].fmModAmount` | lin |
| 87 | FM_FREQ3 | CC 88 | D3 | 0B127 | `drum[2].modOsc.modNodeValue` | coarse |
| 88 | VOL1 | CC 89 | D1 | 0B127 | `drum[0].vol` | lin |
| 89 | VOL2 | CC 90 | D2 | 0B127 | `drum[1].vol` | lin |
| 90 | VOL3 | CC 91 | D3 | 0B127 | `drum[2].vol` | lin |
| 91 | VOL4 | CC 92 | SN | 0B127 | `snareVoice.vol` | lin |
| 92 | VOL5 | CC 93 | CY | 0B127 | `cymbalVoice.vol` | lin |
| 93 | VOL6 | CC 94 | HH | 0B127 | `hatVoice.vol` | lin |
| 94 | PAN1 | CC 95 | D1 | PM63 | `drum[0].pan` | pan |
| 95 | PAN2 | CC 96 | D2 | PM63 | `drum[1].pan` | pan |
| 96 | PAN3 | CC 97 | D3 | PM63 | `drum[2].pan` | pan |
| 97 | NRPN_FINE | CC 98 | - | ? | `-` | placeholder |
| 98 | NRPN_COARSE | CC 99 | - | ? | `-` | placeholder |
| 99 | PAN4 | CC 100 | SN | PM63 | `snareVoice.pan` | pan |
| 100 | PAN5 | CC 101 | CY | PM63 | `cymbalVoice.pan` | pan |
| 101 | PAN6 | CC 102 | HH | PM63 | `hatVoice.pan` | pan |
| 102 | DRIVE1 | CC 103 | D1 | 0B127 | `drum[0].distortion.shape` | dist |
| 103 | DRIVE2 | CC 104 | D2 | 0B127 | `drum[1].distortion.shape` | dist |
| 104 | DRIVE3 | CC 105 | D3 | 0B127 | `drum[2].distortion.shape` | dist |
| 105 | SNARE_DISTORTION | CC 106 | SN | 0B127 | `snareVoice.distortion.shape` | dist |
| 106 | CYMBAL_DISTORTION | CC 107 | CY | 0B127 | `cymbalVoice.distortion.shape` | dist |
| 107 | HAT_DISTORTION | CC 108 | HH | 0B127 | `hatVoice.distortion.shape` | dist |
| 108 | VOICE_DECIMATION1 | CC 109 | D1 | 0B127 | `mixer_decimation_rate[0]` | decim |
| 109 | VOICE_DECIMATION2 | CC 110 | D2 | 0B127 | `mixer_decimation_rate[1]` | decim |
| 110 | VOICE_DECIMATION3 | CC 111 | D3 | 0B127 | `mixer_decimation_rate[2]` | decim |
| 111 | VOICE_DECIMATION4 | CC 112 | SN | 0B127 | `mixer_decimation_rate[3]` | decim |
| 112 | VOICE_DECIMATION5 | CC 113 | CY | 0B127 | `mixer_decimation_rate[4]` | decim |
| 113 | VOICE_DECIMATION6 | CC 114 | HH | 0B127 | `mixer_decimation_rate[5]` | decim |
| 114 | VOICE_DECIMATION_ALL | CC 115 | - | 0B127 | `mixer_decimation_rate[6]` | decim |
| 115 | FREQ_LFO1 | CC 116 | D1 | 0B127 | `drum[0].lfo.modNodeValue` | lfoF |
| 116 | FREQ_LFO2 | CC 117 | D2 | 0B127 | `drum[1].lfo.modNodeValue` | lfoF |
| 117 | FREQ_LFO3 | CC 118 | D3 | 0B127 | `drum[2].lfo.modNodeValue` | lfoF |
| 118 | FREQ_LFO4 | CC 119 | SN | 0B127 | `snareVoice.lfo.modNodeValue` | lfoF |
| 119 | FREQ_LFO5 | CC 120 | CY | 0B127 | `cymbalVoice.lfo.modNodeValue` | lfoF |
| 120 | FREQ_LFO6 | CC 121 | HH | 0B127 | `hatVoice.lfo.modNodeValue` | lfoF |
| 121 | AMOUNT_LFO1 | CC 122 | D1 | 0B127 | `drum[0].lfo.modTarget.amount` | lin |
| 122 | AMOUNT_LFO2 | CC 123 | D2 | 0B127 | `drum[1].lfo.modTarget.amount` | lin |
| 123 | AMOUNT_LFO3 | CC 124 | D3 | 0B127 | `drum[2].lfo.modTarget.amount` | lin |
| 124 | AMOUNT_LFO4 | CC 125 | SN | 0B127 | `snareVoice.lfo.modTarget.amount` | lin |
| 125 | AMOUNT_LFO5 | CC 126 | CY | 0B127 | `cymbalVoice.lfo.modTarget.amount` | lin |
| 126 | AMOUNT_LFO6 | CC 127 | HH | 0B127 | `hatVoice.lfo.modTarget.amount` | lin |
| 127 | RESERVED4 | CC 128 | - | 0B127 | `-` | placeholder |
| 128 | FILTER_DRIVE_1 | NRPN 0 | D1 | 0B127 | `drum[0].filter.drive` | fdrive |
| 129 | FILTER_DRIVE_2 | NRPN 1 | D2 | 0B127 | `drum[1].filter.drive` | fdrive |
| 130 | FILTER_DRIVE_3 | NRPN 2 | D3 | 0B127 | `drum[2].filter.drive` | fdrive |
| 131 | FILTER_DRIVE_4 | NRPN 3 | SN | 0B127 | `snareVoice.filter.drive` | fdrive |
| 132 | FILTER_DRIVE_5 | NRPN 4 | CY | 0B127 | `cymbalVoice.filter.drive` | fdrive |
| 133 | FILTER_DRIVE_6 | NRPN 5 | HH | 0B127 | `hatVoice.filter.drive` | fdrive |
| 134 | MIX_MOD_1 | NRPN 6 | D1 | MIX_FM | `drum[0].mixOscs` | raw |
| 135 | MIX_MOD_2 | NRPN 7 | D2 | MIX_FM | `drum[1].mixOscs` | raw |
| 136 | MIX_MOD_3 | NRPN 8 | D3 | MIX_FM | `drum[2].mixOscs` | raw |
| 137 | VOLUME_MOD_ON_OFF1 | NRPN 9 | D1 | ON_OFF | `drum[0].volumeMod` | raw |
| 138 | VOLUME_MOD_ON_OFF2 | NRPN 10 | D2 | ON_OFF | `drum[1].volumeMod` | raw |
| 139 | VOLUME_MOD_ON_OFF3 | NRPN 11 | D3 | ON_OFF | `drum[2].volumeMod` | raw |
| 140 | VOLUME_MOD_ON_OFF4 | NRPN 12 | SN | ON_OFF | `snareVoice.volumeMod` | raw |
| 141 | VOLUME_MOD_ON_OFF5 | NRPN 13 | CY | ON_OFF | `cymbalVoice.volumeMod` | raw |
| 142 | VOLUME_MOD_ON_OFF6 | NRPN 14 | HH | ON_OFF | `hatVoice.volumeMod` | raw |
| 143 | VELO_MOD_AMT_1 | NRPN 15 | D1 | 0B127 | `velocityModulators[0].amount` | lin |
| 144 | VELO_MOD_AMT_2 | NRPN 16 | D2 | 0B127 | `velocityModulators[1].amount` | lin |
| 145 | VELO_MOD_AMT_3 | NRPN 17 | D3 | 0B127 | `velocityModulators[2].amount` | lin |
| 146 | VELO_MOD_AMT_4 | NRPN 18 | SN | 0B127 | `velocityModulators[3].amount` | lin |
| 147 | VELO_MOD_AMT_5 | NRPN 19 | CY | 0B127 | `velocityModulators[4].amount` | lin |
| 148 | VELO_MOD_AMT_6 | NRPN 20 | HH | 0B127 | `velocityModulators[5].amount` | lin |
| 149 | VEL_DEST_1 | NRPN 21 | D1 | TARGET_SELECTION_VELO | `velocityModulators[0].destination` | dest |
| 150 | VEL_DEST_2 | NRPN 22 | D2 | TARGET_SELECTION_VELO | `velocityModulators[1].destination` | dest |
| 151 | VEL_DEST_3 | NRPN 23 | D3 | TARGET_SELECTION_VELO | `velocityModulators[2].destination` | dest |
| 152 | VEL_DEST_4 | NRPN 24 | SN | TARGET_SELECTION_VELO | `velocityModulators[3].destination` | dest |
| 153 | VEL_DEST_5 | NRPN 25 | CY | TARGET_SELECTION_VELO | `velocityModulators[4].destination` | dest |
| 154 | VEL_DEST_6 | NRPN 26 | HH | TARGET_SELECTION_VELO | `velocityModulators[5].destination` | dest |
| 155 | WAVE_LFO1 | NRPN 27 | D1 | MENU:lfo_waves | `drum[0].lfo.waveform` | raw |
| 156 | WAVE_LFO2 | NRPN 28 | D2 | MENU:lfo_waves | `drum[1].lfo.waveform` | raw |
| 157 | WAVE_LFO3 | NRPN 29 | D3 | MENU:lfo_waves | `drum[2].lfo.waveform` | raw |
| 158 | WAVE_LFO4 | NRPN 30 | SN | MENU:lfo_waves | `snareVoice.lfo.waveform` | raw |
| 159 | WAVE_LFO5 | NRPN 31 | CY | MENU:lfo_waves | `cymbalVoice.lfo.waveform` | raw |
| 160 | WAVE_LFO6 | NRPN 32 | HH | MENU:lfo_waves | `hatVoice.lfo.waveform` | raw |
| 161 | VOICE_LFO1 | NRPN 33 | D1 | VOICE_LFO | `-` | dest |
| 162 | VOICE_LFO2 | NRPN 34 | D2 | VOICE_LFO | `-` | dest |
| 163 | VOICE_LFO3 | NRPN 35 | D3 | VOICE_LFO | `-` | dest |
| 164 | VOICE_LFO4 | NRPN 36 | SN | VOICE_LFO | `-` | dest |
| 165 | VOICE_LFO5 | NRPN 37 | CY | VOICE_LFO | `-` | dest |
| 166 | VOICE_LFO6 | NRPN 38 | HH | VOICE_LFO | `-` | dest |
| 167 | TARGET_LFO1 | NRPN 39 | D1 | TARGET_SELECTION_LFO | `-` | dest |
| 168 | TARGET_LFO2 | NRPN 40 | D2 | TARGET_SELECTION_LFO | `-` | dest |
| 169 | TARGET_LFO3 | NRPN 41 | D3 | TARGET_SELECTION_LFO | `-` | dest |
| 170 | TARGET_LFO4 | NRPN 42 | SN | TARGET_SELECTION_LFO | `-` | dest |
| 171 | TARGET_LFO5 | NRPN 43 | CY | TARGET_SELECTION_LFO | `-` | dest |
| 172 | TARGET_LFO6 | NRPN 44 | HH | TARGET_SELECTION_LFO | `-` | dest |
| 173 | RETRIGGER_LFO1 | NRPN 45 | D1 | MENU:retrigger | `drum[0].lfo.retrigger` | raw |
| 174 | RETRIGGER_LFO2 | NRPN 46 | D2 | MENU:retrigger | `drum[1].lfo.retrigger` | raw |
| 175 | RETRIGGER_LFO3 | NRPN 47 | D3 | MENU:retrigger | `drum[2].lfo.retrigger` | raw |
| 176 | RETRIGGER_LFO4 | NRPN 48 | SN | MENU:retrigger | `snareVoice.lfo.retrigger` | raw |
| 177 | RETRIGGER_LFO5 | NRPN 49 | CY | MENU:retrigger | `cymbalVoice.lfo.retrigger` | raw |
| 178 | RETRIGGER_LFO6 | NRPN 50 | HH | MENU:retrigger | `hatVoice.lfo.retrigger` | raw |
| 179 | SYNC_LFO1 | NRPN 51 | D1 | MENU:sync_rates | `drum[0].lfo.sync` | raw |
| 180 | SYNC_LFO2 | NRPN 52 | D2 | MENU:sync_rates | `drum[1].lfo.sync` | raw |
| 181 | SYNC_LFO3 | NRPN 53 | D3 | MENU:sync_rates | `drum[2].lfo.sync` | raw |
| 182 | SYNC_LFO4 | NRPN 54 | SN | MENU:sync_rates | `snareVoice.lfo.sync` | raw |
| 183 | SYNC_LFO5 | NRPN 55 | CY | MENU:sync_rates | `cymbalVoice.lfo.sync` | raw |
| 184 | SYNC_LFO6 | NRPN 56 | HH | MENU:sync_rates | `hatVoice.lfo.sync` | raw |
| 185 | OFFSET_LFO1 | NRPN 57 | D1 | 0B127 | `drum[0].lfo.phaseOffset` | lfoOfs |
| 186 | OFFSET_LFO2 | NRPN 58 | D2 | 0B127 | `drum[1].lfo.phaseOffset` | lfoOfs |
| 187 | OFFSET_LFO3 | NRPN 59 | D3 | 0B127 | `drum[2].lfo.phaseOffset` | lfoOfs |
| 188 | OFFSET_LFO4 | NRPN 60 | SN | 0B127 | `snareVoice.lfo.phaseOffset` | lfoOfs |
| 189 | OFFSET_LFO5 | NRPN 61 | CY | 0B127 | `cymbalVoice.lfo.phaseOffset` | lfoOfs |
| 190 | OFFSET_LFO6 | NRPN 62 | HH | 0B127 | `hatVoice.lfo.phaseOffset` | lfoOfs |
| 191 | FILTER_TYPE_1 | NRPN 63 | D1 | MENU:filter | `drum[0].filterType` | ftype |
| 192 | FILTER_TYPE_2 | NRPN 64 | D2 | MENU:filter | `drum[1].filterType` | ftype |
| 193 | FILTER_TYPE_3 | NRPN 65 | D3 | MENU:filter | `drum[2].filterType` | ftype |
| 194 | FILTER_TYPE_4 | NRPN 66 | SN | MENU:filter | `snareVoice.filterType` | ftype |
| 195 | FILTER_TYPE_5 | NRPN 67 | CY | MENU:filter | `cymbalVoice.filterType` | ftype |
| 196 | FILTER_TYPE_6 | NRPN 68 | HH | MENU:filter | `hatVoice.filterType` | ftype |
| 197 | TRANS1_VOL | NRPN 69 | D1 | 0B127 | `drum[0].transGen.volume` | lin |
| 198 | TRANS2_VOL | NRPN 70 | D2 | 0B127 | `drum[1].transGen.volume` | lin |
| 199 | TRANS3_VOL | NRPN 71 | D3 | 0B127 | `drum[2].transGen.volume` | lin |
| 200 | TRANS4_VOL | NRPN 72 | SN | 0B127 | `snareVoice.transGen.volume` | lin |
| 201 | TRANS5_VOL | NRPN 73 | CY | 0B127 | `cymbalVoice.transGen.volume` | lin |
| 202 | TRANS6_VOL | NRPN 74 | HH | 0B127 | `hatVoice.transGen.volume` | lin |
| 203 | TRANS1_WAVE | NRPN 75 | D1 | MENU:trans | `drum[0].transGen.waveform` | trW |
| 204 | TRANS2_WAVE | NRPN 76 | D2 | MENU:trans | `drum[1].transGen.waveform` | trW |
| 205 | TRANS3_WAVE | NRPN 77 | D3 | MENU:trans | `drum[2].transGen.waveform` | trW |
| 206 | TRANS4_WAVE | NRPN 78 | SN | MENU:trans | `snareVoice.transGen.waveform` | trW |
| 207 | TRANS5_WAVE | NRPN 79 | CY | MENU:trans | `cymbalVoice.transGen.waveform` | trW |
| 208 | TRANS6_WAVE | NRPN 80 | HH | MENU:trans | `hatVoice.transGen.waveform` | trW |
| 209 | TRANS1_FREQ | NRPN 81 | D1 | 0B127 | `drum[0].transGen.pitch` | trF |
| 210 | TRANS2_FREQ | NRPN 82 | D2 | 0B127 | `drum[1].transGen.pitch` | trF |
| 211 | TRANS3_FREQ | NRPN 83 | D3 | 0B127 | `drum[2].transGen.pitch` | trF |
| 212 | TRANS4_FREQ | NRPN 84 | SN | 0B127 | `snareVoice.transGen.pitch` | trF |
| 213 | TRANS5_FREQ | NRPN 85 | CY | 0B127 | `cymbalVoice.transGen.pitch` | trF |
| 214 | TRANS6_FREQ | NRPN 86 | HH | 0B127 | `hatVoice.transGen.pitch` | trF |
| 215 | AUDIO_OUT1 | NRPN 87 | D1 | MENU:audio_out | `mixer_audioRouting[0]` | raw |
| 216 | AUDIO_OUT2 | NRPN 88 | D2 | MENU:audio_out | `mixer_audioRouting[1]` | raw |
| 217 | AUDIO_OUT3 | NRPN 89 | D3 | MENU:audio_out | `mixer_audioRouting[2]` | raw |
| 218 | AUDIO_OUT4 | NRPN 90 | SN | MENU:audio_out | `mixer_audioRouting[3]` | raw |
| 219 | AUDIO_OUT5 | NRPN 91 | CY | MENU:audio_out | `mixer_audioRouting[4]` | raw |
| 220 | AUDIO_OUT6 | NRPN 92 | HH | MENU:audio_out | `mixer_audioRouting[5]` | raw |
| 221 | MIDI_NOTE1 | NRPN 93 | - | NOTE_NAME | `midi_NoteOverride[0]` | raw |
| 222 | MIDI_NOTE2 | NRPN 94 | - | NOTE_NAME | `midi_NoteOverride[1]` | raw |
| 223 | MIDI_NOTE3 | NRPN 95 | - | NOTE_NAME | `midi_NoteOverride[2]` | raw |
| 224 | MIDI_NOTE4 | NRPN 96 | - | NOTE_NAME | `midi_NoteOverride[3]` | raw |
| 225 | MIDI_NOTE5 | NRPN 97 | - | NOTE_NAME | `midi_NoteOverride[4]` | raw |
| 226 | MIDI_NOTE6 | NRPN 98 | - | NOTE_NAME | `midi_NoteOverride[5]` | raw |
| 227 | MIDI_NOTE7 | NRPN 99 | - | NOTE_NAME | `midi_NoteOverride[6]` | raw |

## Known quirks (from source)
- `MIX_MOD` (D1-D3): mix mode adds modOsc (gain fmAmount) to osc (gain 1-fmAmount). FM mode: modOsc buffer x fmAmount, FM index scaled again by fmAmount x pitchEG value.
- `FILTER_DRIVE` is applied via `SVF_setDrive`; a disabled `USE_FILTER_DRIVE` branch for OSC3_DIST writes `voiceArray[3]` (dead code).
- `COARSE`/`FINE` and `FM_FREQ` targets in `parameterArray` point at `modNodeValue` (a freq multiplier used only for modulation), not `midiFreq`.
- Modulation runs on the engine value after mapping, scaled by `1 + amount x (mod - 1)`; mod source in 0..1.
