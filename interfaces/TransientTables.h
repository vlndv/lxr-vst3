// FILE: dsp/TransientTables.h
#pragma once
#include <cstdint>

namespace lxr {

struct TransientTables {
    const float* volumeTable; // size 69
    const int8_t* data;       // size 12 * 2205

    // Loads from binary files. Call once at startup, NOT on audio thread.
    // Returns true on success, false on failure.
    bool load(const char* dataDir);
    
    // Releases resources.
    void unload();
};

// Global instance for easy access, initialized at startup.
extern TransientTables gTransientTables;

} // namespace lxr