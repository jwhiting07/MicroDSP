/*
    Project 2: Gain Processor
    
    This program demonstrates how to read an existing 16-bit PCM WAV file, process its audio 
    sample-by-sample, and write the modified samples into a new output WAV file. Instead of 
    using audio libraries or abstractions, this code directly reads the 44-byte WAV header, 
    copies it unchanged to preserve the file’s format, and then sequentially reads each 
    16-bit sample from the input stream. A gain factor is applied to every sample, the value 
    is clamped to the valid 16-bit range, and the processed sample is written back out in 
    raw little-endian form. This provides a hands-on introduction to binary audio processing, 
    PCM data interpretation, streaming file I/O, and the foundations of real-world DSP effects.

    Author: Jesse Whiting (jwhiting07)
*/


#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>

int main()
{
    // Settings
    const double gain = 0.5; // Quiet (half volume)

    // Open input WAV
    // Reads an input file and throws an error message if the stream is in a failed state
    std::ifstream inFile("hello_sine.wav", std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Could not open input.wav\n";
        return 1;
    }

    // Open output WAV
    // See hello_sine.cpp for further details
    std::ofstream outFile("gain_output.wav", std::ios::binary);
    if (!inFile)
    {
        std::cerr << "Could not open input.wav\n";
        return 1;
    }

    // Creates a vector that stores "char" (1 byte each), gives us a  block of 44 bytes
    // Vector is used because you need a safe, dynamic, and contiguous block of raw bytes, and vector is the cleanest way to do that
    std::vector<char> header(44);
    inFile.read(header.data(), 44); // Reads raw bytes from the file, returns a raw pointer to the underlying array, and stores the first 44 bytes of the file in header

    // Write the header directly to the output, as we're using the same informative data, just at a lower amplitude
    outFile.write(header.data(), 44);

    // Process sample data
    // Will loop until the end of the file
    // The file stream keeps an internal cursor that automatically moves forward each time you read bytes.
    while (true)
    {

        std::int16_t sample; // Signed 16-bit integer, exactly like our PCM samples

        // Try reading 2 bytes (one 16-bit sample)
        // Tells read where to write the incoming bytes
        inFile.read(reinterpret_cast<char *>(&sample), sizeof(sample));
        if (!inFile)
            break; // Breaks if the end is reached

        // Apply gain
        // Sample is technically an integer, but when you multiply by gain, C++ automatically promotes sample to double
        // If gain = 0.5 and sample = 1000: processed = 500
        double processed = sample * gain;

        // Prevent overflow (16-bit signed)
        // This is done because we will later convert processed to an integer, and if the value is outside of the legal range, casting can wrap around or overflow
        // This could cause distortion or unintended loudness
        if (processed > 32767)
            processed = 32767;
        if (processed < -32768)
            processed = -32768;

        std::int16_t outSample = static_cast<std::int16_t>(processed); // Converts the processed sample into an integer

        outFile.write(reinterpret_cast<char *>(&outSample), sizeof(outSample)); // Writes the sample to output
    }
    std::cout << "Finished writing output.wav\n";
    return 0;
}