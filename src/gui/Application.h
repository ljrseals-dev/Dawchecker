#pragma once
#include <memory>
#include <vector>
#include "core/MSV_Engine.h"
#include "core/CodecScanner.h"
#include "core/SecurityHardening.h"
#include "DashboardPanel.h"
#include "CodecPanel.h"
#include "SpectrumView.h"
#include "ScanControls.h"

struct GLFWwindow;

namespace dawchecker::gui {

// Owns the GLFW window, ImGui context, audio engine, and all panels.
// Also owns the (optional) live PortAudio input stream used to drive the
// engine + spectrum view from real hardware input when available; falls
// back to a synthetic demo signal if no input device is present, so the
// dashboard is never blank.
class Application
{
public:
    Application();
    ~Application();

    // Runs the main loop until the window is closed. Returns a process exit
    // code.
    int run();

private:
    bool initWindow();
    void shutdownWindow();
    void renderFrame();
    void startAudio();
    void stopAudio();
    void rescanCodecs();
    void processAudioBlock(const float* input, unsigned long frameCount) noexcept;

    GLFWwindow* window_ = nullptr;

    dsp::MSV_Engine engine_;
    double sensitivity_ = 5.0;
    double sampleRate_  = 48000.0;

    codec::CodecScanner codecScanner_;
    std::vector<codec::CodecFinding> codecFindings_;
    bool codecScanInFlight_ = false;

    security::HardeningReport hardeningReport_;
    bool showHardeningReport_ = false;

    DashboardPanel dashboardPanel_;
    CodecPanel     codecPanel_;
    SpectrumView   spectrumView_;
    ScanControls   scanControls_;

    struct AudioStreamHandle; // PIMPL so PortAudio types don't leak into this header
    std::unique_ptr<AudioStreamHandle> audioStream_;
};

} // namespace dawchecker::gui
