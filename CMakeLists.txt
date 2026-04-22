cmake_minimum_required(VERSION 3.24)

# vcpkg toolchain integration — set before project() so vcpkg can bootstrap
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "vcpkg toolchain file")
endif()

project(OdysseyEngine
  VERSION 0.1.0
  LANGUAGES CXX
)

# ── C++20 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Find packages ───────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)

# ── Gather sources for the engine library ────────────────────────────────────
file(GLOB_RECURSE CORE_SOURCES      CONFIGURE_DEPENDS "src/core/*.cpp"      "src/core/*.h")
file(GLOB_RECURSE VULKAN_SOURCES    CONFIGURE_DEPENDS "src/vulkan/*.cpp"    "src/vulkan/*.h")
file(GLOB_RECURSE NADIR_SOURCES     CONFIGURE_DEPENDS "src/nadir/*.cpp"     "src/nadir/*.h")
file(GLOB_RECURSE SCENE_SOURCES     CONFIGURE_DEPENDS "src/scene/*.cpp"     "src/scene/*.h")
file(GLOB_RECURSE ASSETS_SOURCES    CONFIGURE_DEPENDS "src/assets/*.cpp"    "src/assets/*.h")
file(GLOB_RECURSE SCRIPTING_SOURCES CONFIGURE_DEPENDS "src/scripting/*.cpp" "src/scripting/*.h")
file(GLOB_RECURSE CLI_SOURCES       CONFIGURE_DEPENDS "src/cli/*.cpp"       "src/cli/*.h")
file(GLOB_RECURSE APP_SOURCES       CONFIGURE_DEPENDS "src/app/*.cpp"       "src/app/*.h")
list(FILTER APP_SOURCES EXCLUDE REGEX "odyssey_main\\.cpp$")
list(FILTER APP_SOURCES EXCLUDE REGEX "odyssey_editor_main\\.cpp$")
file(GLOB_RECURSE NET_SOURCES       CONFIGURE_DEPENDS "src/net/*.cpp"       "src/net/*.h")
# voice_frame.{h,cpp} lives under src/audio/voice/ but is wire-level netcode
# (the VOICE_FRAME deserialize path is on the server relay hot loop). Keep it
# inside odyssey_engine so the engine doesn't link-depend on odyssey_voice.
# The fuller voice stack (codec, jitter buffer, DSP, IO) ships in odyssey_voice.
file(GLOB VOICE_FRAME_SOURCES CONFIGURE_DEPENDS
    "src/audio/voice/voice_frame.cpp"
    "src/audio/voice/voice_frame.h")
file(GLOB_RECURSE MCP_SOURCES       CONFIGURE_DEPENDS "src/mcp/*.cpp"       "src/mcp/*.h")
file(GLOB_RECURSE DEBUG_SOURCES     CONFIGURE_DEPENDS "src/debug/*.cpp"     "src/debug/*.h")
file(GLOB_RECURSE ANIM_SOURCES      CONFIGURE_DEPENDS "src/animation/*.cpp" "src/animation/*.h")
file(GLOB_RECURSE PHYSICS_SOURCES   CONFIGURE_DEPENDS "src/physics/*.cpp"   "src/physics/*.h")
file(GLOB_RECURSE EDITOR_SOURCES    CONFIGURE_DEPENDS "src/editor/*.cpp"    "src/editor/*.h")

set(ENGINE_SOURCES
    ${CORE_SOURCES}
    ${VULKAN_SOURCES}
    ${NADIR_SOURCES}
    ${SCENE_SOURCES}
    ${ASSETS_SOURCES}
    ${SCRIPTING_SOURCES}
    ${CLI_SOURCES}
    ${APP_SOURCES}
    ${NET_SOURCES}
    ${VOICE_FRAME_SOURCES}
    ${MCP_SOURCES}
    ${DEBUG_SOURCES}
    ${ANIM_SOURCES}
    ${PHYSICS_SOURCES}
    ${EDITOR_SOURCES}
)

# ── ImGui Docking static library ────────────────────────────────────────────
add_library(imgui_docking STATIC
    third_party/imgui-docking/imgui.cpp
    third_party/imgui-docking/imgui_draw.cpp
    third_party/imgui-docking/imgui_tables.cpp
    third_party/imgui-docking/imgui_widgets.cpp
    third_party/imgui-docking/imgui_demo.cpp
    third_party/imgui-docking/backends/imgui_impl_glfw.cpp
    third_party/imgui-docking/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_docking
  PUBLIC
    third_party/imgui-docking
    third_party/imgui-docking/backends
)

target_link_libraries(imgui_docking
  PUBLIC
    glfw
    Vulkan::Vulkan
)

# ── ImGuizmo static library ─────────────────────────────────────────────────
add_library(imguizmo STATIC
    third_party/imguizmo/ImGuizmo.cpp
    third_party/imguizmo/ImGuizmo.h
)

target_include_directories(imguizmo
  PUBLIC
    third_party/imguizmo
)

target_link_libraries(imguizmo
  PUBLIC
    imgui_docking
)

# ── Engine static library ───────────────────────────────────────────────────
add_library(odyssey_engine STATIC ${ENGINE_SOURCES})

target_include_directories(odyssey_engine
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(odyssey_engine
  PUBLIC
    Vulkan::Vulkan
    glfw
    GPUOpen::VulkanMemoryAllocator
    glm::glm
    spdlog::spdlog
    unofficial::shaderc::shaderc
    pugixml::pugixml
    CLI11::CLI11
    imgui_docking
)

# Windows: link Winsock2 for networking and MCP
if(WIN32)
  target_link_libraries(odyssey_engine PUBLIC ws2_32)
endif()

# ── Voice static library (proximity chat) ───────────────────────────────────
# Owns the codec wrap, jitter buffer, DSP (marty), and WASAPI IO (marty).
# voice_frame.{h,cpp} is intentionally part of odyssey_engine instead (see
# above) because the server's relay loop needs to parse it — putting it in
# odyssey_voice would create a link cycle engine<->voice.
# Council condition: Opus is wrapped behind odyssey::audio::voice::Codec;
# <opus/opus.h> MUST NOT leak out of this library.
file(GLOB_RECURSE VOICE_SOURCES CONFIGURE_DEPENDS
    "src/audio/voice/*.cpp"
    "src/audio/voice/*.h"
    "src/audio/voice/dsp/*.cpp"
    "src/audio/voice/dsp/*.h"
    "src/audio/voice/io/*.cpp"
    "src/audio/voice/io/*.h"
)
# voice_frame.cpp is already inside odyssey_engine — strip it from the voice
# lib source list to avoid multiple-definition link errors.
list(FILTER VOICE_SOURCES EXCLUDE REGEX "voice_frame\\.(cpp|h)$")

if(VOICE_SOURCES)
  add_library(odyssey_voice STATIC ${VOICE_SOURCES})

  target_include_directories(odyssey_voice
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  )

  target_link_libraries(odyssey_voice
    PUBLIC
      odyssey_engine # for core/result.h, net/protocol.h, voice_frame.h
    PRIVATE
      Opus::opus
  )

  if(WIN32)
    # WASAPI + MMDevice + avrt (Multimedia Class Scheduler) for the IO layer
    # that marty owns under src/audio/voice/io/. ole32/uuid supply the COM
    # apartment + CLSIDs; mmdevapi supplies IMMDeviceEnumerator; avrt the
    # AvSetMmThreadCharacteristics (glitch-free realtime priority).
    target_link_libraries(odyssey_voice PUBLIC ole32 uuid mmdevapi avrt)
  endif()
endif()

# Engine-provided entry point — games just implement create_game().
set(ODYSSEY_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/app/odyssey_main.cpp")

# ── Shooter demo (game executable) ──────────────────────────────────────────
# Game sources live outside the engine library — this is the key separation.
# To create a new game, copy the template/ directory and replace these.
file(GLOB SHOOTER_SOURCES CONFIGURE_DEPENDS
  "demo/shooter_game.cpp" "demo/shooter_game.h"
  "demo/scripts/*.cpp" "demo/scripts/*.h"
)

add_executable(odyssey_shooter ${ODYSSEY_MAIN_SOURCE} ${SHOOTER_SOURCES})

target_include_directories(odyssey_shooter
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/demo
)

target_link_libraries(odyssey_shooter
  PRIVATE
    odyssey_engine
)

# ── FPS humanoid demo (game executable) ────────────────────────────────────
file(GLOB_RECURSE FPS_SOURCES CONFIGURE_DEPENDS
  "demo/fps_humanoid/*.cpp" "demo/fps_humanoid/*.h"
)

add_executable(odyssey_fps ${ODYSSEY_MAIN_SOURCE} ${FPS_SOURCES})

target_include_directories(odyssey_fps
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/demo
)

target_link_libraries(odyssey_fps
  PRIVATE
    odyssey_engine
)

# ── Editor executable ──────────────────────────────────────────────────────
# Phase 1: a thin front-end that hosts GLFW+Vulkan+ImGui and loads a scene
# for inspection. Shares the engine's scene/entity/result types but owns
# its own render loop (the engine's loop will be factored for hosted use
# in Phase 2; until then, keeping them separate avoids refactoring the
# renderer's frame graph).
set(ODYSSEY_EDITOR_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/app/odyssey_editor_main.cpp")

add_executable(odyssey_editor ${ODYSSEY_EDITOR_MAIN})

target_link_libraries(odyssey_editor
  PRIVATE
    odyssey_engine
    imgui_docking
    imguizmo
)

add_dependencies(odyssey_editor copy_engine_assets copy_demo_assets)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

file(GLOB_RECURSE UNIT_TEST_SOURCES     CONFIGURE_DEPENDS "tests/unit/*.cpp")
file(GLOB_RECURSE PIPELINE_TEST_SOURCES CONFIGURE_DEPENDS "tests/pipeline/*.cpp")

if(UNIT_TEST_SOURCES)
  add_executable(odyssey_tests_unit ${UNIT_TEST_SOURCES})
  target_link_libraries(odyssey_tests_unit
    PRIVATE
      odyssey_engine
      GTest::gtest_main
  )
  # Link odyssey_voice into tests if it was built — test_codec and
  # test_jitter_buffer exercise its surface directly.
  if(TARGET odyssey_voice)
    target_link_libraries(odyssey_tests_unit PRIVATE odyssey_voice)
  endif()
  add_test(NAME unit_tests COMMAND odyssey_tests_unit)
endif()

if(PIPELINE_TEST_SOURCES)
  add_executable(odyssey_tests_pipeline ${PIPELINE_TEST_SOURCES})
  target_link_libraries(odyssey_tests_pipeline
    PRIVATE
      odyssey_engine
      GTest::gtest_main
  )
  add_test(NAME pipeline_tests COMMAND odyssey_tests_pipeline)
endif()

# ── Copy engine assets to build output ────────────────────────────────────────
add_custom_target(copy_engine_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/behaviors/lib"
    "${CMAKE_CURRENT_BINARY_DIR}/behaviors/lib"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/schemas"
    "${CMAKE_CURRENT_BINARY_DIR}/schemas"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/engine.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/engine.xml"
  COMMENT "Copying engine assets to build directory"
)

# ── Copy demo assets to build output ──────────────────────────────────────────
add_custom_target(copy_demo_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/demo"
    "${CMAKE_CURRENT_BINARY_DIR}/demo"
  COMMENT "Copying demo assets to build directory"
)

add_dependencies(odyssey_shooter copy_engine_assets copy_demo_assets)
add_dependencies(odyssey_fps copy_engine_assets copy_demo_assets)
add_dependencies(odyssey_tests_unit copy_engine_assets)

# ── Windows: help find Vulkan SDK ────────────────────────────────────────────
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
  list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}")
endif()
