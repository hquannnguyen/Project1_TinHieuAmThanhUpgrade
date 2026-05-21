# Project1 - Hệ Thống Xử Lý Tín Hiệu Âm Thanh Số Nâng Cấp (Upgrade)
## Advanced Digital Audio Signal Processing System (C++14 OOP & Design Patterns)

---

## 📚 1. Cơ Sở Lý Thuyết & Kiến Thức Nền Tảng (DSP Fundamentals)

Để hiểu và vận hành hệ thống, các thành viên cần nắm vững các khái niệm xử lý tín hiệu số (DSP) cốt lõi sau:

### 1.1. Tín Hiệu Âm Thanh Số Tuyến Tính (PCM Audio)
Âm thanh tự nhiên là tín hiệu tương tự (Analog) liên tục theo thời gian. Để đưa vào máy tính, nó phải trải qua quá trình **Số hóa** gồm 2 bước:
1. **Lấy mẫu (Sampling)**: Trích xuất biên độ của sóng âm tại các khoảng thời gian cách đều nhau. Số lần lấy mẫu trong 1 giây gọi là **Sample Rate** ($f_s$). Theo định lý Nyquist-Shannon, để tái tạo hoàn hảo một tín hiệu có tần số tối đa là $f_{max}$, ta phải lấy mẫu với tần số $f_s \ge 2 \cdot f_{max}$. Định dạng CD tiêu chuẩn sử dụng $44100\text{ Hz}$ vì tai người nghe được dải tần tối đa $20000\text{ Hz}$.
2. **Lượng tử hóa (Quantization)**: Làm tròn giá trị biên độ thô vào các mức số nguyên cố định. Số lượng bit dùng để biểu diễn một mẫu gọi là **Bit Depth**. Hệ thống 16-bit PCM cung cấp $2^{16} = 65536$ mức biên độ khác nhau (tương đương dải động $96\text{ dB}$). Trong code, toàn bộ dữ liệu này được chuẩn hóa về kiểu số thực `double` nằm trong dải tuyến tính từ `[-1.0, 1.0]`.

### 1.2. Phân Tích Năng Lượng Miền Thời Gian (Time-Domain Analysis)
- **RMS (Root Mean Square - Giá trị hiệu dụng)**: Đo lường tổng năng lượng thực tế của dòng tín hiệu, đại diện trực tiếp cho cảm nhận của tai người về độ to của âm thanh (*Loudness*). Công thức tính:
  $$RMS = \sqrt{\frac{1}{N} \sum_{n=0}^{N-1} x^2[n]}$$
- **dBFS (Decibels relative to Full Scale)**: Đơn vị đo cường độ âm thanh trong miền số kỹ thuật số theo thang Logarit. Mức đỉnh biên độ tối đa mà hệ thống không bị méo tiếng (Clipping) được quy định cố định là $0\text{ dBFS}$. Mọi giá trị RMS thực tế sẽ âm dải và nhỏ hơn $0\text{ dBFS}$:
  $$dBFS = 20 \cdot \log_{10}(RMS)$$
- **ZCR (Zero-Crossing Rate)**: Tần suất biên độ tín hiệu đổi dấu từ âm sang dương hoặc ngược lại trong một giây. Tín hiệu có tính chu kỳ tuần hoàn (như giọng nói, nhạc cụ) có ZCR thấp, trong khi các tín hiệu nhiễu vô định hình (Noise/Hiss) có ZCR cực kỳ cao.

### 1.3. Biến Đổi Fourier Rời Rạc (DFT) và Biến Đổi Fourier Nhanh (FFT)
Chuyển đổi tín hiệu từ miền thời gian sang miền tần số để phân tích xem âm thanh được cấu thành từ những tần số nào (như bóc tách các nốt nhạc trong một hợp âm).
- **DFT (Discrete Fourier Transform)**: Phép toán quét thô với độ phức tạp thuật toán là $O(N^2)$:
  $$X[k] = \sum_{n=0}^{N-1} x[n] \cdot e^{-j\frac{2\pi}{N}kn}$$
- **FFT (Fast Fourier Transform)**: Giải thuật tối ưu Cooley-Tukey Radix-2 theo nguyên lý chia để trị (DIT), giảm độ phức tạp xuống còn $O(N \log N)$. Thuật toán bắt buộc độ dài mảng đầu vào phải là lũy thừa của 2 ($N = 2^m$). Do đó, nếu mảng gốc có kích thước lẻ, hệ thống phải thực hiện cơ chế **Zero-padding** (chèn thêm các mẫu $0$ vào cuối mảng).

---

## 🏗️ 2. Kiến Trúc Hệ Thống & Sơ Đồ Khối (Class Architecture)

Hệ thống được thiết kế theo nguyên lý đơn nhiệm (Single Responsibility Principle) và áp dụng hai mẫu thiết kế kinh điển: **Factory Pattern** cho khâu phát sinh tín hiệu và **Strategy Pattern** cho khâu hoán đổi thuật toán phân tích phổ tần số lúc runtime.

### 2.1. Mã Sơ Đồ Lớp (Class Diagram Code)
Sao chép đoạn mã PlantUML dưới đây dán vào trang [PlantText](https://www.planttext.com/) để xuất ảnh cấu trúc lớp:

```text
@startuml
skinparam classAttributeIconSize 0
skinparam monochrome false

class AudioSignal {
    - samples_ : std::vector<double>
    - sampleRate_ : int
    - numChannels_ : int
    - bitDepth_ : int
    - name_ : std::string
    + AudioSignal()
    + getSamples() : std::vector<double>&
    + getSampleRate() : int
    + getDuration() : double
    + getPeakAmplitude() : double
    + getRMS() : double
    + getdBFS() : double
}

interface FFTStrategy {
    + {abstract} compute(samples : std::vector<double>) : std::vector<std::complex<double>>
    + {abstract} name() : std::string
}

class DFTImpl {
    + compute(samples : std::vector<double>) : std::vector<std::complex<double>>
    + name() : std::string
}

class CooleyTukeyFFT {
    + compute(samples : std::vector<double>) : std::vector<std::complex<double>>
    - fft(a : std::vector<std::complex<double>>&) : void
}

class FFTProcessor {
    - strategy_ : std::unique_ptr<FFTStrategy>
    + setStrategy(s : std::unique_ptr<FFTStrategy>) : void
    + computeSpectrum(sig : AudioSignal) : SpectralResult
    + plotSpectrum(sig : AudioSignal) : void
}

class WaveformAnalyzer {
    - sig_ : const AudioSignal&
    + getZCR() : double
    + printStats() : void
    + plotWaveform() : void
}

class SignalProcessor {
    {static} + normalize(sig : AudioSignal&, target : double) : void
    {static} + resample(sig : AudioSignal&, newSR : int) : AudioSignal
    {static} + lowPassFilter(sig : AudioSignal&, cutoff : double) : void
    {static} + highPassFilter(sig : AudioSignal&, cutoff : double) : void
}

FFTStrategy <|.. DFTImpl
FFTStrategy <|.. CooleyTukeyFFT
FFTProcessor *-- FFTStrategy
WaveformAnalyzer o-- AudioSignal
@endum
