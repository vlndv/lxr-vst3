local helper_directory = rawget(_G, "LXR_REAPER_HELPERS") or os.getenv("LXR_REAPER_HELPERS") or
  assert(debug.getinfo(1, "S").source:sub(2):match("^(.*)[\\/]"), "cannot locate REAPER helpers")
local paths = dofile(helper_directory .. "/checkpoint_paths.lua")
-- P12b-4c: automated REAPER host-PDC and audio verification for LXR VST3.
-- Run in a new, empty REAPER instance. The optional global
-- P12B4C_SMOKE_ONLY is set by p12b4c_pdc_smoke.lua.

local smoke_only = P12B4C_SMOKE_ONLY == true
local case_tag = P12B4C_CASE_TAG
local midi_channel = P12B4C_MIDI_CHANNEL or 0
local note_times = P12B4C_NOTE_TIMES or {1.0}
local repo_root = paths.repo_root
local output_root = paths.output_root("reaper-p12b4c")
local installed_vst3 = paths.plugin()
local repo_head = "280279debe94cb26810ea89b89f36b9bf965a747"
local engine_rate = 44002.7578125
local physical_delay_ms = 0.534057435676
local render_start = 0.0
local render_end = P12B4C_RENDER_END or 2.5
local note_time = note_times[1]
local note_off_time = note_time + 0.1
local note_pitch = 60
local note_velocity = 110
local alignment_tolerance_frames = 0.25
local rates = case_tag and {48000} or (smoke_only and {48000} or {44100, 48000, 96000})
local expected_latency = { [44100] = 24, [48000] = 26, [96000] = 51 }
local report_json = output_root .. (case_tag and ("\\" .. case_tag .. "_results.json") or (smoke_only and "\\pdc_smoke_48000_retry.json" or "\\pdc_results.json"))
local report_md = output_root .. (case_tag and ("\\" .. case_tag .. "_report.md") or (smoke_only and "\\pdc_smoke_48000_retry.md" or "\\pdc_report.md"))
local project_path = output_root .. (case_tag and ("\\" .. case_tag .. ".rpp") or (smoke_only and "\\p12b4c_smoke_48000_retry.rpp" or "\\p12b4c_pdc_matrix.rpp"))

local function file_exists(path)
  local f = io.open(path, "rb")
  if f then f:close(); return true end
  return false
end

local function json_escape(value)
  return value:gsub('\\', '\\\\'):gsub('"', '\\"'):gsub('\b', '\\b')
    :gsub('\f', '\\f'):gsub('\n', '\\n'):gsub('\r', '\\r'):gsub('\t', '\\t')
end

local function is_array(t)
  local n = 0
  for k in pairs(t) do
    if type(k) ~= "number" or k < 1 or k % 1 ~= 0 then return false end
    if k > n then n = k end
  end
  for i = 1, n do if t[i] == nil then return false end end
  return true, n
end

local function json_encode(value, indent, level)
  indent = indent or "  "
  level = level or 0
  local kind = type(value)
  if kind == "nil" then return "null" end
  if kind == "boolean" then return value and "true" or "false" end
  if kind == "number" then
    if value ~= value or value == math.huge or value == -math.huge then return "null" end
    return string.format("%.15g", value)
  end
  if kind == "string" then return '"' .. json_escape(value) .. '"' end
  if kind ~= "table" then return '"' .. json_escape(tostring(value)) .. '"' end
  local array, n = is_array(value)
  local pad = string.rep(indent, level)
  local child_pad = string.rep(indent, level + 1)
  local parts = {}
  if array then
    for i = 1, n do parts[#parts + 1] = child_pad .. json_encode(value[i], indent, level + 1) end
    if #parts == 0 then return "[]" end
    return "[\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "]"
  end
  local keys = {}
  for k in pairs(value) do keys[#keys + 1] = k end
  table.sort(keys, function(a, b) return tostring(a) < tostring(b) end)
  for _, k in ipairs(keys) do
    parts[#parts + 1] = child_pad .. '"' .. json_escape(tostring(k)) .. '": ' .. json_encode(value[k], indent, level + 1)
  end
  if #parts == 0 then return "{}" end
  return "{\n" .. table.concat(parts, ",\n") .. "\n" .. pad .. "}"
end

local function write_file(path, contents)
  local f, err = io.open(path, "wb")
  if not f then error("cannot create " .. path .. ": " .. tostring(err)) end
  f:write(contents)
  f:close()
end

local function config(track, fx, name)
  local ok, value = reaper.TrackFX_GetNamedConfigParm(track, fx, name)
  return ok, ok and value or nil
end

local function set_project_string(proj, key, value)
  local ok = reaper.GetSetProjectInfo_String(proj, key, value, true)
  if not ok then error("failed to set project string " .. key) end
end

local function i24le(data, pos)
  local a, b, c = data:byte(pos, pos + 2)
  local value = a + b * 256 + c * 65536
  if value >= 0x800000 then value = value - 0x1000000 end
  return value
end

local function read_wav(path)
  local f, err = io.open(path, "rb")
  if not f then error("render file missing: " .. path .. " (" .. tostring(err) .. ")") end
  local data = f:read("*a")
  f:close()
  if #data < 44 or data:sub(1, 4) ~= "RIFF" or data:sub(9, 12) ~= "WAVE" then
    error("unsupported or invalid WAV container: " .. path)
  end
  local pos, fmt, data_pos, data_size = 13, nil, nil, nil
  while pos + 7 <= #data do
    local id = data:sub(pos, pos + 3)
    local size = string.unpack("<I4", data, pos + 4)
    local body = pos + 8
    if id == "fmt " then
      local format, channels, sample_rate, _, block_align, bits = string.unpack("<I2I2I4I4I2I2", data, body)
      if format == 0xFFFE and size >= 40 then format = string.unpack("<I2", data, body + 24) end
      fmt = { format = format, channels = channels, sample_rate = sample_rate,
              block_align = block_align, bits = bits }
    elseif id == "data" then
      data_pos, data_size = body, math.min(size, #data - body + 1)
      break
    end
    pos = body + size + (size % 2)
  end
  if not fmt or not data_pos then error("WAV is missing fmt or data chunk: " .. path) end
  local bytes_per_sample = fmt.bits // 8
  if bytes_per_sample < 1 or fmt.block_align ~= bytes_per_sample * fmt.channels then
    error("unsupported WAV block layout in " .. path)
  end
  local frames = data_size // fmt.block_align
  local mono, peak, sumsq, finite = {}, 0.0, 0.0, true
  local p = data_pos
  for frame = 1, frames do
    local mix = 0.0
    for channel = 1, fmt.channels do
      local sample
      if fmt.format == 1 and fmt.bits == 16 then
        sample = string.unpack("<i2", data, p) / 32768.0
      elseif fmt.format == 1 and fmt.bits == 24 then
        sample = i24le(data, p) / 8388608.0
      elseif fmt.format == 1 and fmt.bits == 32 then
        sample = string.unpack("<i4", data, p) / 2147483648.0
      elseif fmt.format == 3 and fmt.bits == 32 then
        sample = string.unpack("<f", data, p)
      elseif fmt.format == 3 and fmt.bits == 64 then
        sample = string.unpack("<d", data, p)
      else
        error(string.format("unsupported WAV format=%d bits=%d", fmt.format, fmt.bits))
      end
      p = p + bytes_per_sample
      finite = finite and sample == sample and sample ~= math.huge and sample ~= -math.huge
      local magnitude = math.abs(sample)
      if magnitude > peak then peak = magnitude end
      sumsq = sumsq + sample * sample
      mix = mix + sample
    end
    mono[frame] = mix / fmt.channels
  end
  local onset, peak_frame, mono_peak = nil, 0, 0.0
  for i, sample in ipairs(mono) do
    local magnitude = math.abs(sample)
    if not onset and magnitude > 1.0e-7 then onset = i - 1 end
    if magnitude > mono_peak then mono_peak, peak_frame = magnitude, i - 1 end
  end
  return {
    path = path, sample_rate = fmt.sample_rate, channels = fmt.channels,
    frames = frames, peak = peak,
    rms = math.sqrt(sumsq / math.max(1, frames * fmt.channels)),
    finite = finite, nonzero = peak > 1.0e-7,
    onset_frame = onset, peak_frame = peak_frame, mono = mono
  }
end

local function compare_alignment(compensated, ignored, expected)
  local a, b = compensated.mono, ignored.mono
  local start = math.max(1, math.floor(0.75 * compensated.sample_rate))
  local finish = math.min(#a, #b, math.floor(1.75 * compensated.sample_rate))
  local best_lag, best_error = nil, math.huge
  for lag = math.max(0, expected - 4), expected + 4 do
    local sum, count = 0.0, 0
    for i = start, finish do
      local j = i + lag
      if j <= #b then
        local d = a[i] - b[j]
        sum, count = sum + d * d, count + 1
      end
    end
    local error = math.sqrt(sum / math.max(1, count))
    if error < best_error then best_error, best_lag = error, lag end
  end
  return best_lag, best_error
end

local function window_stats(wav, start_seconds, end_seconds)
  local first = math.max(1, math.floor(start_seconds * wav.sample_rate) + 1)
  local last = math.min(#wav.mono, math.ceil(end_seconds * wav.sample_rate))
  local peak, sum, count = 0.0, 0.0, 0
  for i = first, last do
    local value = wav.mono[i]
    peak = math.max(peak, math.abs(value))
    sum = sum + value * value
    count = count + 1
  end
  return { start_seconds = start_seconds, end_seconds = end_seconds,
    frames = count, peak = peak, rms = math.sqrt(sum / math.max(1, count)) }
end

local function create_lxr_case(proj, rate, pdc_mode, label, render_path)
  for i = 0, reaper.CountTracks(proj) - 1 do
    reaper.SetMediaTrackInfo_Value(reaper.GetTrack(proj, i), "B_MUTE", 1)
  end
  reaper.InsertTrackAtIndex(reaper.CountTracks(proj), true)
  local track = reaper.GetTrack(proj, reaper.CountTracks(proj) - 1)
  reaper.GetSetMediaTrackInfo_String(track, "P_NAME", string.format("LXR_%d_%s_%s", rate, case_tag or "pdc", label), true)
  reaper.SetMediaTrackInfo_Value(track, "B_MUTE", 0)
  reaper.SetMediaTrackInfo_Value(track, "D_VOL", 1.0)
  local fx = reaper.TrackFX_AddByName(track, "VST3i: LXR (Sonic Potions)", false, -1)
  if fx < 0 then fx = reaper.TrackFX_AddByName(track, "LXR (Sonic Potions)", false, -1) end
  if fx < 0 then error("REAPER could not instantiate the installed LXR VST3i") end
  local name_ok, fx_name = reaper.TrackFX_GetFXName(track, fx)
  local type_ok, fx_type = config(track, fx, "fx_type")
  local ident_ok, fx_ident = config(track, fx, "fx_ident")
  if not name_ok or not fx_name:find("LXR", 1, true) or not type_ok or not fx_type:find("VST3", 1, true) then
    error("loaded FX is not the intended LXR VST3: " .. tostring(fx_name) .. " / " .. tostring(fx_type))
  end
  local mode_set = reaper.TrackFX_SetNamedConfigParm(track, fx, "chain_pdc_mode", tostring(pdc_mode))
  local mode_ok, mode_value = config(track, fx, "chain_pdc_mode")
  local final_note_time = note_times[#note_times]
  local item = reaper.CreateNewMIDIItemInProj(track, note_time, final_note_time + 0.2, false)
  if not item then error("failed to create MIDI item") end
  local take = reaper.GetActiveTake(item)
  for _, event_time in ipairs(note_times) do
    local start_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, event_time)
    local end_ppq = reaper.MIDI_GetPPQPosFromProjTime(take, event_time + 0.1)
    if not reaper.MIDI_InsertNote(take, false, false, start_ppq, end_ppq, midi_channel, note_pitch, note_velocity, true) then
      error("failed to insert MIDI note")
    end
  end
  reaper.MIDI_Sort(take)
  local count_ok, note_count, cc_count, text_count = reaper.MIDI_CountEvts(take)
  if not count_ok or note_count ~= #note_times then error("unexpected MIDI event count") end
  local pdc_before_ok, pdc_before = config(track, fx, "pdc")
  set_project_string(proj, "RENDER_FILE", output_root)
  set_project_string(proj, "RENDER_PATTERN", render_path:match("([^\\]+)%.wav$") or label)
  set_project_string(proj, "RENDER_FORMAT", "evaw")
  set_project_string(proj, "RENDER_FORMAT2", "")
  reaper.GetSetProjectInfo(proj, "RENDER_SETTINGS", 0, true)
  reaper.GetSetProjectInfo(proj, "RENDER_BOUNDSFLAG", 0, true)
  reaper.GetSetProjectInfo(proj, "RENDER_STARTPOS", render_start, true)
  reaper.GetSetProjectInfo(proj, "RENDER_ENDPOS", render_end, true)
  reaper.GetSetProjectInfo(proj, "RENDER_CHANNELS", 2, true)
  reaper.GetSetProjectInfo(proj, "RENDER_SRATE", rate, true)
  reaper.GetSetProjectInfo(proj, "RENDER_ADDTOPROJ", 0, true)
  local targets_ok, targets = reaper.GetSetProjectInfo_String(proj, "RENDER_TARGETS", "", false)
  local stats_ok, stats = reaper.GetSetProjectInfo_String(proj, "RENDER_STATS", "42230", false)
  local actual_path = render_path
  if not file_exists(actual_path) and targets_ok then
    for target in targets:gmatch("[^;]+") do
      if file_exists(target) then actual_path = target; break end
    end
  end
  local wav = read_wav(actual_path)
  local pdc_after_ok, pdc_after = config(track, fx, "pdc")
  local actual_ok, chain_actual = config(track, fx, "chain_pdc_actual")
  local reporting_ok, chain_reporting = config(track, fx, "chain_pdc_reporting")
  return {
    label = label, rate_requested = rate, fx_index = fx,
    fx_name = fx_name, fx_type = fx_type, fx_ident = ident_ok and fx_ident or nil,
    chain_pdc_mode_set = mode_set, chain_pdc_mode_supported = mode_ok,
    chain_pdc_mode = mode_value,
    pdc_before_supported = pdc_before_ok, pdc_before = tonumber(pdc_before),
    pdc_after_supported = pdc_after_ok, pdc_after = tonumber(pdc_after),
    chain_pdc_actual_supported = actual_ok, chain_pdc_actual = tonumber(chain_actual),
    chain_pdc_reporting_supported = reporting_ok, chain_pdc_reporting = tonumber(chain_reporting),
    midi_notes = note_count, midi_events_sent = note_count * 2 + cc_count + text_count,
    render_stats_supported = stats_ok, render_stats = stats,
    render_targets_supported = targets_ok, render_targets = targets,
    audio = wav
  }
end

local function strip_samples(case)
  case.audio.mono = nil
end

local function make_markdown(result)
  local lines = {
    "# P12b-4c REAPER PDC verification", "",
    "- Overall: **" .. result.overall .. "**",
    "- Timestamp: " .. result.timestamp,
    "- REAPER: " .. result.reaper_version,
    "- Repository: `" .. result.repository.branch .. "` @ `" .. result.repository.head .. "`",
    "- Installed VST3: `" .. result.plugin.path .. "`", "",
    "## Sample-rate and timing matrix", "",
    "| Rate | Expected/report PDC | Host chain actual/reporting | Measured PDC shift | Nominal residual | Alignment error | Peak | RMS | Status |",
    "|---:|---:|---:|---:|---:|---:|---:|---:|---|"
  }
  for _, row in ipairs(result.sample_rates) do
    local chain = row.chain_pdc_actual ~= nil and tostring(row.chain_pdc_actual) or "unsupported"
    if row.chain_pdc_reporting ~= nil then chain = chain .. "/" .. tostring(row.chain_pdc_reporting) end
    lines[#lines + 1] = string.format("| %d | %d/%s | %s | %s | %+.6f | %+.6f | %.9g | %.9g | %s |",
      row.requested_rate, row.expected_latency, tostring(row.plugin_reported_latency), chain,
      tostring(row.measured_pdc_shift), row.nominal_fractional_residual,
      row.alignment_error_frames, row.compensated_audio.peak, row.compensated_audio.rms, row.status)
  end
  lines[#lines + 1] = ""
  lines[#lines + 1] = "## Method"
  lines[#lines + 1] = ""
  lines[#lines + 1] = "For each rate, the script created fresh LXR VST3i instances with the same channel-1 note-on/off at 1.0/1.1 s. It rendered once with REAPER's normal chain PDC mode and once with `chain_pdc_mode=2` (ignore PDC). The reported `pdc` value was queried through `TrackFX_GetNamedConfigParm`. The host-applied shift was independently measured by integer-lag RMS alignment of the two rendered waveforms; it was not inferred from the FIR calculator or from the drum waveform peak. WAV headers confirm the offline processing/output rate."
  lines[#lines + 1] = ""
  lines[#lines + 1] = "## Capability notes"
  lines[#lines + 1] = ""
  for _, note in ipairs(result.capability_notes) do lines[#lines + 1] = "- " .. note end
  lines[#lines + 1] = ""
  lines[#lines + 1] = "## Errors"
  lines[#lines + 1] = ""
  if #result.errors == 0 then lines[#lines + 1] = "None." else
    for _, err in ipairs(result.errors) do lines[#lines + 1] = "- " .. err end
  end
  return table.concat(lines, "\n") .. "\n"
end

local function main()
  reaper.RecursiveCreateDirectory(output_root, 0)
  if file_exists(report_json) or file_exists(report_md) or file_exists(project_path) then
    error("refusing to overwrite an existing report or project artifact")
  end
  local proj, project_filename = reaper.EnumProjects(-1, "")
  if not proj then error("no active REAPER project") end
  if project_filename ~= "" or reaper.IsProjectDirty(proj) ~= 0 or reaper.CountTracks(proj) ~= 0 then
    error("safety guard: run in a new, empty, unmodified REAPER project")
  end
  local result = {
    schema = "lxr-p12b4c-reaper-pdc-v1", timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    mode = case_tag or (smoke_only and "48k-smoke" or "required-matrix"),
    reaper_version = reaper.GetAppVersion(),
    repository = { branch = "p12b-4c-fir-resampler", head = repo_head },
    plugin = { name = "LXR (Sonic Potions)", version = "1.0.0",
      windows_file_version = "0.0.1", path = installed_vst3,
      sha256 = P12B4C_PLUGIN_SHA256 or "F51FF17924512FD364ADE5704E0A6CEFBFCD930AE91F803E576343F8DF0DA5B8" },
    physical_delay_ms = physical_delay_ms, engine_sample_rate = engine_rate,
    alignment_tolerance_frames = alignment_tolerance_frames,
    sample_rates = {}, errors = {}, capability_notes = {
      "Lua ReaScript is embedded in this REAPER installation.",
      "EEL2 is also embedded; Python ReaScript availability was not verified because it requires separate preference configuration.",
      "The documented pdc named configuration parameter is queried on the actual LXR instance.",
      "Host compensation is tested by normal-versus-ignore-PDC render alignment, separately from plugin reporting.",
      "The project remains isolated; no pre-existing project was opened, closed, or overwritten.",
      "MIDI channels 2-7 and real-time audio-device rate changes are outside this offline PDC matrix."
    }
  }
  for _, rate in ipairs(rates) do
    reaper.GetSetProjectInfo(proj, "PROJECT_SRATE", rate, true)
    reaper.GetSetProjectInfo(proj, "PROJECT_SRATE_USE", 1, true)
    local stem = case_tag and string.format("%s_lxr_%d", case_tag, rate) or (smoke_only and string.format("lxr_%d_smoke_retry", rate) or string.format("matrix_lxr_%d", rate))
    local normal_path = string.format("%s\\%s_pdc.wav", output_root, stem)
    local ignore_path = string.format("%s\\%s_ignore_pdc.wav", output_root, stem)
    if file_exists(normal_path) or file_exists(ignore_path) then error("refusing to overwrite render media") end
    local normal = create_lxr_case(proj, rate, 1, "pdc", normal_path)
    local ignored = create_lxr_case(proj, rate, 2, "ignore_pdc", ignore_path)
    local expected = expected_latency[rate]
    local reported = normal.pdc_after or normal.pdc_before
    local lag, waveform_error = compare_alignment(normal.audio, ignored.audio, expected)
    local nominal = 23.5 * rate / engine_rate
    local alignment_error = lag - expected
    local physical_residual = nominal - lag
    local status = "PASS"
    local reasons = {}
    local function require_case(ok, reason)
      if not ok then status = "FAIL"; reasons[#reasons + 1] = reason end
    end
    require_case(normal.audio.sample_rate == rate and ignored.audio.sample_rate == rate, "rendered WAV rate mismatch")
    require_case(reported == expected, "plugin pdc does not match nearest-integer reference")
    require_case(normal.chain_pdc_mode_set and ignored.chain_pdc_mode_set and ignored.chain_pdc_mode == "2", "chain PDC mode API unavailable")
    require_case(normal.audio.finite and normal.audio.nonzero and ignored.audio.finite and ignored.audio.nonzero, "audio is silent or non-finite")
    require_case(math.abs(alignment_error) <= alignment_tolerance_frames, "measured host shift differs from reported PDC")
    require_case(math.abs(physical_residual) <= math.abs(nominal - expected) + alignment_tolerance_frames,
      "compensated residual exceeds nominal fractional residual plus tolerance")
    local repeated_hits = nil
    if case_tag then
      repeated_hits = {}
      for _, event_time in ipairs(note_times) do
        local early = window_stats(normal.audio, event_time, event_time + 0.08)
        local late = window_stats(normal.audio, event_time + 0.25, event_time + 0.42)
        repeated_hits[#repeated_hits + 1] = { note_time = event_time, early = early, late = late,
          decay_ratio = early.rms > 0 and late.rms / early.rms or nil }
        require_case(early.peak > 0 and early.rms > 0, "repeated MIDI hit did not produce audio")
        require_case(late.rms < early.rms, "snare render did not decay between early and late windows")
      end
    end
    result.sample_rates[#result.sample_rates + 1] = {
      requested_rate = rate, effective_render_rate = normal.audio.sample_rate,
      expected_latency = expected, plugin_reported_latency = reported,
      pdc_query_supported = normal.pdc_after_supported or normal.pdc_before_supported,
      chain_pdc_actual = normal.chain_pdc_actual,
      chain_pdc_reporting = normal.chain_pdc_reporting,
      measured_pdc_shift = lag, alignment_error_frames = alignment_error,
      waveform_alignment_rms_error = waveform_error,
      nominal_host_frames = nominal, nominal_fractional_residual = nominal - expected,
      measured_residual_to_nominal_frames = physical_residual,
      midi_events_sent = normal.midi_events_sent,
      midi_channel_one_based = midi_channel + 1, repeated_hits = repeated_hits,
      compensated_audio = normal.audio, ignored_pdc_audio = ignored.audio,
      normal_case = normal, ignored_case = ignored,
      status = status, reasons = reasons
    }
  end
  local all_pass = true
  for _, row in ipairs(result.sample_rates) do if row.status ~= "PASS" then all_pass = false end end
  result.overall = all_pass and "PASS" or "FAIL"
  reaper.Main_SaveProjectEx(proj, project_path, 0)
  result.project_path = project_path
  result.report_paths = { json = report_json, markdown = report_md }
  for _, row in ipairs(result.sample_rates) do
    strip_samples(row.normal_case); strip_samples(row.ignored_case)
    row.compensated_audio.mono = nil; row.ignored_pdc_audio.mono = nil
  end
  write_file(report_json, json_encode(result) .. "\n")
  write_file(report_md, make_markdown(result))
  reaper.ShowConsoleMsg(string.format("P12b-4c REAPER PDC %s: %s\n%s\n%s\n", result.mode, result.overall, report_json, report_md))
end

local ok, err = xpcall(main, debug.traceback)
if not ok then
  reaper.RecursiveCreateDirectory(output_root, 0)
  local blocked = {
    schema = "lxr-p12b4c-reaper-pdc-v1", timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ"),
    mode = case_tag or (smoke_only and "48k-smoke" or "required-matrix"), overall = "BLOCKED",
    reaper_version = reaper.GetAppVersion(),
    repository = { branch = "p12b-4c-fir-resampler", head = repo_head },
    plugin = { path = installed_vst3 }, sample_rates = {}, errors = { tostring(err) },
    capability_notes = { "The harness stopped without claiming unobserved measurements." }
  }
  if not file_exists(report_json) then write_file(report_json, json_encode(blocked) .. "\n") end
  if not file_exists(report_md) then
    write_file(report_md, "# P12b-4c REAPER PDC verification\n\n- Overall: **BLOCKED**\n- Error: `" .. tostring(err):gsub("`", "'") .. "`\n")
  end
  reaper.ShowConsoleMsg("P12b-4c REAPER PDC BLOCKED: " .. tostring(err) .. "\n")
end
