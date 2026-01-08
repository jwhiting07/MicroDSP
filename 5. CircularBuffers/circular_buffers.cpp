/*
    MicroDSP - Day 5: Circular Buffer Delay

    This program simulates how a real delay plugin works internally.
    It processes audio sample-by-sample using a fixed-size circular buffer.

    Circular buffer model:
    - We continuously write incoming samples into delayBuffer[writeIndex]
    - To get a delayed sample, we read from delayBuffer[readIndex] where
      readIndex = writeIndex - delaySamples (wrapped into valid range)

    Author: Jesse Whiting (GhostWire Audio)
    GitHub: ghostwireaudio
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

    // Input and output file paths (must be in the same folder as the .exe file)
    const char* inputPath = "input.wav";
    const char* outputPath = "output_delay.wav";

    // Delay parameters
    const float delayMs = 250.0f; // How long the delay is, in milliseconds
    const float dry = 0.8f; // How much original signal is kept
    const float wet = 0.5f; // How much delayed signal is added

    // Open input file in binary mode
    std::ifstream in(inputPath, std::ios::binary);
    if (!in) {
        std::cerr << "Error: Could not open input file.\n";
        return 1;
    }

    // Reads the WAV header (first 44 bytes) into memory
    WavHeader header{};
    // read(char* buffer, std::streamsize count)102938102938
    in.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (!in) {
        std::cerr << "Error: Failed to read WAV header.\n";
        return 1;
    }

    // Clauclate number of samples
    // subchunk2Size is in bytes, so we divide by bytes per sample
    const uint32_t bytesPerSample = header.bitsPerSample / 8;
    const uint32_t numSamples = header.subchunk2Size / bytesPerSample;

    // Allocate input buffer
    // We store audio samples as int16_t because the WAV is assumed to be
    // 16-bit PCM, and each entry is one sample.
    std::vector<int16_t> input(numSamples, 0);

    // Reads audio sample data
    // input.data() returns a pointer to the underlying contiguous array
    // We reinterpret it as a char* because read() operates in raw bytes
    in.read(reinterpret_cast<char*>(input.data()), header.subchunk2Size);
    if (!in) {
        std::cerr << "Error: Failed to read audio data.\n";
        return 1;
    }
    in.close();

    // Converts delay time from milliseconds to samples
    // delaySamples = delaySeconds * sampleRate
    const uint32_t delaySamples = static_cast<uint32_t>((delayMs / 1000.0f) * header.sampleRate);
    
    // Output buffer
    // Will hold processed audio samples
    std::vector<int16_t> output(numSamples, 0);

    // Circular buffer capacity (maximum delay supported)
    // Here we set it to sampleRate, meaning 1 second of delay memory
    const uint32_t maxDelaySamples = header.sampleRate;

    // Delay line storage (circular buffer)
    // Holds past samples, uses float for precision
    std::vector<float> delayBuffer(maxDelaySamples, 0.0f);

    // The position where we'll write the next incoming sample
    // Advances every sample and wraps back to 0 when it reaches the end
    uint32_t writeIndex = 0;

    // Main processing loop
    for (uint32_t n = 0; n < numSamples; ++n) {

        // Current input sample (converted to float for mixing math)
        // The input is still int16_t, but float allows for fractional mixing
        const float x = static_cast<float>(input[n]);

        // Computes the read index = "delaySamples behind the write head"
        // Must be done using signed integers so we can detect negatives
        int32_t readIndex = static_cast<int32_t>(writeIndex) - static_cast<int32_t>(delaySamples);
        
        // If the readIndex is negative, wrap it around to the end of the buffer
        if (readIndex < 0) {
            readIndex += maxDelaySamples;
        }

        // Read the delayed sample from the delay buffer
        const float d = delayBuffer[readIndex];

        float mix = dry * x + wet * d; // Computes the mix value

        // Clamp to valid 16-bit range
        mix = std::clamp(mix, -32768.0f, 32767.0f);

        // Stores the processed sample to the output buffer
        output[n] = static_cast<int16_t>(mix);

        // Write the current input sample into the delay buffer at writeIndex
        // which updates the delay line memory for future samples.
        delayBuffer[writeIndex] = x;

        writeIndex++; // Advances the write head by one sample

        // Wrap the write index back to 0 when we reach the end (hence "circular")
        if (writeIndex >= maxDelaySamples) {
            writeIndex = 0;
        }
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