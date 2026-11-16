-- Task-owned real REAPER transport exercise; no synthetic plugin transport setter.
local root=settings.root
local track=reaper.GetTrack(0,0)
local _,ident=reaper.TrackFX_GetNamedConfigParm(track,0,"fx_ident")
assert(ident and ident:find(settings.module,1,true),"final candidate guard")
local f=assert(io.open(root.."\\live_transport.csv","wb"));f:write("elapsed,stage,play_state,position,bpm,peak_left,peak_right,roll_mask\n");f:close()
local started=reaper.time_precise();local stage=0;local at=started;local maximum=0;local wraps=0;local prior=0;local loopStarted=false
local function row(label)
 local pos=reaper.GetPlayPosition2();local peak=math.max(reaper.Track_GetPeakInfo(track,0),reaper.Track_GetPeakInfo(track,1));maximum=math.max(maximum,peak)
 if loopStarted and pos<prior-0.25 then wraps=wraps+1 end;prior=pos
 local f=assert(io.open(root.."\\live_transport.csv","ab"));f:write(string.format("%.8f,%s,%d,%.9f,%.6f,%.12g,%.12g,%.12g\n",reaper.time_precise()-started,label,reaper.GetPlayState(),pos,reaper.Master_GetTempo(),reaper.Track_GetPeakInfo(track,0),reaper.Track_GetPeakInfo(track,1),reaper.TrackFX_GetParamNormalized(track,0,244)));f:close()
end
local function step()
 local now=reaper.time_precise();local elapsed=now-at
 if stage==0 and elapsed>=2 then
  if settings.record==false then
   reaper.Main_OnCommand(1016,0);reaper.SetCurrentBPM(0,120,false);reaper.SetEditCurPos2(0,0,false,false);reaper.Main_OnCommand(1007,0);stage=10;at=now;row("play_zero_120")
  else
  reaper.Main_OnCommand(1016,0);reaper.SetCurrentBPM(0,120,false);reaper.SetEditCurPos2(0,0,false,false)
  reaper.SetMediaTrackInfo_Value(track,"I_RECMODE",1);reaper.SetMediaTrackInfo_Value(track,"I_RECINPUT",-1);reaper.SetMediaTrackInfo_Value(track,"I_RECARM",1)
  reaper.Main_OnCommand(1013,0);stage=1;at=now;row("record_120_start")
  end
 elseif stage==10 and elapsed>=0.8 then
  row("playing_120_before_stop");reaper.Main_OnCommand(1016,0);stage=11;at=now;row("stop_120")
 elseif stage==11 and elapsed>=0.3 then
  reaper.Main_OnCommand(1007,0);stage=12;at=now;row("resume_120")
 elseif stage==12 and elapsed>=0.8 then
  row("resumed_120_pcm");reaper.Main_OnCommand(1016,0);reaper.SetCurrentBPM(0,60,false);reaper.SetEditCurPos2(0,0.5,false,false);reaper.Main_OnCommand(1007,0);stage=2;at=now;row("play_nonzero_60")
 elseif stage==1 and elapsed>=2.2 then
  reaper.Main_OnCommand(1016,0);reaper.SetMediaTrackInfo_Value(track,"I_RECARM",0)
  -- Keep recorded item as evidence; mute it for subsequent synth-only playback.
  for i=0,reaper.CountTrackMediaItems(track)-1 do reaper.SetMediaItemInfo_Value(reaper.GetTrackMediaItem(track,i),"B_MUTE",1)end
  reaper.SetEditCurPos2(0,0.5,false,false);reaper.SetCurrentBPM(0,60,false);reaper.Main_OnCommand(1007,0);stage=2;at=now;row("resume_nonzero_60")
 elseif stage==2 and elapsed>=0.7 then
  reaper.SetEditCurPos2(0,3,false,true);stage=3;at=now;row("seek_forward_request")
 elseif stage==3 and elapsed>=0.7 then
  reaper.SetEditCurPos2(0,0.25,false,true);stage=4;at=now;row("seek_backward_request")
 elseif stage==4 and elapsed>=0.7 then
  reaper.GetSet_LoopTimeRange2(0,true,true,0,1,false);reaper.GetSetRepeatEx(0,1);reaper.SetEditCurPos2(0,0,false,true)
  loopStarted=true;stage=5;at=now;row("loop_enabled")
 elseif stage==5 and elapsed>=2.2 then
  reaper.SetCurrentBPM(0,180,false);reaper.TrackFX_SetParamNormalized(track,0,243,1);reaper.TrackFX_SetParamNormalized(track,0,244,1/127)
  stage=6;at=now;row("live_tempo_180_roll_press")
 elseif stage==6 and elapsed>=0.7 then
  reaper.TrackFX_SetParamNormalized(track,0,244,0);stage=7;at=now;row("manual_roll_release")
 elseif stage==7 and elapsed>=0.5 then
  reaper.TrackFX_SetParamNormalized(track,0,244,1/127);stage=8;at=now;row("press_before_stop")
 elseif stage==8 and elapsed>=0.3 then
  -- Wait for the host's parameter queue to deliver the held state while
  -- playing. A press queued until after stop is an indistinguishable new press.
  reaper.Main_OnCommand(1016,0);loopStarted=false;stage=9;at=now;row("stop_while_held")
 elseif stage==9 and elapsed>=0.3 then
  row("stopped_readback");local released=reaper.TrackFX_GetParamNormalized(track,0,244)==0
  reaper.Main_SaveProjectEx(0,root.."\\live_transport_evidence.rpp",0)
  local f=assert(io.open(root.."\\live_results.txt","wb"));f:write("candidate="..settings.sha256.."\nmodule="..ident.."\npeak="..maximum.."\nloop_wraps="..wraps.."\nroll_released="..tostring(released).."\nstatus=EXERCISED_PENDING_PCM_TIMING_ANALYSIS\n");f:close();return
 end
 if stage>0 then row("sample_"..stage)end
 reaper.defer(step)
end
reaper.defer(step)
