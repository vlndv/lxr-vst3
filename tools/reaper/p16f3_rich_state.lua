-- Real REAPER controller writes, save/close/reopen and matched MIDI PCM.
local root=settings.root
local tokens={34,68,102,135,169,203,34,68,102,135,169,203}
local expected={}
local checks={}
local function write(name,s)local f=assert(io.open(root.."\\"..name,"wb"));f:write(s);f:close()end
local function check(name,ok)checks[#checks+1]=name.."="..(ok and"PASS"or"FAIL");write("rich_results.txt",table.concat(checks,"\n"));assert(ok,name)end
local function track(i)
 local t=assert(reaper.GetTrack(0,i));local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
 assert(ident and ident:find(settings.module,1,true),"candidate module guard");return t
end
local function snapshot(name)
 for i=0,1 do local ok,c=reaper.GetTrackStateChunk(track(i),"",false);assert(ok);write(name.."_"..i.."_chunk.txt",c)end
end
local function open(name)reaper.Main_OnCommand(1016,0);reaper.Main_openProject("noprompt:"..root.."\\"..name..".rpp");check("two_tracks_after_"..name,reaper.CountTracks(0)==2)end
local function pcm(name)
 local f=assert(io.open(root.."\\"..name..".wav","rb"));local s=f:read("*a");f:close();local p=13
 while p+7<=#s do local n=string.unpack("<I4",s,p+4);if s:sub(p,p+3)=="data"then return s:sub(p+8,p+7+n)end;p=p+8+n+n%2 end;error("no PCM")
end
local function render(name)
 write("progress.txt","before_render_"..name)
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",name,true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=3,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_OnCommand(42230,0);return pcm(name)
end
local function validate(label)
 for i=0,1 do
  local t=track(i);local same=true
  for id=0,244 do same=same and math.abs(reaper.TrackFX_GetParamNormalized(t,0,id)-expected[i][id])<1e-7 end
  check(label.."_all245_controller_values_"..i,same)
  check(label.."_bypass_"..i,reaper.TrackFX_GetEnabled(t,0)==(i==0))
 end
end
local function setup()
 check("two_task_instances",reaper.CountTracks(0)==2)
 for i=0,1 do
  local t=track(i)
  for _,p in ipairs({{0,77-i*11},{8,57+i*3},{9,71-i*5},{37,72},{43,38}})do assert(reaper.TrackFX_SetParamNormalized(t,0,p[1],p[2]/127))end
  for slot=0,11 do assert(reaper.TrackFX_SetParamNormalized(t,0,228+slot,tokens[slot+1]/65535))end
  assert(reaper.TrackFX_SetParamNormalized(t,0,240,1));assert(reaper.TrackFX_SetParamNormalized(t,0,241,i==0 and 1 or 0));assert(reaper.TrackFX_SetParamNormalized(t,0,242,(137-i*46-1)/254))
  reaper.TrackFX_SetEnabled(t,0,i==0)
  local item=assert(reaper.CreateNewMIDIItemInProj(t,.2,.6,false));local take=reaper.GetActiveTake(item)
  assert(reaper.MIDI_InsertNote(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,.2),reaper.MIDI_GetPPQPosFromProjTime(take,.35),i==0 and 0 or 2,60,118,true));reaper.MIDI_Sort(take)
 end
end
local started=reaper.time_precise();local prepared=false
local function run()
 local ok,err=xpcall(function()
  if not prepared then
   if reaper.time_precise()-started<3 then reaper.defer(run);return end
   setup();prepared=true;started=reaper.time_precise();reaper.defer(run);return
  end
  if reaper.time_precise()-started<2 then reaper.defer(run);return end
  for i=0,1 do
   expected[i]={};local t=track(i)
   for id=0,244 do expected[i][id]=reaper.TrackFX_GetParamNormalized(t,0,id)end
   for slot=0,11 do check("nonzero_route_"..i.."_"..slot,math.abs(expected[i][228+slot]-tokens[slot+1]/65535)<1e-7)end
  end
  snapshot("rich_before");reaper.Main_SaveProjectEx(0,root.."\\rich_saved.rpp",0)
  local a=render("rich_before");check("nonzero_midi_and_sequence",a:find("[^%z]")~=nil)
  open("rich_saved");validate("reopened");snapshot("rich_after");check("save_reopen_PCM_identity",render("rich_after")==a)
  open("rich_saved");validate("repeat");check("repeated_MIDI_PCM_identity",render("rich_repeat")==a)
  local t=track(0);assert(reaper.TrackFX_SetParamNormalized(t,0,243,1));assert(reaper.TrackFX_SetParamNormalized(t,0,244,1/127))
  reaper.Main_SaveProjectEx(0,root.."\\rich_held.rpp",0);open("rich_held");validate("held_restore");snapshot("rich_held_reopened")
  check("no_stale_roll",reaper.TrackFX_GetParamNormalized(track(0),0,244)==0)
  write("complete.txt","status=PASS\ncandidate="..settings.sha256.."\nmodule="..settings.module.."\n")
 end,debug.traceback)
 if not ok then write("complete.txt","status=FAIL\n"..err)end
end
reaper.defer(run)
