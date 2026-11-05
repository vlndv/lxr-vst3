#pragma once

// Accepted P16-B core plus the P16-C fixed-capacity native trigger scheduler.
// Host transport, persistence and parameter-lock application remain outside.
#include "../dsp/SequencerTypes.h"
#include "../dsp/SequencerRng.h"
#include "../dsp/PatternBank.h"
#include "../dsp/SequencerClock.h"
#include "../dsp/PatternStateAuthority.h"
#include "../dsp/TriggerScheduler.h"
