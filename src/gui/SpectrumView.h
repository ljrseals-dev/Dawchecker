#pragma once
#include <array>
#include <atomic>
#include <vector>

namespace dawchecker::gui {

// Lock-light ring buffer fed by the audio thread; the GUI thread pulls a
// snapshot and runs a small FFT purely for display (never on the audio
// thread itself, to keep the render cost off the real-time path).
class SpectrumView
{
public:
    static constexpr std::size_t kFftSize = 1024;

    void pushSample(float liveSample) noexcept; // audio thread
    void render(double markerHzWatermark, double markerHzEnf, double markerHzUltrasonic, double sampleRate);

private:
    std::array<std::atomic<float>, kFftSize> ring_{};
    std::atomic<std::size_t> writeIndex_{0};
};

} // namespace dawchecker::gui
