#include "LXRResourceLocator.h"

#include <filesystem>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
#else
  #error "LXR resource resolution requires a platform implementation"
#endif

namespace lxr {

std::string GetPluginAssetDirectory()
{
#if defined(_WIN32)

  // Resolve the actual DLL containing this function, rather than
  // the executable path of REAPER, Cubase or another host.
  HMODULE module = nullptr;

  const auto address =
    reinterpret_cast<LPCWSTR>(&GetPluginAssetDirectory);

  if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        address,
        &module))
    return {};

  wchar_t buffer[32768] = {};

  const DWORD length = GetModuleFileNameW(
    module, buffer, static_cast<DWORD>(32768));

  if (length == 0 || length >= 32768)
    return {};

  // Windows VST3:
  // LXR.vst3/Contents/x86_64-win/LXR.vst3
  //                         -> Contents/Resources/LXR
  const auto binaryDir =
    std::filesystem::path(buffer).parent_path();

  const auto resourceDir =
    binaryDir.parent_path() / "Resources" / "LXR";

  // Engine currently accepts a narrow char path. Restrict this
  // bridge to paths representable in the Windows active code page.
  const std::string result = resourceDir.string();

  if (!std::filesystem::is_directory(resourceDir))
    return {};

  return result;

#elif defined(__APPLE__)

  // Resolve the bundle containing the plug-in function, not the
  // main application's bundle.
  // A format-specific bundle reference should be supplied by the
  // AU/VST3 wrapper when implementing the macOS target.
  return {};

#endif
}

} // namespace lxr
