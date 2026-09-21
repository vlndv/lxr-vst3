// dsp/OscTables.h
// Read-only data tables used by the oscillators (exported by tools/export_data.py from the original 0.37 source).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace lxr {

// Non-owning view used by the audio thread. All pointers stay valid as long as the OscTableStore lives.
struct OscTables {
    const int16_t* sine   = nullptr;   // 4097 entries (TABLESIZE + 1)
    const int16_t* saw    = nullptr;   // 11 rows x 1024, plus one guard element after the last row
    const int16_t* tri    = nullptr;   // same layout
    const int16_t* rec    = nullptr;   // same layout
    const uint8_t* crash  = nullptr;   // 32768 entries, plus one guard element
    const float*   noteFreq = nullptr; // 128 entries (MidiNoteFrequencies)
};

// Owns the table memory. Load once on a non-real-time thread (it allocates and reads files).
class OscTableStore {
public:
    // Reads sine_table.bin, saw_table.bin, tri_table.bin, rec_table.bin, crash_sample.bin and
    // midi_note_frequencies.bin from dataDir (little-endian files written by tools/export_data.py).
    // Returns false and leaves the store unusable if a file is missing or has the wrong size.
    bool load(const std::string& dataDir);

    // UNSURE: the original reads ONE element past the end of sawTable/triTable/recTable (row 10, index 1024) and of
    // crashSample (index 32768). On the hardware that is whatever follows in flash (unknown). The port returns these guard values (default 0).
    // Call before tables() is used by the audio thread.
    void setGuards(int16_t saw, int16_t tri, int16_t rec, uint8_t crash);

    bool loaded() const { return loaded_; }
    const OscTables& tables() const { return view_; }

private:
    std::vector<int16_t> sine_, saw_, tri_, rec_;
    std::vector<uint8_t> crash_;
    std::vector<float>   note_;
    OscTables view_;
    bool loaded_ = false;
};

} // namespace lxr
