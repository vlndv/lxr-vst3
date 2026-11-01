-- Test-harness configuration only. Never changes a REAPER project or plugin.
local M = {}
local sep = package.config:sub(1, 1)
local function normalize(path)
  return (path:gsub("[\\/]", sep):gsub("[\\/]+$", ""))
end
local function option(name)
  local value = rawget(_G, name) or os.getenv(name)
  return value and value ~= "" and value or nil
end
local source = debug.getinfo(1, "S").source:sub(2)
M.directory = normalize(assert(source:match("^(.*)[\\/]"), "cannot locate REAPER helpers"))
M.repo_root = normalize(option("LXR_REPO_ROOT") or
  assert(M.directory:match("^(.*)[\\/]tools[\\/]reaper$"), "set LXR_REPO_ROOT for relocated helpers"))
M.normalize = normalize
function M.join(root, leaf) return normalize(root) .. sep .. leaf:gsub("[\\/]", sep) end
function M.require_file(path, label)
  local file, message = io.open(path, "rb")
  assert(file, (label or "missing dependency") .. ": " .. path .. " (" .. tostring(message) .. ")")
  file:close()
  return path
end
M.require_file(M.join(M.repo_root, "AGENTS.md"), "LXR repository not found; set LXR_REPO_ROOT")
function M.output_root(default_leaf)
  return normalize(option("LXR_TEST_OUTPUT") or M.join(M.repo_root, "out/" .. default_leaf))
end
function M.plugin()
  local explicit = option("LXR_VST3_BINARY")
  local local_app_data = os.getenv("LOCALAPPDATA")
  assert(explicit or local_app_data, "set LXR_VST3_BINARY to the VST3 executable inside its bundle")
  return M.require_file(normalize(explicit or M.join(local_app_data,
    "Programs/Common/VST3/LXR.vst3/Contents/x86_64-win/LXR.vst3")), "VST3 executable missing; set LXR_VST3_BINARY")
end
function M.project(default_leaf)
  return normalize(option("LXR_TEST_PROJECT") or M.join(M.output_root("reaper-p12b4c"), default_leaf))
end
return M
