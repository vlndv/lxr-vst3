-- P15-M2H: isolated REAPER certification for the private VST3 bundle.
-- Run only in a new REAPER process with the task-owned isolated configuration.

local output_root = [[C:\Projects\lxr-vst3\out\p15-m2h\reaper-isolated-014]]
local project_path = output_root .. [[\p15_m2h_host_controls.rpp]]
local velocity_project_path = output_root .. [[\p15_m2h_velocity_only.rpp]]
local lfo_project_path = output_root .. [[\p15_m2h_lfo_only.rpp]]
local off_project_path = output_root .. [[\p15_m2h_routes_off.rpp]]
local active_before_path = output_root .. [[\active_before.wav]]
local velocity_only_path = output_root .. [[\velocity_only.wav]]
local lfo_only_path = output_root .. [[\lfo_only.wav]]
local routes_off_path = output_root .. [[\routes_off.wav]]
local active_after_path = output_root .. [[\active_after.wav]]
local result_path = output_root .. [[\reaper_results.json]]
local private_plugin = [[C:\Projects\lxr-vst3\out\p15-m2h\private-bundle-002\LXR.vst3\Contents\x86_64-win\LXR.vst3]]
local private_sha256 = [[708EB4C1FEB55598762E1D4F9EE3C113D3C959FBCE325B580EDB8915371D1DD0]]
local sample_rate = 48000
local expected_pdc = 26
local patch_count = 228
local route_first = 228
local route_count = 12
local route_names = {
  "Velocity Destination D1", "Velocity Destination D2",
  "Velocity Destination D3", "Velocity Destination SN",
  "Velocity Destination CY", "Velocity Destination HH",
  "LFO Destination D1", "LFO Destination D2", "LFO Destination D3",
  "LFO Destination SN", "LFO Destination CY", "LFO Destination HH"
}
local route_a = {2, 68, 102, 135, 169, 203, 2, 68, 102, 135, 169, 203}
local route_b = {31, 68, 102, 135, 169, 203, 34, 68, 102, 135, 169, 203}

local function file_exists(path)
  local f = io.open(path, "rb")
  if f then f:close(); return true end
  return false
end

local function require_test(condition, message)
  if not condition then error(message, 2) end
end

local function json_escape(value)
  return value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
end

local function is_array(value)
  local maximum = 0
  for key in pairs(value) do
    if type(key) ~= "number" or key < 1 or key % 1 ~= 0 then return false end
    maximum = math.max(maximum, key)
  end
  for index = 1, maximum do if value[index] == nil then return false end end
  return true, maximum
end

local function json_encode(value, level)
  level = level or 0
  local kind = type(value)
  if kind == "nil" then return "null" end
  if kind == "boolean" then return value and "true" or "false" end
  if kind == "number" then return string.format("%.17g", value) end
  if kind == "string" then return '"' .. json_escape(value) .. '"' end
  local array, count = is_array(value)
  local pad, child = string.rep("  ", level), string.rep("  ", level + 1)
  local parts = {}
  if array then
    for index = 1, count do parts[#parts + 1] = child .. json_encode(value[index], level + 1) end
    return #parts == 0 and "[]" or "[\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "]"
  end
  local keys = {}
  for key in pairs(value) do keys[#keys + 1] = key end
  table.sort(keys)
  for _, key in ipairs(keys) do
    parts[#parts + 1] = child .. '"' .. json_escape(key) .. '": ' .. json_encode(value[key], level + 1)
  end
  return #parts == 0 and "{}" or "{\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "}"
end

local function write_file(path, value)
  local f, err = io.open(path, "wb")
  if not f then error("cannot create " .. path .. ": " .. tostring(err)) end
  f:write(value)
  f:close()
end

local function named_config(track, fx, key)
  local ok, value = reaper.TrackFX_GetNamedConfigParm(track, fx, key)
  return ok, ok and value or nil
end

local function route_token(track, fx, slot)
  return math.floor(reaper.TrackFX_GetParamNormalized(track, fx, route_first + slot) * 65535.0 + 0.5)
end

local function patch_value(track, fx, parameter)
  return math.floor(reaper.TrackFX_GetParamNormalized(track, fx, parameter) * 127.0 + 0.5)
end

local function snapshot(track, fx)
  local result = {patch = {}, routes = {}}
  for parameter = 0, patch_count - 1 do result.patch[parameter + 1] = patch_value(track, fx, parameter) end
  for slot = 0, route_count - 1 do result.routes[slot + 1] = route_token(track, fx, slot) end
  return result
end

local function same_array(a, b)
  if #a ~= #b then return false end
  for index = 1, #a do if a[index] ~= b[index] then return false end end
  return true
end

local function add_note(track, time, note, velocity)
  local item = reaper.CreateNewMIDIItemInProj(track, time, time + 0.35, false)
  require_test(item ~= nil, "failed to create MIDI item")
  local take = reaper.GetActiveTake(item)
  local start_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, time)
  local end_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, time + 0.12)
  require_test(reaper.MIDI_InsertNote(take, false, false, start_ppq, end_ppq, 0, note, velocity, true),
    "failed to insert MIDI note")
  reaper.MIDI_Sort(take)
end

local function add_instance(project, name, routes, patch_edits, note_time)
  reaper.InsertTrackAtIndex(reaper.CountTracks(project), true)
  local track = reaper.GetTrack(project, reaper.CountTracks(project) - 1)
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", name, true)
  local fx = reaper.TrackFX_AddByName(track, "VST3i: LXR (Sonic Potions)", false, -1)
  if fx < 0 then fx = reaper.TrackFX_AddByName(track, "LXR (Sonic Potions)", false, -1) end
  require_test(fx >= 0, "REAPER could not instantiate isolated LXR VST3i")
  local name_ok, fx_name = reaper.TrackFX_GetFXName(track, fx)
  local type_ok, fx_type = named_config(track, fx, "fx_type")
  local ident_ok, fx_ident = named_config(track, fx, "fx_ident")
  require_test(name_ok and fx_name:find("LXR", 1, true), "wrong FX name")
  require_test(type_ok and fx_type:find("VST3", 1, true), "loaded FX is not VST3")
  require_test(reaper.TrackFX_GetNumParams(track, fx) >= patch_count + route_count,
    "REAPER did not expose all 240 plugin parameters")
  for slot = 0, route_count - 1 do
    local _, parameter_name = reaper.TrackFX_GetParamName(track, fx, route_first + slot)
    require_test(parameter_name == route_names[slot + 1], "route parameter name mismatch at ID " .. (route_first + slot))
  end
  for _, edit in ipairs(patch_edits) do
    require_test(reaper.TrackFX_SetParamNormalized(track, fx, edit[1], edit[2] / 127.0),
      "failed to set patch parameter " .. edit[1])
  end
  for slot = 0, route_count - 1 do
    require_test(reaper.TrackFX_SetParamNormalized(track, fx, route_first + slot, routes[slot + 1] / 65535.0),
      "failed to set route parameter " .. (route_first + slot))
  end
  add_note(track, note_time, 60, 118)
  return {track = track, fx = fx, fx_name = fx_name, fx_type = fx_type,
    fx_ident = ident_ok and fx_ident or nil}
end

local function reacquire(project, first, second)
  require_test(reaper.CountTracks(project) == 2, "saved project did not restore two tracks")
  first.track, first.fx = reaper.GetTrack(project, 0), 0
  second.track, second.fx = reaper.GetTrack(project, 1), 0
  require_test(reaper.TrackFX_GetCount(first.track) == 1 and reaper.TrackFX_GetCount(second.track) == 1,
    "saved project did not restore two plugin instances")
end

local function set_project_string(project, key, value)
  require_test(reaper.GetSetProjectInfo_String(project, key, value, true), "failed to set " .. key)
end

local function render(project, path)
  set_project_string(project, "RENDER_FILE", output_root)
  set_project_string(project, "RENDER_PATTERN", path:match("([^\\]+)%.wav$"))
  set_project_string(project, "RENDER_FORMAT", "evaw")
  set_project_string(project, "RENDER_FORMAT2", "")
  reaper.GetSetProjectInfo(project, "RENDER_SETTINGS", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_BOUNDSFLAG", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_STARTPOS", 0.0, true)
  reaper.GetSetProjectInfo(project, "RENDER_ENDPOS", 1.2, true)
  reaper.GetSetProjectInfo(project, "RENDER_CHANNELS", 2, true)
  reaper.GetSetProjectInfo(project, "RENDER_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "RENDER_ADDTOPROJ", 0, true)
  local targets_ok, targets = reaper.GetSetProjectInfo_String(project, "RENDER_TARGETS", "", false)
  reaper.GetSetProjectInfo_String(project, "RENDER_STATS", "42230", false)
  local actual_path = path
  if not file_exists(actual_path) and targets_ok then
    for target in targets:gmatch("[^;]+") do if file_exists(target) then actual_path = target; break end end
  end
  require_test(file_exists(actual_path), "REAPER did not create render " .. path)
  local f = assert(io.open(actual_path, "rb")); local bytes = f:read("*a"); f:close()
  require_test(#bytes > 44 and bytes:sub(1, 4) == "RIFF", "invalid or empty WAV render")
  return bytes
end

local function data_payload(bytes)
  local position = 13
  while position + 7 <= #bytes do
    local id = bytes:sub(position, position + 3)
    local size = string.unpack("<I4", bytes, position + 4)
    local body = position + 8
    if id == "data" then return bytes:sub(body, body + size - 1) end
    position = body + size + (size % 2)
  end
  error("WAV has no data chunk")
end

local function has_nonzero(bytes)
  for index = 1, #bytes do if bytes:byte(index) ~= 0 then return true end end
  return false
end

local function main()
  reaper.RecursiveCreateDirectory(output_root, 0)
  for _, path in ipairs({project_path, velocity_project_path, lfo_project_path, off_project_path,
      active_before_path, velocity_only_path, lfo_only_path, routes_off_path, active_after_path, result_path}) do
    require_test(not file_exists(path), "refusing to overwrite existing artifact: " .. path)
  end
  local project, filename = reaper.EnumProjects(-1, "")
  require_test(project and filename == "" and reaper.IsProjectDirty(project) == 0 and reaper.CountTracks(project) == 0,
    "safety guard: isolated test requires a new empty project")
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE_USE", 1, true)

  local patch_one = {{0,77},{1,0},{8,57},{9,71},{20,0},{37,72},{43,38},{49,0},{50,49},{62,31},
    {70,0},{74,0},{78,0},{82,0},{83,68},{88,100},{89,0},{90,0},{91,0},{92,0},{93,0},
    {94,64},{102,0},{108,127},{114,127},{115,37},{121,127},{134,1},{137,1},{143,127},{191,1},{197,0}}
  local patch_two = {{1,2},{8,85},{9,21},{20,1},{37,91},{43,16},{49,0},{50,63},{62,12},
    {70,11},{74,31},{78,22},{82,9},{83,44},{88,96},{94,82},{102,9},{108,127},
    {114,127},{115,69},{121,0},{137,1},{191,2}}
  local first = add_instance(project, "P15-M2H private A", route_a, patch_one, 0.20)
  local second = add_instance(project, "P15-M2H private B", route_b, patch_two, 0.65)
  reaper.SetMediaTrackInfo_Value(second.track, "B_MUTE", 1)
  local expected_first, expected_second = snapshot(first.track, first.fx), snapshot(second.track, second.fx)
  require_test(same_array(expected_first.routes, route_a) and same_array(expected_second.routes, route_b),
    "route authority did not accept all twelve host controls")
  require_test(not same_array(expected_first.patch, expected_second.patch), "instances are not independent")

  local invalid_result = reaper.TrackFX_SetParamNormalized(first.track, first.fx, 228, 204 / 65535.0)
  require_test(route_token(first.track, first.fx, 0) == route_a[1], "unsupported row 204 changed accepted route")
  local envelope = reaper.GetFXEnvelope(first.track, first.fx, 229, true)
  require_test(envelope ~= nil, "failed to create route automation envelope")
  require_test(reaper.InsertEnvelopePoint(envelope, 0.0, route_a[2] / 65535.0, 0, 0, false, true),
    "failed to add first automation point")
  require_test(reaper.InsertEnvelopePoint(envelope, 0.15, 0.0, 0, 0, false, true),
    "failed to add second automation point")
  reaper.Envelope_SortPoints(envelope)
  require_test(reaper.CountEnvelopePoints(envelope) == 2, "automation points not retained")
  reaper.SetEditCurPos(0.0, false, false)
  reaper.TrackList_AdjustWindows(false)
  expected_first = snapshot(first.track, first.fx)
  expected_first.routes[1] = route_a[1]

  reaper.TrackFX_SetEnabled(second.track, second.fx, false)
  require_test(not reaper.TrackFX_GetEnabled(second.track, second.fx), "failed to bypass second instance")
  reaper.Main_SaveProjectEx(project, project_path, 0)
  require_test(file_exists(project_path), "project save failed")

  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  local restored_first, restored_second = snapshot(first.track, first.fx), snapshot(second.track, second.fx)
  require_test(same_array(restored_first.patch, expected_first.patch), "228-value patch A did not restore")
  require_test(same_array(restored_second.patch, expected_second.patch), "228-value patch B did not restore")
  require_test(same_array(restored_first.routes, expected_first.routes), "twelve routes A did not restore")
  require_test(same_array(restored_second.routes, expected_second.routes), "twelve routes B did not restore")
  require_test(not reaper.TrackFX_GetEnabled(second.track, second.fx), "bypass did not restore")
  envelope = reaper.GetFXEnvelope(first.track, first.fx, 229, false)
  require_test(envelope and reaper.CountEnvelopePoints(envelope) == 2, "route automation did not restore")

  local before = data_payload(render(project, active_before_path))
  require_test(has_nonzero(before), "active-route render is silent")

  -- Prove the two source classes independently from clean restored projects.
  require_test(reaper.TrackFX_SetParamNormalized(first.track, first.fx, 234, 0.0),
    "failed to switch D1 LFO route OFF")
  reaper.Main_SaveProjectEx(project, velocity_project_path, 0)
  require_test(file_exists(velocity_project_path), "velocity-only project save failed")
  reaper.Main_openProject("noprompt:" .. velocity_project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  require_test(route_token(first.track, first.fx, 0) == route_a[1] and
    route_token(first.track, first.fx, 6) == 0, "velocity-only routes did not restore")
  local velocity_only = data_payload(render(project, velocity_only_path))
  require_test(has_nonzero(velocity_only), "velocity-only render is silent")

  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  require_test(reaper.TrackFX_SetParamNormalized(first.track, first.fx, 228, 0.0),
    "failed to switch D1 velocity route OFF")
  reaper.Main_SaveProjectEx(project, lfo_project_path, 0)
  require_test(file_exists(lfo_project_path), "LFO-only project save failed")
  reaper.Main_openProject("noprompt:" .. lfo_project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  require_test(route_token(first.track, first.fx, 0) == 0 and
    route_token(first.track, first.fx, 6) == route_a[7], "LFO-only routes did not restore")
  local lfo_only = data_payload(render(project, lfo_only_path))
  require_test(has_nonzero(lfo_only), "LFO-only render is silent")

  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  for slot = 0, route_count - 1 do
    require_test(reaper.TrackFX_SetParamNormalized(first.track, first.fx, route_first + slot, 0.0),
      "failed to switch route OFF")
  end
  reaper.Main_SaveProjectEx(project, off_project_path, 0)
  require_test(file_exists(off_project_path), "OFF-route project save failed")
  reaper.Main_openProject("noprompt:" .. off_project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  local off_snapshot = snapshot(first.track, first.fx)
  for slot = 1, route_count do
    local expected = slot == 2 and route_a[2] or 0
    require_test(off_snapshot.routes[slot] == expected, "OFF route did not restore")
  end
  local off = data_payload(render(project, routes_off_path))
  require_test(has_nonzero(off), "OFF-route render is silent")
  require_test(before ~= off, "active and OFF route audio are identical")
  require_test(velocity_only ~= off, "velocity-only and OFF route audio are identical")
  require_test(lfo_only ~= off, "LFO-only and OFF route audio are identical")

  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire(project, first, second)
  local after = data_payload(render(project, active_after_path))
  require_test(before == after, "pre/post-reopen PCM is not bit-identical")
  for repetition = 1, 2 do reaper.Main_OnCommand(1007, 0); reaper.Main_OnCommand(1016, 0) end
  local pdc_one_ok, pdc_one = named_config(first.track, first.fx, "pdc")
  local pdc_two_ok, pdc_two = named_config(second.track, second.fx, "pdc")
  require_test(pdc_one_ok and pdc_two_ok and tonumber(pdc_one) == expected_pdc and tonumber(pdc_two) == expected_pdc,
    "REAPER PDC is not 26 samples")
  reaper.Main_SaveProjectEx(project, project_path, 0)

  local result = {
    schema = "lxr-p15-m2h-reaper-v1", overall = "PASS",
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"), reaper_version = reaper.GetAppVersion(),
    private_plugin = {path = private_plugin, sha256 = private_sha256,
      instance_one = {name = first.fx_name, type = first.fx_type, identifier = first.fx_ident},
      instance_two = {name = second.fx_name, type = second.fx_type, identifier = second.fx_ident}},
    project = {path = project_path, sample_rate = sample_rate, instances = 2},
    parameters = {total = 240, original = 228, route_ids = "228-239", names = route_names},
    routing = {all_twelve_set = true, invalid_204_rejected = true,
      invalid_set_api_result = invalid_result, controller_readback = "PASS", multi_instance = "PASS"},
    automation = {status = "PASS", points = 2, restored = true},
    persistence = {lxrs_v2 = "PASS", patch_values_checked = 456, routes_checked = 24,
      bypass_restored = true, save_reopen = "PASS"},
    audio = {active_vs_off = "PASS", velocity_vs_off = "PASS", lfo_vs_off = "PASS",
      deterministic_reopen_pcm = "PASS", active_bytes = #before,
      velocity_only_bytes = #velocity_only, lfo_only_bytes = #lfo_only, off_bytes = #off},
    transport = {status = "PASS", cycles = 2},
    pdc = {status = "PASS", expected = expected_pdc, instance_one = tonumber(pdc_one), instance_two = tonumber(pdc_two)},
    errors = {}
  }
  write_file(result_path, json_encode(result) .. "\n")
  reaper.ShowConsoleMsg("P15-M2H isolated REAPER certification PASS\n" .. result_path .. "\n")
end

local ok, err = xpcall(main, debug.traceback)
if not ok then
  reaper.RecursiveCreateDirectory(output_root, 0)
  local result = {schema = "lxr-p15-m2h-reaper-v1", overall = "FAIL",
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"), reaper_version = reaper.GetAppVersion(),
    private_plugin = {path = private_plugin, sha256 = private_sha256}, errors = {tostring(err)}}
  if not file_exists(result_path) then write_file(result_path, json_encode(result) .. "\n") end
  reaper.ShowConsoleMsg("P15-M2H isolated REAPER certification FAIL\n" .. tostring(err) .. "\n")
end
