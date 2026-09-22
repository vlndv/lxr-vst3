// FILE: dsp/TransientTables.cpp
#include "TransientTables.h"
#include <cstdio>
#include <vector>

namespace lxr {

TransientTables gTransientTables;

bool TransientTables::load(const char* dataDir) {
    // Note: This allocates memory, which is acceptable for startup, but must NOT be called on the audio thread.
    static std::vector<float> volTable(69);
    static std::vector<int8_t> tData(12 * 2205);

    char volPath[512];
    std::snprintf(volPath, sizeof(volPath), "%s/transient_volume_table.bin", dataDir);
    FILE* f = std::fopen(volPath, "rb");
    if (!f) return false;
    if (std::fread(volTable.data(), sizeof(float), 69, f) != 69) { std::fclose(f); return false; }
    std::fclose(f);

    char dataPath[512];
    std::snprintf(dataPath, sizeof(dataPath), "%s/transient_data.bin", dataDir);
    f = std::fopen(dataPath, "rb");
    if (!f) return false;
    if (std::fread(tData.data(), sizeof(int8_t), 12 * 2205, f) != 12 * 2205) { std::fclose(f); return false; }
    std::fclose(f);

    volumeTable = volTable.data();
    data = tData.data();
    return true;
}

void TransientTables::unload() {
    volumeTable = nullptr;
    data = nullptr;
}

} // namespace lxr