/*
    MicroDSP - Day 4: Array Indexing Delay

    What this program does:
    - Reads a 16-bit PCM mono WAV file: input.wav
    - Applies a simple delay using array indexing
    - Writes the result to: output_delay.wav

    Delay model (with no feedback):
        y[n] = dry * x[n] + wet * x[n - D]  (for n >= D)
        y[n] = dry * x[n]                   (for n < D)

    Notes:
    - This is OFFLINE processing: we load the whole file into memory.
    - This is perfect for learning indexing, but not how real-time plugins usually work

    Author: Jesse Whiting (GhostWire Audio)
    GitHub: jwhiting07
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <algorithm>

// Force the compiler to pack the struct exactly as written
// with no padding bytes added for alignment, which ensures
// sizeof(WavHeader) == 44 bytes
#pragma pack(push, 1)
struct WavHeader {
    // RIFF Chunk Descriptor
    char     riff[4];        // "RIFF"
    uint32_t chunkSize;      // 36 + Subchunk2Size
    char     wave[4];        // "WAVE"

    // fmt subchunk
    char     fmt[4];         // "fmt "
    uint32_t subchunk1Size;  // 16 for PCM
    uint16_t audioFormat;    // 1 for PCM
    uint16_t numChannels;    // 1 for mono
    uint32_t sampleRate;     // e.g., 44100
    uint32_t byteRate;       // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;     // numChannels * bitsPerSample/8
    uint16_t bitsPerSample;  // 16

    // data subchunk
    char     data[4];        // "data"
    uint32_t subchunk2Size;  // NumSamples * numChannels * bitsPerSample/8
};
#pragma pack(pop)

int main() {
    const char* inputPath = "input.wav";
    const char* outputPath = "output_delay.wav";

    // Delay parameters
    const float delayMs = 250.0f;   // Delay time in milliseconds
    const float dry = 0.8f;         // Volume of the original signal
    const float wet = 0.5f;               // Volume of delayed signal

    // Open input file in binary mode
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error: Could not open input file.\n";
        return 1;
    }

    // Reads the WAV header (first 44 bytes) into memory
    WavHeader header{};
    // read(char* buffer, std::streamsize count)
    in.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (!in) {
        std::cerr << "Error: Failed to read WAV header.\n";
        return 1;
    }

    // Clauclate number of samples
    // subchunk2Size is in bytes, so we divide by bytes per sample
    const uint32_t bytesPerSample = header.bitsPerSample / 8;
    const uint32_t numSamples = header.subchunk2Size / bytesPerSample;

    // Allocate buffer for input samples
    // Vector will hold the entire audio file in memory
    std::vector<int16_t> input(numSamples);

    // A vector is used here because audio buffers are "size known at runtime"
    // memory that must be contiguous and safe

    // Reads audio sample data
    in.read(reinterpret_cast<char*>(input.data()), header.subchunk2Size);
    if (!in) {
        std::cerr << "Error: Failed to read audio data.\n";
        return 1;
    }
    in.close();

    // Converts delay time to samples
    const uint32_t delaySamples = static_cast<uint32_t>((delayMs / 1000.0f) * header.sampleRate);

    // Output buffer to hold the processed audio samples
    std::vector<int16_t> output(numSamples);

    // Main delay loop, walk forward in time
    for (uint32_t n = 0; n < numSamples; ++n) {

        // Current input sample
        const float x = static_cast<float>(input[n]);

        // Delayed sample (array indexing into the past)
        // If we haven't reached the delay time yet, output silence
        const float d = (n >= delaySamples) ? static_cast<float>(input[n - delaySamples]) : 0.0f;

        // Mix dry and wet signals
        float mix = dry * x + wet * d;

        // Clamp to valid 16-bit range
        mix = std::clamp(mix, -32768.0f, 32767.0f);

        // Store result
        output[n] = static_cast<int16_t>(mix);
    }

    // Write output WAV file
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        std::cerr << "Error: Could not open output file.\n";
        return 1;
    }

    out.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    out.write(reinterpret_cast<const char*>(output.data()), header.subchunk2Size);
    out.close();
    
    return 0;
}