-- Source NRPN200..206, through real REAPER MIDI items and the production VST3.
local root=settings.root
local t=reaper.GetTrack(0,0);assert(t)
local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident");assert(ident:find(settings.module,1,true))
local item=reaper.CreateNewMIDIItemInProj(t,0,4,false);assert(item);local take=reaper.GetActiveTake(item)
local function cc(time,number,value)
 assert(reaper.MIDI_InsertCC(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,time),0xb0,0,number,value))
end
for track=0,6 do
 local time=track*0.15;cc(time,99,1);cc(time+0.05,98,72+track);cc(time+0.1,6,127)
end
reaper.MIDI_Sort(take)
local rate=reaper.GetFXEnvelope(t,0,243,true);assert(reaper.InsertEnvelopePoint(rate,0,1,1,0,false,true));reaper.Envelope_SortPoints(rate)
local held=reaper.GetFXEnvelope(t,0,244,true)
for _,p in ipairs({{0,0},{1.5,1/127},{2.5,0}})do assert(reaper.InsertEnvelopePoint(held,p[1],p[2],1,0,false,true))end;reaper.Envelope_SortPoints(held)
local started=reaper.time_precise()
local function render()
 if reaper.time_precise()-started<3 then reaper.defer(render);return end
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN","midi_mute_roll",true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=4,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_SaveProjectEx(0,root.."\\midi_mute_roll.rpp",0);reaper.Main_OnCommand(42230,0)
 local f=assert(io.open(root.."\\midi_mute_ready.txt","wb"));f:write("candidate="..settings.sha256.."\nstatus=RENDERED_PENDING_PCM_AND_EVENT_ORACLE\n");f:close()
end
reaper.defer(render)
