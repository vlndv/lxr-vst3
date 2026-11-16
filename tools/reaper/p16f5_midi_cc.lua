-- Exact private module, real REAPER MIDI items, ordinary CC/NRPN ingress.
local root=settings.root
local _,project=reaper.EnumProjects(-1,"")
assert(project==root.."\\musical.rpp","exact MIDI task project required")
local function write(name,s)local f=assert(io.open(root.."\\"..name,"wb"));f:write(s);f:close()end
local checks={}
local function check(name,ok)checks[#checks+1]=name.."="..(ok and"PASS"or"FAIL");write("midi_cc_checks.txt",table.concat(checks,"\n"));assert(ok,name)end
local function track()
 local t=assert(reaper.GetTrack(0,0));local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
 assert(ident and ident:find(settings.module,1,true),"candidate path guard");return t
end
local function open(name)reaper.Main_OnCommand(1016,0);reaper.Main_openProject("noprompt:"..root.."\\"..name..".rpp");return track()end
local function snapshot(name,t)local ok,s=reaper.GetTrackStateChunk(t,"",false);assert(ok);write(name.."_chunk.txt",s)end
local function pcm(name)
 local f=assert(io.open(root.."\\"..name..".wav","rb"));local b=f:read("*a");f:close();local pos=13
 while pos+7<=#b do local n=string.unpack("<I4",b,pos+4);if b:sub(pos,pos+3)=="data"then return b:sub(pos+8,pos+7+n)end;pos=pos+8+n+n%2 end;error("no PCM")
end
local function render(name)
 write("progress.txt","before_"..name)
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",name,true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=4,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_OnCommand(42230,0);return pcm(name)
end
local cc={{9600,88,0},{9664,88,100},{24000,88,20},{24032,88,60},{24064,88,100},
 {48000,99,0},{48001,98,1},{48002,6,45},{48003,88,72},{48032,6,70},{48100,88,100}}
local notes={{9500,16800,0,60,118},{24000,30000,0,60,127},{48000,54000,0,60,100},
 {72000,73000,5,60,100},{74000,78000,6,60,118},{96000,102000,0,60,118}}
local function prepare(sync,withCC)
 local t=open("musical");reaper.TrackFX_SetParamNormalized(t,0,240,sync and 1 or 0);reaper.TrackFX_SetParamNormalized(t,0,241,1);reaper.TrackFX_SetParamNormalized(t,0,88,100/127)
 local item=assert(reaper.CreateNewMIDIItemInProj(t,0,4,false));local take=assert(reaper.GetActiveTake(item))
 for _,n in ipairs(notes)do assert(reaper.MIDI_InsertNote(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,n[1]/48000),reaper.MIDI_GetPPQPosFromProjTime(take,n[2]/48000),n[3],n[4],n[5],true))end
 if withCC then for _,e in ipairs(cc)do assert(reaper.MIDI_InsertCC(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,e[1]/48000),0xb0,0,e[2],e[3]))end end
 reaper.MIDI_Sort(take);return t
end
local started=reaper.time_precise()
local function run()
 if reaper.time_precise()-started<3 then reaper.defer(run);return end
 local ok,err=xpcall(function()
  check("REAPER_7_80",reaper.GetAppVersion()=="7.80/x64");track()
  for _,mode in ipairs({"disabled","sync"})do
   local sync=mode=="sync";local t=prepare(sync,true);snapshot(mode.."_initial",t)
   reaper.Main_SaveProjectEx(0,root.."\\"..mode.."_saved.rpp",0)
   local a=render(mode.."_cc_a");check(mode.."_nonempty",a:find("[^%z]")~=nil)
   t=open(mode.."_saved");snapshot(mode.."_reopened",t)
   check(mode.."_save_reopen_exact",render(mode.."_cc_b")==a)
   local _,value=reaper.TrackFX_GetFormattedParamValue(t,0,88,"");write(mode.."_final_CC88.txt",tostring(value))
   prepare(sync,false);local control=render(mode.."_notes_control")
   check(mode.."_controller_changes_observable_PCM",a~=control)
  end
  write("completion.txt","PASS\ncandidate="..settings.sha256)
 end,debug.traceback)
 if not ok then write("completion.txt","HARNESS_ERROR_OR_FAILURE\n"..err)end
end
reaper.defer(run)
