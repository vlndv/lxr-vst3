local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- P12c-R1 wrapper: reuse the accepted P12c harness with unique, non-overwriting
-- artifacts and the exact concurrency-hardened installed binary hash.
local source_path = paths.join(paths.directory, "p12c_state_test.lua")
local source_file, open_error = io.open(source_path, "rb")
if not source_file then error("cannot read base harness: " .. tostring(open_error)) end
local source = source_file:read("*a")
source_file:close()

source = source:gsub("_final", "_r1_b170e009")
source = source:gsub(
  "617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F",
  "B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953")

local harness, load_error = load(source, "@" .. source_path)
if not harness then error("cannot load base harness: " .. tostring(load_error)) end
harness()
