#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// This program reads a 16-bit PCM WAV file and writes an uncompressed AVI video that
// visualizes the waveform scrolling across the screen. It intentionally avoids external
// dependencies, relying only on the C++ standard library to both parse the WAV file and
// write a valid RIFF/AVI container with 24-bit RGB frames.
//
// Usage (example):
//   g++ waveform_video.cpp -o waveform_video
//   ./waveform_video input.wav output.avi
//
// The output uses a 1280x720 canvas at 30 FPS. Each frame shows one slice of audio that
// spans 1 / FPS seconds, mapped horizontally across the frame. The program averages stereo
// files down to mono so the display stays simple.

struct WavData
{
    int sampleRate = 0;
    int numChannels = 0;
    std::vector<float> samples; // normalized to [-1, 1]
};

static std::uint16_t readUint16LE(std::ifstream &in)
{
    std::uint8_t bytes[2];
    in.read(reinterpret_cast<char *>(bytes), 2);
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

static std::uint32_t readUint32LE(std::ifstream &in)
{
    std::uint8_t bytes[4];
    in.read(reinterpret_cast<char *>(bytes), 4);
    return static_cast<std::uint32_t>(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
}

static void writeUint16LE(std::ofstream &out, std::uint16_t value)
{
    char bytes[2];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    out.write(bytes, 2);
}

static void writeUint32LE(std::ofstream &out, std::uint32_t value)
{
    char bytes[4];
    bytes[0] = static_cast<char>(value & 0xFF);
    bytes[1] = static_cast<char>((value >> 8) & 0xFF);
    bytes[2] = static_cast<char>((value >> 16) & 0xFF);
    bytes[3] = static_cast<char>((value >> 24) & 0xFF);
    out.write(bytes, 4);
}

static WavData readWav(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Failed to open WAV file: " + path);
    }

    char riff[4];
    in.read(riff, 4);
    if (std::string(riff, 4) != "RIFF")
    {
        throw std::runtime_error("Not a RIFF file");
    }
    (void)readUint32LE(in); // overall chunk size, unused here

    char wave[4];
    in.read(wave, 4);
    if (std::string(wave, 4) != "WAVE")
    {
        throw std::runtime_error("Not a WAVE file");
    }

    bool fmtFound = false;
    bool dataFound = false;
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
    std::vector<float> samples;

    while (in && (!fmtFound || !dataFound))
    {
        char chunkId[4];
        if (!in.read(chunkId, 4))
        {
            break;
        }
        std::uint32_t chunkSize = readUint32LE(in);
        std::string id(chunkId, 4);

        if (id == "fmt ")
        {
            fmtFound = true;
            std::uint16_t audioFormat = readUint16LE(in);
            channels = readUint16LE(in);
            sampleRate = static_cast<int>(readUint32LE(in));
            (void)readUint32LE(in); // byteRate
            (void)readUint16LE(in); // blockAlign
            bitsPerSample = static_cast<int>(readUint16LE(in));

            // Skip any remaining fmt bytes
            if (chunkSize > 16)
            {
                in.seekg(chunkSize - 16, std::ios::cur);
            }

            if (audioFormat != 1 || bitsPerSample != 16)
            {
                throw std::runtime_error("Only 16-bit PCM WAV files are supported");
            }
        }
        else if (id == "data")
        {
            dataFound = true;
            std::vector<std::int16_t> raw(chunkSize / 2);
            in.read(reinterpret_cast<char *>(raw.data()), chunkSize);
            samples.reserve(raw.size());
            for (std::size_t i = 0; i < raw.size(); i += channels)
            {
                int accumulator = 0;
                for (int c = 0; c < channels; ++c)
                {
                    accumulator += raw[i + c];
                }
                float mono = static_cast<float>(accumulator) / (32768.0f * channels);
                samples.push_back(std::clamp(mono, -1.0f, 1.0f));
            }
        }
        else
        {
            // Skip unknown chunk
            in.seekg(chunkSize, std::ios::cur);
        }

        // Chunks are word aligned; if size is odd, skip a padding byte
        if (chunkSize % 2 == 1)
        {
            in.seekg(1, std::ios::cur);
        }
    }

    if (!fmtFound || !dataFound)
    {
        throw std::runtime_error("Invalid WAV file: missing fmt or data chunk");
    }

    WavData wav;
    wav.sampleRate = sampleRate;
    wav.numChannels = channels;
    wav.samples = std::move(samples);
    return wav;
}

static void drawPixel(std::vector<std::uint8_t> &frame, int rowStride, int x, int y, int height,
                      std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    if (x < 0 || y < 0 || y >= height)
        return;

    // BMP data in AVI is stored bottom-up, so row 0 is the bottom.
    int invertedY = height - 1 - y;
    std::size_t index = invertedY * rowStride + x * 3;
    frame[index + 0] = b;
    frame[index + 1] = g;
    frame[index + 2] = r;
}

static void writeAvi(const std::string &path, const WavData &wav)
{
    const int width = 1280;
    const int height = 720;
    const int fps = 30;

    const int bytesPerPixel = 3;
    const int rowStride = ((width * bytesPerPixel + 3) / 4) * 4; // 4-byte aligned rows
    const std::uint32_t frameDataSize = rowStride * height;

    const double samplesPerFrame = static_cast<double>(wav.sampleRate) / fps;
    const std::size_t frameCount = static_cast<std::size_t>(std::ceil(wav.samples.size() / samplesPerFrame));
    if (frameCount == 0)
    {
        throw std::runtime_error("Input WAV contained no samples");
    }

    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("Failed to open output file: " + path);
    }

    auto riffSizePos = out.tellp();
    out.write("RIFF", 4);
    writeUint32LE(out, 0); // Placeholder for RIFF chunk size
    out.write("AVI ", 4);

    // LIST hdrl
    out.write("LIST", 4);
    const std::uint32_t hdrlSize = 4 + (8 + 56) + (8 + 56) + (8 + 40);
    writeUint32LE(out, hdrlSize);
    out.write("hdrl", 4);

    // avih chunk (Main AVI Header)
    out.write("avih", 4);
    writeUint32LE(out, 56);
    const std::uint32_t microSecPerFrame = static_cast<std::uint32_t>(1'000'000 / fps);
    writeUint32LE(out, microSecPerFrame);
    const std::uint32_t maxBytesPerSec = frameDataSize * fps;
    writeUint32LE(out, maxBytesPerSec);
    writeUint32LE(out, 0);               // padding granularity
    writeUint32LE(out, 0x10);            // flags: AVIF_HASINDEX
    writeUint32LE(out, frameCount);      // total frames
    writeUint32LE(out, 0);               // initial frames
    writeUint32LE(out, 1);               // streams
    writeUint32LE(out, frameDataSize);   // suggested buffer size
    writeUint32LE(out, width);
    writeUint32LE(out, height);
    writeUint32LE(out, 0); // reserved[0]
    writeUint32LE(out, 0); // reserved[1]
    writeUint32LE(out, 0); // reserved[2]
    writeUint32LE(out, 0); // reserved[3]

    // LIST strl
    out.write("LIST", 4);
    const std::uint32_t strlSize = 4 + (8 + 56) + (8 + 40);
    writeUint32LE(out, strlSize);
    out.write("strl", 4);

    // strh chunk (Stream header)
    out.write("strh", 4);
    writeUint32LE(out, 56);
    out.write("vids", 4);      // fccType
    out.write("DIB ", 4);      // fccHandler (uncompressed)
    writeUint32LE(out, 0);     // flags
    writeUint16LE(out, 0);     // priority
    writeUint16LE(out, 0);     // language
    writeUint32LE(out, 0);     // initial frames
    writeUint32LE(out, 1);     // scale
    writeUint32LE(out, fps);   // rate
    writeUint32LE(out, 0);     // start
    writeUint32LE(out, frameCount);
    writeUint32LE(out, frameDataSize); // suggested buffer size
    writeUint32LE(out, 0xFFFFFFFF);    // quality
    writeUint32LE(out, 0);             // sample size (0 for video)
    writeUint16LE(out, 0);             // rcFrame left
    writeUint16LE(out, 0);             // rcFrame top
    writeUint16LE(out, 0);             // rcFrame right
    writeUint16LE(out, 0);             // rcFrame bottom

    // strf chunk (BITMAPINFOHEADER)
    out.write("strf", 4);
    writeUint32LE(out, 40);
    writeUint32LE(out, width);
    writeUint32LE(out, height);
    writeUint16LE(out, 1);              // planes
    writeUint16LE(out, 24);             // bit count
    writeUint32LE(out, 0);              // compression (BI_RGB)
    writeUint32LE(out, frameDataSize);  // image size
    writeUint32LE(out, 0);              // XPelsPerMeter
    writeUint32LE(out, 0);              // YPelsPerMeter
    writeUint32LE(out, 0);              // ClrUsed
    writeUint32LE(out, 0);              // ClrImportant

    // LIST movi
    out.write("LIST", 4);
    std::streampos moviSizePos = out.tellp();
    writeUint32LE(out, 0); // placeholder
    out.write("movi", 4);
    std::streampos moviDataStart = out.tellp();

    std::vector<std::uint32_t> frameOffsets;
    frameOffsets.reserve(frameCount);

    std::vector<std::uint8_t> frame(frameDataSize, 0);
    const int midY = height / 2;
    const int waveformHeight = static_cast<int>(height * 0.4);

    for (std::size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        std::fill(frame.begin(), frame.end(), 0);

        // Draw center line
        for (int x = 0; x < width; ++x)
        {
            drawPixel(frame, rowStride, x, midY, height, 30, 30, 30);
        }

        const std::size_t startSample = static_cast<std::size_t>(frameIndex * samplesPerFrame);
        for (int x = 0; x < width; ++x)
        {
            double samplePos = startSample + (samplesPerFrame * x) / width;
            std::size_t idx = static_cast<std::size_t>(samplePos);
            if (idx >= wav.samples.size())
            {
                break;
            }
            double amplitude = wav.samples[idx];
            int y = static_cast<int>(midY - amplitude * waveformHeight);
            drawPixel(frame, rowStride, x, y, height, 50, 200, 120);
        }

        std::streampos chunkStart = out.tellp();
        frameOffsets.push_back(static_cast<std::uint32_t>(chunkStart - moviDataStart));

        out.write("00db", 4);
        writeUint32LE(out, frameDataSize);
        out.write(reinterpret_cast<const char *>(frame.data()), frameDataSize);

        if (frameDataSize % 2 == 1)
        {
            out.put('\0');
        }
    }

    std::streampos moviEnd = out.tellp();
    std::uint32_t moviSize = static_cast<std::uint32_t>(moviEnd - moviSizePos - 4);
    out.seekp(moviSizePos);
    writeUint32LE(out, moviSize);
    out.seekp(moviEnd);

    // idx1 chunk
    out.write("idx1", 4);
    writeUint32LE(out, frameCount * 16);
    for (std::size_t i = 0; i < frameCount; ++i)
    {
        out.write("00db", 4);
        writeUint32LE(out, 0x10); // AVIIF_KEYFRAME
        writeUint32LE(out, frameOffsets[i]);
        writeUint32LE(out, frameDataSize);
    }

    std::streampos fileEnd = out.tellp();
    std::uint32_t riffSize = static_cast<std::uint32_t>(fileEnd - riffSizePos - 4);
    out.seekp(riffSizePos + std::streamoff(4));
    writeUint32LE(out, riffSize);
    out.close();

    std::cout << "Wrote " << frameCount << " frames to " << path << "\n";
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " input.wav output.avi\n";
        return 1;
    }

    try
    {
        WavData wav = readWav(argv[1]);
        writeAvi(argv[2], wav);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
