#pragma once
#include "AudioSignal.h"
#include "Constants.h"
#include <fstream>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace wav_impl {
#pragma pack(push,1)
struct WAVHeader {
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
} 

class AudioIO {
public:
    static bool writeWAV(const std::string& path, const AudioSignal& sig) {
        if (sig.empty()) return false;
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        const int SR = sig.getSampleRate();
        const int CH = sig.getNumChannels();
        const size_t N = sig.getNumSamples();

        wav_impl::WAVHeader h;
        std::memcpy(h.riff, "RIFF", 4);
        std::memcpy(h.wave, "WAVE", 4);
        std::memcpy(h.fmt,  "fmt ", 4);
        std::memcpy(h.data,  "data", 4);

        h.subchunk1Size = 16;
        h.audioFormat = 1;
        h.numChannels = static_cast<uint16_t>(CH);
        h.sampleRate = static_cast<uint32_t>(SR);
        h.bitsPerSample = 16;
        h.blockAlign = static_cast<uint16_t>(CH * 2);
        h.byteRate = SR * h.blockAlign;
        h.subchunk2Size = static_cast<uint32_t>(N * 2);
        h.chunkSize = 36 + h.subchunk2Size;

        f.write(reinterpret_cast<const char*>(&h), sizeof(h));
        for (double s : sig.getSamples()) {
            double clamped = std::max(-1.0, std::min(1.0, s));
            int16_t pcm = static_cast<int16_t>(clamped * 32767.0);
            f.write(reinterpret_cast<const char*>(&pcm), 2);
        }
        return f.good();
    }

    static AudioSignal readWAV(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open file: " + path);

        wav_impl::WAVHeader h;
        f.read(reinterpret_cast<char*>(&h), sizeof(h));
        if (!f || std::strncmp(h.riff, "RIFF", 4) || std::strncmp(h.wave, "WAVE", 4))
            throw std::runtime_error("Invalid WAV file structure");

        const size_t sampleCount = h.subchunk2Size / (h.bitsPerSample / 8);
        std::vector<double> samples(sampleCount);

        for (size_t i = 0; i < sampleCount; ++i) {
            if (h.bitsPerSample == 16) {
                int16_t pcm;
                f.read(reinterpret_cast<char*>(&pcm), 2);
                samples[i] = pcm / 32768.0;
            } else {
                uint8_t pcm;
                f.read(reinterpret_cast<char*>(&pcm), 1);
                samples[i] = (pcm - 128) / 128.0;
            }
        }

        std::string name = path;
        auto slash = path.find_last_of("/\\");
        if (slash != std::string::npos) name = path.substr(slash + 1);

        return AudioSignal(std::move(samples), h.sampleRate, h.numChannels, h.bitsPerSample, name);
    }
};

class AudioFactory {
public:
    static AudioSignal fromFile(const std::string& path) { return AudioIO::readWAV(path); }

    static AudioSignal fromSine(double freq, double duration, double amplitude = 0.8, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i)
            buf[i] = amplitude * std::sin(2.0 * AudioConstants::PI * freq * i / sr);
        std::ostringstream ss; ss << "sine_" << (int)freq << "Hz";
        return AudioSignal(std::move(buf), sr, 1, 16, ss.str());
    }

    static AudioSignal fromSquare(double freq, double duration, double amplitude = 0.6, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i)
            buf[i] = amplitude * (std::sin(2.0 * AudioConstants::PI * freq * i / sr) >= 0.0 ? 1.0 : -1.0);
        std::ostringstream ss; ss << "square_" << (int)freq << "Hz";
        return AudioSignal(std::move(buf), sr, 1, 16, ss.str());
    }

    static AudioSignal fromTriangle(double freq, double duration, double amplitude = 0.8, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i) {
            double t = std::fmod(freq * i / sr, 1.0);
            buf[i] = amplitude * (2.0 * std::abs(2.0 * t - 1.0) - 1.0);
        }
        std::ostringstream ss; ss << "triangle_" << (int)freq << "Hz";
        return AudioSignal(std::move(buf), sr, 1, 16, ss.str());
    }

    static AudioSignal fromWhiteNoise(double duration, double amplitude = 0.3, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        std::srand(42);
        for (size_t i = 0; i < N; ++i)
            buf[i] = amplitude * (2.0 * std::rand() / RAND_MAX - 1.0);
        return AudioSignal(std::move(buf), sr, 1, 16, "white_noise");
    }

    static AudioSignal fromDTMF(double f1, double f2, double duration, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i)
            buf[i] = 0.5 * (std::sin(2.0 * AudioConstants::PI * f1 * i / sr) + std::sin(2.0 * AudioConstants::PI * f2 * i / sr));
        std::ostringstream ss; ss << "dtmf_" << (int)f1 << "_" << (int)f2;
        return AudioSignal(std::move(buf), sr, 1, 16, ss.str());
    }

    static AudioSignal mix_freqs(double f1, double f2, double duration, double amp = 0.5, int sr = 44100) {
        size_t N = static_cast<size_t>(sr * duration);
        std::vector<double> buf(N);
        for (size_t i = 0; i < N; ++i)
            buf[i] = amp * (std::sin(2.0 * AudioConstants::PI * f1 * i / sr) + std::sin(2.0 * AudioConstants::PI * f2 * i / sr)) / 2.0;
        std::ostringstream ss; ss << "mix_" << (int)f1 << "_" << (int)f2 << "Hz";
        return AudioSignal(std::move(buf), sr, 1, 16, ss.str());
    }
};