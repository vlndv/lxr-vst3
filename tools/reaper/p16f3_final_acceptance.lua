-- The launcher prepends task-specific settings. All data/actions remain in the
-- isolated task profile. API: https://www.reaper.fm/sdk/reascript/reascripthelp.html
local root=settings.root
local results={version=reaper.GetAppVersion(),candidate=settings.sha256,module=settings.module,classification="IN_PROGRESS",checks={}}
local function write(path,s)local f=assert(io.open(root.."\\"..path,"wb"));f:write(s);f:close()end
local function json(x)
 local t=type(x);if t=="nil"then return"null"elseif t=="boolean"or t=="number"then return tostring(x)elseif t=="string"then return'"'..x:gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n'):gsub('\r','\\r')..'"'end
 local keys={};for k in pairs(x)do keys[#keys+1]=k end;table.sort(keys,function(a,b)return tostring(a)<tostring(b)end)
 local p={};for _,k in ipairs(keys)do p[#p+1]=json(tostring(k))..":"..json(x[k])end;return"{"..table.concat(p,",").."}"
end
local function checkpoint(label)write("progress.txt",label);write("reaper_results.json",json(results))end
local function check(name,ok,detail)results.checks[name]={status=ok and"PASS"or"FAIL",detail=detail};checkpoint(name);assert(ok,name)end
local function track()
 local t=reaper.GetTrack(0,0);assert(t,"missing task track")
 local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
 assert(ident and ident:find(settings.module,1,true),"candidate module path mismatch: "..tostring(ident))
 return t
end
local function open(name)reaper.Main_OnCommand(1016,0);reaper.Main_openProject("noprompt:"..root.."\\"..name..".rpp");return track()end
local function snapshot(name,t)local ok,c=reaper.GetTrackStateChunk(t,"",false);assert(ok);write(name.."_chunk.txt",c)end
local function payload(name)
 local f=assert(io.open(root.."\\"..name..".wav","rb"));local b=f:read("*a");f:close();local pos=13
 while pos+7<=#b do local n=string.unpack("<I4",b,pos+4);if b:sub(pos,pos+3)=="data"then return b:sub(pos+8,pos+7+n)end;pos=pos+8+n+n%2 end;error("no PCM")
end
local function render(name,seconds)
 checkpoint("before_render_"..name)
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",name,true)
 reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=seconds or 3,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_OnCommand(42230,0);checkpoint("after_render_"..name);return payload(name)
end
local function nonzero(b)return b:find("[^%z]")~=nil end
local function envelope(t,id,points)
 local e=reaper.GetFXEnvelope(t,0,id,true);assert(e,"automation envelope")
 for _,p in ipairs(points)do assert(reaper.InsertEnvelopePoint(e,p[1],p[2],1,0,false,true))end;reaper.Envelope_SortPoints(e)
end
local function main()
 local t=track();check("reaper_7_80",reaper.GetAppVersion()=="7.80/x64")
 if settings.mode=="midi_only" then
  local baseline=render("midi_control",4)
  local item=reaper.CreateNewMIDIItemInProj(t,0.2,0.6,false);assert(item)
  local take=reaper.GetActiveTake(item);assert(reaper.MIDI_InsertNote(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,0.2),reaper.MIDI_GetPPQPosFromProjTime(take,0.35),0,60,118,true));reaper.MIDI_Sort(take)
  local coexist=render("midi_coexistence",4);check("external_midi_coexistence",nonzero(coexist) and coexist~=baseline)
  results.classification="PARTIAL_PASS";checkpoint("completed_midi_subset");return
 end
 snapshot("initial_loaded",t)
 local expected={};for id=0,244 do expected[id]=reaper.TrackFX_GetParamNormalized(t,0,id)end
 check("original_controls_registered",reaper.TrackFX_GetNumParams(t,0)>=247)
 reaper.Main_SaveProjectEx(0,root.."\\saved_musical.rpp",0)
 snapshot("saved",t);local first=render("musical_a",4)
 check("sequencer_pcm_nonzero",nonzero(first))
 t=open("saved_musical");snapshot("reopened",t)
 local mismatch={};for id=0,244 do if math.abs(reaper.TrackFX_GetParamNormalized(t,0,id)-expected[id])>1e-7 then mismatch[#mismatch+1]=id end end
 check("controller_restore",#mismatch==0,json(mismatch))
 check("save_reopen_pcm",render("musical_reopened",4)==first)
 t=open("saved_musical");check("offline_repeat_pcm",render("musical_repeat",4)==first)
 local _,pdc=reaper.TrackFX_GetNamedConfigParm(t,0,"pdc");check("pdc_48k",tonumber(pdc)==26,pdc)
 reaper.TrackFX_SetEnabled(t,0,false);reaper.Main_SaveProjectEx(0,root.."\\saved_bypass.rpp",0)
 t=open("saved_bypass");check("bypass_restore",not reaper.TrackFX_GetEnabled(t,0));t=open("saved_musical")
 reaper.SetCurrentBPM(0,60,false);check("host_60_pcm",nonzero(render("musical_60",4)))
 t=open("saved_musical");reaper.TrackFX_SetParamNormalized(t,0,241,0);check("internal_clock_pcm",nonzero(render("internal",4)))
 t=open("saved_musical");assert(reaper.SetTempoTimeSigMarker(0,-1,1,-1,-1,60,4,4,false));check("tempo_map_pcm",nonzero(render("tempo_map",4)))
 t=open("random_next");snapshot("random_loaded",t);local random=render("random_a",8);t=open("random_next");check("random_next_pcm_identity",nonzero(random) and render("random_b",8)==random)
 t=open("10_shuffle");local shuffled=render("shuffle_a",3);t=open("10_shuffle");check("shuffle_pcm_identity",render("shuffle_b",3)==shuffled)
 t=open("01_drum1");envelope(t,243,{{0,8/13},{0.5,11/13},{1.5,1}});envelope(t,244,{{0,0},{0.5,1/127},{2,0}})
 reaper.Main_SaveProjectEx(0,root.."\\roll_automation.rpp",0);local roll=render("roll_a",3)
 t=open("roll_automation");check("roll_automation_pcm_identity",render("roll_b",3)==roll)
 t=open("01_drum1");local normal=render("roll_control",3);check("manual_roll_changes_pcm",roll~=normal and nonzero(roll))
 t=open("01_drum1");reaper.TrackFX_SetParamNormalized(t,0,243,1);reaper.TrackFX_SetParamNormalized(t,0,244,1/127)
 reaper.Main_SaveProjectEx(0,root.."\\held_roll_saved.rpp",0);t=open("held_roll_saved")
 check("no_stale_roll_after_restore",reaper.TrackFX_GetParamNormalized(t,0,244)==0 and math.abs(reaper.TrackFX_GetParamNormalized(t,0,243)-8/13)<1e-7)
 -- Source algorithms not all require independent UI controls. These projects
 -- exercise their production audio path; independent expected boundaries stay
 -- in the exact-DLL/native suites, explicitly distinguished in the certificate.
 for _,name in ipairs({"08_simultaneous_hats","09_substep_retrigger","12_pitch_automation","13_filter_automation","14_velocity_lfo"})do
  t=open(name);check(name.."_pcm",nonzero(render(name.."_host",3)))
 end
 for selector=0,13 do
  -- REAPER prepares time-zero automation while stopped. A held one-shot at
  -- zero has already fired before rendering; create an actual later press.
  t=open("roll_empty");envelope(t,243,{{0,selector/13}});envelope(t,244,{{0,0},{0.25,1/127},{2.5,0}})
  check("roll_selector_"..selector.."_pcm",nonzero(render("roll_selector_"..selector,4)))
 end
 t=open("two_instances");local t2=reaper.GetTrack(0,1);check("two_instances_present",t2 and reaper.CountTracks(0)==2)
 snapshot("instance_one",t);snapshot("instance_two",t2)
 reaper.TrackFX_SetParamNormalized(t,0,243,1);reaper.TrackFX_SetParamNormalized(t,0,244,1/127)
 check("second_instance_controls_independent",reaper.TrackFX_GetParamNormalized(t2,0,244)==0)
 reaper.Main_SaveProjectEx(0,root.."\\saved_two_instances.rpp",0);t=open("saved_two_instances");t2=reaper.GetTrack(0,1)
 snapshot("reopened_instance_one",t);snapshot("reopened_instance_two",t2)
 check("two_instances_release_restore",reaper.TrackFX_GetParamNormalized(t,0,244)==0 and reaper.TrackFX_GetParamNormalized(t2,0,244)==0)
 local paired=render("two_instances_a",3);t=open("saved_two_instances");check("two_instances_pcm_identity",nonzero(paired) and render("two_instances_b",3)==paired)
 t=open("saved_musical");snapshot("final",t)
 local midi=reaper.CreateNewMIDIItemInProj(t,0.2,0.6,false);assert(midi)
 local take=reaper.GetActiveTake(midi);assert(reaper.MIDI_InsertNote(take,false,false,reaper.MIDI_GetPPQPosFromProjTime(take,0.2),reaper.MIDI_GetPPQPosFromProjTime(take,0.35),0,60,118,true));reaper.MIDI_Sort(take)
 local coexist=render("midi_coexistence",4);check("external_midi_coexistence",nonzero(coexist) and coexist~=first)
 results.classification="PARTIAL_PASS";results.pending="Live transport boundary capture, two instances, callback-size second profile, UI import bank snapshot and human listening remain separate gates.";checkpoint("completed_offline_subset")
end
local started=reaper.time_precise()
local function start()
 if reaper.time_precise()-started<3 then reaper.defer(start);return end
 local ok,err=xpcall(main,debug.traceback);if not ok then results.classification="FAIL";results.error=err;checkpoint("FAILED")end
end
checkpoint("startup");reaper.defer(start)
