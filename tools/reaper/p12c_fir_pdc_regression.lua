local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- P12c post-build regression against the exact installed state-enabled binary.
P12B4C_CASE_TAG = "p12c_fir_pdc_final_48000"
P12B4C_MIDI_CHANNEL = 0
P12B4C_NOTE_TIMES = {1.0}
P12B4C_RENDER_END = 1.8
P12B4C_PLUGIN_SHA256 = "617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F"
dofile(paths.join(paths.directory, "p12b4c_pdc_test.lua"))
