#pragma once
#include "AudioSignal.h"
#include "Constants.h"
#include <complex>
#include <vector>
#include <cmath>
#include <memory>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>

using Complex = std::complex<double>;

class FFTStrategy {
public:
    virtual ~FFTStrategy() = default;
    virtual std::vector<Complex> compute(const std::vector<double>& samples) = 0;
    virtual std::string name() const = 0;
};

class DFTImpl : public FFTStrategy {
public:
    std::vector<Complex> compute(const std::vector<double>& x) override {
        const size_t N = x.size();
        const size_t half = N / 2 + 1;
        std::vector<Complex> out(half);
        for (size_t k = 0; k < half; ++k) {
            Complex sum(0.0, 0.0);
            for (size_t n = 0; n < N; ++n)
                sum += x[n] * std::polar(1.0, -2.0 * AudioConstants::PI * k * n / N);
            out[k] = sum;
        }
        return out;
    }
    std::string name() const override { return "DFT (O(N²))"; }
};

class CooleyTukeyFFT : public FFTStrategy {
public:
    std::vector<Complex> compute(const std::vector<double>& x) override {
        size_t N_pad = 1;
        while (N_pad < x.size()) N_pad <<= 1; 

        std::vector<Complex> a(N_pad, 0.0);
        for (size_t i = 0; i < x.size(); ++i) a[i] = x[i];

        fft(a);
        size_t half = N_pad / 2 + 1; 
        return std::vector<Complex>(a.begin(), a.begin() + half);
    }
    std::string name() const override { return "Cooley-Tukey FFT (O(N log N))"; }

private:
    static void fft(std::vector<Complex>& a) {
        const size_t N = a.size();
        for (size_t i = 1, j = 0; i < N; ++i) {
            size_t bit = N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (size_t len = 2; len <= N; len <<= 1) {
            Complex wlen = std::polar(1.0, -2.0 * AudioConstants::PI / len);
            for (size_t i = 0; i < N; i += len) {
                Complex w(1.0, 0.0);
                for (size_t j = 0; j < len/2; ++j) {
                    Complex u = a[i+j], v = a[i+j+len/2] * w;
                    a[i+j] = u + v;
                    a[i+j+len/2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
};

struct SpectralResult {
    std::vector<double> magnitudes;
    std::vector<double> freqBins;
    double spectralCentroid;
    double dominantFreq;
    double totalEnergy;
};

class FFTProcessor {
public:
    explicit FFTProcessor(std::unique_ptr<FFTStrategy> strategy = nullptr) {
        if (strategy) strategy_ = std::move(strategy);
        else          strategy_ = std::make_unique<CooleyTukeyFFT>();
    }

    void setStrategy(std::unique_ptr<FFTStrategy> s) { strategy_ = std::move(s); }
    std::string getStrategyName() const { return strategy_->name(); }

    SpectralResult computeSpectrum(const AudioSignal& sig) {
        const auto& samples = sig.getSamples();
        if (samples.empty()) return {};

        auto complex_spec = strategy_->compute(samples);
        const size_t halfN = complex_spec.size();
        const int SR = sig.getSampleRate();
        const size_t N_fft = (halfN - 1) * 2; 

        SpectralResult result;
        result.magnitudes.resize(halfN);
        result.freqBins.resize(halfN);

        double maxMag = 0.0;
        for (size_t k = 0; k < halfN; ++k) {
            result.magnitudes[k] = std::abs(complex_spec[k]) / samples.size();
            result.freqBins[k] = static_cast<double>(k) * SR / N_fft;
            maxMag = std::max(maxMag, result.magnitudes[k]);
        }

        if (maxMag > 0.0)
            for (auto& m : result.magnitudes) m /= maxMag;

        double num = 0.0, den = 0.0;
        for (size_t k = 0; k < halfN; ++k) {
            num += result.freqBins[k] * result.magnitudes[k];
            den += result.magnitudes[k];
        }
        result.spectralCentroid = den > 0 ? num / den : 0.0;

        auto it = std::max_element(result.magnitudes.begin(), result.magnitudes.end());
        result.dominantFreq = result.freqBins[it - result.magnitudes.begin()];
        result.totalEnergy = den;

        return result;
    }

    void plotSpectrum(const AudioSignal& sig, std::ostream& out = std::cout, int barWidth = 60, double maxFreq = 4000.0) {
        auto result = this->computeSpectrum(sig);
        if (result.magnitudes.empty()) return;

        std::vector<std::pair<double,double>> bins;
        for (size_t k = 0; k < result.freqBins.size(); ++k) {
            if (result.freqBins[k] > maxFreq) break;
            bins.push_back({result.freqBins[k], result.magnitudes[k]});
        }

        const int GROUPS = 24;
        out << "\n  Spectrum Visualizer: " << sig.getName() << " [" << strategy_->name() << "]\n";
        out << "  Dominant Freq: " << std::fixed << std::setprecision(1) << result.dominantFreq << " Hz\n\n";

        if (bins.empty()) return;
        size_t groupSize = std::max(1UL, bins.size() / GROUPS);

        for (int g = 0; g < GROUPS && g * groupSize < bins.size(); ++g) {
            size_t start = g * groupSize;
            size_t end = std::min(start + groupSize, bins.size());
            double mag = 0.0;
            for (size_t k = start; k < end; ++k) mag = std::max(mag, bins[k].second);

            double freq = bins[start].first;
            int bar = static_cast<int>(mag * barWidth);

            out << "  " << std::right << std::setw(6) << std::fixed << std::setprecision(0) << freq << " Hz |";
            out << std::string(bar, '#') << std::string(barWidth - bar, ' ') << "| " << mag << "\n";
        }
    }

private:
    std::unique_ptr<FFTStrategy> strategy_;
};