/*
    Project 1: Hello Sine
    
    This program demonstrates how to generate a pure 440 Hz sine wave and write it as a valid
    PCM WAV file entirely from scratch in C++. Rather than relying on audio libraries, this code
    manually constructs the 44-byte WAV header, calculates all required RIFF/WAVE fields
    (such as chunkSize, byteRate, blockAlign, and dataSize), and writes raw 16-bit little-endian
    PCM samples directly to disk. It’s a practical introduction to digital audio fundamentals,
    binary file I/O, sample-by-sample waveform construction, and the structure of WAV files.

    Author: Jesse Whiting (jwhiting07)
*/

#define _USE_MATH_DEFINES ;
#include <iostream>
#include <fstream> // File creation/writing, std::ofstream
#include <cmath>
#include <cstdint> // WAV files require numbers of specific byte sizes
// std::uint16_t is an unsigned 16-bit integer. This library gives us integer types with exact, guarenteed sizes.
// WAV headers require that certain fields be specific sizes in bytes

int main()
{
    // Basic audio settings
    const int sampleRate = 44100;
    const double durationSeconds = 2;
    const double frequency = 440.0; // A4

    const int numChannels = 1;    // mono
    const int bitsPerSample = 16; // Each sample (one time point) will be stored as a 16-bit integer (16 bits = 2 bytes). This is standard "CD quality" PCM.
    // numSamples = how many discrete audio points we will generate and write
    const int numSamples = static_cast<int>(sampleRate * durationSeconds); // static_cast converts the floating-point result to an integer type.

    // Bytes = bits/8. This is the number of bytes needed to store each sample.
    const int bytesPerSample = bitsPerSample / 8; // This value is later multiplied by bytes to calculate total file size, which is necessary for DAW interpretation

    const int byteRate = sampleRate * numChannels * bytesPerSample; // This value is how many bytes of audio data occur per second

    // This measures how many bytes represent one time step across all channels
    const int blockAlign = numChannels * bytesPerSample; // Every audio frame must be aligned exactly to prevent broken audio

    // Calculates total size in bytes of the actual audio data.
    // The player needs to know exactly where the audio ends. if the data size is wrong, playback fails or continues into garbage.
    const int dataSize = numSamples * numChannels * bytesPerSample;

    // chunkSize is a field in the WAV header that means the total number of bytes in the format
    // WAVE structure: first 4 bytes: "RIFF", next 4 bytes: chunkSize, then the rest (36 bytes of header + data)
    // RIFF readers use chunkSize to verify the entire file's integrity
    // chunkSize = 44 (header size) - 8 (see WAVE structure) + size of data
    const int chunkSize = 36 + dataSize;

    // To clarify any confusion, the chunk size is the size of the entire RIFF chunk, not including the first 8 bytes, which is "RIFF" and the chunkSize itself.

    // Open output file
    std::ofstream outFile("hello_sine.wav", std::ios::binary); // std::ofstream(filename to create/write, open in binary mode)
    // Checks whether the file stream is in a valid state, if not throws an error message and exits main()
    if (!outFile)
    {
        std::cerr << "Failed to open output file.\n";
        return 1;
    }

    // Write WAV header (44 bytes)
    // RIFF chunk descriptor
    outFile.write("RIFF", 4); // outFile.write(const char* data, std::streamsize size) writes raw bytes, identifies the file as a RIFF-format file
    // Tells the file reader how big the main RIFF chunk is
    std::uint32_t chunkSizeLE = chunkSize;                          // Unsigned 32-bit integer, chunkSizeLE holds the chunkSize value in a variable that is explicitly 4 bytes
    outFile.write(reinterpret_cast<const char *>(&chunkSizeLE), 4); // Treats the pointer to a 32-bit integer as a pointer to a sequence of bytes (chars)
    outFile.write("WAVE", 4);                                       // Writes 4 bytes (chars), identifies the file as a WAVE file
    // Total: 12 bytes for this chunk

    // At this point, we've written "RIFF" (4 bytes), chunkSize (4 bytes), and "WAVE" (4 bytes) = 12 bytes

    // Format subchunk
    // Whitespace is intentional, as based on WAVE headers it must be four bytes. The space is a char, and is thus one byte. Tells the reader "the next section describes the audio format"
    outFile.write("fmt ", 4);
    std::uint32_t subchunk1Size = 16; // In PCM WAV, the fmt chunk has 16 bytes of format data
    std::uint16_t audioFormat = 1;    // 1 = PCM (uncompressed). Other values would be compressed formats
    // The following five variables are copies of our previously calculated values into fixed-size types that match teh WAV spec. The LE suffix means the format is Little-Endian.
    // Every data item inside a chunk must use exact fixed sizes defined by RIFF, which is why some are 16 bit and some are 32 bit.
    std::uint16_t numChannelsLE = numChannels;
    std::uint32_t sampleRateLE = sampleRate;
    std::uint32_t byteRateLE = byteRate;
    std::uint16_t blockAlignLE = blockAlign;
    std::uint16_t bitsPerSampleLE = bitsPerSample;

    outFile.write(reinterpret_cast<const char *>(&subchunk1Size), 4);   // Writes 4 bytes: the size of the fmt chunk (16)
    outFile.write(reinterpret_cast<const char *>(&audioFormat), 2);     // Writes 2 bytes: audio format (1 = PCM)
    outFile.write(reinterpret_cast<const char *>(&numChannelsLE), 2);   // Writes 2 bytes: number of channels (1 for mono)
    outFile.write(reinterpret_cast<const char *>(&sampleRateLE), 4);    // Writes 4 bytes: sample rate (44100)
    outFile.write(reinterpret_cast<const char *>(&byteRateLE), 4);      // Writes 4 bytes: byteRate (bytes per second of audio)
    outFile.write(reinterpret_cast<const char *>(&blockAlignLE), 2);    // Writes 2 bytes: blockAlign (bytes per sample frame)
    outFile.write(reinterpret_cast<const char *>(&bitsPerSampleLE), 2); // Writes 2 bytes: bitsPerSample (16)
    // Total: 24 bytes for this subchunk

    // Data subchunk
    outFile.write("data", 4);                                      // Starts the data subchunk: the raw audio data container
    std::uint32_t dataSizeLE = dataSize;                           // Number of bytes of audio data
    outFile.write(reinterpret_cast<const char *>(&dataSizeLE), 4); // Writes 4 bytes: this is Subchunk2Size in WAV spec, tells the reader exactly how many bytes of samples follow
    // Total: 8 bytes for this subchunk

    // Up until this point, we've added a total of 44 bytes, which is the size of a WAV header.

    // Our samples will be 16-bit integers that range from -32768 to +32767. We want the sine wave to stay inside this range to prevent clipping.
    // We half this value, meaning the result will be half as loud as the maximum possible, to give us some headroom.
    const double amplitude = 0.5 * 32767.0; // Max value = 16383.5
    for (int n = 0; n < numSamples; ++n)    // Start from 0, run through each sample, increase n by 1 after each runthrough.
    {
        // sample 0 -> t = 0/44100 = 0 seconds
        // sample 44100 -> t = 44100/44100 = 1 seconds
        double t = static_cast<double>(n) / sampleRate; // Seconds passed, with n cast to a double so we get a fractional time rather than integer.
        // t is the continuous time representation of sample index n

        // This is the main function. std::sin computes the sine of an angle in radians.
        // A sine wave at a given frequency can be described as: x(t) = A * sin(2πft), where:
        // A = amplitude
        // f = frequency in Hz
        // t = time in seconds
        double sampleValue = amplitude * std::sin(2.0 * M_PI * frequency * t); // Floating-point audio value for that sample

        // WAV expects actual integers, not floating-point numbers, so this next piece converts the double to a 16-bit integer by truncating.
        std::int16_t intSample = static_cast<std::int16_t>(sampleValue);

        // Now we have a 16-bit PCM-ready sample in intSample.

        // Now we write those 16 bits (2 bytes) into the file.
        // sizeof(intSample) is how many bytes to write, which is 2 for a 16 bit sample.
        outFile.write(reinterpret_cast<const char *>(&intSample), sizeof(intSample));
    }
    outFile.close(); // Closes the file, flushing remaining buffered data and releases handles.
    std::cout << "Wrote hello_sine.wav with " << numSamples << " samples.\n";
    return 0;
}