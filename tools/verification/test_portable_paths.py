"""Portable helper regression tests. Requires Lua 5.4+ via --lua or LUA.

Uses a new scratch repository with spaces and mocked paths; never launches
REAPER, edits a project, installs a plugin, or writes old certification outputs.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lua", default=os.getenv("LUA") or shutil.which("lua"))
    parser.add_argument("--output", required=True)
    parser.add_argument("--plugin", default=os.getenv("LXR_VST3_BINARY"))
    args = parser.parse_args()
    if not args.lua or not Path(args.lua).is_file():
        parser.error("Lua 5.4+ interpreter required: use --lua or LUA")
    args.lua = str(Path(args.lua).resolve())
    root = Path(__file__).resolve().parents[2]
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    copy = output / "repository with spaces"
    shutil.copytree(root / "tools/reaper", copy / "tools/reaper", ignore=shutil.ignore_patterns("__pycache__"))
    shutil.copytree(root / "tools/verification", copy / "tools/verification", ignore=shutil.ignore_patterns("__pycache__"))
    shutil.copyfile(root / "tools/run_tests.bat", copy / "tools/run_tests.bat")
    required = ["AGENTS.md", "docs/architecture.md", "docs/param_map.md", "docs/risks.md", "docs/phase_log.md",
                "docs/p12c_state_serialization.md", "docs/p12c_r1_concurrency_hardening.md", "docs/snare_renderer_repair.md",
                "vendor/iPlug2/Examples/LXR/LXRState.h", "vendor/iPlug2/Examples/LXR/LXR.h",
                "vendor/iPlug2/Examples/LXR/LXR.cpp", "vendor/iPlug2/IPlug/VST3/IPlugVST3_Common.h"]
    for rel in required:
        destination = copy / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / rel, destination)
    fake_plugin = output / "fake bundle with spaces/Contents/x86_64-win/LXR.vst3"
    fake_plugin.parent.mkdir(parents=True)
    fake_plugin.write_bytes(b"PATH TEST ONLY - never loaded as a plugin")
    env = os.environ.copy()
    env.update(LXR_REPO_ROOT=str(copy), LXR_TEST_OUTPUT=str(output / "output with spaces"), LXR_VST3_BINARY=str(fake_plugin))
    module = copy / "tools/reaper/checkpoint_paths.lua"
    tests = []

    def run(name, command, expected=0, environment=None):
        invocation = command if isinstance(command, str) else list(map(str, command))
        result = subprocess.run(invocation, cwd=output, env=environment or env, capture_output=True, text=True, timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        passed = result.returncode == expected
        tests.append(dict(name=name, passed=passed, exit_code=result.returncode))
        assert passed, (name, result.stdout, result.stderr)

    parse_only = output / "parse-only.lua"
    parse_only.write_text('assert(loadfile(arg[1])); print("PASS Lua syntax")\n')
    for file in sorted((copy / "tools/reaper").glob("*.lua")):
        run("parse-" + file.stem, [args.lua, parse_only, file])
    lua = output / "paths.lua"
    lua.write_text('''local m = dofile(arg[1])
assert(m.repo_root == os.getenv("LXR_REPO_ROOT"))
assert(m.output_root("ignored") == os.getenv("LXR_TEST_OUTPUT"))
assert(m.plugin() == os.getenv("LXR_VST3_BINARY"))
assert(m.project("case.rpp") == m.join(os.getenv("LXR_TEST_OUTPUT"), "case.rpp"))
assert(m.join("a/b/", "c/d"):find("c", 1, true))
print("PASS explicit overrides and paths with spaces")
''')
    run("explicit-overrides", [args.lua, lua, module])
    infer = output / "infer.lua"
    infer.write_text('local m=dofile(arg[1]); assert(m.repo_root == arg[2]); print("PASS script-relative root")\n')
    clean = env.copy()
    clean.pop("LXR_REPO_ROOT", None)
    run("infer-root-outside-cwd", [args.lua, infer, module, copy], environment=clean)
    missing = output / "missing.lua"
    missing.write_text('local m=dofile(arg[1]); local ok,e=pcall(m.plugin); assert(not ok and e:find("LXR_VST3_BINARY",1,true)); print("PASS clear missing dependency error")\n')
    bad = env.copy()
    bad["LXR_VST3_BINARY"] = str(output / "missing plugin.vst3")
    run("missing-plugin", [args.lua, missing, module], environment=bad)
    badroot = env.copy()
    badroot["LXR_REPO_ROOT"] = str(output / "missing repository")
    rooterror = output / "root-error.lua"
    rooterror.write_text('local ok,e=pcall(dofile,arg[1]); assert(not ok and e:find("LXR_REPO_ROOT",1,true)); print("PASS missing root error")\n')
    run("missing-root", [args.lua, rooterror, module], environment=badroot)
    # Exercise dofile-based wrappers without executing the REAPER harness.
    wrapper = output / "wrapper.lua"
    wrapper.write_text('''local real_dofile = dofile
local intercepted = false
dofile = function(path)
 if path:match("checkpoint_paths.lua$") then return real_dofile(path) end
 assert(path:match("p12b4c_pdc_test.lua$")); assert(io.open(path,"rb")); intercepted=true
end
assert(loadfile(arg[1]))()
assert(intercepted)
''')
    for name in ("p12b4c_pdc_smoke", "p12b4c_snare_other_voice", "p12b4c_snare_other_voice_final",
                 "p12b4c_snare_reaper", "p12b4c_snare_reaper_final", "p12c_fir_pdc_regression"):
        run("wrapper-" + name, [args.lua, wrapper, copy / "tools/reaper" / (name + ".lua")])
    runner = root / "tools/verification/run_checkpoint_tests.py"
    run("existing-output-refused", [sys.executable, runner, "--suite", "native", "--output", output], expected=2)
    run("missing-compiler", [sys.executable, runner, "--suite", "native", "--output", output / "missing compiler case",
                            "--cc", output / "compiler does not exist"], expected=2)
    assert "Missing gcc" in (output / "missing-compiler.log").read_text(encoding="utf-8")
    if os.name == "nt":
        run("batch-wrapper-outside-cwd", f'cmd.exe /d /s /c ""{copy / "tools/run_tests.bat"}" --help"')
    import importlib.util
    import hashlib
    spec = importlib.util.spec_from_file_location("historical_analyzer", copy / "tools/reaper/p12c_r1_analyze_certification.py")
    analyzer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(analyzer)
    saved_map = os.environ.get("LXR_EVIDENCE_PATH_MAP")
    try:
        os.environ["LXR_EVIDENCE_PATH_MAP"] = json.dumps({"historical evidence root": str(output)})
        relocated = output / "relocated.bin"
        relocated.write_bytes(b"historical path remapping fixture")
        assert analyzer.digest("historical evidence root/relocated.bin") == hashlib.sha256(relocated.read_bytes()).hexdigest().upper()
        tests.append(dict(name="historical-path-remap", passed=True, exit_code=0))
        try:
            analyzer.digest("historical evidence root/absent.bin")
        except FileNotFoundError as error:
            assert "LXR_EVIDENCE_PATH_MAP" in str(error)
            tests.append(dict(name="missing-historical-dependency", passed=True, exit_code=0))
        else:
            raise AssertionError("missing historical dependency was accepted")
    finally:
        if saved_map is None:
            os.environ.pop("LXR_EVIDENCE_PATH_MAP", None)
        else:
            os.environ["LXR_EVIDENCE_PATH_MAP"] = saved_map
    if args.plugin:
        actual = Path(args.plugin).resolve()
        import hashlib
        prepare_env = env.copy()
        prepare_env["LXR_VST3_BINARY"] = str(actual)
        prepare_env["LXR_EXPECTED_PLUGIN_SHA256"] = hashlib.sha256(actual.read_bytes()).hexdigest().upper()
        generated = copy / "out/generator path test"
        run("python-prepare", [sys.executable, copy / "tools/reaper/p12c_r1_final_certification.py", "prepare", generated], environment=prepare_env)
        for file in generated.glob("*.lua"):
            run("generated-parse-" + file.stem, [args.lua, parse_only, file])
    (output / "results.json").write_text(json.dumps(tests, indent=2))
    print(f"PASS {len(tests)} portable-path checks; REAPER was not launched")


if __name__ == "__main__":
    main()
