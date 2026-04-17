cmake_minimum_required(VERSION 3.20)
project(vml VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(PipeWire REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SPA REQUIRED libspa-0.2)

# nlohmann/json — header-only, fetch if not found
include(FetchContent)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Sources
set(VML_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/audio/AudioEngine.cpp
    src/audio/PipeWireDevice.cpp
    src/audio/EffectPipeline.cpp
    src/effects/EffectRegistry.cpp
    src/effects/GainEffect.cpp
    src/effects/NoiseGateEffect.cpp
    src/effects/EchoEffect.cpp
    src/effects/PitchShiftEffect.cpp
    src/effects/ReverbEffect.cpp
    src/effects/BitCrushEffect.cpp
    src/effects/RingModulatorEffect.cpp
    src/effects/DistortionEffect.cpp
    src/effects/TelephoneFilterEffect.cpp
    src/effects/ChorusEffect.cpp
    src/effects/FlangerEffect.cpp
    src/effects/PhaserEffect.cpp
    src/effects/CompressorEffect.cpp
    src/effects/FormantShifterEffect.cpp
    src/modulation/Modulator.cpp
    src/modulation/ModulationManager.cpp
    src/gui/MainWindow.cpp
    src/gui/PipelineWidget.cpp
    src/gui/EffectBlockWidget.cpp
    src/gui/EffectSettingsPanel.cpp
    src/gui/SystemTray.cpp
    src/gui/VuMeter.cpp
    src/gui/WaveformWidget.cpp
    src/gui/SpectrumWidget.cpp
    src/gui/KeyframeTimelineWidget.cpp
    src/gui/ModulatorEditorDialog.cpp
    src/gui/FolderSidebar.cpp
    src/profile/ProfileManager.cpp
)

add_executable(vml ${VML_SOURCES})

target_include_directories(vml PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
    ${SPA_INCLUDE_DIRS}
)

target_link_libraries(vml PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${PIPEWIRE_LIBRARIES}
    nlohmann_json::nlohmann_json
)

target_compile_options(vml PRIVATE -Wall -Wextra -O2)

# Install
install(TARGETS vml DESTINATION bin)
install(FILES resources/vml.desktop DESTINATION share/applications)
install(FILES resources/icons/vml-icon.svg DESTINATION share/icons/hicolor/scalable/apps)

# Tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_ring_buffer tests/test_ring_buffer.cpp)
    target_include_directories(test_ring_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_ring_buffer PRIVATE Qt6::Test)
    add_test(NAME RingBuffer COMMAND test_ring_buffer)

    add_executable(test_effect_pipeline tests/test_effect_pipeline.cpp
        src/audio/EffectPipeline.cpp src/effects/EffectRegistry.cpp src/effects/GainEffect.cpp
        src/modulation/Modulator.cpp src/modulation/ModulationManager.cpp)
    target_include_directories(test_effect_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
    target_link_libraries(test_effect_pipeline PRIVATE Qt6::Test nlohmann_json::nlohmann_json)
    add_test(NAME EffectPipeline COMMAND test_effect_pipeline)

    add_executable(test_profile_manager tests/test_profile_manager.cpp src/profile/ProfileManager.cpp)
    target_include_directories(test_profile_manager PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_profile_manager PRIVATE Qt6::Test nlohmann_json::nlohmann_json)
    add_test(NAME ProfileManager COMMAND test_profile_manager)

    add_executable(test_folder_structure tests/test_folder_structure.cpp src/profile/ProfileManager.cpp)
    target_include_directories(test_folder_structure PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_folder_structure PRIVATE Qt6::Test nlohmann_json::nlohmann_json)
    add_test(NAME FolderStructure COMMAND test_folder_structure)
endif()
