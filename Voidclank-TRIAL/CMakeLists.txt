cmake_minimum_required(VERSION 3.22)

project(Voidclank VERSION 1.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: link the C++ runtime statically so users do not need the Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(VC_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/VoidLookAndFeel.h
    Source/License.cpp
    Source/License.h
    Source/LicensePublicKey.h)

juce_add_plugin(Voidclank
    COMPANY_NAME "Fomardo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Fmrd
    PLUGIN_CODE Bsph
    FORMATS VST3 Standalone
    PRODUCT_NAME "Voidclank"
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Voidclank)
target_sources(Voidclank PRIVATE ${VC_SOURCES})

# Tester build: no trial, no licence needed, shows a PL/EN "personal use only" notice.
option(VC_FRIENDS_BUILD "Full tester build without trial (for friends)" OFF)
set(VC_FRIENDS_VALUE 0)
if(VC_FRIENDS_BUILD)
    set(VC_FRIENDS_VALUE 1)
endif()

target_compile_definitions(Voidclank PUBLIC
    VC_VERSION="${PROJECT_VERSION}"
    VC_FRIENDS_BUILD=${VC_FRIENDS_VALUE}
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_options(Voidclank PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(Voidclank
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- optional: Neural Amp Modeler (.nam, incl. A2) as an alternative amp stage ----------
option(VC_WITH_NAM "Add NeuralAmpModelerCore (.nam models) as an alternative GRIT amp stage" OFF)
if(VC_WITH_NAM)
    FetchContent_Declare(NamCore
        GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
        GIT_TAG 0b3d3c97b0859a3a8c92a8628c4dd89a25eb5842           # pinned
        GIT_SUBMODULES ""                                           # Eigen is handled below, nlohmann is vendored
        SOURCE_SUBDIR does_not_exist)                                # download only, we compile the sources ourselves
    FetchContent_MakeAvailable(NamCore)                              # the core has no top-level library target we want

    find_package(Eigen3 3.4 QUIET NO_MODULE)
    if(Eigen3_FOUND)
        set(VC_EIGEN_INCLUDE "${EIGEN3_INCLUDE_DIR}")
    else()
        FetchContent_Declare(EigenSrc
            URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
            SOURCE_SUBDIR does_not_exist)                            # header-only: just download
        FetchContent_MakeAvailable(EigenSrc)
        set(VC_EIGEN_INCLUDE "${eigensrc_SOURCE_DIR}")
    endif()

    file(GLOB VC_NAM_CORE_SRC "${namcore_SOURCE_DIR}/NAM/*.cpp" "${namcore_SOURCE_DIR}/NAM/*/*.cpp")
    add_library(voidclank_nam OBJECT Source/nam/NamEngine.cpp ${VC_NAM_CORE_SRC})
    target_compile_features(voidclank_nam PRIVATE cxx_std_20)
    target_include_directories(voidclank_nam PRIVATE "${namcore_SOURCE_DIR}" "${namcore_SOURCE_DIR}/Dependencies/nlohmann")
    target_include_directories(voidclank_nam SYSTEM PRIVATE "${VC_EIGEN_INCLUDE}")
    target_include_directories(voidclank_nam PUBLIC Source/nam)
    target_compile_definitions(voidclank_nam PRIVATE NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST
        $<$<PLATFORM_ID:Windows>:NOMINMAX> $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>)
    set_target_properties(voidclank_nam PROPERTIES POSITION_INDEPENDENT_CODE ON)   # OBJECT lib: keeps the static self-registration of model types
    target_compile_options(voidclank_nam PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3>)

    target_link_libraries(Voidclank PRIVATE voidclank_nam)
    target_compile_definitions(Voidclank PUBLIC VC_WITH_NAM=1)
endif()

# ---- seller tools (never distribute these) -------------------------------
option(VC_BUILD_TOOLS "Build licence key generator and DSP/licence smoke test" OFF)
if(VC_BUILD_TOOLS)
    foreach(tool IN ITEMS VoidclankKeygen VoidclankTest)
        juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
        target_compile_definitions(${tool} PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Voidclank"
            VC_FRIENDS_BUILD=${VC_FRIENDS_VALUE} VC_VERSION="${PROJECT_VERSION}")
        target_compile_options(${tool} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
        target_link_libraries(${tool} PRIVATE
            juce::juce_audio_utils juce::juce_cryptography juce::juce_dsp
            juce::juce_recommended_config_flags)
        juce_generate_juce_header(${tool})
    endforeach()
    target_sources(VoidclankKeygen PRIVATE Tools/Keygen.cpp Source/License.cpp)
    target_sources(VoidclankTest PRIVATE Tests/DspTest.cpp ${VC_SOURCES})
    if(VC_WITH_NAM)
        target_compile_definitions(VoidclankTest PRIVATE VC_WITH_NAM=1)
        target_link_libraries(VoidclankTest PRIVATE voidclank_nam)
    endif()
endif()
