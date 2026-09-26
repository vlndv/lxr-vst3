// FILE: dsp/MidiParser.cpp
#include "MidiParser.h"

namespace lxr {

MidiParser::MidiParser() : m_runningStatus(0), m_nrpnMsb(0), m_nrpnLsb(0), m_nrpnComplete(false) {}

void MidiParser::processByte(uint8_t byte, Engine& engine, ParameterArray& params) {
    // Minimal byte-level parser can be added later if needed.
    // For now, we rely on processEvent for multi-byte messages.
}

void MidiParser::processEvent(const MidiEvent& event, Engine& engine, ParameterArray& params) {
    uint8_t msgType = event.status & 0xF0;
    uint8_t channel = event.status & 0x0F;

    switch (msgType) {
        case 0x90: // Note On
            if (event.data2 > 0) {
                handleNoteOn(channel, event.data1, event.data2, engine);
            } else {
                handleNoteOff(channel, event.data1, 0, engine);
            }
            break;
        case 0x80: // Note Off
            handleNoteOff(channel, event.data1, event.data2, engine);
            break;
        case 0xB0: // CC or NRPN
            if (event.data1 == 99) {
                m_nrpnMsb = event.data2;
                m_nrpnComplete = false;
            } else if (event.data1 == 98) {
                m_nrpnLsb = event.data2;
                m_nrpnComplete = true; // <-- FIXED: Mark NRPN as complete after LSB
            } else if (event.data1 == 6 && m_nrpnComplete) {
                uint16_t nrpn = (static_cast<uint16_t>(m_nrpnMsb) << 7) | m_nrpnLsb;
                handleNRPN(channel, nrpn, event.data2, engine, params);
                m_nrpnComplete = false; // Reset after processing
            } else if (event.data1 >= 1 && event.data1 <= 127) {
                // Standard CC
                handleCC(channel, event.data1, event.data2, engine, params);
            }
            break;
    }
}

void MidiParser::handleNoteOn(uint8_t channel, uint8_t note, uint8_t vel, Engine& engine) {
    if (channel >= kMidiChannelCount) return; // Ignore channels 8-16 for now
    
    switch (channel) {
        case 0: engine.triggerDrum(0, vel, note); break; // Ch 1 -> D1
        case 1: engine.triggerDrum(1, vel, note); break; // Ch 2 -> D2
        case 2: engine.triggerDrum(2, vel, note); break; // Ch 3 -> D3
        case 3: engine.triggerSnare(vel, note); break;   // Ch 4 -> SN
        case 4: engine.triggerCymbal(vel, note); break;  // Ch 5 -> CY
        case 5: engine.triggerHiHat(vel, false, note); break; // Ch 6 -> HH Closed
        case 6: engine.triggerHiHat(vel, true, note); break;  // Ch 7 -> HH Open
    }
}

void MidiParser::handleNoteOff(uint8_t channel, uint8_t note, uint8_t vel, Engine& engine) {
    // LXR voices are percussive and don't typically respond to Note Off for gating.
    // No-op for now.
}

void MidiParser::handleCC(uint8_t channel, uint8_t cc, uint8_t value, Engine& engine, ParameterArray& params) {
    uint8_t par = parFromCC(cc);
    if (par > 0) {
        params.set(par, value, engine);
    }
}

void MidiParser::handleNRPN(uint8_t channel, uint16_t nrpn, uint8_t value, Engine& engine, ParameterArray& params) {
    // SPECIAL CASE: NRPN 200-206 are track mutes in the original firmware.
    // They do NOT map to a PAR index. We intercept them here.
    if (nrpn >= 200 && nrpn <= 206) {
        // TODO: Route to sequencer/trigger-dispatch layer (P16)
        // For now, no-op to prevent out-of-bounds PAR access.
        return;
    }

    uint8_t par = parFromNRPN(nrpn);
    if (par > 0) {
        params.set(par, value, engine);
    }
}

} // namespace lxr