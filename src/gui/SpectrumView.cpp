#include "SpectrumView.h"
#include "Theme.h"
#include "imgui.h"
#include <cmath>
#include <complex>
#include <vector>

namespace dawchecker::gui {

namespace {

// Minimal, dependency-free radix-2 FFT — used only for the GUI's visual
// spectrum, never on the real-time audio thread.
void fft(std::vector<std::complex<float>>& a)
{
    const std::size_t n = a.size();
    if (n <= 1) return;

    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            for (std::size_t k = 0; k < len / 2; ++k)
            {
                auto u = a[i + k];
                auto v = a[i + k + len / 2] * w;
                a[i + k]           = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

} // namespace

void SpectrumView::pushSample(float liveSample) noexcept
{
    const std::size_t idx = writeIndex_.fetch_add(1, std::memory_order_relaxed) % kFftSize;
    ring_[idx] = liveSample;
}

void SpectrumView::render(double markerHzWatermark, double markerHzEnf, double markerHzUltrasonic, double sampleRate)
{
    const auto& p = palette();

    std::vector<std::complex<float>> buffer(kFftSize);
    {
        std::lock_guard<std::mutex> lock(copyMutex_);
        const std::size_t start = writeIndex_.load(std::memory_order_relaxed) % kFftSize;
        for (std::size_t i = 0; i < kFftSize; ++i)
        {
            // Hann window to reduce spectral leakage in the display.
            const float w = 0.5f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                                                     static_cast<float>(kFftSize - 1));
            buffer[i] = ring_[(start + i) % kFftSize] * w;
        }
    }
    fft(buffer);

    const std::size_t bins = kFftSize / 2;
    std::vector<float> magnitudesDb(bins);
    for (std::size_t i = 0; i < bins; ++i)
    {
        const float mag = std::abs(buffer[i]) / static_cast<float>(kFftSize);
        magnitudesDb[i] = 20.0f * std::log10(mag + 1e-9f);
    }

    ImGui::BeginChild("SpectrumPanel", ImVec2(0, 220), true);
    ImGui::TextUnformatted("Real-Time Spectrum");
    ImGui::TextColored(ImVec4(p.textSecondary[0], p.textSecondary[1], p.textSecondary[2], p.textSecondary[3]),
                        "Overlay markers show the dominant frequency of each active detector.");

    ImGui::PlotLines("##spectrum", magnitudesDb.data(), static_cast<int>(bins), 0,
                      nullptr, -100.0f, 0.0f, ImVec2(-1, 130));

    ImGui::Text("Watermark: %.0f Hz   ENF: %.0f Hz   Ultrasonic: %.0f Hz  (Nyquist %.0f Hz)",
                markerHzWatermark, markerHzEnf, markerHzUltrasonic, sampleRate * 0.5);

    ImGui::EndChild();
}

} // namespace dawchecker::gui
