#pragma once
#include "AudioSignal.h"
#include "Constants.h"
#include <cmath>
#include <string>
#include <algorithm>

enum class WindowType { RECTANGULAR, HAMMING, HANNING, BLACKMAN };

class SignalProcessor {
public:
    static void normalize(AudioSignal& sig, double target = 1.0) {
        double pk = sig.getPeakAmplitude();
        if (pk < 1e-10) return;
        double scale = target / pk;
        for (double& s : sig.getSamples()) s *= scale;
    }

    static void fadeIn(AudioSignal& sig, int fadeSamples) {
        auto& s = sig.getSamples();
        int n = std::min(fadeSamples, (int)s.size());
        for (int i = 0; i < n; ++i) s[i] *= static_cast<double>(i) / n;
    }

    static void fadeOut(AudioSignal& sig, int fadeSamples) {
        auto& s = sig.getSamples();
        int n = std::min(fadeSamples, (int)s.size());
        int start = static_cast<int>(s.size()) - n;
        for (int i = 0; i < n; ++i) s[start+i] *= static_cast<double>(n-i) / n;
    }

    static AudioSignal mix(const AudioSignal& a, const AudioSignal& b, double ratioA = 0.5) {
        size_t N = std::max(a.getNumSamples(), b.getNumSamples());
        std::vector<double> buf(N, 0.0);
        double rB = 1.0 - ratioA;
        for (size_t i = 0; i < N; ++i) {
            double sa = i < a.getNumSamples() ? a[i] : 0.0;
            double sb = i < b.getNumSamples() ? b[i] : 0.0;
            buf[i] = ratioA * sa + rB * sb;
        }
        return AudioSignal(std::move(buf), a.getSampleRate(), a.getNumChannels(), a.getBitDepth(), "mix");
    }

    // FIX: Tích hợp LPF chống răng cưa (Anti-aliasing filter) khi Downsampling
    static AudioSignal resample(const AudioSignal& sig, int newSR) {
        int oldSR = sig.getSampleRate();
        if (oldSR == newSR) return sig;

        AudioSignal filteredSig = sig;
        if (newSR < oldSR) {
            // Tần số cắt Nyquist an toàn = 1/2 Tần số lấy mẫu mới
            double nyquistCutoff = newSR / 2.0; 
            lowPassFilter(filteredSig, nyquistCutoff);
        }

        const auto& src = filteredSig.getSamples();
        double ratio = static_cast<double>(oldSR) / newSR;
        size_t newN = static_cast<size_t>(src.size() / ratio);
        std::vector<double> buf(newN);

        for (size_t i = 0; i < newN; ++i) {
            double pos = i * ratio;
            size_t idx = static_cast<size_t>(pos);
            double frac = pos - idx;
            double s0 = idx < src.size() ? src[idx] : 0.0;
            double s1 = (idx+1) < src.size() ? src[idx+1] : 0.0;
            buf[i] = s0 + frac * (s1 - s0);
        }
        return AudioSignal(std::move(buf), newSR, sig.getNumChannels(), sig.getBitDepth(), sig.getName() + "_resampled");
    }

    static void applyWindow(AudioSignal& sig, WindowType type) {
        auto& s = sig.getSamples();
        const size_t N = s.size();
        for (size_t i = 0; i < N; ++i) s[i] *= windowCoeff(type, i, N);
    }

    static double windowCoeff(WindowType type, size_t i, size_t N) {
        switch (type) {
            case WindowType::HAMMING:  return 0.54 - 0.46 * std::cos(2.0 * AudioConstants::PI * i / (N - 1));
            case WindowType::HANNING:  return 0.5 * (1.0 - std::cos(2.0 * AudioConstants::PI * i / (N - 1)));
            case WindowType::BLACKMAN: return 0.42 - 0.5 * std::cos(2.0 * AudioConstants::PI * i / (N - 1)) + 0.08 * std::cos(4.0 * AudioConstants::PI * i / (N - 1));
            default:                   return 1.0;
        }
    }

    static std::string windowName(WindowType t) {
        if (t == WindowType::HAMMING) return "Hamming";
        if (t == WindowType::HANNING) return "Hanning";
        if (t == WindowType::BLACKMAN) return "Blackman";
        return "Rectangular";
    }

    static void lowPassFilter(AudioSignal& sig, double cutoffHz) {
        auto& s = sig.getSamples(); if (s.size() < 2) return;
        double alpha = computeAlpha(cutoffHz, sig.getSampleRate());
        double prev = s[0];
        for (size_t i = 1; i < s.size(); ++i) {
            s[i] = prev + alpha * (s[i] - prev);
            prev = s[i];
        }
    }

    static void highPassFilter(AudioSignal& sig, double cutoffHz) {
        auto& s = sig.getSamples();
        if (s.size() < 2) return;
        
        AudioSignal lpSig = sig; 
        lowPassFilter(lpSig, cutoffHz); 
        
        const auto& lps = lpSig.getSamples();
        for (size_t i = 0; i < s.size(); ++i) {
            s[i] = s[i] - lps[i];
        }
    }

    static void bandPassFilter(AudioSignal& sig, double lowHz, double highHz) {
        highPassFilter(sig, lowHz);
        lowPassFilter(sig, highHz);
    }

    static void echo(AudioSignal& sig, double delaySeconds, double feedback = 0.4) {
        auto& s = sig.getSamples();
        int delaySamples = static_cast<int>(delaySeconds * sig.getSampleRate());
        if (delaySamples <= 0 || delaySamples >= (int)s.size()) return;
        for (int i = delaySamples; i < (int)s.size(); ++i) s[i] += feedback * s[i - delaySamples];
        normalize(sig, 1.0);
    }

    static void reverb(AudioSignal& sig, double roomSize = 0.5) {
        auto& s = sig.getSamples(); int SR = sig.getSampleRate();
        static const double delays_ms[] = {29.7, 37.1, 41.1, 43.7};
        std::vector<double> out(s.size(), 0.0);

        for (int c = 0; c < 4; ++c) {
            int D = static_cast<int>(delays_ms[c] / 1000.0 * SR * (0.5 + roomSize));
            if (D <= 0) continue;
            double g = 0.84 * roomSize;
            std::vector<double> buf(D, 0.0); int ptr = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                double comb = buf[ptr]; buf[ptr] = s[i] + g * comb;
                ptr = (ptr + 1) % D; out[i] += comb;
            }
        }
        double wet = 0.3 * roomSize;
        for (size_t i = 0; i < s.size(); ++i) s[i] = (1.0 - wet) * s[i] + wet * out[i];
        normalize(sig, 1.0);
    }

private:
    static double computeAlpha(double cutoffHz, int SR) {
        double dt = 1.0 / SR;
        double RC = 1.0 / (2.0 * AudioConstants::PI * cutoffHz);
        return dt / (RC + dt);
    }
};