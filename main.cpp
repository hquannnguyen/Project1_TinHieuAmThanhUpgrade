#include "AudioSignal.h"
#include "AudioIO.h"
#include "WaveformAnalyzer.h"
#include "FFTProcessor.h"
#include "SignalProcessor.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>

static void section(const std::string& title) {
   std::cout << "\n";
   std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
   std::cout << "  ║  " << std::left << std::setw(52) << title << "║\n";
   std::cout << "  ╚══════════════════════════════════════════════════════╝\n";
}

static void banner() {
   std::cout << "\n";
   std::cout << "  ╔══════════════════════════════════════════════════════╗\n";
   std::cout << "  ║          HỆ THỐNG XỬ LÝ ÂM THANH TÍN HIỆU SỐ         ║\n";
   std::cout << "  ║        Digital Audio Signal Processing System        ║\n";
   std::cout << "  ║   Strategy Pattern  |  Factory Pattern  |  C++14     ║\n";
   std::cout << "  ╚══════════════════════════════════════════════════════╝\n";
}

int main() {
   banner();

   // PHẦN 1: Factory Pattern
   section("1. FACTORY PATTERN — Tạo tín hiệu từ params");
   auto sine   = AudioFactory::fromSine(440.0,  0.05, 0.8);  
   auto square = AudioFactory::fromSquare(440.0, 0.05, 0.6);
   auto tri    = AudioFactory::fromTriangle(440.0, 0.05, 0.8);
   auto noise  = AudioFactory::fromWhiteNoise(0.05, 0.3);
   auto dtmf   = AudioFactory::fromDTMF(697.0, 1209.0, 0.05);

   std::cout << "\n  Đã tạo " << 5 << " tín hiệu qua AudioFactory:\n";
   for (auto* sig : {&sine, &square, &tri, &noise, &dtmf}) {
       std::cout << "  • " << std::left << std::setw(20) << sig->getName()
                 << "  " << sig->getNumSamples() << " samples"
                 << "  " << std::fixed << std::setprecision(3)
                 << sig->getDuration() << " s\n";
   }

   // PHẦN 2: WaveformAnalyzer
   section("2. WAVEFORM ANALYZER — Phân tích miền thời gian");
   {
       WaveformAnalyzer wa(sine);
       wa.printStats();
       wa.plotWaveform(std::cout, 56, 10);
   }

   std::cout << "\n  Zero-Crossing Rate so sánh:\n";
   for (auto* sig : {&sine, &square, &tri, &noise}) {
       WaveformAnalyzer wa(*sig);
       std::cout << "  " << std::left << std::setw(18) << sig->getName()
                 << "  ZCR = " << std::fixed << std::setprecision(1)
                 << wa.getZCR() << " /s"
                 << "   Crest = " << std::setprecision(3)
                 << wa.getCrestFactor() << "\n";
   }

   // PHẦN 3: Strategy Pattern — FFT vs DFT Thống nhất tần số
   section("3. STRATEGY PATTERN — FFTProcessor");
   FFTProcessor fftProc;
   std::cout << "\n  Backend: " << fftProc.getStrategyName() << "\n";
   auto result_fft = fftProc.computeSpectrum(sine);
   std::cout << "  Dominant freq: " << std::fixed << std::setprecision(1)
             << result_fft.dominantFreq << " Hz  (expect ~440 Hz)\n";

   fftProc.setStrategy(std::make_unique<DFTImpl>());
   std::cout << "\n  [Strategy swapped] Backend: " << fftProc.getStrategyName() << "\n";
   auto result_dft = fftProc.computeSpectrum(sine);
   std::cout << "  Dominant freq: " << std::fixed << std::setprecision(1)
             << result_dft.dominantFreq << " Hz  (Thống nhất hoàn hảo!)\n";

   fftProc.setStrategy(std::make_unique<CooleyTukeyFFT>());
   fftProc.plotSpectrum(sine, std::cout, 48, 3000.0);
   fftProc.plotSpectrum(dtmf, std::cout, 48, 3000.0);

   // PHẦN 4: Window Functions
   section("4. SIGNAL PROCESSOR — Window Functions");
   {
       for (auto wt : {WindowType::RECTANGULAR, WindowType::HAMMING, WindowType::HANNING, WindowType::BLACKMAN}) {
           auto sig_copy = sine;
           SignalProcessor::applyWindow(sig_copy, wt);
           auto res = fftProc.computeSpectrum(sig_copy);
           std::cout << "  " << std::left << std::setw(12) << SignalProcessor::windowName(wt)
                     << "  Peak=" << std::setprecision(4) << sig_copy.getPeakAmplitude()
                     << "  RMS=" << sig_copy.getRMS()
                     << "  DomFreq=" << std::setprecision(1) << res.dominantFreq << " Hz\n";
       }
   }

   // PHẦN 5: Normalize & Resample
   section("5. SIGNAL PROCESSOR — Normalize & Resample");
   {
       auto s = AudioFactory::fromSine(220.0, 0.05, 0.3);
       std::cout << "\n  Trước normalize: Peak=" << std::fixed << std::setprecision(4) << s.getPeakAmplitude() << "\n";
       SignalProcessor::normalize(s, 1.0);
       std::cout << "  Sau normalize:   Peak=" << s.getPeakAmplitude() << "\n";

       auto resampled = SignalProcessor::resample(s, 22050);
       std::cout << "  Original SampleRate:  " << s.getSampleRate() << " Hz\n";
       std::cout << "  Resampled SampleRate: " << resampled.getSampleRate() << " Hz (Anti-aliased)\n";
   }

   // PHẦN 6: Filters
   section("6. FILTERS — LPF / HPF / BPF");
   {
       auto mixed = AudioFactory::mix_freqs(200.0, 2000.0, 0.05);
       std::cout << "\n  Signal có 2 tần số trộn lẫn: 200 Hz + 2000 Hz\n";

       auto lpf = mixed; SignalProcessor::lowPassFilter(lpf, 500.0);
       auto res_lpf = fftProc.computeSpectrum(lpf);
       std::cout << "  LPF 500 Hz → DomFreq = " << std::setprecision(1) << res_lpf.dominantFreq << " Hz\n";

       auto hpf = mixed; SignalProcessor::highPassFilter(hpf, 500.0);
       auto res_hpf = fftProc.computeSpectrum(hpf);
       std::cout << "  HPF 500 Hz → DomFreq = " << res_hpf.dominantFreq << " Hz (Triệt tiêu hoàn toàn DC)\n";
   }

   // PHẦN 7: Effects
   section("7. EFFECTS — Echo & Reverb");
   {
       auto s = AudioFactory::fromSine(440.0, 0.1, 0.8);
       SignalProcessor::echo(s, 0.03, 0.45);
       std::cout << "\n  [Echo Effect applied] Peak amplitude ổn định tại: " << s.getPeakAmplitude() << "\n";
   }

   // PHẦN 8: Đọc/Ghi tệp WAV tin cậy
   section("8. AudioIO — Ghi & Đọc WAV file");
   {
       auto sig = AudioFactory::fromSine(440.0, 0.5, 0.8);
       const std::string fname = "test_sine_440.wav"; 
       if (AudioIO::writeWAV(fname, sig)) {
           std::cout << "\n  Đã ghi file thành công: " << fname << "\n";
           
           try {
               auto loaded = AudioIO::readWAV(fname);
               std::cout << "  Đọc lại tệp tin: " << loaded.getNumSamples() << " samples thành công.\n";
           } catch (const std::exception& e) {
               std::cerr << "  Lỗi đọc tệp: " << e.what() << "\n";
           }
       }
   }

   // PHẦN 9: Mix hợp âm
   section("9. MIX — Trộn nhiều tín hiệu (Hợp âm La Trưởng)");
   {
       auto a = AudioFactory::fromSine(440.0,  0.05, 0.6);
       auto b = AudioFactory::fromSine(550.0,  0.05, 0.4);   
       auto c = AudioFactory::fromSine(659.0,  0.05, 0.4);   

       auto chord_ab = SignalProcessor::mix(a, b, 0.5);
       auto chord    = SignalProcessor::mix(chord_ab, c, 0.67);
       chord.setName("chord_A_maj");

       fftProc.plotSpectrum(chord, std::cout, 48, 2000.0);
   }

   return 0;
}