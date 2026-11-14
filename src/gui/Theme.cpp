#include "Theme.h"
#include "imgui.h"

namespace dawchecker::gui {

const Palette& palette()
{
    static Palette p;
    return p;
}

void applyTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const Palette& p = palette();

    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(18, 18);
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 10);

    ImVec4* colors = style.Colors;
    auto set = [&](ImGuiCol_ idx, const float* c) { colors[idx] = ImVec4(c[0], c[1], c[2], c[3]); };

    set(ImGuiCol_WindowBg, p.bgDeep);
    set(ImGuiCol_ChildBg, p.bgPanel);
    set(ImGuiCol_PopupBg, p.bgPanel);
    set(ImGuiCol_Text, p.textPrimary);
    set(ImGuiCol_TextDisabled, p.textSecondary);
    set(ImGuiCol_FrameBg, p.bgPanelAlt);
    set(ImGuiCol_FrameBgHovered, p.bgPanelAlt);
    set(ImGuiCol_FrameBgActive, p.bgPanelAlt);
    set(ImGuiCol_TitleBg, p.bgDeep);
    set(ImGuiCol_TitleBgActive, p.bgDeep);
    set(ImGuiCol_Button, p.bgPanelAlt);
    set(ImGuiCol_ButtonHovered, p.accentInfo);
    set(ImGuiCol_ButtonActive, p.accentInfo);
    set(ImGuiCol_CheckMark, p.accentSafe);
    set(ImGuiCol_SliderGrab, p.accentInfo);
    set(ImGuiCol_SliderGrabActive, p.accentInfo);
    set(ImGuiCol_Header, p.bgPanelAlt);
    set(ImGuiCol_HeaderHovered, p.accentInfo);
    set(ImGuiCol_HeaderActive, p.accentInfo);
    set(ImGuiCol_Tab, p.bgPanel);
    set(ImGuiCol_TabHovered, p.accentInfo);
    set(ImGuiCol_TabActive, p.bgPanelAlt);
    set(ImGuiCol_Separator, p.gridLine);
    set(ImGuiCol_PlotLines, p.spectrumLine);
    set(ImGuiCol_PlotHistogram, p.accentInfo);
}

} // namespace dawchecker::gui
