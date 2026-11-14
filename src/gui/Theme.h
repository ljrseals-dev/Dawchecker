#pragma once

// Visual language notes:
// Inspired by modern consumer security-scanner dashboards (calm dark UI,
// blue/teal accent for "protected", amber for "attention", red reserved for
// genuine alerts, plain-language status text). Deliberately avoids
// Norton-era conventions: no yellow/black hazard-stripe chrome, no siren/
// countdown urgency language, no "your PC is at risk!!!" scare copy, no
// full-screen red takeovers for anything below a real Alert-level finding.

namespace dawchecker::gui {

struct Palette
{
    float bgDeep[4]      = {0.071f, 0.078f, 0.090f, 1.00f}; // app background
    float bgPanel[4]     = {0.106f, 0.114f, 0.129f, 1.00f}; // card/panel background
    float bgPanelAlt[4]  = {0.133f, 0.145f, 0.165f, 1.00f};
    float textPrimary[4]   = {0.925f, 0.933f, 0.945f, 1.00f};
    float textSecondary[4] = {0.596f, 0.624f, 0.667f, 1.00f};

    float accentSafe[4]    = {0.157f, 0.792f, 0.678f, 1.00f}; // teal — "clean"
    float accentInfo[4]    = {0.318f, 0.596f, 0.941f, 1.00f}; // blue — active scan
    float accentCaution[4] = {0.949f, 0.702f, 0.263f, 1.00f}; // amber — moderate
    float accentAlert[4]   = {0.906f, 0.318f, 0.318f, 1.00f}; // red — reserved for real alerts

    float gridLine[4]   = {1.0f, 1.0f, 1.0f, 0.06f};
    float spectrumFill[4] = {0.318f, 0.596f, 0.941f, 0.35f};
    float spectrumLine[4] = {0.318f, 0.596f, 0.941f, 0.90f};
};

// Applies the palette + rounded, low-chrome Malwarebytes-style widget
// styling to the current Dear ImGui context.
void applyTheme();

const Palette& palette();

} // namespace dawchecker::gui
