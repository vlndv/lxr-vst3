-- P16-F2 task-owned exact-private production-ingress acceptance.
local output_root = [[C:\Projects\lxr-vst3\out\p16f2\reaper-isolated-002]]
local project_path = output_root .. [[\accepted_patterns.rpp]]
local result_path = output_root .. [[\reaper_results.json]]
local private_plugin = [[C:\Projects\lxr-vst3\out\p16f2\private-build-003\bundle\LXR.vst3\Contents\x86_64-win\LXR.vst3]]
local private_sha256 = [[CC4934CA013A51A335D40104EB064D0DEE21F304CAD5D9C81748DB04F3097AD7]]
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
  local project = reaper.EnumProjects(-1, "")
  local track = reaper.GetTrack(project,0)
  require_test(track and reaper.CountTracks(project)==1, "requires the one-track task-owned imported project")
  local _, track_name = reaper.GetSetMediaTrackInfo_String(track,"P_NAME","",false)
  require_test(track_name=="P16-F2 production ingress", "task track guard")
  local _, ident = named_config(track,0,"fx_ident")
  if not ident or not ident:find(private_plugin,1,true) then
    write_file(output_root .. [[\wrong_instance.txt]], tostring(ident)); return
  end
  require_test(not file_exists(result_path), "refuse existing result")
  local ok, imported = reaper.GetTrackStateChunk(track,"",false)
  require_test(ok,"capture imported production bank")
  write_file(output_root .. [[\imported_track_chunk.txt]],imported)
  reaper.Main_OnCommand(1016,0)
  reaper.TrackFX_SetParamNormalized(track,0,0,77/127)
  reaper.TrackFX_SetParamNormalized(track,0,228,2/65535)
  reaper.TrackFX_SetParamNormalized(track,0,240,1)
  reaper.TrackFX_SetParamNormalized(track,0,241,1)
  reaper.TrackFX_SetParamNormalized(track,0,242,90/254)
  reaper.SetCurrentBPM(project,120,false)
  local expected = {}
  for id=0,242 do expected[id+1]=reaper.TrackFX_GetParamNormalized(track,0,id) end
  reaper.Main_SaveProjectEx(project,project_path,0)
  local first,first_bytes = render(project,output_root .. [[\pattern_120_a.wav]])
  require_test(has_nonzero(first),"imported production pattern PCM is silent")
  reaper.Main_openProject("noprompt:" .. project_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  local mismatches={}
  for id=0,242 do
    if math.abs(reaper.TrackFX_GetParamNormalized(track,0,id)-expected[id+1])>1e-7 then mismatches[#mismatches+1]=id end
  end
  require_test(#mismatches==0,"restored controller mismatch")
  local ok, restored = reaper.GetTrackStateChunk(track,"",false)
  require_test(ok,"capture reopened bank")
  write_file(output_root .. [[\restored_track_chunk.txt]],restored)
  local second,second_bytes=render(project,output_root .. [[\pattern_120_b.wav]])
  require_test(first==second,"project reopen pattern PCM differs")
  reaper.SetCurrentBPM(project,60,false)
  local slow=render(project,output_root .. [[\pattern_60.wav]])
  require_test(has_nonzero(slow) and slow~=first,"60 BPM sequence PCM")
  reaper.Main_openProject("noprompt:" .. project_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  reaper.TrackFX_SetParamNormalized(track,0,241,0)
  local internal=render(project,output_root .. [[\pattern_internal.wav]])
  require_test(has_nonzero(internal),"internal mode sequence PCM")
  reaper.Main_openProject("noprompt:" .. project_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  add_note(track)
  local coexist=render(project,output_root .. [[\pattern_and_midi.wav]])
  require_test(has_nonzero(coexist) and coexist~=first,"MIDI coexistence PCM")
  local pdc_ok,pdc=named_config(track,0,"pdc")
  require_test(pdc_ok and tonumber(pdc)==26,"48 kHz PDC")
  reaper.Main_openProject("noprompt:" .. project_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  reaper.TrackFX_SetEnabled(track,0,false)
  local bypass_path=output_root .. [[\bypass_patterns.rpp]]
  reaper.Main_SaveProjectEx(project,bypass_path,0)
  reaper.TrackFX_SetEnabled(track,0,true)
  reaper.Main_openProject("noprompt:" .. bypass_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  require_test(not reaper.TrackFX_GetEnabled(track,0),"bypass persistence")
  reaper.Main_openProject("noprompt:" .. project_path)
  project=reaper.EnumProjects(-1,"");track=reaper.GetTrack(project,0)
  local result={schema="lxr-p16f2-reaper-v1",overall="BLOCKED",reaper_version=reaper.GetAppVersion(),
    private_plugin={path=private_plugin,sha256=private_sha256,identifier=ident},
    ingress={status="PENDING_INDEPENDENT_BANK_COMPARISON",user_completed_file_dialog=true},
    pattern_pcm={status="PASS",host_120=true,host_60=true,internal=true,midi_coexistence=true},
    save_reopen={status="PASS",checked_original_patch_values=228,checked_routes=12,checked_transport_controls=3,mismatches=mismatches,bypass=true},
    repeated_offline_render={status="PASS",pcm="BIT_IDENTICAL",first_bytes=first_bytes,second_bytes=second_bytes},
    pdc={status="PASS",samples=tonumber(pdc)},
    remaining="Live transport timing, shuffle, manual roll, probability, lock isolation, chained/random replay, variable REAPER callbacks and two different nonempty banks still require certification."}
  write_file(result_path,json_encode(result) .. "\n")
end
local ok,err=xpcall(main,debug.traceback)
if not ok then write_file(result_path,json_encode({overall="FAIL",error=tostring(err)}) .. "\n") end
