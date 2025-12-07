#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>

/*
    Project 4: Simple Delay (ArrayIndexing)

    This program demonstrates a simple fixed-time delay effect implemented
    using array indexing on an offline 16-bit PCM mono WAV file
    (hello_sine.wav). It reads the first 44 bytes of the input file as a raw
    WAV header, then loads the remaining sample data into memory, converts
    it from signed 16-bit integers to normalized floats, and processes each
    sample in sequence.

    The delay is created by mixing the current input sample (dry signal)
    with a past sample from the same buffer (wet signal), offset by a
    fixed number of samples corresponding to the desired delay time in
    milliseconds. This is done using simple array indexing: for sample n,
    the delayed component is taken from input[n - delaySamples], as long as
    n is greater than or equal to the delay offset. Before that point, the
    delayed component is treated as silence.

    After processing, the output samples are clipped to the range [-1, 1],
    converted back to 16-bit integers, and written to a new WAV file
    (delayed_file.wav) using the original header with updated size fields.
    This array-indexing approach works well for offline processing where
    the entire audio file is available in memory, and serves as a clear
    stepping stone before implementing a real-time friendly version using
    circular buffers in a separate program.

    Said seperate program is Project 5: Simple Delay (Circular Buffers)

    Author: Jesse Whiting (jwhiting07)
*/


// Pack the struct so there is no padding between numbers
#pragma pack(push, 1) // Tells the compiler not to add padding bytes between fields of the struct, as WAV headers are a binary format with exact byte layouts
struct WavHeader // Struct for the WAV header, makes it easier to access specific data later
{
    // See hello_sine.cpp
    char riff[4];
    uint32_t chunkSize;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t subchunk2Size;
};
#pragma pack(pop)

// Process a simple fixed-time delay on a mono buffer
// Takes a mono signal -> adds a delayed copy of itself -> returns the combined signal
// input: mono input samples (normalized float -1 to 1)
// output: same size as input, will be filled with delayed audio - empty going in
// sampleRate: 44100
// delayTimeMs: delay time in milliseconds
// dryLevel: amount of original signal
// wetLevel: amount of delayed signal
void applySimpleDelay(const std::vector<float> &input, std::vector<float> &output, float sampleRate, float delayTimeMs, float dryLevel, float wetLevel)
{
    const int numSamples = static_cast<int>(input.size());
    output.assign(numSamples, 0.0f); // Makes sure output has the right size

    // Convert delay time from ms -> samples
    // Ex. 500ms at 44100 Hz -> 22050 samples
    const int delaySamples = static_cast<int>(delayTimeMs * sampleRate / 1000.0f);

    // Loops over each sample, if we're far enough into the buffer (past delaySamples) we grab a past sample (input [n - delaySamples])
    for (int n = 0; n < numSamples; ++n) {
        float inSample = input[n];

        float delayedSample = 0.0f;
        if (n >= delaySamples) {
            delayedSample = input[n - delaySamples];
        }

        // y[n] = dryLevel * x[n] + wetLevel * x[n - D]
        float outSample = dryLevel * inSample + wetLevel * delayedSample;

        // Simple clipping to avoid blowing up (optional)
        outSample = std::clamp(outSample, -1.0f, 1.0f);

        output[n] = outSample;
    }
}

int main() {

    // See gain_processor.cpp
    std::ifstream inFile("hello_sine.wav", std::ios::binary);
    if (!inFile) {
        std::cerr << "Could not open hello_sine.wav";
        return 1;
    }

    WavHeader header{}; // Creates a WavHeader and zero-initializes it
    // Reads exactly sizeof(WavHeader) bytes from the file into header
    inFile.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (!inFile)
    {
        std::cerr << "Failed to read 44-byte header.\n";
        return 1;
    }

    // Grab the sample rate from the header
    const int sampleRate = static_cast<int>(header.sampleRate);
    // Computes how many samples are in the data chunk : subchunk2Size is the number of bytes of audio data
    // The file is mono 16-bit, so each sample is 2 bytes -> devide by sizeof(int16_t)
    const uint32_t numSamples = header.subchunk2Size / sizeof(int16_t);

    std::vector<int16_t> rawSamples(numSamples); // Allocate a vector big enough for numSamples
    inFile.read(reinterpret_cast<char*>(rawSamples.data()), header.subchunk2Size); 

    inFile.close(); // Closes the input file

    std::vector<float> inputSamples(numSamples); // Makes a float buffer of the same size
    // For each sample, interpret rawSamples[i] as an int16_t, which is in the range [-32768, +32767]
    // Divide by 32768.0f to map it roughly to [-1.0, 1.0]
    for (uint32_t i = 0; i < numSamples; ++i) {
        inputSamples[i] = static_cast<float>(rawSamples[i]) / 32768.0f;
    }

    std::vector<float> outputSamples; // Starts empty
    // Set delay effect parameters
    float delayTimeMs = 500.0f;
    float dryLevel = 1.0f;
    float wetLevel = 0.5f;

    applySimpleDelay(inputSamples, outputSamples, static_cast<float>(sampleRate), delayTimeMs, dryLevel, wetLevel);

    // At this point, outputSamples is the processed audio in float form

    // Convert float to int16
    std::vector<int16_t> outRaw(outputSamples.size()); // Makes a new vector for PCM data
    for (size_t i = 0; i < outputSamples.size(); ++i) {
        float s = std::clamp(outputSamples[i], -1.0f, 1.0f); // Clamp each float sample for safety
        int16_t intSample = static_cast<int16_t>(s * 32767.0f); // Multiply by 32767.0f to bring it back to the integer range, cast to int16_t
        outRaw[i] = intSample; // Store above data
    }

    header.subchunk2Size = static_cast<uint32_t>(outRaw.size() * sizeof(int16_t)); // subchunk2Size is the size in bytes of the audio data chunk
    header.chunkSize = 36 + header.subchunk2Size;

    // Write the new WAV file
    std::ofstream outFile("delayed_file.wav", std::ios::binary);
    if(!outFile) {
        std::cerr << "delayed_file.wav could not be opened.";
        return 1;
    }

    // Write the 44-byte WAV header at the start
    // Writes all raw PCM data right after
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
    outFile.write(reinterpret_cast<const char*>(outRaw.data()), header.subchunk2Size);
    
    outFile.close();
    std::cout << "Wrote the delayed file to delayed_file.wav";
    return 0;
}