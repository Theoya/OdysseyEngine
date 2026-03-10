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

# ── Gather sources for the engine library ────────────────────────────────────
file(GLOB_RECURSE CORE_SOURCES      CONFIGURE_DEPENDS "src/core/*.cpp"      "src/core/*.h")
file(GLOB_RECURSE VULKAN_SOURCES    CONFIGURE_DEPENDS "src/vulkan/*.cpp"    "src/vulkan/*.h")
file(GLOB_RECURSE NADIR_SOURCES     CONFIGURE_DEPENDS "src/nadir/*.cpp"     "src/nadir/*.h")
file(GLOB_RECURSE SCENE_SOURCES     CONFIGURE_DEPENDS "src/scene/*.cpp"     "src/scene/*.h")
file(GLOB_RECURSE ASSETS_SOURCES    CONFIGURE_DEPENDS "src/assets/*.cpp"    "src/assets/*.h")
file(GLOB_RECURSE SCRIPTING_SOURCES CONFIGURE_DEPENDS "src/scripting/*.cpp" "src/scripting/*.h")
file(GLOB_RECURSE CLI_SOURCES       CONFIGURE_DEPENDS "src/cli/*.cpp"       "src/cli/*.h")
file(GLOB_RECURSE APP_SOURCES       CONFIGURE_DEPENDS "src/app/engine.cpp"  "src/app/engine.h")
file(GLOB_RECURSE NET_SOURCES       CONFIGURE_DEPENDS "src/net/*.cpp"       "src/net/*.h")
file(GLOB_RECURSE MCP_SOURCES       CONFIGURE_DEPENDS "src/mcp/*.cpp"       "src/mcp/*.h")
file(GLOB_RECURSE DEBUG_SOURCES     CONFIGURE_DEPENDS "src/debug/*.cpp"     "src/debug/*.h")

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
    ${MCP_SOURCES}
    ${DEBUG_SOURCES}
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
)

# Windows: link Winsock2 for networking and MCP
if(WIN32)
  target_link_libraries(odyssey_engine PUBLIC ws2_32)
endif()

# ── Main executable ─────────────────────────────────────────────────────────
add_executable(odyssey src/app/main.cpp)

target_link_libraries(odyssey
  PRIVATE
    odyssey_engine
)

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

# ── Copy runtime assets to build output ───────────────────────────────────────
add_custom_target(copy_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/behaviors"
    "${CMAKE_CURRENT_BINARY_DIR}/behaviors"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/demo"
    "${CMAKE_CURRENT_BINARY_DIR}/demo"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/schemas"
    "${CMAKE_CURRENT_BINARY_DIR}/schemas"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/engine.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/engine.xml"
  COMMENT "Copying runtime assets to build directory"
)

add_dependencies(odyssey copy_assets)

# ── Windows: help find Vulkan SDK ────────────────────────────────────────────
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
  list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}")
endif()
