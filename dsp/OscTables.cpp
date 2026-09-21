// dsp/OscTables.cpp
#include "OscTables.h"
#include <cstdio>

namespace lxr {
namespace {
template <typename T>
bool readFile(const std::string& path, std::vector<T>& out, size_t count, size_t extra) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out.assign(count + extra, T());
    const size_t got = std::fread(out.data(), sizeof(T), count, f);
    std::fclose(f);
    return got == count;   // data files are little-endian; this code assumes a little-endian host (x86-64, ARM64)
}
} // namespace

bool OscTableStore::load(const std::string& dir) {
    loaded_ = false;
    const std::string d = dir.empty() ? std::string() : (dir + "/");
    if (!readFile(d + "sine_table.bin", sine_, 4097, 0)) return false;
    if (!readFile(d + "saw_table.bin", saw_, 11 * 1024, 1)) return false;
    if (!readFile(d + "tri_table.bin", tri_, 11 * 1024, 1)) return false;
    if (!readFile(d + "rec_table.bin", rec_, 11 * 1024, 1)) return false;
    if (!readFile(d + "crash_sample.bin", crash_, 32768, 1)) return false;
    if (!readFile(d + "midi_note_frequencies.bin", note_, 128, 0)) return false;
    view_.sine = sine_.data(); view_.saw = saw_.data(); view_.tri = tri_.data(); view_.rec = rec_.data();
    view_.crash = crash_.data(); view_.noteFreq = note_.data();
    loaded_ = true;
    return true;
}

void OscTableStore::setGuards(int16_t saw, int16_t tri, int16_t rec, uint8_t crash) {
    if (!loaded_) return;
    saw_[11 * 1024] = saw; tri_[11 * 1024] = tri; rec_[11 * 1024] = rec; crash_[32768] = crash;
}

} // namespace lxr
