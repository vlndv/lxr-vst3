#ifndef P16C_ORIGINAL_TRIGGER_SCHEDULER_ADAPTER_H
#define P16C_ORIGINAL_TRIGGER_SCHEDULER_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
enum { P16C_TRACKS = 7, P16C_POSITIONS = 128, P16C_STEP_BYTES = 7 };

typedef struct P16CReferenceState {
    int16_t positions[P16C_TRACKS];
    uint8_t random_latches[P16C_TRACKS];
    uint32_t rng_state;
    uint64_t rng_calls;
} P16CReferenceState;

typedef struct P16CReferenceEvent {
    uint8_t track;
    uint8_t owner;
    uint8_t articulation;
    uint8_t position;
    uint8_t main_step;
    uint8_t substep;
    uint8_t velocity;
    uint8_t note;
    uint8_t random_value;
} P16CReferenceEvent;

void p16c_reference_init(P16CReferenceState* state,
                         const uint8_t rotations[P16C_TRACKS],
                         const uint8_t length_codes[P16C_TRACKS],
                         uint32_t seed);

size_t p16c_reference_tick(
    P16CReferenceState* state,
    const uint8_t steps[P16C_TRACKS][P16C_POSITIONS][P16C_STEP_BYTES],
    const uint16_t gates[P16C_TRACKS],
    const uint8_t length_codes[P16C_TRACKS],
    P16CReferenceEvent* events,
    size_t capacity,
    size_t* rejected);

#ifdef __cplusplus
}
#endif

#endif
