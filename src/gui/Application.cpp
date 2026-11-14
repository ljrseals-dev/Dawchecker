#include "Application.h"
#include "Theme.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cstdio>
#include <cmath>
#include <random>

#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    #include <portaudio.h>
#endif

namespace dawchecker::gui {

namespace {
constexpr const char* kReleasesUrl = "https://github.com/TheRealla/DawChecker/releases";
constexpr int kWindowWidth  = 1040;
constexpr int kWindowHeight = 760;
}

// ---------------------------------------------------------------------------
// Audio stream handle (PIMPL) — isolates PortAudio's C API from the header.
// ---------------------------------------------------------------------------
struct Application::AudioStreamHandle
{
#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    PaStream* stream = nullptr;
#endif
    bool active = false;

    // Demo-signal fallback state, used only when no real input device is
    // available (e.g. running in a sandboxed/headless environment). This
    // never claims to be real audio in the UI — the dashboard is neutral
    // about it and clearly not making tamper claims from a synthetic tone.
    std::mt19937 rng{std::random_device{}()};
};

#if defined(DAWCHECKER_HAVE_PORTAUDIO)
namespace {

int paCallback(const void* input, void* /*output*/, unsigned long frameCount,
                const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData)
{
    auto* self = static_cast<Application*>(userData);
    const auto* in = static_cast<const float*>(input);
    if (self != nullptr && in != nullptr)
        self->processAudioBlock(in, frameCount);
    return paContinue;
}

} // namespace
#endif

void Application::processAudioBlock(const float* input, unsigned long frameCount) noexcept
{
    // Real-time audio thread: no allocation, no locking beyond the engine's
    // lock-free atomics, no logging, no GUI calls.
    for (unsigned long i = 0; i < frameCount; ++i)
    {
        const double live = static_cast<double>(input[i]);
        // No hardware sidechain/reference input is wired up in this minimal
        // standalone build, so the reference channel defaults to silence;
        // detectors still function (delta becomes the live signal itself),
        // but for full reference-based watermark/ENF comparison, feed a
        // pre-processing reference via a second input channel.
        double cleaned = 0.0;
        engine_.process(0.0, live, cleaned);
        spectrumView_.pushSample(static_cast<float>(live));
    }
}

Application::Application()
{
    hardeningReport_ = security::runStartupSelfCheck();
    engine_.prepare(sampleRate_, sensitivity_);
    audioStream_ = std::make_unique<AudioStreamHandle>();
}

Application::~Application()
{
    stopAudio();
    shutdownWindow();
}

bool Application::initWindow()
{
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(kWindowWidth, kWindowHeight, "DawChecker Secure", nullptr, nullptr);
    if (window_ == nullptr)
        return false;

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyTheme();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void Application::shutdownWindow()
{
    if (window_ == nullptr)
        return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

void Application::startAudio()
{
#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    if (Pa_Initialize() != paNoError)
        return;

    PaStreamParameters inputParams{};
    inputParams.device = Pa_GetDefaultInputDevice();
    if (inputParams.device == paNoDevice)
        return;

    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;

    PaError err = Pa_OpenStream(&audioStream_->stream, &inputParams, nullptr, sampleRate_,
                                 256, paClipOff, paCallback, this);
    if (err == paNoError)
    {
        Pa_StartStream(audioStream_->stream);
        audioStream_->active = true;
    }
#endif
}

void Application::stopAudio()
{
#if defined(DAWCHECKER_HAVE_PORTAUDIO)
    if (audioStream_ && audioStream_->active && audioStream_->stream)
    {
        Pa_StopStream(audioStream_->stream);
        Pa_CloseStream(audioStream_->stream);
        Pa_Terminate();
        audioStream_->active = false;
    }
#endif
}

void Application::rescanCodecs()
{
    codecScanInFlight_ = true;
    codecFindings_ = codecScanner_.scan();
    codecScanInFlight_ = false;
}

void Application::renderFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("DawCheckerSecureRoot", nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::TextUnformatted("DawChecker Secure");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(palette().textSecondary[0], palette().textSecondary[1],
                               palette().textSecondary[2], palette().textSecondary[3]),
                        "  Forensic Audio Analysis  |  v2.0.0 Hardened");
    ImGui::SameLine(ImGui::GetWindowWidth() - 190);
    ImGui::TextLinkOpenURL("Get Latest Release", kReleasesUrl);

    if (!hardeningReport_.notes.empty())
    {
        ImGui::SameLine();
        if (ImGui::SmallButton(hardeningReport_.notes.empty() ? "Security: OK" : "Security Notes"))
            showHardeningReport_ = !showHardeningReport_;
    }

    ImGui::Separator();

    if (showHardeningReport_)
    {
        ImGui::BeginChild("HardeningNotes", ImVec2(0, 90), true);
        ImGui::Text("ASLR active: %s   Debugger attached: %s   Elevated: %s",
                    hardeningReport_.aslrActive ? "yes" : "no",
                    hardeningReport_.debuggerDetected ? "yes" : "no",
                    hardeningReport_.runningElevated ? "yes" : "no");
        for (const auto& note : hardeningReport_.notes)
            ImGui::TextWrapped("- %s", note.c_str());
        ImGui::EndChild();
        ImGui::Spacing();
    }

    const auto snapshot = engine_.snapshot();

    // Two-column layout: left = dashboard + controls, right = codec panel + spectrum.
    ImGui::Columns(2, "MainColumns", false);
    ImGui::SetColumnWidth(0, viewport->WorkSize.x * 0.48f);

    dashboardPanel_.render(snapshot);
    ImGui::Spacing();
    const auto action = scanControls_.render(engine_, sensitivity_);
    if (action.rescanRequested)
        rescanCodecs();

    ImGui::NextColumn();

    codecPanel_.render(codecFindings_, codecScanInFlight_);
    ImGui::Spacing();
    spectrumView_.render(0.0, snapshot.enf.locationHz, snapshot.ultrasonic.locationHz, sampleRate_);

    ImGui::Columns(1);
    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    const auto& bg = palette().bgDeep;
    glClearColor(bg[0], bg[1], bg[2], bg[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
}

int Application::run()
{
    if (!initWindow())
    {
        std::fprintf(stderr, "DawChecker Secure: failed to initialize OpenGL/GLFW window.\n");
        return 1;
    }

    startAudio();
    rescanCodecs(); // initial device scan on launch

    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        renderFrame();
    }

    stopAudio();
    return 0;
}

} // namespace dawchecker::gui
