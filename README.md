# lxr-vst3

Unofficial, non-commercial hobby port of the Sonic Potions LXR drum synth (firmware 0.37, `github.com/SonicPotions/LXR`, commit `dee4968`) to a VST3 plugin.
Not affiliated with or endorsed by Sonic Potions.

Work in progress: filter, parameter mappings, oscillators and envelopes are ported and tested against the original C code. See `docs/phase_log.md` for the current phase and `docs/architecture.md` for how the original engine works.

## Licence and origin

This repository contains code and data derived from the LXR firmware, Copyright 2013 Julian Schmidt (julian@sonic-potions.com, www.sonic-potions.com).
The original licence applies to everything derived from it, including the exported data tables in `data/`. Its terms, in short: no sale and no commercial use or activity;
modified redistributions must include the complete source; the copyright notice, the conditions and the disclaimer must be reproduced. The full text is in `LICENSE-LXR.txt`.
This project may therefore only be used and shared free of charge and with its source.
