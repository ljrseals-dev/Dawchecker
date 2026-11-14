#include "ScanControls.h"
#include "Theme.h"
#include "imgui.h"

namespace dawchecker::gui {

ScanControlsAction ScanControls::render(dsp::MSV_Engine& engine, double& sensitivity)
{
    ScanControlsAction action;
    const auto& p = palette();

    ImGui::BeginChild("ScanControlsPanel", ImVec2(0, 220), true);
    ImGui::TextUnformatted("Controls");
    ImGui::Spacing();

    float sens = static_cast<float>(sensitivity);
    if (ImGui::SliderFloat("Detector Sensitivity", &sens, 1.0f, 100.0f, "%.0f"))
        sensitivity = sens;

    ImGui::Spacing();
    ImGui::TextUnformatted("Neutralization");

    bool wmScrub = engine.watermarkScrubEnabled.load();
    if (ImGui::Checkbox("Watermark Scrub", &wmScrub))
        engine.watermarkScrubEnabled.store(wmScrub);

    bool enfNotch = engine.enfNotchEnabled.load();
    if (ImGui::Checkbox("ENF Notch Filter", &enfNotch))
        engine.enfNotchEnabled.store(enfNotch);

    bool usRolloff = engine.ultrasonicRolloffEnabled.load();
    if (ImGui::Checkbox("Ultrasonic Rolloff", &usRolloff))
        engine.ultrasonicRolloffEnabled.store(usRolloff);

    ImGui::Spacing();
    bool bypass = engine.bypassAll.load();
    if (ImGui::Checkbox("Bypass All Processing", &bypass))
        engine.bypassAll.store(bypass);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(p.accentInfo[0], p.accentInfo[1], p.accentInfo[2], 1.0f));
    if (ImGui::Button("Scan Audio Devices", ImVec2(-1, 36)))
        action.rescanRequested = true;
    ImGui::PopStyleColor();

    ImGui::EndChild();
    return action;
}

} // namespace dawchecker::gui
