"""Generate bounded task-only REAPER projects from independent canonical fixtures."""
import argparse,base64,hashlib,json,pathlib,re,struct,uuid,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
def crc(data,offset):
    data=bytearray(data);struct.pack_into('<I',data,offset,0);struct.pack_into('<I',data,offset,zlib.crc32(data));return data
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',required=True);ap.add_argument('--candidate',default='out/p16f3/private-build-001/candidate_manifest.json');ap.add_argument('--blocksize',type=int,choices=[127,512],default=127);ap.add_argument('--roll-suite',action='store_true');a=ap.parse_args();out=(ROOT/a.output).resolve();out.mkdir(parents=True,exist_ok=False)
    candidate=json.loads((ROOT/a.candidate).read_text());assert hashlib.sha256(pathlib.Path(candidate['module']).read_bytes()).hexdigest().upper()==candidate['sha256']
    template=(ROOT/'out/p16f2/reaper-isolated-002/accepted_patterns.rpp').read_text()
    match=re.search(r'(?m)^      <VST .*?\n(.*?)^      >',template,re.S);assert match
    original=b''.join(base64.b64decode(line.strip()) for line in match[1].splitlines());assert original[52:56]==b'LXRS'
    original_size=struct.unpack_from('<I',original,44)[0];assert original_size==51065
    def project(name,state):
        assert len(state)==51065 and state[:4]==b'LXRS'
        raw=original[:52]+state+original[52+original_size:]
        encoded=base64.b64encode(raw[:44]).decode()+'\n'+ '\n'.join(base64.b64encode(raw[i:i+96]).decode() for i in range(44,len(raw),96))
        text=template[:match.start(1)]+''.join('        '+line+'\n' for line in encoded.splitlines())+template[match.end(1):]
        text=text.replace('SAMPLERATE 44100 0 0','SAMPLERATE 48000 1 0').replace('P16-F2 production ingress','P16-F3 '+name)
        text=re.sub(r'\{[0-9A-Fa-f-]{36}\}',lambda m:'{'+str(uuid.uuid4()).upper()+'}',text)
        path=out/(name+'.rpp');path.write_text(text);return path
    files={}
    for p in sorted((ROOT/'out/p16f3/listening-002').glob('*.lxrs')):files[p.stem]=project(p.stem,p.read_bytes())
    musical=bytearray((ROOT/'out/p16f3/listening-002/01_drum1.lxrs').read_bytes())
    bank=(ROOT/'out/p16f3/serializer-001/fixtures/musical.lxqb').read_bytes()
    pos=32
    while pos<51061:
        tag,version,flags,size,reserved=struct.unpack_from('<IHHII',musical,pos)
        if tag==2:musical[pos+16:pos+16+size]=bank
        pos+=16+size
    musical[:51061]=crc(musical[:51061],24)
    files['musical']=project('musical',musical)
    empty=bytearray(musical);bank_at=empty.find(b'LXQB');assert bank_at>=0
    empty[bank_at+32+50176:bank_at+32+50176+112]=bytes(112)
    empty[bank_at:bank_at+50449]=crc(empty[bank_at:bank_at+50449],20);empty[:51061]=crc(empty[:51061],24)
    files['roll_empty']=project('roll_empty',empty)
    default=bytearray(musical);default[bank_at:bank_at+50449]=(ROOT/'out/p16f3/serializer-001/fixtures/default.lxqb').read_bytes();default[:51061]=crc(default[:51061],24)
    files['default_empty']=project('default_empty',default)
    random=bytearray(musical)
    at=random.find(b'LXQB');assert at>=0
    # 8*7*128 seven-byte positions, 56 u16 masks, 56 lengths,56 rotations.
    chains=at+32+50176+112+56+56
    random[chains]=8;random[chains+1]=0 # source code8: random pattern0..1
    bank_end=at+50449;random[at:bank_end]=crc(random[at:bank_end],20);random[:51061]=crc(random[:51061],24)
    files['random_next']=project('random_next',random)
    config=out/'config';config.mkdir();(config/'REAPER.ini').write_text('[reaper]\nvstpath64='+str(pathlib.Path(candidate['bundle']).parent)+'\nvst_scan=1\nvstfullstate=49989\nrenderclosewhendone=4\nwnd_state=0\nwnd_x=20\nwnd_y=40\nwnd_w=800\nwnd_h=800\n[audioconfig]\nmode=4\nallow_sr_override=1\ndummy_srate=48000\ndummy_blocksize=127\n[wave sink defaults]\ndefault_size=7\ndefault=65766177180001CC\n')
    ini=config/'REAPER.ini';ini.write_text(ini.read_text().replace('dummy_blocksize=127','dummy_blocksize='+str(a.blocksize)))
    t1=files['01_drum1'].read_text();t2=files['03_drum3'].read_text()
    second=re.search(r'(?ms)^  <TRACK .*?^  >',t2);assert second
    paired=t1.rstrip()[:-1]+second[0]+'\n>\n';files['two_instances']=out/'two_instances.rpp';files['two_instances'].write_text(paired)
    if a.roll_suite:
        text=files['roll_empty'].read_text();m=re.search(r'(?ms)^  <TRACK .*?^  >',text);assert m
        tracks=[]
        for selector in range(14):
            track=re.sub(r'\{[0-9A-Fa-f-]{36}\}',lambda m:'{'+str(uuid.uuid4()).upper()+'}',m[0])
            tracks.append(track.replace('P16-F3 roll_empty','P16-F3 Roll Selector '+str(selector)))
        text=text[:m.start()]+'\n'.join(tracks)+text[m.end():]
        assert len(re.findall(r'(?m)^  <TRACK ',text))==14
        files['rolls_all']=out/'rolls_all.rpp';files['rolls_all'].write_text(text)
    config_lua='return {root=[['+str(out)+']],module=[['+candidate['module']+']],sha256=[['+candidate['sha256']+']]}\n'
    (out/'settings.lua').write_text(config_lua)
    script=(ROOT/'tools/reaper/p16f3_final_acceptance.lua').read_text();(out/'acceptance.lua').write_text('local settings=dofile([['+str(out/'settings.lua')+']])\n'+script)
    if a.roll_suite:
        (out/'rolls.lua').write_text('local settings=dofile([['+str(out/'settings.lua')+']])\n'+(ROOT/'tools/reaper/p16f3_roll_counts.lua').read_text())
    manifest={name:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper()} for name,p in files.items()}
    (out/'project_manifest.json').write_text(json.dumps(manifest,indent=2));print(json.dumps({'output':str(out),'projects':len(files),'candidate':candidate['sha256']}))
if __name__=='__main__':main()
