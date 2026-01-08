/*
    Project 3 (INTENTIONALLY CLICKY): Hard Bypass Gain Switch

    This version is deliberately written to create a click/pop by doing an
    INSTANT switch from dry to wet (no crossfade). The discontinuity at the
    switch sample is what produces the click.
*/

#define _USE_MATH_DEFINES
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>

int main()
{
    // Settings
    const double gain = 2.0;               // Wet gain (makes the jump bigger)
    const int sampleRate = 44100;          // Assume 44.1 kHz WAV (for timing)
    const double bypassUntilSeconds = 1.0; // Stay dry for first 1s, then HARD switch

    const int switchSample = static_cast<int>(sampleRate * bypassUntilSeconds);

    // Open input and output files
    std::ifstream inFile("hello_sine.wav", std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Could not open hello_sine.wav\n";
        return 1;
    }

    std::ofstream outFile("output_clicky.wav", std::ios::binary);
    if (!outFile)
    {
        std::cerr << "Could not open output_clicky.wav\n";
        return 1;
    }

    // Copy 44-byte WAV header
    std::vector<char> header(44);
    inFile.read(header.data(), 44);
    if (!inFile)
    {
        std::cerr << "Failed to read 44-byte header.\n";
        return 1;
    }
    outFile.write(header.data(), 44);

    // Process sample-by-sample with a HARD switch (this causes the click)
    std::int16_t sample = 0;
    int sampleIndex = 0;

    while (true)
    {
        inFile.read(reinterpret_cast<char*>(&sample), sizeof(sample));
        if (!inFile)
            break;

        double dry = static_cast<double>(sample);
        double wet = dry * gain;

        // INTENTIONAL: abrupt mix jump at switchSample
        // Before switchSample: dry only
        // From switchSample onward: wet only
        double outSampleDouble = (sampleIndex < switchSample) ? dry : wet;

        // Clamp to 16-bit signed range
        if (outSampleDouble > 32767.0) outSampleDouble = 32767.0;
        if (outSampleDouble < -32768.0) outSampleDouble = -32768.0;

        std::int16_t outSample = static_cast<std::int16_t>(outSampleDouble);
        outFile.write(reinterpret_cast<const char*>(&outSample), sizeof(outSample));

        ++sampleIndex;
    }

    std::cout << "Finished writing output_clicky.wav (hard switch -> click/pop).\n";
    return 0;
}
