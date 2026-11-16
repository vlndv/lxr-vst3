"""Read final PE/PDB identity, imports and x64 unwind stack allocations."""
import argparse,hashlib,json,pathlib,re,struct,subprocess,uuid
from run_checkpoint_tests import visual_studio
ROOT=pathlib.Path(__file__).resolve().parents[2]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--candidate',required=True);ap.add_argument('--output',required=True);a=ap.parse_args();out=(ROOT/a.output).resolve();out.mkdir(parents=True,exist_ok=False)
 manifest=json.loads((ROOT/a.candidate).read_text());module=pathlib.Path(manifest['module']);b=module.read_bytes();assert hashlib.sha256(b).hexdigest().upper()==manifest['sha256']
 u16=lambda p:struct.unpack_from('<H',b,p)[0];u32=lambda p:struct.unpack_from('<I',b,p)[0]
 pe=u32(0x3c);assert b[pe:pe+4]==b'PE\0\0' and u16(pe+4)==0x8664;optional=pe+24;assert u16(optional)==0x20b
 sections=[];pos=optional+u16(pe+20)
 for i in range(u16(pe+6)):
  s=pos+i*40;sections.append({'name':b[s:s+8].rstrip(b'\0').decode(),'vsize':u32(s+8),'rva':u32(s+12),'size':u32(s+16),'offset':u32(s+20)})
 def offset(rva):
  for s in sections:
   if s['rva']<=rva<s['rva']+max(s['vsize'],s['size']):return s['offset']+rva-s['rva']
  raise ValueError(hex(rva))
 def directory(index):return u32(optional+112+index*8),u32(optional+116+index*8)
 debug_rva,debug_size=directory(6);guid=None;age=None
 for i in range(debug_size//28):
  p=offset(debug_rva)+i*28
  if u32(p+12)==2:
   codeview=u32(p+24);assert b[codeview:codeview+4]==b'RSDS';guid=str(uuid.UUID(bytes_le=b[codeview+4:codeview+20])).upper();age=u32(codeview+20)
 pdbutil=visual_studio()/'VC/Tools/Llvm/x64/bin/llvm-pdbutil.exe';pdb=pathlib.Path(manifest['pdb'])
 summary=subprocess.check_output([pdbutil,'dump','-summary',pdb],text=True);publics=subprocess.check_output([pdbutil,'dump','-publics',pdb],text=True)
 (out/'pdb-summary.txt').write_text(summary);(out/'pdb-publics.txt').write_text(publics)
 assert guid in summary.upper() and re.search(r'Age:\s*'+str(age)+r'\b',summary)
 symbols={}
 for name,section,at in re.findall(r'S_PUB32[^\n]*`([^`]+)`\n\s*flags = [^\n]*addr = (\d+):(\d+)',publics):
  section=int(section);at=int(at)
  if 1<=section<=len(sections):symbols[sections[section-1]['rva']+at]=name
 import_symbols=[];rva,size=directory(1);p=offset(rva)
 while u32(p+12):
  names=offset(u32(p));i=0
  while True:
   value=struct.unpack_from('<Q',b,names+i*8)[0]
   if not value:break
   if value>>63==0:
    n=offset(value)+2;end=b.index(b'\0',n);import_symbols.append(b[n:end].decode())
   i+=1
  p+=20
 def allocation(rva):
  p=offset(rva);version=b[p]&7;flags=b[p]>>3;codes=b[p+2];i=0;total=0;unknown=[]
  while i<codes:
   q=p+4+i*2;op=b[q+1]&15;info=b[q+1]>>4
   if op==0:total+=8;i+=1
   elif op==1:
    if info==0:total+=u16(q+2)*8;i+=2
    elif info==1:total+=u32(q+2);i+=3
    else:unknown.append(op);break
   elif op==2:total+=info*8+8;i+=1
   elif op==3:i+=1
   elif op in [4,8]:i+=2
   elif op in [5,9]:i+=3
   elif op==10:total+=40+info*8;i+=1
   else:unknown.append(op);break
  return total,flags,version,unknown
 exception_rva,exception_size=directory(3);frames=[]
 for i in range(exception_size//12):
  p=offset(exception_rva)+i*12;begin,end,unwind=struct.unpack_from('<III',b,p)
  n,flags,version,unknown=allocation(unwind)
  frames.append({'begin_rva':begin,'end_rva':end,'symbol':symbols.get(begin,''),'fixed_prolog_stack_bytes':n,'chained':bool(flags&4),'version':version,'unknown_codes':unknown})
 selected=[f for f in frames if any(k in f['symbol'] for k in ['ProcessBlock@LXR','ApplyPendingStateAndParameters@LXR','ApplySequencerTransport@LXR','ApplyRollControls@LXR','ImportSequencerBank@LXR','SerializeState@LXR','UnserializeState@LXR','processNativeBoundary@TriggerScheduler','getNextHostST1Sample@LXRAudioAdapter'])]
 result={'candidate_sha256':manifest['sha256'],'pdb_sha256':hashlib.sha256(pdb.read_bytes()).hexdigest().upper(),'pe_pdb_guid':guid,'pe_pdb_age':age,'pdb_matches_pe':True,'imports':import_symbols,'allocator_imports':[x for x in import_symbols if x in ['HeapAlloc','HeapReAlloc','GlobalAlloc','CoTaskMemAlloc','VirtualAlloc','VirtualAllocEx','LocalAlloc','malloc','calloc','realloc','FlsAlloc']],'import_function_rva':next(r for r,n in symbols.items() if n.startswith('?ImportSequencerBank@LXR')),'timer_list_rva':next(r for r,n in symbols.items() if n.startswith('?sTimers@Timer_impl')),'selected_frames':selected,'largest_frames':sorted(frames,key=lambda f:f['fixed_prolog_stack_bytes'],reverse=True)[:20],'unwind_record_count':len(frames),'unknown_unwind_records':[f for f in frames if f['unknown_codes']],'unknown_unwind_record_count':sum(bool(f['unknown_codes'])for f in frames),'limits':'Per-function prolog storage only, not a whole-call-chain bound; chained records and dynamic stack require separate inspection. Runtime watermark is independently reported.'}
 (out/'binary_analysis.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:result[k]for k in ['candidate_sha256','pdb_matches_pe','import_function_rva','unwind_record_count','unknown_unwind_record_count']}))
if __name__=='__main__':main()
