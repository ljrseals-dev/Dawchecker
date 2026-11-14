// DawChecker Secure — entry point.
//
// Standalone, OpenGL-GUI forensic audio analysis tool. This is a from-scratch
// rewrite of the original JUCE VST3 plugin as a self-contained desktop app,
// with compiler/linker hardening (see CMakeLists.txt) and a new codec/driver
// recognition module (see src/core/CodecScanner.*).
#include "gui/Application.h"
#include <cstdio>
#include <exception>

int main()
{
    try
    {
        dawchecker::gui::Application app;
        return app.run();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "DawChecker Secure: fatal error: %s\n", e.what());
        return 1;
    }
    catch (...)
    {
        std::fprintf(stderr, "DawChecker Secure: fatal error: unknown exception\n");
        return 1;
    }
}
