local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- P12c: isolated REAPER save/reopen and deterministic LXR VST3 state test.
-- Launch only in a new REAPER instance/project. The harness refuses to overwrite
-- any project, render, or report artifact.

local repo_root = paths.repo_root
local output_root = paths.output_root("reaper-p12c")
local project_path = output_root .. "\\p12c_state_roundtrip_final.rpp"
local before_path = output_root .. "\\p12c_before_reload_final.wav"
local after_path = output_root .. "\\p12c_after_reload_final.wav"
local json_path = output_root .. "\\p12c_results_final.json"
local markdown_path = output_root .. "\\p12c_report_final.md"
local installed_vst3 = paths.plugin()
local installed_sha256 = os.getenv("LXR_EXPECTED_PLUGIN_SHA256") or "617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F"
local repository_head = "280279debe94cb26810ea89b89f36b9bf965a747"
local sample_rate = 48000
local expected_pdc = 26
local parameter_count = 228
local active_parameter_count = 227

local function file_exists(path)
  local f = io.open(path, "rb")
  if f then f:close(); return true end
  return false
end

local function json_escape(value)
  return value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\b', '\\b')
    :gsub('\f', '\\f'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
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
  if kind == "number" then
    if value ~= value or value == math.huge or value == -math.huge then return "null" end
    return string.format("%.17g", value)
  end
  if kind == "string" then return '"' .. json_escape(value) .. '"' end
  if kind ~= "table" then return '"' .. json_escape(tostring(value)) .. '"' end
  local array, count = is_array(value)
  local pad, child = string.rep("  ", level), string.rep("  ", level + 1)
  local parts = {}
  if array then
    for index = 1, count do parts[#parts + 1] = child .. json_encode(value[index], level + 1) end
    return #parts == 0 and "[]" or "[\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "]"
  end
  local keys = {}
  for key in pairs(value) do keys[#keys + 1] = key end
  table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
  for _, key in ipairs(keys) do
    parts[#parts + 1] = child .. '"' .. json_escape(tostring(key)) .. '": ' .. json_encode(value[key], level + 1)
  end
  return #parts == 0 and "{}" or "{\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "}"
end

local function write_file(path, contents)
  local f, err = io.open(path, "wb")
  if not f then error("cannot create " .. path .. ": " .. tostring(err)) end
  f:write(contents)
  f:close()
end

local function require_test(condition, message)
  if not condition then error(message, 2) end
end

local function named_config(track, fx, key)
  local ok, value = reaper.TrackFX_GetNamedConfigParm(track, fx, key)
  return ok, ok and value or nil
end

local function set_project_string(project, key, value)
  require_test(reaper.GetSetProjectInfo_String(project, key, value, true), "failed to set " .. key)
end

local function i24le(data, position)
  local a, b, c = data:byte(position, position + 2)
  local value = a + b * 256 + c * 65536
  if value >= 0x800000 then value = value - 0x1000000 end
  return value
end

local function read_wav(path)
  local f, err = io.open(path, "rb")
  if not f then error("render file missing: " .. path .. " (" .. tostring(err) .. ")") end
  local bytes = f:read("*a")
  f:close()
  require_test(#bytes >= 44 and bytes:sub(1, 4) == "RIFF" and bytes:sub(9, 12) == "WAVE",
    "invalid WAV container: " .. path)
  local position, format, data_position, data_size = 13, nil, nil, nil
  while position + 7 <= #bytes do
    local id = bytes:sub(position, position + 3)
    local size = string.unpack("<I4", bytes, position + 4)
    local body = position + 8
    if id == "fmt " then
      local encoding, channels, rate, _, block_align, bits = string.unpack("<I2I2I4I4I2I2", bytes, body)
      if encoding == 0xFFFE and size >= 40 then encoding = string.unpack("<I2", bytes, body + 24) end
      format = { encoding = encoding, channels = channels, rate = rate, block_align = block_align, bits = bits }
    elseif id == "data" then
      data_position, data_size = body, math.min(size, #bytes - body + 1)
      break
    end
    position = body + size + (size % 2)
  end
  require_test(format and data_position, "WAV lacks fmt or data chunk: " .. path)
  local bytes_per_sample = format.bits // 8
  require_test(format.block_align == bytes_per_sample * format.channels, "unsupported WAV layout")
  local frames = data_size // format.block_align
  local samples, peak, sumsq, finite = {}, 0.0, 0.0, true
  position = data_position
  for frame = 1, frames do
    for channel = 1, format.channels do
      local sample
      if format.encoding == 1 and format.bits == 16 then
        sample = string.unpack("<i2", bytes, position) / 32768.0
      elseif format.encoding == 1 and format.bits == 24 then
        sample = i24le(bytes, position) / 8388608.0
      elseif format.encoding == 1 and format.bits == 32 then
        sample = string.unpack("<i4", bytes, position) / 2147483648.0
      elseif format.encoding == 3 and format.bits == 32 then
        sample = string.unpack("<f", bytes, position)
      elseif format.encoding == 3 and format.bits == 64 then
        sample = string.unpack("<d", bytes, position)
      else
        error(string.format("unsupported WAV encoding=%d bits=%d", format.encoding, format.bits))
      end
      position = position + bytes_per_sample
      samples[#samples + 1] = sample
      finite = finite and sample == sample and sample ~= math.huge and sample ~= -math.huge
      peak = math.max(peak, math.abs(sample))
      sumsq = sumsq + sample * sample
    end
  end
  return { path = path, sample_rate = format.rate, channels = format.channels, frames = frames,
    samples = samples, peak = peak, rms = math.sqrt(sumsq / math.max(1, #samples)),
    finite = finite, nonzero = peak > 1.0e-7 }
end

local function add_lxr_track(project, name, edits, note_time)
  reaper.InsertTrackAtIndex(reaper.CountTracks(project), true)
  local track = reaper.GetTrack(project, reaper.CountTracks(project) - 1)
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", name, true)
  local fx = reaper.TrackFX_AddByName(track, "VST3i: LXR (Sonic Potions)", false, -1)
  if fx < 0 then fx = reaper.TrackFX_AddByName(track, "LXR (Sonic Potions)", false, -1) end
  require_test(fx >= 0, "REAPER could not instantiate installed LXR VST3i")
  local name_ok, fx_name = reaper.TrackFX_GetFXName(track, fx)
  local type_ok, fx_type = named_config(track, fx, "fx_type")
  local ident_ok, fx_ident = named_config(track, fx, "fx_ident")
  require_test(name_ok and fx_name:find("LXR", 1, true), "wrong FX name: " .. tostring(fx_name))
  require_test(type_ok and fx_type:find("VST3", 1, true), "loaded FX is not VST3")
  require_test(reaper.TrackFX_GetNumParams(track, fx) >= parameter_count, "native parameter range is incomplete")
  for _, edit in ipairs(edits) do
    require_test(reaper.TrackFX_SetParamNormalized(track, fx, edit[1], edit[2] / 127.0),
      "failed to set parameter " .. tostring(edit[1]))
  end
  local item = reaper.CreateNewMIDIItemInProj(track, note_time, note_time + 0.3, false)
  require_test(item ~= nil, "failed to create MIDI item")
  local take = reaper.GetActiveTake(item)
  local start_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, note_time)
  local end_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, note_time + 0.1)
  require_test(reaper.MIDI_InsertNote(take, false, false, start_ppq, end_ppq, 0, 60, 110, true),
    "failed to insert channel-1 D1 note")
  reaper.MIDI_Sort(take)
  local count_ok, notes, ccs, text = reaper.MIDI_CountEvts(take)
  require_test(count_ok and notes == 1, "unexpected MIDI event count")
  return { track = track, fx = fx, fx_name = fx_name, fx_type = fx_type,
    fx_ident = ident_ok and fx_ident or nil, midi_events = notes * 2 + ccs + text }
end

local function snapshot(track, fx)
  local values = {}
  for parameter = 0, parameter_count - 1 do
    values[parameter + 1] = math.floor(reaper.TrackFX_GetParamNormalized(track, fx, parameter) * 127.0 + 0.5)
  end
  return values
end

local function compare_parameters(expected, actual, instance_index)
  local mismatches = {}
  for parameter = 0, parameter_count - 1 do
    if expected[parameter + 1] ~= actual[parameter + 1] then
      mismatches[#mismatches + 1] = { instance = instance_index, parameter = parameter,
        expected = expected[parameter + 1], actual = actual[parameter + 1] }
    end
  end
  return mismatches
end

local function reacquire_instances(project, first, second)
  require_test(project and reaper.CountTracks(project) == 2, "saved project did not reopen with two tracks")
  first.track, second.track = reaper.GetTrack(project, 0), reaper.GetTrack(project, 1)
  first.fx, second.fx = 0, 0
  require_test(reaper.TrackFX_GetCount(first.track) == 1 and reaper.TrackFX_GetCount(second.track) == 1,
    "saved project did not recreate both plugin instances")
end

local function render(project, path)
  set_project_string(project, "RENDER_FILE", output_root)
  set_project_string(project, "RENDER_PATTERN", path:match("([^\\]+)%.wav$"))
  set_project_string(project, "RENDER_FORMAT", "evaw")
  set_project_string(project, "RENDER_FORMAT2", "")
  reaper.GetSetProjectInfo(project, "RENDER_SETTINGS", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_BOUNDSFLAG", 0, true)
  reaper.GetSetProjectInfo(project, "RENDER_STARTPOS", 0.0, true)
  reaper.GetSetProjectInfo(project, "RENDER_ENDPOS", 1.4, true)
  reaper.GetSetProjectInfo(project, "RENDER_CHANNELS", 2, true)
  reaper.GetSetProjectInfo(project, "RENDER_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "RENDER_ADDTOPROJ", 0, true)
  local targets_ok, targets = reaper.GetSetProjectInfo_String(project, "RENDER_TARGETS", "", false)
  local stats_ok, stats = reaper.GetSetProjectInfo_String(project, "RENDER_STATS", "42230", false)
  local actual_path = path
  if not file_exists(actual_path) and targets_ok then
    for target in targets:gmatch("[^;]+") do if file_exists(target) then actual_path = target; break end end
  end
  require_test(file_exists(actual_path), "REAPER did not create render " .. path)
  local audio = read_wav(actual_path)
  audio.render_stats_supported = stats_ok
  audio.render_stats = stats_ok and stats or nil
  audio.render_targets_supported = targets_ok
  return audio
end

local function compare_audio(before, after)
  if before.sample_rate ~= after.sample_rate or before.channels ~= after.channels or before.frames ~= after.frames or
      #before.samples ~= #after.samples then
    return false, math.huge, -1
  end
  local maximum, first = 0.0, nil
  for index = 1, #before.samples do
    local difference = math.abs(before.samples[index] - after.samples[index])
    if difference > maximum then maximum = difference end
    if not first and difference ~= 0.0 then first = index - 1 end
  end
  return maximum == 0.0, maximum, first
end

local function compact_audio(audio)
  audio.samples = nil
  return audio
end

local function make_markdown(result)
  local lines = {
    "# P12c REAPER state persistence", "",
    "- Overall: **" .. result.overall .. "**",
    "- Timestamp: " .. result.timestamp,
    "- REAPER: " .. result.reaper_version,
    "- Repository: `" .. result.repository.branch .. "` @ `" .. result.repository.head .. "`",
    "- Installed VST3 SHA-256: `" .. result.plugin.sha256 .. "`",
    "- Project: `" .. result.project_path .. "`", "",
    "## Results", "",
    "| Check | Result |",
    "|---|---|",
    "| Native parameters restored | " .. tostring(result.parameters.restored) .. "/" .. tostring(result.parameters.checked) .. " |",
    "| Parameter mismatches | " .. tostring(#result.parameters.mismatches) .. " |",
    "| Independent instances | " .. result.multi_instance.status .. " |",
    "| Deterministic render | " .. result.audio.status .. " (max difference " .. string.format("%.9g", result.audio.maximum_difference) .. ") |",
    "| Audio peak/RMS | " .. string.format("%.9g / %.9g", result.audio.after.peak, result.audio.after.rms) .. " |",
    "| Effective rate/channels/frames | " .. tostring(result.audio.after.sample_rate) .. " / " .. tostring(result.audio.after.channels) .. " / " .. tostring(result.audio.after.frames) .. " |",
    "| REAPER PDC | " .. tostring(result.pdc.instance_one) .. ", " .. tostring(result.pdc.instance_two) .. " samples |",
    "| Repeated transport start/stop | " .. result.transport.status .. " |", "",
    "## Method", "",
    "The harness created two differently configured LXR VST3i instances in a new empty REAPER process, captured all native slots 0-227, saved a unique RPP, rendered deterministic channel-1 D1 notes, deliberately changed one parameter per instance, and reopened the saved project with REAPER's `Main_openProject` API. It then compared every native slot and rendered again. The WAV sample arrays must be bit-identical; output rate, channels, finite/nonzero audio, and the exposed `pdc` values are checked independently.", "",
    "## Errors", ""
  }
  if #result.errors == 0 then lines[#lines + 1] = "None." else
    for _, message in ipairs(result.errors) do lines[#lines + 1] = "- " .. message end
  end
  return table.concat(lines, "\n") .. "\n"
end

local function main()
  reaper.RecursiveCreateDirectory(output_root, 0)
  for _, path in ipairs({project_path, before_path, after_path, json_path, markdown_path}) do
    require_test(not file_exists(path), "refusing to overwrite existing artifact: " .. path)
  end
  local project, filename = reaper.EnumProjects(-1, "")
  require_test(project and filename == "" and reaper.IsProjectDirty(project) == 0 and reaper.CountTracks(project) == 0,
    "safety guard: run in a new, empty, unmodified REAPER project")
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE", sample_rate, true)
  reaper.GetSetProjectInfo(project, "PROJECT_SRATE_USE", 1, true)

  local patch_one = {{0,77},{1,0},{8,57},{9,71},{20,0},{37,72},{43,38},{49,0},{50,49},{62,31},
    {70,0},{74,0},{78,0},{82,0},{83,68},{88,100},{89,0},{90,0},{91,0},{92,0},{93,0},
    {94,64},{102,0},{108,127},{114,127},{115,37},{121,0},{134,1},{137,1},{191,1},
    {197,0}}
  local patch_two = {{1,2},{8,85},{9,21},{20,1},{37,91},{43,16},{49,0},{50,63},{62,12},
    {70,11},{74,31},{78,22},{82,9},{83,44},{88,96},{94,82},{102,9},{108,127},
    {114,127},{115,69},{121,0},{137,1},{191,2}}
  local first = add_lxr_track(project, "P12c LXR instance A", patch_one, 0.25)
  local second = add_lxr_track(project, "P12c LXR instance B", patch_two, 0.70)
  reaper.SetMediaTrackInfo_Value(second.track, "B_MUTE", 1)
  local expected_first, expected_second = snapshot(first.track, first.fx), snapshot(second.track, second.fx)
  require_test(expected_first[1] == 77 and expected_second[1] == 0,
    "reserved slot 0 was not captured consistently")
  require_test(json_encode(expected_first) ~= json_encode(expected_second), "independent instance states are identical")

  reaper.Main_SaveProjectEx(project, project_path, 0)
  require_test(file_exists(project_path), "REAPER project was not saved")

  -- Establish the baseline from a state-restored instance. A freshly edited
  -- instance can legitimately have different oscillator history even though
  -- its parameter state is identical.
  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire_instances(project, first, second)
  require_test(#compare_parameters(expected_first, snapshot(first.track, first.fx), 1) == 0 and
    #compare_parameters(expected_second, snapshot(second.track, second.fx), 2) == 0,
    "baseline project reopen did not restore all parameters")
  local before = render(project, before_path)
  require_test(before.sample_rate == sample_rate and before.channels == 2 and before.finite and before.nonzero,
    "pre-reload render is invalid or silent")

  require_test(reaper.TrackFX_SetParamNormalized(first.track, first.fx, 8, 3 / 127.0), "failed temporary mutation A")
  require_test(reaper.TrackFX_SetParamNormalized(second.track, second.fx, 8, 123 / 127.0), "failed temporary mutation B")
  require_test(snapshot(first.track, first.fx)[9] == 3 and snapshot(second.track, second.fx)[9] == 123,
    "temporary mutations did not take effect")

  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire_instances(project, first, second)
  local actual_first, actual_second = snapshot(first.track, first.fx), snapshot(second.track, second.fx)
  local mismatches = compare_parameters(expected_first, actual_first, 1)
  local second_mismatches = compare_parameters(expected_second, actual_second, 2)
  for _, mismatch in ipairs(second_mismatches) do mismatches[#mismatches + 1] = mismatch end
  require_test(#mismatches == 0, "one or more native parameters differ after project reopen")

  local after = render(project, after_path)
  require_test(after.sample_rate == sample_rate and after.channels == 2 and after.finite and after.nonzero,
    "post-reload render is invalid or silent")
  local exact, maximum_difference, first_difference = compare_audio(before, after)
  require_test(exact, "pre/post project render samples are not bit-identical")
  for repetition = 1, 2 do
    reaper.Main_OnCommand(1007, 0)
    reaper.Main_OnCommand(1016, 0)
  end
  local pdc_one_ok, pdc_one = named_config(first.track, first.fx, "pdc")
  local pdc_two_ok, pdc_two = named_config(second.track, second.fx, "pdc")
  pdc_one, pdc_two = tonumber(pdc_one), tonumber(pdc_two)
  require_test(pdc_one_ok and pdc_two_ok and pdc_one == expected_pdc and pdc_two == expected_pdc,
    "REAPER pdc value changed or is unavailable")

  local result = {
    schema = "lxr-p12c-reaper-state-v1", timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"), overall = "PASS",
    reaper_version = reaper.GetAppVersion(), project_path = project_path,
    repository = { branch = "p12b-4c-fir-resampler", head = repository_head },
    plugin = { name = "LXR (Sonic Potions)", path = installed_vst3, sha256 = installed_sha256,
      instance_one = { name = first.fx_name, type = first.fx_type, identifier = first.fx_ident },
      instance_two = { name = second.fx_name, type = second.fx_type, identifier = second.fx_ident } },
    state_format = { magic = "LXRS", version = 1, native_slots = parameter_count,
      active_parameters = active_parameter_count, custom_bytes = 252, vst3_bytes = 256 },
    parameters = { checked = parameter_count * 2, restored = parameter_count * 2,
      active_checked = active_parameter_count * 2, reserved_slot_values = {actual_first[1], actual_second[1]},
      mismatches = mismatches },
    multi_instance = { status = "PASS", distinct_before_save = true, both_recreated = true,
      instance_one_midi_events = first.midi_events, instance_two_midi_events = second.midi_events },
    audio = { status = "PASS", exact_samples = exact, maximum_difference = maximum_difference,
      first_difference_sample = first_difference, before = compact_audio(before), after = compact_audio(after) },
    pdc = { status = "PASS", expected = expected_pdc, instance_one = pdc_one, instance_two = pdc_two },
    transport = { status = "PASS", start_stop_cycles = 2 },
    report_paths = { json = json_path, markdown = markdown_path }, errors = {}
  }
  write_file(json_path, json_encode(result) .. "\n")
  write_file(markdown_path, make_markdown(result))
  reaper.ShowConsoleMsg("P12c REAPER state persistence PASS\n" .. json_path .. "\n" .. markdown_path .. "\n")
end

local ok, err = xpcall(main, debug.traceback)
if not ok then
  reaper.RecursiveCreateDirectory(output_root, 0)
  local result = { schema = "lxr-p12c-reaper-state-v1", timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    overall = "FAIL", reaper_version = reaper.GetAppVersion(),
    repository = { branch = "p12b-4c-fir-resampler", head = repository_head },
    plugin = { path = installed_vst3, sha256 = installed_sha256 }, errors = { tostring(err) } }
  if not file_exists(json_path) then write_file(json_path, json_encode(result) .. "\n") end
  if not file_exists(markdown_path) then
    write_file(markdown_path, "# P12c REAPER state persistence\n\n- Overall: **FAIL**\n- Error: `" .. tostring(err):gsub("`", "'") .. "`\n")
  end
  reaper.ShowConsoleMsg("P12c REAPER state persistence FAIL\n" .. tostring(err) .. "\n")
end
