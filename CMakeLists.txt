cmake_minimum_required(VERSION 4.4)

project(game_2d LANGUAGES CXX)

find_package(
    engine 0.4.0 EXACT CONFIG REQUIRED
    COMPONENTS graphics
)

if(NOT TARGET engine::platform)
    message(FATAL_ERROR "engine::platform was not imported by find_package(engine).")
endif()
if(NOT TARGET engine::vulkan_graphics)
    message(FATAL_ERROR "engine::vulkan_graphics was not imported by find_package(engine).")
endif()

find_program(GAME_2D_GLSLC NAMES glslc REQUIRED)

set(GAME_2D_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
set(GAME_2D_VERTEX_SPV "${GAME_2D_SHADER_DIR}/square.vert.spv")
set(GAME_2D_FRAGMENT_SPV "${GAME_2D_SHADER_DIR}/square.frag.spv")

add_custom_command(
    OUTPUT "${GAME_2D_VERTEX_SPV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GAME_2D_SHADER_DIR}"
    COMMAND "${GAME_2D_GLSLC}"
        --target-env=vulkan1.3
        -fshader-stage=vert
        -o "${GAME_2D_VERTEX_SPV}"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/square.vert"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/square.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${GAME_2D_FRAGMENT_SPV}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GAME_2D_SHADER_DIR}"
    COMMAND "${GAME_2D_GLSLC}"
        --target-env=vulkan1.3
        -fshader-stage=frag
        -o "${GAME_2D_FRAGMENT_SPV}"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/square.frag"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/square.frag"
    VERBATIM
)

add_custom_target(game_2d_shaders DEPENDS "${GAME_2D_VERTEX_SPV}" "${GAME_2D_FRAGMENT_SPV}")

add_executable(game_2d src/main.cpp)
add_dependencies(game_2d game_2d_shaders)
target_compile_definitions(
    game_2d
    PRIVATE
        GAME_2D_VERTEX_SPV="${GAME_2D_VERTEX_SPV}"
        GAME_2D_FRAGMENT_SPV="${GAME_2D_FRAGMENT_SPV}"
)
target_link_libraries(
    game_2d
    PRIVATE
        engine::platform
        engine::vulkan_graphics
)
