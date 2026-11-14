#include "CodecPanel.h"
#include "Theme.h"
#include "imgui.h"

namespace dawchecker::gui {

namespace {

const float* riskColor(codec::RiskLevel risk)
{
    const auto& p = palette();
    switch (risk)
    {
        case codec::RiskLevel::Alert:   return p.accentAlert;
        case codec::RiskLevel::Caution: return p.accentCaution;
        default:                        return p.accentSafe;
    }
}

const char* riskLabel(codec::RiskLevel risk)
{
    switch (risk)
    {
        case codec::RiskLevel::Alert:   return "ALERT";
        case codec::RiskLevel::Caution: return "Review";
        default:                        return "Recognized";
    }
}

} // namespace

void CodecPanel::render(const std::vector<codec::CodecFinding>& findings, bool scanInProgress)
{
    const auto& p = palette();

    ImGui::BeginChild("CodecPanel", ImVec2(0, 300), true);
    ImGui::TextUnformatted("Audio Codec / Driver Recognition");
    ImGui::Spacing();

    bool anyAlert = false;
    for (const auto& f : findings)
        if (f.risk == codec::RiskLevel::Alert)
            anyAlert = true;

    if (anyAlert)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(p.accentAlert[0], p.accentAlert[1], p.accentAlert[2], 0.14f));
        ImGui::BeginChild("CrystalAlertBanner", ImVec2(0, 64), true);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(p.accentAlert[0], p.accentAlert[1], p.accentAlert[2], 1.0f));
        ImGui::TextUnformatted("Crystal Codec / Crystal ASIO detected");
        ImGui::PopStyleColor();
        ImGui::TextWrapped(
            "One or more audio devices on this system match a driver pattern that is commonly bundled with "
            "unofficial installers rather than distributed by an accountable audio hardware vendor. "
            "See the entry below for details.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (scanInProgress)
    {
        ImGui::TextColored(ImVec4(p.textSecondary[0], p.textSecondary[1], p.textSecondary[2], p.textSecondary[3]),
                            "Scanning connected audio devices...");
    }
    else if (findings.empty())
    {
        ImGui::TextColored(ImVec4(p.textSecondary[0], p.textSecondary[1], p.textSecondary[2], p.textSecondary[3]),
                            "No devices enumerated yet. Run a scan to inspect connected audio drivers.");
    }

    ImGui::BeginChild("CodecList", ImVec2(0, 0), false);
    for (const auto& f : findings)
    {
        const float* c = riskColor(f.risk);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(c[0], c[1], c[2], c[3]));
        ImGui::TextUnformatted("\u25CF");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("%s", f.deviceName.c_str());
        ImGui::SameLine(320);
        ImGui::TextColored(ImVec4(c[0], c[1], c[2], c[3]), "%s", riskLabel(f.risk));

        ImGui::Indent();
        ImGui::TextColored(ImVec4(p.textSecondary[0], p.textSecondary[1], p.textSecondary[2], p.textSecondary[3]),
                            "%s | matched: %s", f.hostApi.c_str(), f.matchedCodec.c_str());
        if (!f.note.empty())
            ImGui::TextWrapped("%s", f.note.c_str());
        ImGui::Unindent();
        ImGui::Separator();
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

} // namespace dawchecker::gui
