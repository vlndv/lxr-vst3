-- Fourteen real production instances. Read-only external probe observes event
-- totals after REAPER renders; no internal setter configures any pattern/roll.
local root=settings.root
assert(reaper.CountTracks(0)==14,"fourteen task tracks required")
for i=0,13 do
 local t=reaper.GetTrack(0,i);assert(t)
 reaper.SetMediaTrackInfo_Value(t,"D_VOL",0.1) -- diagnostic mix headroom, outside plugin
 local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident");assert(ident:find(settings.module,1,true))
 local rate=reaper.GetFXEnvelope(t,0,243,true);local held=reaper.GetFXEnvelope(t,0,244,true)
 assert(reaper.InsertEnvelopePoint(rate,0,i/13,1,0,false,true));reaper.Envelope_SortPoints(rate)
 for _,p in ipairs({{0,0},{0.25,1/127},{2.5,0}})do assert(reaper.InsertEnvelopePoint(held,p[1],p[2],1,0,false,true))end;reaper.Envelope_SortPoints(held)
end
local started=reaper.time_precise()
local function render()
 if reaper.time_precise()-started<3 then reaper.defer(render);return end
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN","fourteen_roll_rates",true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=4,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_SaveProjectEx(0,root.."\\fourteen_roll_rates.rpp",0)
 reaper.Main_OnCommand(42230,0)
 local f=assert(io.open(root.."\\roll_counts_ready.txt","wb"));f:write("candidate="..settings.sha256.."\nstatus=RENDERED_AWAITING_READ_ONLY_PROBE\n");f:close()
end
reaper.defer(render)
