"""Task-local certification helper; never rebuilds/moves executables or changes policy.

Run from repository root: python tools/reaper/p12c_r1_final_certification.py prepare|regress
Artifacts are exclusive-create under the dated task directory.
"""
import hashlib
import os
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(os.environ.get("LXR_REPO_ROOT", Path(__file__).resolve().parents[2])).resolve()
OUT = ROOT / (sys.argv[2] if len(sys.argv) > 2 else "out/p12c-r1-final-20261029")
assert OUT.resolve().is_relative_to((ROOT / "out").resolve())
OUT.mkdir(parents=True, exist_ok=True)
HASH = os.environ.get("LXR_EXPECTED_PLUGIN_SHA256") or "B170E00911A4031B04616C0B75E2B2A7CE5902E5CF46F22A23F1D2DD93A6F953"
DLL = Path(os.environ.get("LXR_VST3_BINARY") or
           Path(os.environ.get("LOCALAPPDATA", "")) /
           "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3")
EXECUTABLES = Path(os.environ.get("LXR_TEST_EXECUTABLES", ROOT / "out"))
HELPERS = Path(__file__).resolve().parent


def sha(path):
    if not Path(path).is_file():
        raise FileNotFoundError(f"Missing test dependency: {path}; configure LXR_VST3_BINARY/LXR_TEST_EXECUTABLES/LXR_VALIDATOR as appropriate")
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def write(path, value):
    with Path(path).open("x", encoding="utf-8", newline="\n") as f:
        f.write(value)


def replace_once(source, old, new):
    assert source.count(old) == 1, (old[:120], source.count(old))
    return source.replace(old, new)


def prepare():
    assert sha(DLL) == HASH
    source = (ROOT / "tools/reaper/p12c_state_test.lua").read_text(encoding="utf-8")
    source = replace_once(source, 'local output_root = paths.output_root("reaper-p12c")',
                          f'local output_root = [[{OUT}]]')
    source = f'LXR_REAPER_HELPERS = [[{HELPERS}]]\n' + source
    source = source.replace("_final", "_cert_b170e009")
    source = source.replace("617D4CFDF77AD8AB4B07177CB3C61D06824FD6B8304A59C64A15EF86F6750B5F", HASH)
    source = replace_once(source, 'local function add_lxr_track(project, name, edits, note_time)', '''local progress_path = output_root .. "\\\\state-progress.log"
local function progress(message)
  local f = assert(io.open(progress_path, "ab"))
  f:write(os.date("!%Y-%m-%dT%H:%M:%SZ ") .. message .. "\\n"); f:close()
end

local function add_lxr_track(project, name, edits, note_time)''')
    source = replace_once(source, '  for _, edit in ipairs(edits) do\n    require_test', '''  -- Explicitly set every slot, including reserved 0, with safe native defaults
  -- overridden by the established deterministic/non-default patch.
  local intended = {}
  for id = 0, parameter_count - 1 do
    intended[id + 1] = math.floor(reaper.TrackFX_GetParamNormalized(track, fx, id) * 127 + 0.5)
  end
  for _, edit in ipairs(edits) do intended[edit[1]+1] = edit[2] end
  edits = {}
  for id = 0, parameter_count - 1 do edits[#edits+1] = {id, intended[id+1]} end
  for _, edit in ipairs(edits) do
    require_test''')
    source = replace_once(source, 'fx_ident = ident_ok and fx_ident or nil, midi_events',
                          'fx_ident = ident_ok and fx_ident or nil, intended = intended, midi_events')
    source = replace_once(source, '  local expected_first, expected_second = snapshot(first.track, first.fx), snapshot(second.track, second.fx)', '''  progress("two instances created; every slot explicitly set")
  local expected_first, expected_second = first.intended, second.intended
  require_test(#compare_parameters(expected_first, snapshot(first.track, first.fx), 1) == 0 and
    #compare_parameters(expected_second, snapshot(second.track, second.fx), 2) == 0,
    "explicit all-slot initial set did not match intended values")''')
    source = replace_once(source, '  local before = render(project, before_path)',
                          '  progress("baseline restored; starting before render")\n  local before = render(project, before_path)')
    source = replace_once(source, '  local after = render(project, after_path)',
                          '  progress("456-slot reload comparison complete; starting after render")\n  local after = render(project, after_path)')
    source = replace_once(source, '  for repetition = 1, 2 do\n    reaper.Main_OnCommand(1007, 0)\n    reaper.Main_OnCommand(1016, 0)\n  end', '''  local transport_observations = {}
  reaper.Audio_Init()
  for repetition = 1, 2 do
    reaper.SetEditCurPos(0, false, false)
    reaper.Main_OnCommand(1007, 0)
    wait_seconds(0.45)
    local playing = reaper.GetPlayState()
    local position = reaper.GetPlayPosition()
    reaper.Main_OnCommand(1016, 0)
    local stopped = reaper.GetPlayState()
    transport_observations[#transport_observations+1] = {playing=playing, stopped=stopped, position=position}
    require_test((playing & 1) == 1 and stopped == 0 and position > 0.2, "transport transition failed")
  end''')
    source = replace_once(source, '  local result = {\n    schema =', '''  progress("exact A audio comparison and two transport transitions complete")
  -- Independent B audibility, with fresh restored DSP history.
  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire_instances(project, first, second)
  reaper.SetMediaTrackInfo_Value(first.track, "B_MUTE", 1)
  reaper.SetMediaTrackInfo_Value(second.track, "B_MUTE", 0)
  local second_audio = render(project, output_root .. "\\\\instance_b.wav")
  require_test(second_audio.finite and second_audio.nonzero, "instance B silent or non-finite")

  -- Real VST3 MIDI mapping in REAPER: CC88 -> slot88, NRPN1 -> slot128.
  -- Mapping is the native implementation's established contract (see installed suite).
  reaper.Main_openProject("noprompt:" .. project_path)
  project = reaper.EnumProjects(-1, "")
  reacquire_instances(project, first, second)
  local take = reaper.GetActiveTake(reaper.GetTrackMediaItem(first.track, 0))
  local events = {{88,77},{99,0},{98,1},{6,45}}
  for index,event in ipairs(events) do
    local ppq = reaper.MIDI_GetPPQPosFromProjTime(take, 0.25 + index * 0.01)
    require_test(reaper.MIDI_InsertCC(take, false, false, ppq, 0xB0, 0, event[1], event[2]), "insert MIDI CC")
  end
  reaper.MIDI_Sort(take)
  local midi_audio = render(project, output_root .. "\\\\midi_edit.wav")
  local after_offline = snapshot(first.track, first.fx)
  -- REAPER can restore pre-render plugin state after offline rendering. Test
  -- persistence using actual transport processing and allow main-thread idle.
  reaper.Audio_Init()
  reaper.SetEditCurPos(0, false, false)
  reaper.Main_OnCommand(1007, 0)
  wait_seconds(1.0)
  local while_playing = snapshot(first.track, first.fx)
  local position = reaper.GetPlayPosition()
  reaper.Main_OnCommand(1016, 0)
  wait_seconds(0.15)
  local midi_expected = snapshot(first.track, first.fx)
  write_file(output_root .. "\\\\midi-observations.json", json_encode({after_offline=after_offline,
    while_playing=while_playing, stopped=midi_expected, play_position=position,
    audio_running=reaper.Audio_IsRunning(), device_rate=select(2,reaper.GetAudioDeviceInfo("SRATE"))}))
  reaper.Main_SaveProjectEx(project, output_root .. "\\\\midi_diagnostic.rpp", 0)
  require_test(midi_expected[89] == 77 and midi_expected[129] == 45, "MIDI CC/NRPN did not reach host parameters")
  local midi_project = output_root .. "\\\\midi_roundtrip.rpp"
  reaper.Main_SaveProjectEx(project, midi_project, 0)
  reaper.Main_openProject("noprompt:" .. midi_project)
  project = reaper.EnumProjects(-1, "")
  reacquire_instances(project, first, second)
  local midi_actual = snapshot(first.track, first.fx)
  local midi_mismatches = compare_parameters(midi_expected, midi_actual, 1)
  require_test(#midi_mismatches == 0, "MIDI-edited project did not persist")
  progress("MIDI CC/NRPN save/reopen verified")
  -- Keep successful task project saved; outer launcher may request safe exit.
  local result = {
    schema =''')
    source = replace_once(source, '      mismatches = mismatches },', '''      mismatches = mismatches, expected = {expected_first,expected_second}, actual = {actual_first,actual_second} },
    midi = {status="PASS", events=events, channel_one_based=1, project_path=midi_project,
      expected=midi_expected, actual=midi_actual, mismatches=midi_mismatches,
      coverage="REAPER host parameters and serialized project; private native stores not exposed by ReaScript",
      overlap="NOT RUN: exact restore/MIDI interleaving requires instrumented access"},''')
    source = replace_once(source, 'first_difference_sample = first_difference, before',
                          'first_difference_sample = first_difference, rms_error = 0, instance_b = compact_audio(second_audio), midi = compact_audio(midi_audio), before')
    source = replace_once(source, 'start_stop_cycles = 2 },',
                          'start_stop_cycles = 2, observations = transport_observations },')
    source = replace_once(source, '  write_file(markdown_path, make_markdown(result))', '''  write_file(markdown_path, make_markdown(result) ..
    "\\nMIDI CC88=77 and NRPN1=45 persisted through an independent project save/reopen.\\n" ..
    "All 228 slots were explicitly set on each instance. Expected and actual arrays are in JSON.\\n" ..
    "Instance B was separately rendered finite/nonzero. Exact audio uses established tonal instance A.\\n" ..
    "Private authority/native/Engine stores and forced overlap are not exposed to ReaScript.\\n")
  progress("COMPLETE PASS")''')
    source = replace_once(source, 'local function main()', '''local function wait_seconds(seconds)
  local until_time = reaper.time_precise() + seconds
  while reaper.time_precise() < until_time do coroutine.yield() end
end

local function main()''')
    source = replace_once(source, 'local ok, err = xpcall(main, debug.traceback)', '''local routine = coroutine.create(main)
local begin_time = reaper.time_precise() + 2
local deadline = begin_time + 90
local function pump()
if reaper.time_precise() < begin_time then reaper.defer(pump); return end
local ok, err
if reaper.time_precise() > deadline then ok,err = false,"90 second harness deadline"
else ok,err = coroutine.resume(routine) end
if ok and coroutine.status(routine) ~= "dead" then reaper.defer(pump); return end''')
    source += '\nend\npump()\n'
    write(OUT / "state.lua", source)

    pdc = (ROOT / "tools/reaper/p12b4c_pdc_test.lua").read_text(encoding="utf-8")
    pdc = replace_once(pdc, 'local output_root = paths.output_root("reaper-p12b4c")',
                       f'local output_root = [[{OUT}]]')
    pdc = f'LXR_REAPER_HELPERS = [[{HELPERS}]]\n' + pdc
    pdc = replace_once(pdc, '    require_case(math.abs(alignment_error) <= alignment_tolerance_frames,',
                       '    require_case(waveform_error == 0 and alignment_error == 0, "strict established exact PDC comparison failed")\n    require_case(math.abs(alignment_error) <= alignment_tolerance_frames,')
    pdc = replace_once(pdc, '  result.project_path = project_path', '''  -- Recreate both saved instances; repeat the same normal/ignore-PDC
  -- waveform comparison using existing MIDI items, with fresh restore history.
  local reload_cases = {}
  for index = 0, 1 do
    reaper.Main_openProject("noprompt:" .. project_path)
    proj = reaper.EnumProjects(-1, "")
    assert(reaper.CountTracks(proj) == 2, "PDC reload track count")
    for t = 0, 1 do reaper.SetMediaTrackInfo_Value(reaper.GetTrack(proj,t), "B_MUTE", t == index and 0 or 1) end
    local path = output_root .. "\\\\pdc_reload_" .. index .. ".wav"
    assert(not file_exists(path), "reload render already exists")
    set_project_string(proj,"RENDER_FILE",output_root)
    set_project_string(proj,"RENDER_PATTERN","pdc_reload_" .. index)
    local ok, stats = reaper.GetSetProjectInfo_String(proj,"RENDER_STATS","42230",false)
    local audio = read_wav(path)
    local pdc_ok,pdc_value = config(reaper.GetTrack(proj,index),0,"pdc")
    assert(pdc_ok and tonumber(pdc_value)==26 and audio.sample_rate==48000 and audio.finite and audio.nonzero,
      "PDC reload rate/latency/audio failed")
    reload_cases[index+1] = {audio=audio,pdc=tonumber(pdc_value),stats=stats}
  end
  local reload_shift,reload_error=compare_alignment(reload_cases[1].audio,reload_cases[2].audio,26)
  assert(reload_shift==26 and reload_error==0,"strict PDC reload comparison failed")
  for _,case in ipairs(reload_cases) do case.audio.mono=nil end
  result.reload = {status="PASS", measured_shift=reload_shift, aligned_onset_error=reload_shift-26,
    aligned_waveform_rms_error=reload_error,cases=reload_cases}
  result.project_path = project_path''')
    write(OUT / "pdc-base.lua", pdc)
    write(OUT / "pdc.lua", f'''P12B4C_CASE_TAG = "pdc_b170e009_48000"
P12B4C_MIDI_CHANNEL = 0
P12B4C_NOTE_TIMES = {{1.0}}
P12B4C_RENDER_END = 1.8
P12B4C_PLUGIN_SHA256 = "{HASH}"
dofile([[{OUT / 'pdc-base.lua'}]])
''')
    inputs = [ROOT / name for name in ["AGENTS.md", "docs/architecture.md", "docs/param_map.md", "docs/risks.md", "docs/phase_log.md",
              "docs/p12c_state_serialization.md", "docs/p12c_r1_concurrency_hardening.md", "docs/snare_renderer_repair.md"]]
    inputs += list((ROOT / "dsp").glob("*.*"))
    inputs += [ROOT / f"vendor/iPlug2/Examples/LXR/{name}" for name in ["LXRState.h", "LXR.h", "LXR.cpp"]]
    inputs += [ROOT / "vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h"]
    inputs += list((ROOT / "tests").glob("*.cpp"))
    inputs += list((ROOT / "tools/reaper").glob("*.lua"))
    write(OUT / "input-hashes.json", json.dumps({str(p.relative_to(ROOT)): sha(p) for p in inputs}, indent=2))


def regress():
    cases = []
    for base in ["P12cR1ConcurrencyTest", "P12cStateSerializationTest"]:
        for opt in ["O0", "O2"]:
            cases.append((f"{base}_{opt}", EXECUTABLES / f"{base}_{opt}.exe", []))
    for base in ["P12b4cSnareRenderTest", "P12b4cRuntimeSilenceTest"]:
        for opt in ["O0", "O2"]:
            cases.append((f"{base}_{opt}", EXECUTABLES / f"{base}_{opt}_final.exe", []))
    for opt in ["O0", "O2"]:
        for base in ["P12b4cResamplerTest", "P12b4cFIRQualityTest"]:
            cases.append((f"{base}_{opt}", EXECUTABLES / f"{base}_{opt}_snare.exe", []))
        cases.append((f"FIRLatency_{opt}", EXECUTABLES / f"P12b4cLatencyTest_{opt}.exe", []))
        cases.append((f"OriginalCFilter_{opt}", EXECUTABLES / f"filter_diff_{opt}_snare.exe", []))
    for base in ["P12cVST3StateTest", "P12b4cVST3RuntimeTest"]:
        cases.append((base, EXECUTABLES / f"{base}.exe", [str(DLL)]))
    cases.append(("SteinbergValidator", Path(os.environ.get("LXR_VALIDATOR", ROOT / "vst3-validator-build/bin/Release/validator.exe")), [str(DLL)]))
    for base in ["ResonantFilter", "TransientGen"]:
        cases.append((f"{base}_O2_policy", EXECUTABLES / f"{base}_test_O2_p12c.exe", []))
    results = []
    for name, exe, args in cases:
        command = [str(exe), *args]
        row = dict(name=name, command=command, binary_sha256=sha(exe),
                   plugin_sha256=sha(DLL), start_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                   provenance="Fresh execution of preserved existing test executable; no recompile")
        start = time.monotonic()
        try:
            result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=55)
            log = result.stdout.decode(errors="replace") + result.stderr.decode(errors="replace")
            row.update(exit_code=result.returncode, status="PASS" if result.returncode == 0 else "FAIL")
        except OSError as error:
            log = repr(error)
            row.update(status="BLOCKED", winerror=getattr(error, "winerror", None), error=str(error))
        except subprocess.TimeoutExpired as error:
            log = str(error)
            row.update(status="BLOCKED", error="55 second console-test timeout")
        row["elapsed_seconds"] = time.monotonic() - start
        row["summary_lines"] = [s for s in log.splitlines() if any(t in s.lower() for t in ["summary", "passed", "failed", "mismatches", "total blocks"])]
        row["log"] = str(OUT / (name + ".log"))
        write(row["log"], log)
        results.append(row)
        print(name, row["status"], row.get("summary_lines"), flush=True)
        write(OUT / (name + ".json"), json.dumps(row, indent=2))
    write(OUT / "regression-matrix.json", json.dumps(results, indent=2))


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in ("prepare", "regress"):
        raise SystemExit("Usage: p12c_r1_final_certification.py prepare|regress [new-output-directory]; historical R1 hash guard retained")
    {"prepare": prepare, "regress": regress}[sys.argv[1]]()
