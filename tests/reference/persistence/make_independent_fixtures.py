"""Independent Python struct/zlib fixtures for the PROPOSED M2F test contract.
Does not import the C++ encoder/decoder. Inputs are the retained original-table
audit CSV and the pinned original AVR Cc2Text.c. Original identities Copyright
2013 Julian Schmidt; derived assets remain subject to LICENSE-LXR.txt.
"""
from pathlib import Path
import struct,zlib,csv,hashlib,json,argparse
args=argparse.ArgumentParser();args.add_argument('--output');options=args.parse_args()
R=Path(__file__).resolve().parents[3];D=Path(options.output).resolve() if options.output else Path(__file__).parent/'fixtures'
D.mkdir(parents=True,exist_ok=False)
audit=Path(__file__).parent/'original_target_fixture.csv'
rows=list(csv.DictReader(audit.open(encoding='utf-8')))
table=R/'ref/LXR-0.37/front/LxrAvr/Menu/Cc2Text.c';digest=hashlib.sha256(table.read_bytes()).digest()
patch=bytes((i*37+11)%128 for i in range(228))
def supported(p):
 return any(a<=p<=b for a,b in [(8,19),(27,59),(62,67),(70,77),(82,96),(99,113),(128,133),(191,196),(209,214)])
types={'TYPE_UINT8':0,'TYPE_FLT':1,'TYPE_SPECIAL_F':2,'TYPE_UINT32':3,'NONE':255}
def record(source,owner,row=None):
 if row is None:return struct.pack('<BBHHHHBBHBB',owner,source,0,0,0,0,255,0,0,0,0)
 key=min(int(r['parameter_id']) for r in rows if r['destination_symbol']==row['destination_symbol'])
 return struct.pack('<BBHHHHBBHBB',owner,source,1,1,int(row['index']),int(row['parameter_id']),types[row['firmware_type']],1,key,owner+1 if source else 0,0)
def fixture(active,transport):
 records=[];indices=[]
 for source in range(2):
  for owner in range(6):
   eligible=[r for r in rows if r['voice']==str(owner) and supported(int(r['parameter_id']))]
   row=eligible[-1] if active else None;indices.append(int(row['index']) if row else 0)
   records.append(record(source,owner,row))
 body=struct.pack('<HHHHI',1,16,12,1,1)+digest+struct.pack('<I',0)+b''.join(records)
 assert len(body)==240
 ext=struct.pack('<IHHII',1,1,1,len(body),0)+body
 header=struct.pack('<4sHHIHHIIII',b'LXRS',2,32,32+228+len(ext),228,0,228,len(ext),0,0)
 blob=bytearray(header+patch+ext);struct.pack_into('<I',blob,24,zlib.crc32(blob))
 if transport:blob+=struct.pack('<i',1)
 return blob,indices
active,indices=fixture(True,True);off,_=fixture(False,False)
v1=struct.pack('<4sHHIHHII',b'LXRS',1,24,228,228,0,0,zlib.crc32(patch))+patch+struct.pack('<i',1)
legacy=struct.pack('<228d',*patch)+struct.pack('<i',1)
files={'v2_active.bin':active,'v2_off.bin':off,'v1.bin':v1,'legacy.bin':legacy,'table_sha256.bin':digest}
for n,b in files.items():(D/n).write_bytes(b)
receipt=dict(method='Independent Python struct little-endian fields and standard-library zlib CRC; no C++ model import',
 original_table=str(table),original_table_sha256=digest.hex().upper(),target_csv_sha256=hashlib.sha256(audit.read_bytes()).hexdigest().upper(),
 active_indices=indices,patch_formula='(id*37+11)%128',files={n:dict(size=len(b),sha256=hashlib.sha256(b).hexdigest().upper()) for n,b in files.items()})
(D/'receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print(json.dumps(receipt,indent=2))
