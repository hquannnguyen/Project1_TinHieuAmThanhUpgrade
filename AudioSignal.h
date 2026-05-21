#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <numeric>

class AudioSignal {
public:
    AudioSignal() = default;

    AudioSignal(int sampleRate, int numChannels = 1, int bitDepth = 16, const std::string& name = "")
        : sampleRate_(sampleRate), numChannels_(numChannels), bitDepth_(bitDepth), name_(name) {}

    AudioSignal(std::vector<double> samples, int sampleRate, int numChannels = 1, int bitDepth = 16, const std::string& name = "")
        : samples_(std::move(samples)), sampleRate_(sampleRate), numChannels_(numChannels), bitDepth_(bitDepth), name_(name) {}

    const std::vector<double>& getSamples() const { return samples_; }
    std::vector<double>& getSamples() { return samples_; }
    int getSampleRate() const { return sampleRate_; }
    int getNumChannels() const { return numChannels_; }
    int getBitDepth() const { return bitDepth_; }
    const std::string& getName() const { return name_; }
    size_t getNumSamples() const { return samples_.size(); }

    double getDuration() const {
        return sampleRate_ > 0 ? static_cast<double>(samples_.size()) / sampleRate_ : 0.0;
    }

    void setSamples(std::vector<double> s) { samples_ = std::move(s); }
    void addSample(double s) { samples_.push_back(s); }
    void setSampleRate(int sr) { sampleRate_ = sr; }
    void setName(const std::string& n) { name_ = n; }
    void clear() { samples_.clear(); }

    double getPeakAmplitude() const {
        if (samples_.empty()) return 0.0;
        double pk = 0.0;
        for (double s : samples_) pk = std::max(pk, std::abs(s));
        return pk;
    }

    double getRMS() const {
        if (samples_.empty()) return 0.0;
        double sum = 0.0;
        for (double s : samples_) sum += s * s;
        return std::sqrt(sum / samples_.size());
    }

    double getdBFS() const {
        double rms = getRMS();
        return rms > 0.0 ? 20.0 * std::log10(rms) : -std::numeric_limits<double>::infinity();
    }

    double getMean() const {
        if (samples_.empty()) return 0.0;
        return std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size();
    }

    double getMin() const { return samples_.empty() ? 0.0 : *std::min_element(samples_.begin(), samples_.end()); }
    double getMax() const { return samples_.empty() ? 0.0 : *std::max_element(samples_.begin(), samples_.end()); }

    double operator[](size_t i) const { return samples_[i]; }
    double& operator[](size_t i) { return samples_[i]; }
    bool empty() const { return samples_.empty(); }

private:
    std::vector<double> samples_;
    int sampleRate_ = 44100;
    int numChannels_ = 1;
    int bitDepth_ = 16;
    std::string name_;
};