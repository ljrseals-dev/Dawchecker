cmake_minimum_required(VERSION 3.16)
project(DawCheckerSecure VERSION 3.0.0 LANGUAGES CXX)

# ============================================================================
# C++ STANDARD & GLOBAL SETTINGS
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# PLATFORM DETECTION
# ============================================================================
if(WIN32)
    add_compile_definitions(DAWCHECKER_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(DAWCHECKER_PLATFORM_MACOS)
elseif(UNIX)
    add_compile_definitions(DAWCHECKER_PLATFORM_LINUX)
endif()

# ============================================================================
# HARDENED BUILD FLAGS
# ============================================================================
# Applied globally so both the core library and the GUI binary inherit them.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wconversion -Wshadow -Wsign-conversion
        -Werror=return-type -Werror=uninitialized
        -Wformat -Werror=format-security
        -fstack-protector-strong
    )

    if(UNIX AND NOT APPLE)
        add_compile_options(-fPIE)
    endif()

    # These require recent GCC/Clang; guard so older toolchains still build.
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-fstack-clash-protection" HAS_STACK_CLASH)
    if(HAS_STACK_CLASH)
        add_compile_options(-fstack-clash-protection)
    endif()
    check_cxx_compiler_flag("-fcf-protection=full" HAS_CF_PROTECTION)
    if(HAS_CF_PROTECTION)
        add_compile_options(-fcf-protection=full)
    endif()

    # FORTIFY_SOURCE needs optimization; only enable outside Debug.
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-D_FORTIFY_SOURCE=2)
    endif()

    # Release-specific hardening
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -DNDEBUG")
    if(UNIX AND NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE
            "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack")
    endif()

    # Debug: sanitizers (optional, useful during development)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3")
    # Uncomment the next two lines to enable ASan/UBSan in Debug builds:
    # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
    # set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")
elseif(MSVC)
    add_compile_options(/W4 /WX /GS /sdl /permissive- /Zc:__cplusplus)
    add_link_options(/DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /GUARD:CF)
endif()

# Optional development-only sanitizers. Keep disabled for release builds.
option(DAWCHECKER_ENABLE_SANITIZERS
       "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(DAWCHECKER_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "DAWCHECKER_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

# ============================================================================
# DEPENDENCY DETECTION
# ============================================================================
find_package(PkgConfig QUIET)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# GLFW (required only when building the GUI)
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(GLFW glfw3)
endif()
# Defer hard failure until we know ImGui is present and GUI will be built.

# PortAudio (optional — live input; falls back to synthetic demo signal)
option(DAWCHECKER_USE_PORTAUDIO "Enable PortAudio for live audio input" ON)
set(DAWCHECKER_HAVE_PORTAUDIO OFF)
if(DAWCHECKER_USE_PORTAUDIO)
    if(PkgConfig_FOUND)
        pkg_check_modules(PORTAUDIO portaudio-2.0)
    endif()
    if(NOT PORTAUDIO_FOUND)
        find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
        find_library(PORTAUDIO_LIBRARY NAMES portaudio)
        if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
            set(PORTAUDIO_FOUND TRUE)
            set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
            set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY})
        endif()
    endif()
    if(PORTAUDIO_FOUND)
        set(DAWCHECKER_HAVE_PORTAUDIO ON)
        message(STATUS "PortAudio found — live audio input enabled")
    else()
        message(STATUS "PortAudio not found — building with synthetic demo signal only")
    endif()
endif()

# ALSA fallback path for Linux device enumeration when PortAudio is absent
if(UNIX AND NOT APPLE AND NOT DAWCHECKER_HAVE_PORTAUDIO)
    find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h)
    find_library(ALSA_LIBRARY asound)
    if(ALSA_INCLUDE_DIR AND ALSA_LIBRARY)
        add_compile_definitions(DAWCHECKER_HAVE_ALSA)
        message(STATUS "ALSA found — used for device enumeration fallback")
    endif()
endif()

# ============================================================================
# IMGUI (vendored) — required only for the GUI binary
# ============================================================================
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui" CACHE PATH "ImGui source directory")
set(DAWCHECKER_BUILD_GUI OFF)
if(EXISTS "${IMGUI_DIR}/imgui.h")
    set(DAWCHECKER_BUILD_GUI ON)
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    message(STATUS "ImGui found — GUI target will be built")
else()
    message(WARNING
        "ImGui not found at ${IMGUI_DIR}. GUI binary will be skipped.\n"
        "  Run:  ./scripts/vendor_imgui.sh\n"
        "  or:   git clone --depth 1 --branch v1.90.9 https://github.com/ocornut/imgui.git third_party/imgui\n"
        "Core library and engine_tests will still build.")
endif()

# ============================================================================
# CORE ENGINE LIBRARY (no GUI / no windowing deps)
# ============================================================================
add_library(dawchecker_core STATIC
    src/core/AudioDeviceManager.cpp
    src/core/CodecScanner.cpp
    src/core/ENFAnalyzer.cpp
    src/core/EvidenceAccumulator.cpp
    src/core/MSV_Engine.cpp
    src/core/Neutralizers.cpp
    src/core/SecurityHardening.cpp
    src/core/SharedFeaturePipeline.cpp
    src/core/UltrasonicScanner.cpp
    src/core/WatermarkDetector.cpp
)

target_include_directories(dawchecker_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    PRIVATE
        ${PORTAUDIO_INCLUDE_DIRS}
)

target_link_libraries(dawchecker_core
    PUBLIC
        Threads::Threads
)

if(DAWCHECKER_HAVE_PORTAUDIO)
    target_compile_definitions(dawchecker_core PUBLIC DAWCHECKER_HAVE_PORTAUDIO)
    target_include_directories(dawchecker_core PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
    target_link_libraries(dawchecker_core PUBLIC ${PORTAUDIO_LIBRARIES})
    if(PORTAUDIO_LIBRARY_DIRS)
        target_link_directories(dawchecker_core PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
    endif()
endif()

if(ALSA_LIBRARY)
    target_include_directories(dawchecker_core PRIVATE ${ALSA_INCLUDE_DIR})
    target_link_libraries(dawchecker_core PUBLIC ${ALSA_LIBRARY})
endif()

# ============================================================================
# GUI LIBRARY + MAIN EXECUTABLE (only when ImGui is present)
# ============================================================================
if(DAWCHECKER_BUILD_GUI)
    if(NOT glfw3_FOUND AND NOT GLFW_FOUND)
        message(FATAL_ERROR
            "ImGui is present but GLFW was not found.\n"
            "Install libglfw3-dev (Debian/Ubuntu), glfw (Homebrew), or glfw via vcpkg.")
    endif()

    add_library(dawchecker_gui STATIC
        src/gui/Application.cpp
        src/gui/CodecPanel.cpp
        src/gui/DashboardPanel.cpp
        src/gui/ScanControls.cpp
        src/gui/SpectrumView.cpp
        src/gui/Theme.cpp
        ${IMGUI_SOURCES}
    )

    target_include_directories(dawchecker_gui
        PUBLIC
            ${CMAKE_SOURCE_DIR}/src
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        PRIVATE
            ${GLFW_INCLUDE_DIRS}
    )

    target_link_libraries(dawchecker_gui
        PUBLIC
            dawchecker_core
            OpenGL::GL
    )

    # Link GLFW (either modern CMake target or pkg-config variables)
    if(TARGET glfw)
        target_link_libraries(dawchecker_gui PUBLIC glfw)
    elseif(TARGET glfw3)
        target_link_libraries(dawchecker_gui PUBLIC glfw3)
    else()
        target_include_directories(dawchecker_gui PRIVATE ${GLFW_INCLUDE_DIRS})
        target_link_libraries(dawchecker_gui PUBLIC ${GLFW_LIBRARIES})
        if(GLFW_LIBRARY_DIRS)
            target_link_directories(dawchecker_gui PUBLIC ${GLFW_LIBRARY_DIRS})
        endif()
    endif()

    # Linux needs dl; X11/Wayland come in transitively via glfw on most distros.
    if(UNIX AND NOT APPLE)
        target_link_libraries(dawchecker_gui PUBLIC dl)
    endif()

    add_executable(dawchecker_secure
        src/main.cpp
    )

    target_link_libraries(dawchecker_secure PRIVATE
        dawchecker_gui
        dawchecker_core
    )

    install(TARGETS dawchecker_secure RUNTIME DESTINATION bin)

    # The hardened build script creates the SHA-256 sidecar after all tests pass.
    # Do not use shell redirection in add_custom_command: CMake does not run
    # COMMAND through a shell, so ">" would be passed as a literal argument.
endif()

# ============================================================================
# OPTIONAL VST3 PLUGIN COMPONENT SDK COMPILE CHECK
# ============================================================================
option(DAWCHECKER_BUILD_VST3_PLUGIN
       "Compile the VST3 plugin component against an external Steinberg SDK" OFF)
set(VST3_SDK_ROOT "" CACHE PATH "Root of a checked-out Steinberg VST3 SDK")

if(DAWCHECKER_BUILD_VST3_PLUGIN)
    if(NOT EXISTS "${VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.h" OR
       NOT EXISTS "${VST3_SDK_ROOT}/pluginterfaces/base/fplatform.h" OR
       NOT EXISTS "${VST3_SDK_ROOT}/base/source/fobject.h")
        message(FATAL_ERROR
            "DAWCHECKER_BUILD_VST3_PLUGIN=ON requires VST3_SDK_ROOT with "
            "the public.sdk, pluginterfaces, and base submodules initialized.")
    endif()

    # Compile the plugin translation unit against the real SDK. This is a
    # compile check, not a packaged .vst3 bundle or host-validation target.
    add_library(dawchecker_vst3_compilecheck OBJECT
        Source/dawchecker_vst3.cpp
    )
    target_include_directories(dawchecker_vst3_compilecheck
        SYSTEM PRIVATE "${VST3_SDK_ROOT}"
    )
    target_link_libraries(dawchecker_vst3_compilecheck PRIVATE dawchecker_core)
    target_compile_features(dawchecker_vst3_compilecheck PRIVATE cxx_std_20)
    set_target_properties(dawchecker_vst3_compilecheck PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# ============================================================================
# TESTS (dependency-free, links only core — always available)
# ============================================================================
enable_testing()

add_executable(engine_tests
    tests/EngineTests.cpp
)

target_include_directories(engine_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_tests PRIVATE dawchecker_core)

# The crypto acceptance gate is mandatory by default. Keep OpenSSL scoped to
# the forensic hash library and its direct consumers, not the general core/GUI.
option(DAWCHECKER_REQUIRE_CRYPTO
       "Fail the build if OpenSSL is unavailable (crypto acceptance gate)" ON)

if(DAWCHECKER_REQUIRE_CRYPTO)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    add_library(dawchecker_crypto STATIC
        src/core/ForensicHash.cpp
    )
    target_include_directories(dawchecker_crypto
        PUBLIC ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(dawchecker_crypto PUBLIC OpenSSL::Crypto)
    target_link_libraries(engine_tests PRIVATE dawchecker_crypto)
    target_compile_definitions(engine_tests PRIVATE DAWCHECKER_HAVE_CRYPTO=1)
    message(STATUS "Crypto gate: OpenSSL ${OPENSSL_VERSION} — SHA-256 tests enabled")
else()
    message(WARNING
        "Crypto acceptance gate DISABLED. Do not merge repair/p0-crypto "
        "with this build configuration.")
endif()

add_test(NAME engine_tests COMMAND engine_tests)

add_executable(audio_pipeline_tests
    tests/SharedFeaturePipelineTests.cpp
)
target_include_directories(audio_pipeline_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(audio_pipeline_tests PRIVATE dawchecker_core)
add_test(NAME audio_pipeline_tests COMMAND audio_pipeline_tests)

add_executable(evidence_accumulator_tests
    tests/EvidenceAccumulatorTests.cpp
)
target_include_directories(evidence_accumulator_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(evidence_accumulator_tests PRIVATE dawchecker_core)
add_test(NAME evidence_accumulator_tests COMMAND evidence_accumulator_tests)

# ============================================================================
# CONVENIENCE TARGETS
# ============================================================================
add_custom_target(vendor_imgui
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/vendor_imgui.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Vendoring Dear ImGui into third_party/imgui"
)

if(DAWCHECKER_BUILD_GUI)
    add_custom_target(hardened
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target dawchecker_secure --config Release
        COMMENT "Building hardened Release binary"
    )
endif()
