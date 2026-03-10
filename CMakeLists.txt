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
file(GLOB_RECURSE CORE_SOURCES   CONFIGURE_DEPENDS "src/core/*.cpp"   "src/core/*.h")
file(GLOB_RECURSE VULKAN_SOURCES CONFIGURE_DEPENDS "src/vulkan/*.cpp" "src/vulkan/*.h")
file(GLOB_RECURSE NADIR_SOURCES  CONFIGURE_DEPENDS "src/nadir/*.cpp"  "src/nadir/*.h")

set(ENGINE_SOURCES ${CORE_SOURCES} ${VULKAN_SOURCES} ${NADIR_SOURCES})

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
)

# ── Main executable ─────────────────────────────────────────────────────────
add_executable(odyssey src/app/main.cpp)

target_link_libraries(odyssey
  PRIVATE
    odyssey_engine
    CLI11::CLI11
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

# ── Copy behaviors/ to build output ─────────────────────────────────────────
add_custom_target(copy_behaviors ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/behaviors"
    "${CMAKE_CURRENT_BINARY_DIR}/behaviors"
  COMMENT "Copying behaviors/ to build directory"
)

# Ensure behaviors are copied before the main executable runs
add_dependencies(odyssey copy_behaviors)

# ── Windows: help find Vulkan SDK ────────────────────────────────────────────
if(WIN32 AND DEFINED ENV{VULKAN_SDK})
  list(APPEND CMAKE_PREFIX_PATH "$ENV{VULKAN_SDK}")
endif()
