-- Run only in the guarded task-owned P16-F3 REAPER profile after real UI import.
local root=[[C:\Projects\lxr-vst3\out\p16f3\reaper-001]]
local module=[[C:\Projects\lxr-vst3\out\p16f3\private-build-001\bundle\LXR.vst3\Contents\x86_64-win\LXR.vst3]]
local track=reaper.GetTrack(0,0)
assert(track and reaper.CountTracks(0)==1,"task project guard")
local _,name=reaper.GetSetMediaTrackInfo_String(track,"P_NAME","",false)
local _,ident=reaper.TrackFX_GetNamedConfigParm(track,0,"fx_ident")
assert(name=="P16-F3 production ingress" and ident:find(module,1,true),"exact task instance guard")
local function write(path,s)local f=assert(io.open(root.."\\"..path,"wb"));f:write(s);f:close()end
local ok,chunk=reaper.GetTrackStateChunk(track,"",false);assert(ok)
write("imported_track_chunk.txt",chunk)
reaper.Main_OnCommand(1016,0)
reaper.TrackFX_SetParamNormalized(track,0,240,1)
reaper.TrackFX_SetParamNormalized(track,0,241,1)
reaper.TrackFX_SetParamNormalized(track,0,242,119/254)
reaper.TrackFX_SetParamNormalized(track,0,0,77/127)
reaper.TrackFX_SetParamNormalized(track,0,228,2/65535)
reaper.SetCurrentBPM(0,120,false)
reaper.TrackFX_Show(track,0,0)
reaper.Main_SaveProjectEx(0,root.."\\imported_base.rpp",0)
local f=assert(io.open(root.."\\capture.txt","wb"))
f:write("REAPER="..reaper.GetAppVersion().."\nidentifier="..ident.."\nparams="..reaper.TrackFX_GetNumParams(track,0))
for id=0,244 do f:write("\n"..id.."="..string.format("%.17g",reaper.TrackFX_GetParamNormalized(track,0,id)))end
local pdc,value=reaper.TrackFX_GetNamedConfigParm(track,0,"pdc");f:write("\npdc="..tostring(value));f:close()
