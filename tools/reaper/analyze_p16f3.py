"""Independent REAPER chunk and PCM oracle. Never launches or changes a host."""
import argparse,base64,hashlib,json,pathlib,re,struct,zlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def state(path):
 s=path.read_text();m=re.search(r'(?m)^\s*<VST .*?\n(.*?)^\s*>',s,re.S);assert m,path
 b=b''.join(base64.b64decode(line.strip(),validate=True) for line in m[1].splitlines());start=b.find(b'LXRS');assert start>=0
 size=struct.unpack_from('<I',b,start+8)[0];assert size<=65536 and start+size+4<=len(b)
 wire=b[start:start+size+4];check=bytearray(wire[:-4]);crc=struct.unpack_from('<I',check,24)[0];struct.pack_into('<I',check,24,0);assert zlib.crc32(check)==crc
 modules={};pos=32
 while pos<size:
  tag,version,flags,n,reserved=struct.unpack_from('<IHHII',wire,pos);assert tag not in modules and n<=size-pos-16
  modules[tag]=wire[pos+16:pos+16+n];pos+=16+n
 assert pos==size and len(modules[2])==50449
 bank=modules[2];tmp=bytearray(bank);bank_crc=struct.unpack_from('<I',tmp,20)[0];struct.pack_into('<I',tmp,20,0);assert zlib.crc32(tmp)==bank_crc
 # Independent bank semantics: all7168 records,112 mask bytes and metadata.
 assert len(bank[32:32+50176])==7168*7
 lengths=bank[32+50176+112:32+50176+112+56];rotations=bank[32+50176+112+56:32+50176+112+112]
 assert max(lengths)<=15 and max(rotations)<=15
 chain=bank[-17:-1];assert all(v<=14 for v in chain[::2]) and all(v<=255 for v in chain[1::2])
 return wire,modules
def pcm(path):
 b=path.read_bytes();assert b[:4]==b'RIFF';pos=12;format=None;data=None
 while pos+8<=len(b):
  n=struct.unpack_from('<I',b,pos+4)[0];assert n<=len(b)-pos-8
  if b[pos:pos+4]==b'fmt ':format=struct.unpack_from('<HHIIHH',b,pos+8)
  if b[pos:pos+4]==b'data':data=b[pos+8:pos+8+n]
  pos+=8+n+n%2
 assert data is not None and format
 codec,ch,rate,byte_rate,align,bits=format;assert rate==48000 and ch==2
 samples=[]
 if codec==1 and bits==24:
  for i in range(0,len(data),3):v=int.from_bytes(data[i:i+3],'little',signed=True);samples.append(v/8388608)
 elif codec==3 and bits==32:samples=struct.unpack('<'+'f'*(len(data)//4),data)
 else:raise ValueError(format)
 return data,{'wav_bytes':len(b),'pcm_bytes':len(data),'frames':len(data)//align,'sample_rate':rate,'channels':ch,'bits':bits,'pcm_sha256':sha(data),'nonzero':any(data),'peak':max(map(abs,samples),default=0),'rms':(sum(x*x for x in samples)/len(samples))**.5}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--directory',required=True);ap.add_argument('--compare');a=ap.parse_args();d=(ROOT/a.directory).resolve()
 result={'directory':str(d),'source':'independent Python struct/base64/zlib','states':{},'pcm':{},'comparisons':{}}
 chunks={}
 for p in sorted(d.glob('*_chunk.txt')):
  try:w,m=state(p);chunks[p.stem]=(w,m);result['states'][p.stem]={'status':'PASS','bytes':len(w),'state_sha256':sha(w),'bank_sha256':sha(m[2]),'step_records':7168,'patch_routes_sha256':sha(m[1]),'transport_hex':m[3].hex(),'bypass':struct.unpack('<I',w[-4:])[0]}
  except Exception as e:result['states'][p.stem]={'status':'FAIL','error':str(e)}
 for left,right in [('saved_chunk','reopened_chunk'),('instance_one_chunk','reopened_instance_one_chunk'),('instance_two_chunk','reopened_instance_two_chunk')]:
  if left in chunks and right in chunks:result['comparisons'][left+'__'+right]={'status':'PASS' if chunks[left][0]==chunks[right][0] else 'FAIL','all_state_bytes_identical':chunks[left][0]==chunks[right][0]}
 fixture=(ROOT/'out/p16f3/serializer-001/fixtures/musical.lxqb').read_bytes()
 if 'initial_loaded_chunk' in chunks:result['comparisons']['musical_fixture_bank_identity']={'status':'PASS' if chunks['initial_loaded_chunk'][1][2]==fixture else 'FAIL','expected':sha(fixture),'actual':sha(chunks['initial_loaded_chunk'][1][2])}
 for p in sorted(d.glob('*.wav')):
  data,metrics=pcm(p);result['pcm'][p.stem]=metrics
  if a.compare:
   prior=(ROOT/a.compare/p.name)
   if prior.exists():old,_=pcm(prior);index=next((i for i,(x,y) in enumerate(zip(data,old)) if x!=y),None);equal=data==old;result['comparisons']['callback_sizes_'+p.stem]={'status':'PASS' if equal else 'FAIL','pcm_identical':equal,'first_differing_byte':index}
 result['status']='PASS' if all(r['status']=='PASS' for section in ['states','comparisons'] for r in result[section].values()) else 'FAIL'
 (d/'independent_analysis.json').write_text(json.dumps(result,indent=2));print(json.dumps({'status':result['status'],'states':len(result['states']),'wav_files':len(result['pcm']),'comparisons':len(result['comparisons'])}));return 0 if result['status']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
