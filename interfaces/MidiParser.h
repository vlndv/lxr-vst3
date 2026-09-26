// FILE: dsp/MidiParser.h
#pragma once
#include <cstdint>
#include "Engine.h"
#include "ParameterArray.h"

namespace lxr {

// MIDI Channel to Voice mapping (1-indexed channels)
// Ch 1=D1, Ch 2=D2, Ch 3=D3, Ch 4=SN, Ch 5=CY, Ch 6=HH, Ch 7=HH_OPEN
constexpr uint8_t kMidiChannelCount = 7;

struct MidiEvent {
    uint8_t status;   // 0x80-0xEF (Note Off/On, CC, NRPN, etc.)
    uint8_t channel;  // 0-15
    uint8_t data1;
    uint8_t data2;
};

class MidiParser {
public:
    MidiParser();
    
    // Process a single raw MIDI byte (handles running status internally)
    void processByte(uint8_t byte, Engine& engine, ParameterArray& params);
    
    // Alternative: Process a fully formed MIDI event
    void processEvent(const MidiEvent& event, Engine& engine, ParameterArray& params);

private:
    uint8_t m_runningStatus = 0;
    
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t vel, Engine& engine);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t vel, Engine& engine);
    void handleCC(uint8_t channel, uint8_t cc, uint8_t value, Engine& engine, ParameterArray& params);
    void handleNRPN(uint8_t channel, uint16_t nrpn, uint8_t value, Engine& engine, ParameterArray& params);
    
    // NRPN state machine
    uint8_t m_nrpnMsb = 0;
    uint8_t m_nrpnLsb = 0;
    bool m_nrpnComplete = false;
};

// Helper: Convert CC number to PAR index (CC 1-127 -> PAR 1-127)
inline uint8_t parFromCC(uint8_t cc) {
    return (cc >= 1 && cc <= 127) ? cc : 0;
}

// Helper: Convert NRPN number to PAR index (NRPN 1-100 -> PAR 128-227)
// NRPN 200-206 are special (track mutes) and handled separately.
inline uint8_t parFromNRPN(uint16_t nrpn) {
    if (nrpn >= 1 && nrpn <= 100) {
        return 127 + nrpn;
    }
    return 0; // Invalid or unhandled NRPN
}

} // namespace lxr