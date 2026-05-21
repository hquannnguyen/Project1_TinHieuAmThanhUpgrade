#pragma once
#include "AudioSignal.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

struct WaveformStats {
    double rms; double peak; double mean; double variance;
    double minVal; double maxVal; double zcr; double dBFS;
    double duration; size_t numSamples; int sampleRate;
};

class WaveformAnalyzer {
public:
    explicit WaveformAnalyzer(const AudioSignal& sig) : sig_(sig) {}

    double getRMS() const { return sig_.getRMS(); }
    double getPeak() const { return sig_.getPeakAmplitude(); }
    double getdBFS() const { return sig_.getdBFS(); }

    double getZCR() const {
        const auto& s = sig_.getSamples();
        if (s.size() < 2) return 0.0;
        int zc = 0;
        for (size_t i = 1; i < s.size(); ++i)
            if ((s[i-1] >= 0.0) != (s[i] >= 0.0)) ++zc;
        return static_cast<double>(zc) / sig_.getDuration();
    }

    double getVariance() const {
        const auto& s = sig_.getSamples();
        if (s.empty()) return 0.0;
        double mu = sig_.getMean();
        double var = 0.0;
        for (double x : s) var += (x - mu) * (x - mu);
        return var / s.size();
    }

    double getCrestFactor() const {
        double rms = getRMS();
        return rms > 0.0 ? getPeak() / rms : 0.0;
    }

    WaveformStats getStats() const {
        return WaveformStats{
            getRMS(), getPeak(), sig_.getMean(), getVariance(),
            sig_.getMin(), sig_.getMax(), getZCR(), getdBFS(),
            sig_.getDuration(), sig_.getNumSamples(), sig_.getSampleRate()
        };
    }

    void printStats(std::ostream& out = std::cout) const {
        auto st = getStats();
        out << "\n  ┌─────────────────────────────────────────┐\n";
        out << "  │  Waveform Analysis: " << std::left << std::setw(20) << sig_.getName() << "│\n";
        out << "  ├─────────────────────────────────────────┤\n";
        auto row = [&](const std::string& k, const std::string& v) {
            out << "  │  " << std::left << std::setw(20) << k << std::right << std::setw(18) << v << "  │\n";
        };
        auto fmt = [](double v, int p=4) {
            std::ostringstream ss; ss << std::fixed << std::setprecision(p) << v; return ss.str();
        };
        row("Sample Rate",  std::to_string(st.sampleRate) + " Hz");
        row("Duration",      fmt(st.duration, 3) + " s");
        row("Num Samples",  std::to_string(st.numSamples));
        out << "  ├─────────────────────────────────────────┤\n";
        row("Peak",          fmt(st.peak));
        row("RMS",           fmt(st.rms));
        row("dBFS",          fmt(st.dBFS, 2) + " dB");
        row("Mean",          fmt(st.mean));
        row("ZCR",           fmt(st.zcr, 1) + " /s");
        row("Crest Factor",  fmt(getCrestFactor(), 2));
        out << "  └─────────────────────────────────────────┘\n";
    }

    void plotWaveform(std::ostream& out = std::cout, int width = 60, int height = 12, int maxSamples = 400) const {
        const auto& s = sig_.getSamples();
        if (s.empty()) return;

        size_t step = std::max((size_t)1, s.size() / maxSamples);
        std::vector<double> disp;
        for (size_t i = 0; i < s.size(); i += step) disp.push_back(s[i]);

        double pk = getPeak(); if (pk == 0.0) pk = 1.0;
        int W = std::min(width, (int)disp.size());
        std::vector<std::string> grid(height, std::string(W, ' '));

        for (int x = 0; x < W; ++x) {
            size_t idx = x * disp.size() / W;
            double val = disp[idx] / pk;
            int y = static_cast<int>((1.0 - val) / 2.0 * (height - 1));
            y = std::max(0, std::min(height-1, y));
            grid[y][x] = '*';
        }

        out << "\n  Waveform Graphic Display: " << sig_.getName() << "\n";
        out << "   +1.0 |"; for (int x = 0; x < W; ++x) out << '-'; out << "|\n";
        for (int row = 0; row < height; ++row) {
            if (row == height/2) out << "    0.0 |" << grid[row] << "|\n";
            else                 out << "        |" << grid[row] << "|\n";
        }
        out << "   -1.0 |"; for (int x = 0; x < W; ++x) out << '-'; out << "|\n";
        out << "          0" << std::string(W-10, ' ') << std::fixed << std::setprecision(3) << sig_.getDuration() << "s\n";
    }

private:
    const AudioSignal& sig_;
};