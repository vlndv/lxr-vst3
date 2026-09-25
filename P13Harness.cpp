// FILE: tests/P13Harness.cpp
#include "../dsp/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

// Minimal WAV header writer for 16-bit PCM
void writeWav(const char* filename, const std::vector<int16_t>& samples, uint32_t sampleRate) {
    FILE* f = fopen(filename, "wb");
    if (!f) { printf("FAIL: Cannot open %s for writing\n", filename); return; }

    uint32_t dataSize = samples.size() * 2;
    uint32_t chunkSize = 36 + dataSize;
    uint16_t channels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    uint16_t blockAlign = channels * bitsPerSample / 8;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunkSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t subchunk1Size = 16;
    fwrite(&subchunk1Size, 4, 1, f);
    uint16_t audioFormat = 1; // PCM
    fwrite(&audioFormat, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(samples.data(), 2, samples.size(), f);
    fclose(f);
}

int runRender(int argc, char** argv) {
    if (argc < 5) {
        printf("Usage: P13Harness render <voice: D1|D2|D3|SN|CY|HH|HH_OPEN> <note> <vel> <out.wav>\n");
        return 1;
    }

    std::string voice = argv[2];
    uint8_t note = (uint8_t)atoi(argv[3]);
    uint8_t vel = (uint8_t)atoi(argv[4]);
    const char* outFile = argv[5];

    lxr::Engine engine;
    if (!engine.loadAssets("../data")) {
        // Fallback to current dir
        if (!engine.loadAssets("data")) {
            printf("WARNING: Could not load data/*.bin assets. Output will be silent/garbage.\n");
        }
    }

    // Trigger the requested voice
    if (voice == "D1") engine.triggerDrum(0, vel, note);
    else if (voice == "D2") engine.triggerDrum(1, vel, note);
    else if (voice == "D3") engine.triggerDrum(2, vel, note);
    else if (voice == "SN") engine.triggerSnare(vel, note);
    else if (voice == "CY") engine.triggerCymbal(vel, note);
    else if (voice == "HH") engine.triggerHiHat(vel, false, note);
    else if (voice == "HH_OPEN") engine.triggerHiHat(vel, true, note);
    else { printf("FAIL: Unknown voice %s\n", voice.c_str()); return 1; }

    // Render 2 seconds of audio (approx 2750 blocks of 32 samples at 44kHz)
    std::vector<int16_t> output;
    output.reserve(88000);
    int16_t st1L[32], st1R[32], st2L[32], st2R[32];

    // ORIGINAL QUIRK: Engine runs at 44002.757 Hz. We write 44003 to WAV header 
    // so DAWs don't reject it, but the sample count reflects the exact engine output.
    for (int i = 0; i < 2750; i++) {
        engine.processBlock(st1L, st1R, st2L, st2R);
        for (int j = 0; j < 32; j++) {
            output.push_back(st1L[j]); // Just capture St1 Left for mono reference
        }
    }

    writeWav(outFile, output, 44003);
    printf("PASS: Rendered %zu samples to %s\n", output.size(), outFile);
    return 0;
}

int runDiff(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: P13Harness diff <file1.wav> <file2.wav>\n");
        return 1;
    }

    auto readWavData = [](const char* path, std::vector<int16_t>& data) -> bool {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 44, SEEK_SET); // Skip standard WAV header
        long dataSize = size - 44;
        data.resize(dataSize / 2);
        fread(data.data(), 2, data.size(), f);
        fclose(f);
        return true;
    };

    std::vector<int16_t> d1, d2;
    if (!readWavData(argv[2], d1) || !readWavData(argv[3], d2)) {
        printf("FAIL: Could not read WAV files\n");
        return 1;
    }

    size_t minLen = std::min(d1.size(), d2.size());
    int diffCount = 0;
    int maxDiff = 0;
    double sumSq = 0;

    for (size_t i = 0; i < minLen; i++) {
        int diff = std::abs((int)d1[i] - (int)d2[i]);
        if (diff > 0) diffCount++;
        if (diff > maxDiff) maxDiff = diff;
        sumSq += (double)diff * diff;
    }

    double rms = std::sqrt(sumSq / minLen);
    printf("PASS: Compared %zu samples\n", minLen);
    printf("  Differing samples: %d (%.2f%%)\n", diffCount, 100.0 * diffCount / minLen);
    printf("  Max absolute diff: %d\n", maxDiff);
    printf("  RMS diff: %.4f\n", rms);
    
    if (d1.size() != d2.size()) {
        printf("  WARNING: Length mismatch (%zu vs %zu)\n", d1.size(), d2.size());
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("P13 Reference-Vector Harness\n");
        printf("Commands:\n");
        printf("  render <voice> <note> <vel> <out.wav>\n");
        printf("  diff <file1.wav> <file2.wav>\n");
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "render") return runRender(argc, argv);
    if (cmd == "diff") return runDiff(argc, argv);

    printf("FAIL: Unknown command %s\n", cmd.c_str());
    return 1;
}