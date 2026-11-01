local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- Final installed-binary 48 kHz REAPER render for the restored snare pipeline.
P12B4C_CASE_TAG = "snare_repair_final_48000"
P12B4C_MIDI_CHANNEL = 3 -- REAPER/Lua is zero-based: MIDI channel 4.
P12B4C_NOTE_TIMES = {0.5, 1.0, 1.5}
P12B4C_RENDER_END = 2.4
dofile(paths.join(paths.directory, "p12b4c_pdc_test.lua"))
