#pragma once
#include "core/MSV_Engine.h"

namespace dawchecker::gui {

// Renders the top-level Tamper Score dashboard: a large readout plus
// plain-language status ("Clean", "Attention", "Tampering Detected") in the
// calm, low-alarm style described in Theme.h.
class DashboardPanel
{
public:
    void render(const dsp::EngineSnapshot& snapshot);
};

} // namespace dawchecker::gui
