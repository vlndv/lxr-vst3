"""Verify/acquire immutable source archives without Git checkout or overwrite.

No repository topology change: keep the existing LXR overlay and explicit
framework patch. Downloads and receipts live under ignored output paths.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
PINS = json.loads((HERE / "dependencies.json").read_text(encoding="utf-8"))


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def acquire(package, destination, cache, skip=(), verify=False):
    revision, repository = package["revision"], package["repository"]
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / (repository.replace("/", "-") + "-" + revision + ".zip")
    if not archive.exists():
        if verify:
            raise RuntimeError(f"Archive missing for verification: {archive}; run once without --verify-only")
        url = f"https://codeload.github.com/{repository}/zip/{revision}"
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        with archive.open("xb") as handle:
            handle.write(data)
    data = archive.read_bytes()
    if package.get("archive_sha256") and sha(data) != package["archive_sha256"]:
        raise RuntimeError(f"Archive hash mismatch: {archive}")
    count = 0
    with zipfile.ZipFile(io.BytesIO(data)) as source:
        for item in source.infolist():
            rel = "/".join(item.filename.split("/")[1:])
            if not rel or item.is_dir() or any(rel == p or rel.startswith(p + "/") for p in skip):
                continue
            path = destination / rel
            if not path.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError("Archive path escapes destination")
            expected = source.read(item)
            if path.exists():
                actual = path.read_bytes()
                if actual.replace(b"\r\n", b"\n") != expected.replace(b"\r\n", b"\n"):
                    raise RuntimeError(f"Refusing to overwrite differing dependency source: {path}")
            elif verify:
                raise RuntimeError(f"Missing dependency source: {path}")
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with path.open("xb") as handle:
                    handle.write(expected)
            count += 1
    receipt = dict(repository=repository, revision=revision, archive_sha256=sha(data), verified_files=count)
    print(repository, revision, count, "source files verified", flush=True)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=HERE.parents[1])
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--firmware", action="store_true", help="also acquire original non-commercial reference sources")
    parser.add_argument("--verify-only", action="store_true", help="no network or source writes; cached archives required")
    args = parser.parse_args()
    root = args.root.resolve()
    cache = (args.cache or root / "out/dependency-cache").resolve()
    vendor = root / "vendor/iPlug2"
    patch = vendor / PINS["framework_patch"]["path"]
    if not patch.is_file() or sha(patch.read_bytes().replace(b"\r\n", b"\n")) != PINS["framework_patch"]["normalized_lf_sha256"]:
        raise RuntimeError("The source-controlled framework state-reader patch is missing or changed")
    receipts = [acquire(PINS["iplug2"], vendor, cache,
                        skip=(PINS["framework_patch"]["path"], "Examples/LXR", "Dependencies/IPlug/VST3_SDK"),
                        verify=args.verify_only)]
    sdk = vendor / "Dependencies/IPlug/VST3_SDK"
    receipts.append(acquire(PINS["sdk"], sdk, cache, verify=args.verify_only))
    for path, (repository, revision) in PINS["sdk_modules"].items():
        receipts.append(acquire(dict(repository=repository, revision=revision), sdk / path, cache,
                                verify=args.verify_only))
    if args.firmware:
        destination = root / "ref/LXR-0.37"
        receipt = acquire(PINS["firmware"], destination, cache, verify=args.verify_only)
        receipts.append(receipt)
        if not args.verify_only and not (destination / ".git").exists():
            marker = destination / ".lxr-source-receipt.json"
            if not marker.exists():
                marker.write_text(json.dumps(receipt, indent=2))
    if not args.verify_only:
        manifest = cache / ("receipts-" + sha(str(root).encode())[:16] + ".json")
        if not manifest.exists():
            manifest.write_text(json.dumps(receipts, indent=2))
    print("Dependency sources match pins; existing overlay/patch preserved. No Git writes or source replacement.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print("DEPENDENCY ERROR:", error, file=sys.stderr)
        raise SystemExit(2)
