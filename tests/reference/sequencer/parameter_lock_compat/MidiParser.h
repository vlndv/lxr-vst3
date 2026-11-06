#ifndef P16D_COMPAT_MIDI_PARSER_H
#define P16D_COMPAT_MIDI_PARSER_H

#include <stdint.h>

#define MIDI_CC 0xb0
#define MIDI_CC2 0xf4
#define NO_AUTOMATION 0xff

typedef struct MidiMsgStruct
{
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} MidiMsg;

extern uint8_t midiParser_originalCcValues[0xff];
void midiParser_ccHandler(MidiMsg msg, uint8_t updateOriginalValue);

#endif
