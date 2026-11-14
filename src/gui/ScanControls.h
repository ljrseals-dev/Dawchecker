#pragma once
#include "core/MSV_Engine.h"

namespace dawchecker::gui {

struct ScanControlsAction
{
    bool rescanRequested = false;
};

// Renders sensitivity slider, per-module neutralization toggles, bypass
// switch, and the "Scan Audio Devices" button.
class ScanControls
{
public:
    ScanControlsAction render(dsp::MSV_Engine& engine, double& sensitivity);
};

} // namespace dawchecker::gui
