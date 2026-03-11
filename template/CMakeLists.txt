cmake_minimum_required(VERSION 3.24)

project(MyGame LANGUAGES CXX)

# Point to the OdysseyEngine root directory.
# Adjust this path for your setup.
set(ODYSSEY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Path to OdysseyEngine root")

# Include the engine's CMakeLists.txt to get odyssey_engine and ODYSSEY_MAIN_SOURCE
add_subdirectory(${ODYSSEY_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/engine EXCLUDE_FROM_ALL)

# ── Game sources ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.h")

add_executable(my_game ${ODYSSEY_MAIN_SOURCE} ${GAME_SOURCES})

target_include_directories(my_game
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(my_game
  PRIVATE
    odyssey_engine
)

# ── Copy assets ──────────────────────────────────────────────────────────────
add_custom_target(copy_game_assets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ODYSSEY_ROOT}/behaviors/lib"
    "${CMAKE_CURRENT_BINARY_DIR}/behaviors/lib"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ODYSSEY_ROOT}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ODYSSEY_ROOT}/schemas"
    "${CMAKE_CURRENT_BINARY_DIR}/schemas"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/engine.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/engine.xml"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/scenes"
    "${CMAKE_CURRENT_BINARY_DIR}/scenes"
  COMMENT "Copying game assets to build directory"
)

add_dependencies(my_game copy_game_assets)
