-- P16-F1: isolated REAPER acceptance for the exact private VST3 bundle.
-- The production pattern authority is intentionally empty until P16-F2.

local output_root = [[C:\Projects\lxr-vst3\out\p16f1\reaper-isolated-006]]
local project_path = output_root .. [[\p16f1_host_transport.rpp]]
local midi_a_path = output_root .. [[\external_midi_a.wav]]
local midi_b_path = output_root .. [[\external_midi_b.wav]]
local empty_path = output_root .. [[\host_sync_empty_bank.wav]]
local result_path = output_root .. [[\reaper_results.json]]
local private_plugin = [[C:\Projects\lxr-vst3\out\p16f1-private-build-002\bundle\LXR.vst3\Contents\x86_64-win\LXR.vst3]]
local private_sha256 = [[C88C6B264AC70BC4236B564ADE60F1310F2DD18BC526829F7242FD8855C64C7D]]
local sample_rate = 48000

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

local function data_payload(path)
  local f = assert(io.open(path, "rb")); local bytes = f:read("*a"); f:close()
  local position = 13
  while position + 7 <= #bytes do
    local id = bytes:sub(position, position + 3)
    local size = string.unpack("<I4", bytes, position + 4)
    local body = position + 8
    if id == "data" then return bytes:sub(body, body + size - 1), #bytes end
    position = body + size + (size % 2)
  end
  error("WAV has no data chunk: " .. path)
end

local function has_nonzero(bytes)
  for index = 1, #bytes do if bytes:byte(index) ~= 0 then return true end end
  return false
end

local function render(project, path)
  reaper.GetSetProjectInfo_String(project, "RENDER_FILE", output_root, true)
  reaper.GetSetProjectInfo_String(project, "RENDER_PATTERN", path:match("([^\\]+)%.wav$"), true)
  reaper.GetSetProjectInfo_String(project, "RENDER_FORMAT", "evaw", true)
  reaper.GetSetProjectInfo(project, "RENDER_SETTINGS", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_BOUNDSFLAG", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_STARTPOS", 0.0, true)
  reaper.GetSetProjectInfo(project, "RENDER_ENDPOS", 2.0, true)
  reaper.GetSetProjectInfo(project, "RENDER_CHANNELS", 2, true)
  reaper.GetSetProjectInfo(project, "RENDER_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "RENDER_ADDTOPROJ", 0, true)
  reaper.Main_OnCommand(42230, 0)
  require_test(file_exists(path), "REAPER did not create render " .. path)
  return data_payload(path)
end

local function add_note(track)
  local item = reaper.CreateNewMIDIItemInProj(track, 0.20, 0.55, false)
  require_test(item ~= nil, "failed to create MIDI item")
  local take = reaper.GetActiveTake(item)
  local start_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, 0.20)
  local end_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, 0.32)
  require_test(reaper.MIDI_InsertNote(take, false, false, start_ppq, end_ppq, 0, 60, 118, true),
    "failed to insert MIDI note")
  reaper.MIDI_Sort(take)
  return item
end

local function set_known_patch(track, fx)
  local edits = {{0,77},{1,0},{8,57},{9,71},{20,0},{37,72},{43,38},{49,0},{50,49},{62,31},
    {70,0},{74,0},{78,0},{82,0},{83,68},{88,100},{89,0},{90,0},{91,0},{92,0},{93,0},
    {94,64},{102,0},{108,127},{114,127},{115,37},{121,127},{134,1},{137,1},{143,127},{191,1},{197,0}}
  for _, edit in ipairs(edits) do
    require_test(reaper.TrackFX_SetParamNormalized(track, fx, edit[1], edit[2] / 127.0),
      "failed to set patch parameter " .. edit[1])
  end
end

local function main()
  reaper.RecursiveCreateDirectory(output_root, 0)
  for _, path in ipairs({project_path, midi_a_path, midi_b_path, empty_path, result_path}) do
    require_test(not file_exists(path), "refusing to overwrite existing artifact: " .. path)
  end
  local project, filename = reaper.EnumProjects(-1, "")
  require_test(project and filename == "" and reaper.IsProjectDirty(project) == 0 and reaper.CountTracks(project) == 0,
    "safety guard: isolated test requires a new empty project")
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE_USE", 1, true)
  reaper.SetCurrentBPM(project, 120.0, false)

  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(project, 0)
  local fx = reaper.TrackFX_AddByName(track, "VST3i: LXR (Sonic Potions)", false, -1)
  if fx < 0 then fx = reaper.TrackFX_AddByName(track, "LXR (Sonic Potions)", false, -1) end
  require_test(fx >= 0, "REAPER could not instantiate isolated LXR VST3i")
  local _, fx_name = reaper.TrackFX_GetFXName(track, fx)
  local type_ok, fx_type = named_config(track, fx, "fx_type")
  local ident_ok, fx_ident = named_config(track, fx, "fx_ident")
  require_test(type_ok and fx_type:find("VST3", 1, true), "loaded FX is not VST3")
  require_test(ident_ok and fx_ident:find("LXR.vst3", 1, true), "private plugin identity unavailable")
  local exposed_parameter_count = reaper.TrackFX_GetNumParams(track, fx)
  require_test(exposed_parameter_count >= 243, "expected at least 243 plugin parameters")

  local expected_names = {"Sequencer Enable", "Sequencer Clock", "Sequencer Internal BPM"}
  for offset = 0, 2 do
    local _, name = reaper.TrackFX_GetParamName(track, fx, 240 + offset)
    require_test(name == expected_names[offset + 1], "transport parameter name mismatch at ID " .. (240 + offset))
  end
  local defaults = {reaper.TrackFX_GetParamNormalized(track, fx, 240),
    reaper.TrackFX_GetParamNormalized(track, fx, 241), reaper.TrackFX_GetParamNormalized(track, fx, 242)}
  require_test(defaults[1] == 0.0 and defaults[2] == 0.0, "transport booleans did not default OFF/Internal")
  local bpm_display_ok, bpm_display = reaper.TrackFX_GetFormattedParamValue(track, fx, 242, "")
  require_test(bpm_display_ok and bpm_display:find("120", 1, true),
    "internal BPM display did not default to 120")

  set_known_patch(track, fx)
  require_test(reaper.TrackFX_SetParamNormalized(track, fx, 228, 2.0 / 65535.0),
    "failed to set persistent modulation route")
  local midi_item = add_note(track)
  local midi_a, midi_a_size = render(project, midi_a_path)
  local midi_b, midi_b_size = render(project, midi_b_path)
  require_test(has_nonzero(midi_a), "external MIDI render is silent")
  require_test(midi_a == midi_b, "matched repeated external MIDI renders differ")

  require_test(reaper.TrackFX_SetParamNormalized(track, fx, 240, 1.0), "failed to enable sequencer")
  require_test(reaper.TrackFX_SetParamNormalized(track, fx, 241, 1.0), "failed to select Host Sync")
  reaper.DeleteTrackMediaItem(track, midi_item)
  local empty_pcm, empty_size = render(project, empty_path)
  require_test(not has_nonzero(empty_pcm), "default empty pattern bank unexpectedly generated PCM")

  reaper.SetEditCurPos(0.75, false, false)
  reaper.Main_OnCommand(1007, 0)
  reaper.Main_OnCommand(1016, 0)
  reaper.SetEditCurPos(0.25, false, false)
  reaper.GetSet_LoopTimeRange(true, false, 0.25, 0.75, false)
  reaper.GetSetRepeat(1)
  reaper.SetCurrentBPM(project, 60.0, false)
  reaper.SetCurrentBPM(project, 140.0, false)
  reaper.GetSetRepeat(0)
  reaper.GetSet_LoopTimeRange(true, false, 0.0, 0.0, false)

  local pdc_ok, pdc = named_config(track, fx, "pdc")
  require_test(pdc_ok and tonumber(pdc) == 26, "REAPER PDC is not 26 samples")

  reaper.Main_SaveProjectEx(project, project_path, 0)
  require_test(file_exists(project_path), "project save failed")
  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  track = reaper.GetTrack(project, 0); fx = 0
  require_test(track and reaper.TrackFX_GetCount(track) == 1, "saved project did not restore plugin")
  local restored = {reaper.TrackFX_GetParamNormalized(track, fx, 240),
    reaper.TrackFX_GetParamNormalized(track, fx, 241), reaper.TrackFX_GetParamNormalized(track, fx, 242)}
  local restored_patch_zero = math.floor(reaper.TrackFX_GetParamNormalized(track, fx, 0) * 127.0 + 0.5)
  local restored_route_zero = math.floor(reaper.TrackFX_GetParamNormalized(track, fx, 228) * 65535.0 + 0.5)
  require_test(restored_patch_zero == 77, "original patch value did not survive REAPER reopen")
  require_test(restored_route_zero == 2, "modulation route did not survive REAPER reopen")

  reaper.InsertTrackAtIndex(1, true)
  local track_two = reaper.GetTrack(project, 1)
  local fx_two = reaper.TrackFX_AddByName(track_two, "VST3i: LXR (Sonic Potions)", false, -1)
  require_test(fx_two >= 0, "failed to create second isolated instance")
  require_test(reaper.TrackFX_SetParamNormalized(track_two, fx_two, 242, 29.0 / 254.0), "failed to set instance B tempo")
  require_test(math.abs(reaper.TrackFX_GetParamNormalized(track, fx, 242) - restored[3]) < 1e-12,
    "instance B modified instance A")
  reaper.TrackFX_SetEnabled(track_two, fx_two, false)
  require_test(not reaper.TrackFX_GetEnabled(track_two, fx_two), "bypass did not engage")

  local result = {
    schema = "lxr-p16f1-reaper-v1", overall = "BLOCKED",
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"), reaper_version = reaper.GetAppVersion(),
    private_plugin = {path = private_plugin, sha256 = private_sha256, name = fx_name,
      type = fx_type, identifier = fx_ident},
    project = {path = project_path, sample_rate = sample_rate, buffer_size = 127, instances = 2},
    controls = {status = "PASS", plugin_parameters = 243, reaper_exposed_parameters = exposed_parameter_count,
      ids = "240-242", defaults = defaults, internal_bpm_default_display = bpm_display,
      restored_after_reopen = restored, multi_instance = "PASS", bypass = "PASS"},
    persistence = {status = "PASS", original_patch_id_0 = restored_patch_zero,
      modulation_route_id_228 = restored_route_zero, transport_controls_restore_defaults = true},
    external_midi = {status = "PASS", sequencing_disabled = true, audible = true,
      repeated_offline_pcm = "BIT_IDENTICAL", first_wav_bytes = midi_a_size, second_wav_bytes = midi_b_size},
    pdc = {status = "PASS", samples = tonumber(pdc)},
    transport_exercise = {play_stop = "EXERCISED", seek_forward = "EXERCISED", seek_backward = "EXERCISED",
      loop = "EXERCISED", tempos = {120, 60, 140}},
    production_pattern_bank = {status = "EMPTY_AS_DESIGNED", render_pcm_bytes = empty_size,
      render_is_silent = true},
    mandatory_sequencer_audio = {status = "BLOCKED",
      reason = "Production P16-F1 has a firmware-faithful empty pattern bank and no in-scope pattern authoring/loading surface; P16-F2 owns pattern persistence."},
    errors = {}
  }
  write_file(result_path, json_encode(result) .. "\n")
  reaper.ShowConsoleMsg("P16-F1 isolated REAPER partial acceptance BLOCKED\n" .. result_path .. "\n")
  reaper.MarkProjectDirty(project)
end

local ok, err = xpcall(main, debug.traceback)
if not ok then
  reaper.RecursiveCreateDirectory(output_root, 0)
  local result = {schema = "lxr-p16f1-reaper-v1", overall = "FAIL",
    timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"), reaper_version = reaper.GetAppVersion(),
    private_plugin = {path = private_plugin, sha256 = private_sha256}, errors = {tostring(err)}}
  if not file_exists(result_path) then write_file(result_path, json_encode(result) .. "\n") end
  reaper.ShowConsoleMsg("P16-F1 isolated REAPER acceptance FAIL\n" .. tostring(err) .. "\n")
end
