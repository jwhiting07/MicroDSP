/*
    This program demonstrates a "zero-latency" style bypass with a smooth
    crossfade between dry and wet audio. It reads a 16-bit PCM mono WAV
    file (input.wav), copies its 44-byte header to a new file
    (output_bypass.wav), and then processes each sample in sequence.

    For the first second of audio, the output is fully dry (original signal).
    Then, over a short fade window (e.g., 10 ms), it linearly ramps from
    dry to wet, where the wet signal is simply the input scaled by a gain
    factor. After the fade is complete, the output is fully wet.

    This avoids clicks that would occur if you switched instantly from dry
    to wet, and models the kind of smoothing you would use for a bypass
    switch or parameter change in a real-time audio plugin.

    Author: Jesse Whiting (jwhiting07)
*/

#define _USE_MATH_DEFINES
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>

int main()
{
    // Settings
    const double gain = 2.0;               // Effect gain (wet signal multiplier)
    const int sampleRate = 44100;          // Assime 44.1 kHz WAV (for timing)
    const double fadeMs = 10.0;            // Crossfade duration in milliseconds.
    const double bypassUntilSeconds = 1.0; // Bypass for the first 1s, then fade to wet

    const int fadeSamples = static_cast<int>(sampleRate * (fadeMs / 1000));
    const int fadeStartSample = static_cast<int>(sampleRate * bypassUntilSeconds);
    const int fadeEndSample = fadeStartSample + fadeSamples;

    // Open input and output files
    std::ifstream inFile("hello_sine.wav", std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Could not open hello_sine.wav\n";
        return 1;
    }

    std::ofstream outFile("output_bypass.wav", std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Could not open output_bypass.wav";
        return 1;
    }

    // Copy of 44-byte WAV header
    std::vector<char> header(44);
    inFile.read(header.data(), 44);
    if (!inFile)
    {
        std::cerr << "Failed to read 44-byte header.\n";
        return 1;
    }
    outFile.write(header.data(), 44);

    // Process sample by sample with smooth bypass fade
    std::int16_t sample = 0;
    int sampleIndex = 0;

    while (true)
    {
        // Read one 16-bit sample (2 bytes)
        inFile.read(reinterpret_cast<char *>(&sample), sizeof(sample));
        if (!inFile)
            break;

        // Dry and wet versions of the signal
        double dry = static_cast<double>(sample);
        double wet = dry * gain;

        // Compute mix value based on time/sample index
        // mix = 0 -> fully dry
        // mix = 1.0 -> fully wet
        double mix = 0.0;

        if (sampleIndex < fadeStartSample)
        {
            // Fully dry
            mix = 0.0;
        }
        else if (sampleIndex >= fadeEndSample)
        {
            // Fully wet
            mix = 1.0;
        }
        else
        {
            // During fade: ramp linearly from 0 to 1
            int fadePos = sampleIndex - fadeStartSample;
            mix = static_cast<double>(fadePos) / static_cast<double>(fadeSamples);
            // Ensures mix moves smoothly from (almost) 0 to (almost) 1

            // Optional safety clamp
            if (mix < 0.0)
                mix = 0.0;
            if (mix > 1.0)
                mix = 1.0;
        }

        // If mix = 0, then (1 - 0) * sample + 0 * altered sample = sample, which is just the dry signal
        // If mix = 1, then (1 - 1) * sample + 1 * altered sample = altered sample, which is the wet signal
        double outSampleDouble = (1.0 - mix) * dry + mix * wet;

        // Clamp to 16-bit signed range (see gain_processor.cpp)
        if (outSampleDouble > 32767.0)
            outSampleDouble = 32767.0;
        if (outSampleDouble < -32768.0)
            outSampleDouble = -32768.0;

        // Convert back to 16-bit integer
        std::int16_t outSample = static_cast<std::int16_t>(outSampleDouble);

        // Write processed sample
        outFile.write(reinterpret_cast<const char *>(&outSample), sizeof(outSample));

        ++sampleIndex;
    }

    std::cout << "Finished writing output_bypass.wav with smooth bypass fade.\n";
    return 0;
}