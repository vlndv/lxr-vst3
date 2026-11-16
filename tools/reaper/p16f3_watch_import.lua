-- Launcher supplies settings; watch only the task project's production import.
local root=settings.root
local track=reaper.GetTrack(0,0);assert(track and reaper.CountTracks(0)==1,"one task track required")
local _,ident=reaper.TrackFX_GetNamedConfigParm(track,0,"fx_ident")
assert(ident and ident:find(settings.module,1,true),"exact candidate guard")
local function write(name,s)local f=assert(io.open(root.."\\"..name,"wb"));f:write(s);f:close()end
local alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local function decode(s)
 local bytes={};local value,bits=0,0
 for c in s:gmatch(".")do
  local at=alphabet:find(c,1,true)
  if at then value=(value<<6)|(at-1);bits=bits+6;if bits>=8 then bits=bits-8;bytes[#bytes+1]=string.char((value>>bits)&255);value=value&((1<<bits)-1)end end
 end
 return table.concat(bytes)
end
local function bank()
 local ok,chunk=reaper.GetTrackStateChunk(track,"",false);assert(ok)
 local section=assert(chunk:match("<VST%s[^\n]*\n(.-)\n%s*>"));local pieces={}
 for line in section:gmatch("[^\r\n]+")do pieces[#pieces+1]=decode(line:match("^%s*(.-)%s*$"))end
 local raw=table.concat(pieces);local pos=assert(raw:find("LXQB",1,true));local n=string.unpack("<I4",raw,pos+8);assert(n==50449)
 return raw:sub(pos,pos+n-1),chunk
end
local before
local started=reaper.time_precise();local shown=false
local function watch()
 local ok,err=xpcall(function()
  local available,current,chunk=pcall(bank)
  if not available then reaper.defer(watch);return end -- one-attempt state snapshot may be Busy
  if not before then before=current;write("before_import_chunk.txt",chunk)end
  if not shown and reaper.time_precise()-started>3 then reaper.TrackFX_Show(track,0,1);shown=true end
  if current~=before then
   write("ui_imported_chunk.txt",chunk);write("ui_imported_bank.lxqb",current)
   reaper.TrackFX_SetParamNormalized(track,0,240,1);reaper.TrackFX_SetParamNormalized(track,0,241,1)
   reaper.Main_SaveProjectEx(0,root.."\\ui_imported.rpp",0)
   reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN","ui_imported_audio",true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
   for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=3,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
   reaper.Main_OnCommand(42230,0)
   write("ingress_result.txt","status=IMPORTED_PENDING_INDEPENDENT_ORACLE\ncandidate="..settings.sha256.."\nmodule="..ident.."\n");return
  end
  if reaper.time_precise()-started>1800 then write("ingress_result.txt","status=BLOCKED_WAITING_PRODUCTION_UI_IMPORT\n");return end
  reaper.defer(watch)
 end,debug.traceback)
 if not ok then write("ingress_result.txt","status=HARNESS_ERROR\n"..err)end
end
reaper.defer(watch)
