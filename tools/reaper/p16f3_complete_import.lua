-- One-shot guarded continuation for the actual UI-imported instance.
local root=settings.root
local function write(name,s)local f=assert(io.open(root.."\\"..name,"wb"));f:write(s);f:close()end
local t=reaper.GetTrack(0,0)
if not t or reaper.CountTracks(0)~=1 then write("continuation_route.txt","WRONG_INSTANCE_TRACK_COUNT");return end
local _,name=reaper.GetSetMediaTrackInfo_String(t,"P_NAME","",false)
local _,ident=reaper.TrackFX_GetNamedConfigParm(t,0,"fx_ident")
if name~="P16-F3 FINAL BANK IMPORT" or not ident or not ident:find(settings.module,1,true) then write("continuation_route.txt","WRONG_INSTANCE\n"..tostring(name).."\n"..tostring(ident));return end
write("continuation_route.txt","CORRECT_UI_IMPORTED_INSTANCE\n"..ident)
local function snapshot(label)
 local ok,chunk=reaper.GetTrackStateChunk(t,"",false);assert(ok and #chunk>68000,"complete state snapshot Busy/unavailable")
 write(label.."_chunk.txt",chunk)
end
local function render(label)
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",root,true);reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",label,true);reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=3,RENDER_CHANNELS=2,RENDER_SRATE=48000,RENDER_ADDTOPROJ=0,RENDER_TAILFLAG=0})do reaper.GetSetProjectInfo(0,k,v,true)end
 reaper.Main_OnCommand(42230,0)
end
local attempts=0
local function complete()
 attempts=attempts+1
 local ok,err=xpcall(function()
  snapshot("ui_imported")
  reaper.TrackFX_SetParamNormalized(t,0,240,1);reaper.TrackFX_SetParamNormalized(t,0,241,1)
  reaper.Main_SaveProjectEx(0,root.."\\ui_imported.rpp",0)
  render("ui_imported_audio")
  reaper.Main_openProject("noprompt:"..root.."\\ui_imported.rpp");t=reaper.GetTrack(0,0)
  snapshot("ui_reopened");render("ui_reopened_audio")
  write("continuation_results.txt","status=RENDERED_SAVED_REOPENED_PENDING_INDEPENDENT_ORACLE\ncandidate="..settings.sha256)
 end,debug.traceback)
 if not ok then
  if attempts<4 then reaper.defer(complete)else write("continuation_results.txt","status=HARNESS_ERROR\n"..err)end
 end
end
reaper.defer(complete)
