#include "original_parameter_lock_adapter.h"

#include "automationNode.h"

#include <string.h>

uint8_t midiParser_originalCcValues[0xff];

static AutomationNode nodes[P16D_ORIGINAL_TRACKS][P16D_ORIGINAL_SLOTS];
static uint8_t effective[P16D_ORIGINAL_PARAMETERS];
static P16DOriginalWrite writes[P16D_ORIGINAL_MAX_WRITES];
static size_t write_count;

void midiParser_ccHandler(MidiMsg msg, uint8_t updateOriginalValue)
{
    uint16_t parameter;
    if (msg.status == MIDI_CC)
    {
        if (msg.data1 == 0u)
            return; /* PAR_NONE underflow reaches no setter in the original. */
        parameter = (uint16_t)(msg.data1 - 1u);
    }
    else
    {
        parameter = (uint16_t)(msg.data1 + 128u);
    }
    if (parameter >= P16D_ORIGINAL_PARAMETERS)
        return;
    if (updateOriginalValue)
        midiParser_originalCcValues[msg.status == MIDI_CC ? msg.data1 : parameter] = msg.data2;
    effective[parameter] = msg.data2;
    if (write_count < P16D_ORIGINAL_MAX_WRITES)
    {
        writes[write_count].parameter = parameter;
        writes[write_count].value = msg.data2;
        writes[write_count].status = msg.status;
        ++write_count;
    }
}

void p16d_original_init(const uint8_t base[P16D_ORIGINAL_PARAMETERS])
{
    memset(midiParser_originalCcValues, 0, sizeof(midiParser_originalCcValues));
    memcpy(effective, base, sizeof(effective));
    for (uint16_t parameter = 1u; parameter < P16D_ORIGINAL_PARAMETERS; ++parameter)
    {
        const uint16_t selector = parameter < 128u ? parameter + 1u : parameter;
        midiParser_originalCcValues[selector] = base[parameter];
    }
    for (uint8_t track = 0u; track < P16D_ORIGINAL_TRACKS; ++track)
        for (uint8_t slot = 0u; slot < P16D_ORIGINAL_SLOTS; ++slot)
            autoNode_init(&nodes[track][slot]);
    write_count = 0u;
}

void p16d_original_update_base(uint16_t parameter, uint8_t value)
{
    if (parameter >= P16D_ORIGINAL_PARAMETERS)
        return;
    const uint16_t selector = parameter < 128u ? parameter + 1u : parameter;
    midiParser_originalCcValues[selector] = value;
    effective[parameter] = value;
}

void p16d_original_pair(uint8_t track,
                         uint8_t parameter1, uint8_t value1,
                         uint8_t parameter2, uint8_t value2)
{
    if (track >= P16D_ORIGINAL_TRACKS)
        return;
    write_count = 0u;
    autoNode_setDestination(&nodes[track][0], parameter1);
    autoNode_setDestination(&nodes[track][1], parameter2);
    autoNode_updateValue(&nodes[track][0], value1);
    autoNode_updateValue(&nodes[track][1], value2);
}

uint8_t p16d_original_effective(uint16_t parameter)
{
    return parameter < P16D_ORIGINAL_PARAMETERS ? effective[parameter] : 0u;
}

uint16_t p16d_original_destination(uint8_t track, uint8_t slot)
{
    return track < P16D_ORIGINAL_TRACKS && slot < P16D_ORIGINAL_SLOTS
        ? nodes[track][slot].destination : NO_AUTOMATION;
}

size_t p16d_original_write_count(void) { return write_count; }

P16DOriginalWrite p16d_original_write(size_t index)
{
    P16DOriginalWrite empty = {0u, 0u, 0u};
    return index < write_count ? writes[index] : empty;
}
