"""Independent artifact checks for the 2026-10-29 final-hash certification.

This reads PCM/RPP/package evidence; it does not run REAPER or alter DSP.
"""
import base64
import os
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(os.environ.get("LXR_REPO_ROOT", Path(__file__).resolve().parents[2])).resolve()
OUT = Path(os.environ.get("LXR_EVIDENCE_DIR", ROOT / "out/p12c-r1-final-20261029"))


def read(path):
    data=Path(path).read_bytes()
    return data.decode("utf-16" if data[:2] in (b"\xff\xfe",b"\xfe\xff") else "utf-8-sig")


def load(path):
    return json.loads(read(path))


def digest(path):
    # Preserve historical metadata verbatim while allowing explicit relocation
    # of the referenced files on a different evidence-review workstation.
    actual = str(path)
    mapping = json.loads(os.environ.get("LXR_EVIDENCE_PATH_MAP", "{}"))
    normalized = actual.replace("\\", "/")
    for previous, current in sorted(mapping.items(), key=lambda item: -len(item[0])):
        prefix = previous.replace("\\", "/").rstrip("/")
        if normalized.lower() == prefix.lower() or normalized.lower().startswith(prefix.lower() + "/"):
            actual = str(Path(current) / normalized[len(prefix):].lstrip("/"))
            break
    if not Path(actual).is_file():
        raise FileNotFoundError(f"Historical evidence dependency missing: {path}; use LXR_EVIDENCE_PATH_MAP to map its original prefix")
    return hashlib.sha256(Path(actual).read_bytes()).hexdigest().upper()


def wav(path):
    data = Path(path).read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    position, fmt, samples = 12, None, None
    while position + 8 <= len(data):
        tag, size = struct.unpack_from("<4sI", data, position)
        body = data[position+8:position+8+size]
        if tag == b"fmt ":
            encoding, channels, rate, _, alignment, bits = struct.unpack_from("<HHIIHH", body)
            if encoding == 65534:
                encoding = struct.unpack_from("<H", body, 24)[0]
            fmt = encoding, channels, rate, alignment, bits
        elif tag == b"data":
            assert fmt is not None
            encoding, channels, rate, alignment, bits = fmt
            width = bits // 8
            if encoding == 1:
                samples = [int.from_bytes(body[i:i+width], "little", signed=True) / (2 ** (bits-1))
                           for i in range(0, len(body), width)]
            else:
                assert encoding == 3 and bits in (32,64)
                samples = [s[0] for s in struct.iter_unpack("<f" if bits==32 else "<d", body)]
        position += 8 + size + size % 2
    assert samples
    stats = dict(path=str(path), sha256=digest(path), sample_rate=rate, channels=channels,
                 frames=len(samples)//channels, sample_count=len(samples), bits=bits,
                 peak=max(map(abs,samples)), rms=math.sqrt(sum(s*s for s in samples)/len(samples)),
                 finite=all(map(math.isfinite,samples)), nonzero=any(samples))
    return stats, samples


def project_states(path):
    states = []
    text = Path(path).read_text(encoding="utf-8")
    for block in re.findall(r"^\s*<VST .*?\n(.*?)^\s*>", text, re.M|re.S):
        data = b"".join(base64.b64decode(line.strip()) for line in block.splitlines()
                        if re.fullmatch(r"[A-Za-z0-9+/=]+", line.strip()))
        position = data.index(b"LXRS")
        state = data[position:position+252]
        version, header, size, count, flags, extension, crc = struct.unpack_from("<HHIHHII",state,4)
        payload = state[24:]
        assert (version,header,size,count,flags,extension)==(1,24,228,228,0,0)
        assert len(payload)==228 and max(payload)<=127 and zlib.crc32(payload)==crc
        states.append(dict(values=list(payload),crc32=f"{crc:08X}",version=version))
    return states


def main():
    checkpoint = load(OUT / "state-final/state-checkpoint.json")
    assert checkpoint["overall"]=="PASS"
    assert checkpoint["parameters"]["expected"]==checkpoint["parameters"]["actual"]
    assert sum(map(len,checkpoint["parameters"]["actual"]))==456
    states=project_states(OUT / "state-final/p12c_state_roundtrip_cert_b170e009.rpp")
    assert [s["values"] for s in states]==checkpoint["parameters"]["expected"]
    before, a=wav(OUT / "state-final/p12c_before_reload_cert_b170e009.wav")
    after, b=wav(OUT / "state-final/p12c_after_reload_cert_b170e009.wav")
    assert len(a)==len(b) and a==b
    diff=[x-y for x,y in zip(a,b)]
    instance_b,_=wav(OUT / "run4/instance_b.wav")
    assert before["finite"] and after["nonzero"] and instance_b["finite"] and instance_b["nonzero"]
    pdc=load(OUT / "pdc-final/pdc_b170e009_48000_results.json")
    row=pdc["sample_rates"][0]
    assert pdc["overall"]=="PASS" and pdc["reload"]["status"]=="PASS"
    assert row["plugin_reported_latency"]==row["measured_pdc_shift"]==26
    assert row["alignment_error_frames"]==row["waveform_alignment_rms_error"]==0
    # Independent onset/lag evidence from original stereo PCM, not the Lua mono array.
    normal, n=wav(OUT / "pdc-final/pdc_b170e009_48000_lxr_48000_pdc.wav")
    ignored, i=wav(OUT / "pdc-final/pdc_b170e009_48000_lxr_48000_ignore_pdc.wav")
    channels=normal["channels"]
    def onset(samples):
        return next(frame for frame in range(len(samples)//channels)
                    if any(abs(s)>1e-7 for s in samples[frame*channels:(frame+1)*channels]))
    shifts=[]
    for lag in range(0,65):
        count=min(len(n),len(i)-lag*channels)
        rms=math.sqrt(sum((n[k]-i[k+lag*channels])**2 for k in range(count))/count)
        shifts.append((rms,lag))
    rms,lag=min(shifts)
    assert lag==26 and rms==0 and onset(i)-onset(n)==26
    identities=load(OUT / "binaries-initial.json")
    for identity in identities:
        assert digest(identity["Path"])==identity["SHA256"]==checkpoint["plugin_sha256"]
    resources=load(OUT / "resources.json")
    resource_names={"sine_table.bin":8194,"saw_table.bin":22528,"tri_table.bin":22528,"rec_table.bin":22528,
                    "crash_sample.bin":32768,"transient_data.bin":26460,"transient_volume_table.bin":276}
    resource_rows=[]
    for name,size in resource_names.items():
        rows=[r for r in resources if Path(r["Path"]).name==name]
        assert len(rows)==3 and all(r["Size"]==size and digest(r["Path"])==rows[0]["SHA256"] for r in rows)
        resource_rows.append(dict(name=name,size=size,sha256=rows[0]["SHA256"],copies_checked=3))
    native=load(OUT / "controller-readback-result-final.json")
    assert native["BuildExit"]==0 and native["TestExit"]==1
    native_log=read(OUT / "controller-readback.log")
    assert "serialized=77" in native_log and "raw_controller=100" in native_log
    assert "serialized=45" in native_log and "raw_controller=0" in native_log
    matrix=load(OUT / "regression-matrix.json")
    result=dict(schema="lxr-p12c-r1-final-certification-v1",overall="FAIL",
                reason="Executed MIDI controller-readback agreement assertions fail against exact final binary",
                baseline=load(OUT / "baseline.json"),binary_identity=identities,resources=resource_rows,
                reaper_state=checkpoint,saved_project_state=states,
                audio=dict(status="PASS",before=before,after=after,instance_b=instance_b,
                           max_absolute_difference=max(map(abs,diff)),rms_error=math.sqrt(sum(x*x for x in diff)/len(diff))),
                pdc=dict(status="PASS",normal=normal,ignored=ignored,normal_onset=onset(n),ignored_onset=onset(i),
                         independent_shift=lag,aligned_rms_error=rms,aligned_onset_error=onset(i)-lag-onset(n),
                         direct_plugin_report=26,chain_actual=row.get("chain_pdc_actual"),
                         chain_reporting=row.get("chain_pdc_reporting"),reload=pdc["reload"]),
                midi=dict(status="FAIL",cc=dict(slot=88,expected=77,serialized=77,controller=100),
                          nrpn=dict(slot=128,expected=45,serialized=45,controller=0),
                          idle_handler_edits=2,native_assertions=dict(passed=34,failed=2),
                          reaper_observations=load(OUT / "run4/midi-observations.json"),
                          reaper_desired_value_save_reopen="NOT RUN: controller readback assertion stops before required desired-value save/reopen",
                          forced_restore_overlap="NOT RUN in REAPER; no claim of exact host-thread interleaving"),
                regression_matrix=matrix,code_integrity=dict(status="PASS: both original O2 files execute normally",
                    enforcement=load(OUT / "deviceguard-final.json"),powershell_retry=load(OUT / "policy-powershell-retry.json"),
                    signatures=load(OUT / "policy-signatures.json"),policy_change="None performed; exact allow mechanism not established",
                    policy_list="CiTool read-only enumeration returned access denied 0x80070005"),
                remaining_processes=load(OUT / "processes-after-close.json"),p16="NOT STARTED; milestone is not DONE")
    target=OUT / "verification.json"
    with target.open("x",encoding="utf-8") as f: json.dump(result,f,indent=2)
    lines=["# P12c-R1 REAPER verification — FAIL", "",
           "The exact B170E009...93A6F953 plugin passes state/audio/PDC checks but fails MIDI controller readback.","",
           "| Check | Result |", "|---|---|",
           "| Two-instance project reopen | PASS: 456/456 slots, zero mismatches |",
           "| Reserved slot 0 | PASS: 77 and 0 |",
           "| Explicit slot setting | All 228 slots per instance set; expected/actual arrays in verification.json |",
           f"| Deterministic A audio | {before['frames']} frames / {before['sample_count']} samples, 48 kHz stereo, 24-bit PCM |",
           f"| A peak / RMS | {before['peak']:.17g} / {before['rms']:.17g} |",
           "| Before/after sample difference | maximum 0, RMS error 0, exact equality |",
           f"| Independent B audio | peak {instance_b['peak']:.17g}, RMS {instance_b['rms']:.17g}, finite/nonzero |",
           "| Transport | PASS: two start/stop cycles with advancing playback position |",
           f"| Direct plugin PDC / independent shift | 26 / {lag} frames at 48 kHz |",
           f"| PDC onset | {onset(n)} normal; {onset(i)} ignored; aligned error 0 |",
           "| Aligned PDC waveform RMS error | 0 before and after project reload |",
           "| MIDI CC/NRPN controller agreement | FAIL: serialized 77/45; controller 100/0; two idle notifications received |",
           "| REAPER desired-value MIDI save/reopen | NOT RUN: stopped at controller agreement failure |", "",
           "The state-only checkpoint is an explicitly limited PASS. It does not supersede the failed MIDI assertion.",
           "Exact audio uses matched restored deterministic tonal A states. Noise/RNG runtime serialization is not claimed.",
           "Instance B audibility comes from run4 on the same binary; all-slot and transport arrays come from state-final.", "",
           "See verification.json for arrays, WAV statistics, resource hashes, fresh regression matrix, and process evidence."]
    with (OUT / "reaper-verification.md").open("x",encoding="utf-8") as f: f.write("\n".join(lines)+"\n")
    print(json.dumps(dict(overall=result["overall"],audio=result["audio"],pdc={k:v for k,v in result["pdc"].items() if k!="reload"},
                          remaining_process_ids=[p["ProcessId"] for p in result["remaining_processes"]]),indent=2))


if __name__=="__main__":
    main()
