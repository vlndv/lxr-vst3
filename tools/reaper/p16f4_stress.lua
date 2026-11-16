-- Launcher supplies settings and a separate task-only REAPER configuration.
local root=settings.root
local _,project=reaper.EnumProjects(-1,"")
assert(project==root.."\\F4_STRESS.rpp","exact stress project required")
assert(reaper.CountTracks(0)==14,"fourteen independent instances required")
local function write(name,s)local f=assert(io.open(root.."\\"..name,"wb"));f:write(s);f:close()end
local function snapshot(label)
 for i=0,13 do
  local t=reaper.GetTrack(0,i)
  local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
  assert(ident and ident:find(settings.module,1,true),"private module identity")
  local ok,chunk=reaper.GetTrackStateChunk(t,"",false);assert(ok,"coherent state available")
  write(label.."_"..i.."_chunk.txt",chunk)
 end
end
for i=0,13 do
 local t=reaper.GetTrack(0,i);local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
 assert(ident and ident:find(settings.module,1,true),"private module required")
 reaper.SetMediaTrackInfo_Value(t,"D_VOL",0.05)
 local rate=reaper.GetFXEnvelope(t,0,243,true);local held=reaper.GetFXEnvelope(t,0,244,true)
 assert(reaper.InsertEnvelopePoint(rate,0,i/13,1,0,false,true));reaper.Envelope_SortPoints(rate)
 for _,p in ipairs({{0,0},{0.25,1/127},{2.5,0}})do
  assert(reaper.InsertEnvelopePoint(held,p[1],p[2],1,0,false,true))
 end
 reaper.Envelope_SortPoints(held)
 local item=assert(reaper.CreateNewMIDIItemInProj(t,0.2,0.6,false));local take=assert(reaper.GetActiveTake(item))
 assert(reaper.MIDI_InsertNote(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,0.2),
  reaper.MIDI_GetPPQPosFromProjTime(take,0.35),0,60,118,true));reaper.MIDI_Sort(take)
end
local function render(label)
 write("progress.txt","before_render_"..label)
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true)
 reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",label,true)
 reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=4,
  RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do
  reaper.GetSetProjectInfo(0,k,v,true)
 end
 reaper.Main_OnCommand(42230,0)
 write("progress.txt","after_render_"..label)
end
local started=reaper.time_precise()
local function run()
 if reaper.time_precise()-started<3 then reaper.defer(run);return end
 local ok,err=xpcall(function()
  snapshot("initial");reaper.Main_SaveProjectEx(0,root.."\\matched.rpp",0)
  render("stress_a")
  reaper.Main_openProject("noprompt:"..root.."\\matched.rpp");snapshot("reopened")
  render("stress_b")
  write("completion.txt","COMPLETED_PENDING_INDEPENDENT_STATE_PCM_RESOURCE_CHECKS\n"..settings.sha256)
 end,debug.traceback)
 if not ok then write("completion.txt","HARNESS_ERROR\n"..err)end
end
reaper.defer(run)
