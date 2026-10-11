#pragma once

#include <string>

namespace lxr {

// Resolve the data directory belonging to the loaded plug-in module.
// Empty string means that the bundle layout could not be resolved.
std::string GetPluginAssetDirectory();

} // namespace lxr