local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- Reload and repeated-transport probe for the final rebuilt snare project.
local project_path = paths.project("snare_repair_final_48000.rpp")
local output_path = paths.join(paths.output_root("reaper-p12b4c"), "snare_repair_final_transport_reload.json")

local function escape(value)
  return value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r')
end
local function write(contents)
  local existing = io.open(output_path, "rb")
  if existing then existing:close(); error("refusing to overwrite " .. output_path) end
  local file, err = io.open(output_path, "wb")
  if not file then error(tostring(err)) end
  file:write(contents); file:close()
end

local function main()
  local proj, filename = reaper.EnumProjects(-1, "")
  if not proj or filename:lower() ~= project_path:lower() then error("probe requires the saved isolated final snare project") end
  local tracks = reaper.CountTracks(proj)
  local loaded, names, pdc = 0, {}, {}
  for i = 0, tracks - 1 do
    local track = reaper.GetTrack(proj, i)
    for fx = 0, reaper.TrackFX_GetCount(track) - 1 do
      local ok_name, name = reaper.TrackFX_GetFXName(track, fx)
      local ok_pdc, value = reaper.TrackFX_GetNamedConfigParm(track, fx, "pdc")
      if ok_name and name:find("LXR", 1, true) then
        loaded = loaded + 1; names[#names + 1] = name; pdc[#pdc + 1] = ok_pdc and tonumber(value) or -1
      end
    end
  end
  local cycles = {}
  for i = 1, 2 do
    reaper.SetEditCurPos(0.45, false, false)
    reaper.OnPlayButton(); local started = reaper.GetPlayState()
    reaper.OnStopButton(); local stopped = reaper.GetPlayState()
    cycles[#cycles + 1] = { start_state = started, stop_state = stopped }
  end
  local pass = tracks == 2 and loaded == 2 and reaper.IsProjectDirty(proj) == 0
  for _, value in ipairs(pdc) do pass = pass and value == 26 end
  for _, cycle in ipairs(cycles) do pass = pass and (cycle.start_state & 1) == 1 and cycle.stop_state == 0 end
  local name_json, pdc_json, cycle_json = {}, {}, {}
  for _, value in ipairs(names) do name_json[#name_json + 1] = '"' .. escape(value) .. '"' end
  for _, value in ipairs(pdc) do pdc_json[#pdc_json + 1] = tostring(value) end
  for _, cycle in ipairs(cycles) do cycle_json[#cycle_json + 1] = string.format('{"start_state":%d,"stop_state":%d}', cycle.start_state, cycle.stop_state) end
  write(string.format(
    '{\n  "status":"%s",\n  "reaper_version":"%s",\n  "project":"%s",\n  "project_dirty":%d,\n  "track_count":%d,\n  "lxr_instances_reloaded":%d,\n  "fx_names":[%s],\n  "pdc_values_after_reload":[%s],\n  "transport_cycles":[%s]\n}\n',
    pass and "PASS" or "FAIL", escape(reaper.GetAppVersion()), escape(filename), reaper.IsProjectDirty(proj),
    tracks, loaded, table.concat(name_json, ","), table.concat(pdc_json, ","), table.concat(cycle_json, ",")))
end

local ok, err = xpcall(main, debug.traceback)
if not ok then pcall(write, '{"status":"BLOCKED","error":"' .. escape(tostring(err)) .. '"}\n') end
