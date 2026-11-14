#pragma once
#include "core/CodecScanner.h"

namespace dawchecker::gui {

// Renders the recognized audio codec/driver list, with a dedicated banner
// treatment when a critical match (Crystal Codec / Crystal ASIO) is found.
class CodecPanel
{
public:
    void render(const std::vector<codec::CodecFinding>& findings, bool scanInProgress);
};

} // namespace dawchecker::gui
