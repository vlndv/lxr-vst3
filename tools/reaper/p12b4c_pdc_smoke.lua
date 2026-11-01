-- Runs the shared harness as the Stage C 48 kHz smoke test.
P12B4C_SMOKE_ONLY = true
local source = debug.getinfo(1, "S").source:sub(2)
local directory = source:match("^(.*[\\/])") or ""
dofile(directory .. "p12b4c_pdc_test.lua")
