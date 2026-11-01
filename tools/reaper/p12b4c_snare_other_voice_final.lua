local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- Final installed-binary REAPER regression for an unaffected D1 voice.
P12B4C_CASE_TAG = "snare_repair_final_d1_48000"
P12B4C_MIDI_CHANNEL = 0 -- MIDI channel 1 / D1.
P12B4C_NOTE_TIMES = {0.5}
P12B4C_RENDER_END = 1.4
dofile(paths.join(paths.directory, "p12b4c_pdc_test.lua"))
