#!/usr/bin/env python3
"""Measure allocation and stack bounds on one exact private P19-E DLL."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import shutil
import subprocess

from run_checkpoint_tests import msvc_environment, visual_studio


ROOT = pathlib.Path(__file__).resolve().parents[2]


def sha(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=pathlib.Path, required=True)
    parser.add_argument("--host-evidence", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()

    candidate = json.loads(args.candidate.resolve().read_text(encoding="utf-8"))
    module = pathlib.Path(candidate["module"])
    if sha(module) != candidate["sha256"]:
        raise SystemExit("candidate identity changed")
    host = args.host_evidence.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = host / "source/tests/P16F3ExactDllResourcesTest.cpp"
    if not source.is_file():
        shutil.copy2(ROOT / "tests/P16F3ExactDllResourcesTest.cpp", source)

    vs = visual_studio()
    env = msvc_environment(vs, "14.51")
    compiler = vs / "VC/Tools/Llvm/x64/bin/clang-cl.exe"
    sdk = ROOT / "vendor/iPlug2/Dependencies/IPlug/VST3_SDK"
    iplug = ROOT / "vendor/iPlug2"
    definitions = [
        "VST3_API", "IPLUG_EDITOR=1", "IPLUG_DSP=1", "IGRAPHICS_NANOVG",
        "IGRAPHICS_GL2", "WIN32", "_CRT_SECURE_NO_DEPRECATE",
        "_CRT_NONSTDC_NO_DEPRECATE", "NOMINMAX", "_WINDLL", "_MBCS",
        "LXR_R2_INTERNAL_PROBE",
    ]
    include_names = [
        "WDL", "IPlug", "IPlug/VST3", "IPlug/Extras", "IPlug/Extras/OSC",
        "IPlug/Extras/Synth", "IPlug/Extras/WebView",
        "Dependencies/Extras/nlohmann", "IGraphics", "IGraphics/Controls",
        "IGraphics/Platforms", "IGraphics/Drawing",
        "Dependencies/IGraphics/NanoVG/src", "Dependencies/IGraphics/NanoSVG/src",
        "Dependencies/IGraphics/STB", "Dependencies/IGraphics/glad_GL2/include",
    ]
    common = [
        str(compiler), "/nologo", "/std:c++17", "/O2", "/MD", "/GR-",
        "/EHs-c-", "/DNDEBUG", "/I" + str(sdk),
        *["/D" + value for value in definitions],
        *["/I" + str(iplug / value) for value in include_names],
        "/I" + str(host / "source/vendor/iPlug2/Examples/LXR"),
    ]
    obj = output / "probe.obj"
    exe = output / "P16F3ExactDllResourcesTest.exe"
    compile_command = [*common, "/c", str(source), "/Fo" + str(obj)]
    link_command = [
        str(compiler), str(obj), str(host / "host/memorystream.obj"),
        str(host / "host/PatternBank.obj"),
        str(host / "host/PatternStateAuthority.obj"), "/Fe" + str(exe),
        "/link", "/LIBPATH:" + str(host / "sdk-build/lib/Release"),
        "sdk_hosting.lib", "sdk_common.lib", "sdk.lib", "base.lib",
        "pluginterfaces.lib", "ole32.lib", "uuid.lib", "user32.lib",
        "/STACK:1048576",
    ]
    build_log = ""
    for command in (compile_command, link_command):
        result = subprocess.run(command, cwd=host / "source", env=env,
                                text=True, capture_output=True)
        build_log += result.stdout + result.stderr
        if result.returncode:
            (output / "build.log").write_text(build_log, encoding="utf-8")
            return result.returncode
    (output / "build.log").write_text(build_log, encoding="utf-8")

    command = [str(exe), str(module)]
    result = subprocess.run(command, cwd=ROOT, env=env,
                            text=True, capture_output=True, timeout=180)
    log = result.stdout + result.stderr
    (output / "run.log").write_text(log, encoding="utf-8")
    summary = re.search(r"SUMMARY pass=(\d+) fail=(\d+)", log)
    measurements = [
        {"scope": name, "allocations": int(allocations), "bytes": int(byte_count),
         "stack_watermark": int(stack)}
        for name, allocations, byte_count, stack in re.findall(
            r"MEASURE (\S+) allocations=(\d+) bytes=(\d+) stack_watermark=(\d+)", log)
    ]
    report = {
        "status": "PASS" if result.returncode == 0 else "FAIL",
        "candidate_sha256": candidate["sha256"],
        "module": str(module),
        "source": str(source),
        "source_sha256": sha(source),
        "compiler": str(compiler),
        "compile_commands": [compile_command, link_command],
        "command": command,
        "executable_sha256": sha(exe),
        "exit_code": result.returncode,
        "pass": int(summary.group(1)) if summary else None,
        "fail": int(summary.group(2)) if summary else None,
        "measurements": measurements,
    }
    (output / "results.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "pass": report["pass"],
                      "fail": report["fail"], "measurements": measurements}))
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
