#include "DashboardPanel.h"
#include "Theme.h"
#include "imgui.h"
#include <cstdio>

namespace dawchecker::gui {

namespace {

const char* statusLabelFor(double score)
{
    if (score <= 20.0)  return "Clean";
    if (score <= 50.0)  return "Low-Level Artifacts";
    if (score <= 80.0)  return "Attention Recommended";
    return "Tampering Detected";
}

const float* colorFor(double score)
{
    const auto& p = palette();
    if (score <= 20.0)  return p.accentSafe;
    if (score <= 50.0)  return p.accentInfo;
    if (score <= 80.0)  return p.accentCaution;
    return p.accentAlert;
}

void drawDetectorRow(const char* name, const dsp::DetectorResult& result)
{
    const auto& p = palette();
    const float* dot =
        result.status == dsp::DetectorStatus::Alert  ? p.accentAlert :
        result.status == dsp::DetectorStatus::Active ? p.accentCaution :
                                                        p.accentSafe;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(dot[0], dot[1], dot[2], dot[3]));
    ImGui::TextUnformatted("\u25CF"); // filled circle status dot
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(name);
    ImGui::SameLine(220);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f%%", result.confidence);
    ImGui::TextUnformatted(buf);
    ImGui::SameLine(280);
    ImGui::TextColored(ImVec4(p.textSecondary[0], p.textSecondary[1], p.textSecondary[2], p.textSecondary[3]),
                        "%s", result.label.c_str());
}

} // namespace

void DashboardPanel::render(const dsp::EngineSnapshot& snapshot)
{
    ImGui::BeginChild("TamperScorePanel", ImVec2(0, 220), true);

    ImGui::TextUnformatted("Master Bus Tamper Score");
    ImGui::Spacing();

    const float* c = colorFor(snapshot.tamperScore);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(c[0], c[1], c[2], c[3]));
    ImGui::SetWindowFontScale(2.4f);
    ImGui::Text("%.0f", snapshot.tamperScore);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::TextUnformatted(statusLabelFor(snapshot.tamperScore));
    ImGui::EndGroup();

    ImGui::ProgressBar(static_cast<float>(snapshot.tamperScore / 100.0),
                        ImVec2(-1, 6), "");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    drawDetectorRow("Watermark Detection", snapshot.watermark);
    drawDetectorRow("ENF Analysis",        snapshot.enf);
    drawDetectorRow("Ultrasonic Scanner",  snapshot.ultrasonic);

    ImGui::EndChild();
}

} // namespace dawchecker::gui
