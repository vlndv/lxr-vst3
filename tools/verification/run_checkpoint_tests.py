"""Explicit, source-built checkpoint tests; no install, Git writes or cleanup.

Python 3.10+, GCC/MinGW C/C++17 for native tests; VS/MSVC and SDK CMake
for Windows VST3 interface tests. Every invocation requires a NEW output path.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
MANIFEST = json.loads((HERE / "test_manifest.json").read_text(encoding="utf-8"))


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest().upper()


def find_tool(value, name):
    result = (shutil.which(value) or value) if value else shutil.which(name)
    if not result or not Path(result).is_file():
        raise RuntimeError(f"Missing {name}; supply its command-line override or put it on PATH")
    return str(Path(result).resolve())


def visual_studio(explicit=None):
    if explicit:
        return Path(explicit).resolve()
    installer = Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not installer.is_file():
        raise RuntimeError("Visual Studio discovery unavailable; specify --vs-root")
    result = subprocess.check_output([str(installer), "-latest", "-products", "*", "-requires",
                                      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"])
    path = result.decode().strip()
    if not path:
        raise RuntimeError("Install Visual Studio C++ tools or specify --vs-root")
    return Path(path)


def msvc_environment(vs, version):
    setup = vs / "VC/Auxiliary/Build/vcvars64.bat"
    if not setup.is_file():
        raise RuntimeError(f"Missing Visual Studio environment script: {setup}")
    if not re.fullmatch(r"[0-9.]+", version) or any(c in str(setup) for c in '\r\n"%!'):
        raise RuntimeError("Unsupported characters in Visual Studio setup path/version")
    # Pass an exact Windows command line: list2cmdline would turn the nested
    # quotes into literal backslash-quote characters for cmd.exe.
    command = f'cmd.exe /d /s /c ""{setup}" -vcvars_ver={version} >nul && set"'
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError("vcvars64 failed: " + result.stdout + result.stderr)
    env = os.environ.copy()
    for line in result.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key] = value
    return env


class Runner:
    def __init__(self, args):
        self.args = args
        self.root = Path(args.root or HERE.parents[1]).resolve()
        self.out = Path(args.output).resolve()
        self.out.mkdir(parents=True, exist_ok=False)
        self.source = self.out / "source"
        self.rows = []
        self.commands = []
        self.environment = os.environ.copy()

    def command(self, name, command, cwd=None, env=None, test=False, timeout=240):
        command = list(map(str, command))
        started = time.monotonic()
        try:
            r = subprocess.run(command, cwd=cwd or self.source, env=env or self.environment,
                               capture_output=True, timeout=timeout)
            log = r.stdout.decode(errors="replace") + r.stderr.decode(errors="replace")
            code = r.returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            log, code = str(error), -1
        (self.out / (name + ".log")).write_text(log, encoding="utf-8")
        row = dict(name=name, command=command, cwd=str(cwd or self.source), exit_code=code,
                   elapsed_seconds=time.monotonic() - started,
                   status="PASS" if code == 0 else "FAIL", fresh_source_build=True,
                   summary=[line for line in log.splitlines() if any(token in line.lower()
                            for token in ("summary", "mismatches", "tests passed"))])
        self.commands.append(row)
        if test:
            self.rows.append(row)
            print(name, row["status"], row["summary"][-1:], flush=True)
        elif code:
            raise RuntimeError(f"Build command {name} failed; see {self.out / (name + '.log')}")
        self.save()
        return code

    def save(self):
        (self.out / "commands.json").write_text(json.dumps(self.commands, indent=2))
        (self.out / "results.json").write_text(json.dumps(self.rows, indent=2))

    def snapshot(self):
        for directory in ("dsp", "tests", "data"):
            shutil.copytree(self.root / directory, self.source / directory,
                            ignore=shutil.ignore_patterns("__pycache__", "*.exe", "*.obj", "*.o"))
        overlay = self.source / "vendor/iPlug2/Examples/LXR"
        overlay.mkdir(parents=True)
        for name in ("LXR.h", "LXRState.h", "LXRParamContract.h", "config.h"):
            shutil.copyfile(self.root / "vendor/iPlug2/Examples/LXR" / name, overlay / name)
        # The production project supplies projects/ as include context for its
        # five-parent DSP includes. A standalone probe has a different include
        # stack; adapt only include spellings in this disposable header copy so
        # it cannot accidentally import DSP headers from outside the snapshot.
        # Class declarations, layout, and repository production files are intact.
        header = overlay / "LXR.h"
        original = header.read_bytes()
        adapted = original.replace(b'"../../../../../dsp/', b'"../../../../dsp/')
        header.write_bytes(adapted)
        (self.out / "probe-header-adaptation.json").write_text(json.dumps(dict(
            original_sha256=hashlib.sha256(original).hexdigest().upper(),
            adapted_sha256=digest(header), only_change='include prefix: five parents to four in disposable LXR.h copy'), indent=2))
        manifest = []
        for p in sorted(self.source.rglob("*")):
            if p.is_file():
                manifest.append(dict(path=str(p.relative_to(self.source)), sha256=digest(p)))
        (self.out / "source-manifest.json").write_text(json.dumps(manifest, indent=2))

    def native(self):
        args = self.args
        cc, cxx = find_tool(args.cc or os.getenv("CC"), "gcc"), find_tool(args.cxx or os.getenv("CXX"), "g++")
        self.environment["PATH"] = str(Path(cxx).parent) + os.pathsep + self.environment.get("PATH", "")
        reference = Path(args.firmware or os.getenv("LXR_FIRMWARE_ROOT", self.root / "ref/LXR-0.37")).resolve()
        refdsp = reference / "mainboard/LxrStm32/src/DSPAudio"
        if not (refdsp / "Snare.c").is_file():
            raise RuntimeError("Missing original firmware; run prepare_dependencies.py --firmware or use --firmware")
        pins = json.loads((HERE / "dependencies.json").read_text(encoding="utf-8"))
        if (reference / ".git").exists():
            actual = subprocess.check_output(["git", "-C", str(reference), "rev-parse", "HEAD"]).decode().strip()
            dirty = subprocess.check_output(["git", "-C", str(reference), "status", "--porcelain"])
            if actual != pins["firmware"]["revision"] or dirty:
                raise RuntimeError("Original firmware must be clean at the pinned revision")
        else:
            receipt = reference / ".lxr-source-receipt.json"
            if not receipt.is_file() or json.loads(receipt.read_text(encoding="utf-8"))["revision"] != pins["firmware"]["revision"]:
                raise RuntimeError("Firmware without Git metadata needs the pinned acquisition receipt")
        shutil.copytree(reference / "mainboard/LxrStm32/src", self.source / "ref/LXR-0.37/mainboard/LxrStm32/src")
        refdsp = self.source / "ref/LXR-0.37/mainboard/LxrStm32/src/DSPAudio"
        for name, compiler in (("gcc-version", cc), ("gxx-version", cxx)):
            self.command(name, [compiler, "--version"])
        suffix = ".exe" if os.name == "nt" else ""
        for opt in MANIFEST["configurations"]:
            obj = self.out / opt / "obj"
            obj.mkdir(parents=True)
            flags = MANIFEST["native_flags"] + ["-" + opt, "-I" + str(self.source / "dsp")]
            sources = [self.source / "dsp" / (name + ".cpp") for name in MANIFEST["dsp_sources"]]
            self.command("compile-dsp-" + opt, [cxx, *flags, "-c", *sources], cwd=obj)
            objects = [obj / (name + ".o") for name in MANIFEST["dsp_sources"]]
            cases = [(name + "Test", self.source / "dsp" / (name + "Test.cpp")) for name in MANIFEST["core_tests"]]
            cases += [(name, self.source / "tests" / (name + ".cpp")) for name in MANIFEST["native_tests"]]
            for name, source in cases:
                exe = obj.parent / (name + suffix)
                overrides = MANIFEST["test_flag_overrides"].get(name, [])
                self.command("link-" + name + "-" + opt, [cxx, *flags, *overrides, source, *objects, "-o", exe])
                self.command(name + "_" + opt, [exe], test=True, timeout=55)
            original_snare = obj / "original_snare.o"
            cflags = MANIFEST["reference_c_flags"] + ["-" + opt]
            self.command("compile-original-snare-" + opt, [cc, *cflags,
                "-I" + str(self.source / "tests/reference/snare/compat"), "-I" + str(refdsp), "-c",
                self.source / "tests/reference/snare/original_snare_harness.c", "-o", original_snare])
            exe = obj.parent / ("P12b4cSnareRenderTest" + suffix)
            self.command("link-snare-" + opt, [cxx, *flags, self.source / "tests/P12b4cSnareRenderTest.cpp",
                         *objects, original_snare, "-o", exe])
            self.command("P12b4cSnareRenderTest_" + opt, [exe], test=True, timeout=55)
            original_filter = obj / "original_filter.o"
            self.command("compile-original-filter-" + opt, [cc, *cflags,
                "-I" + str(self.source / "tests/reference/filter/compat"), "-I" + str(refdsp), "-c",
                refdsp / "ResonantFilter.c", "-o", original_filter])
            exe = obj.parent / ("OriginalCFilter" + suffix)
            self.command("link-filter-" + opt, [cxx, *flags, self.source / "tests/reference/filter/diff_test.cpp",
                         *objects, original_filter, "-o", exe])
            self.command("OriginalCFilter_" + opt, [exe], test=True, timeout=55)

    def host(self):
        if os.name != "nt":
            raise RuntimeError("The certified VST3 host procedure requires Windows x64")
        args = self.args
        sdk = Path(args.sdk or os.getenv("LXR_VST3_SDK", self.root / "vendor/iPlug2/Dependencies/IPlug/VST3_SDK")).resolve()
        iplug = Path(args.iplug or os.getenv("LXR_IPLUG2_ROOT", self.root / "vendor/iPlug2")).resolve()
        if not (sdk / "public.sdk/source/common/memorystream.cpp").is_file():
            raise RuntimeError("Missing VST3 SDK source; acquire pinned dependencies or use --sdk")
        plugin = Path(args.plugin or os.getenv("LXR_VST3_BINARY", Path(os.getenv("LOCALAPPDATA", "")) /
                      "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3")).resolve()
        if not plugin.is_file():
            raise RuntimeError("Missing VST3 executable; specify --plugin (no installation is performed)")
        before = digest(plugin)
        (self.out / "plugin-before.json").write_text(json.dumps(dict(path=str(plugin), sha256=before), indent=2))
        vs = visual_studio(args.vs_root)
        env = msvc_environment(vs, args.tools_version)
        cmake = find_tool(args.cmake or os.getenv("CMAKE") or
                          str(vs / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"), "cmake")
        build = self.out / "sdk-build"
        self.command("configure-sdk", [cmake, "-S", sdk, "-B", build, "-G", args.generator, "-A", "x64",
            "-T", args.toolset, "-DSMTG_ENABLE_VSTGUI_SUPPORT=OFF", "-DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF",
            "-DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON", "-DSMTG_CREATE_PLUGIN_LINK=OFF"], env=env)
        self.command("build-sdk-hosting", [cmake, "--build", build, "--config", "Release", "--target", "validator", "sdk_hosting", "sdk"], env=env)
        (self.out / "toolchain.json").write_text(json.dumps(dict(visual_studio=str(vs),
            vc_tools_version=env.get("VCToolsVersion"), windows_sdk=env.get("WindowsSDKVersion"),
            generator=args.generator, toolset=args.toolset, sdk=str(sdk), iplug=str(iplug)), indent=2))
        cl = shutil.which("cl.exe", path=env.get("Path", env.get("PATH")))
        if not cl:
            raise RuntimeError("cl.exe missing after vcvars64")
        self.command("cl-version", [cl, "/?"], env=env)
        hostout = self.out / "host"
        hostout.mkdir()
        common = ["/nologo", "/std:c++17", "/O2", "/MD", "/GR-", "/EHs-c-", "/DNDEBUG", "/I" + str(sdk)]
        memory = hostout / "memorystream.obj"
        self.command("compile-memorystream", [cl, *common, "/c", sdk / "public.sdk/source/common/memorystream.cpp", "/Fo" + str(memory)], env=env)
        cases = [(name, []) for name in MANIFEST["host_tests"]]
        definitions = ["VST3_API", "IPLUG_EDITOR=1", "IPLUG_DSP=1", "IGRAPHICS_NANOVG", "IGRAPHICS_GL2",
                       "WIN32", "_CRT_SECURE_NO_DEPRECATE", "_CRT_NONSTDC_NO_DEPRECATE", "NOMINMAX", "_WINDLL", "_MBCS", "LXR_R2_INTERNAL_PROBE"]
        includes = ["WDL", "IPlug", "IPlug/VST3", "IPlug/Extras", "IPlug/Extras/OSC", "IPlug/Extras/Synth",
                    "IPlug/Extras/WebView", "Dependencies/Extras/nlohmann", "IGraphics", "IGraphics/Controls",
                    "IGraphics/Platforms", "IGraphics/Drawing", "Dependencies/IGraphics/NanoVG/src",
                    "Dependencies/IGraphics/NanoSVG/src", "Dependencies/IGraphics/STB", "Dependencies/IGraphics/glad_GL2/include"]
        probe = ["/D" + definition for definition in definitions] + ["/I" + str(iplug / path) for path in includes]
        probe += ["/I" + str(self.source / "vendor/iPlug2/Examples/LXR")]
        cases.append(("ControllerSyncProbe", probe))
        for name, flags in cases:
            source = "P12cR2ControllerSyncTest" if name == "ControllerSyncProbe" else name
            exe = hostout / (name + ".exe")
            self.command("build-" + name, [cl, *common, *flags, self.source / "tests" / (source + ".cpp"), memory,
                "/Fo" + str(hostout / (name + ".obj")), "/Fe" + str(exe), "/link", "/LIBPATH:" + str(build / "lib/Release"),
                *MANIFEST["host_libraries"]], env=env)
            self.command(name, [exe, plugin], env=env, test=True, timeout=55)
        self.command("SteinbergValidator", [build / "bin/Release/validator.exe", plugin], env=env, test=True, timeout=55)
        assert digest(plugin) == before, "Plugin changed during read-only host tests"
        (self.out / "plugin-after.json").write_text(json.dumps(dict(path=str(plugin), sha256=digest(plugin)), indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="new directory; existing directories are refused")
    parser.add_argument("--root")
    parser.add_argument("--suite", choices=["native", "host", "all"], default="all")
    for key in ("cc", "cxx", "firmware", "sdk", "iplug", "plugin", "vs-root", "cmake"):
        parser.add_argument("--" + key)
    parser.add_argument("--tools-version", default="14.51")
    parser.add_argument("--toolset", default="v145")
    parser.add_argument("--generator", default="Visual Studio 18 2026")
    args = parser.parse_args()
    runner = None
    try:
        runner = Runner(args)
        runner.snapshot()
        if args.suite in ("native", "all"):
            runner.native()
        if args.suite in ("host", "all"):
            runner.host()
        return 1 if any(row["exit_code"] for row in runner.rows) else 0
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print("CHECKPOINT TEST ERROR:", error, file=sys.stderr)
        if runner:
            runner.save()
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
