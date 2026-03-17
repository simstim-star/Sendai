cmake_minimum_required (VERSION 3.8)

project(__sendai LANGUAGES C)
set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES "src/core/camera.c" "src/core/timer.c" "src/win32/win_main.c" "src/win32/win_path.c" "src/win32/file_dialog.c"
"src/renderer/renderer.c" "src/ui/ui.c" "src/error/error.c" "src/core/engine.c" "src/core/log.c" "src/assets/gltf.c"
"src/core/arena.c" "src/renderer/shader.c")

set(HEADER_FILES "src/core/camera.h" "src/core/timer.h" "src/win32/win_path.h" "src/win32/file_dialog.h" "src/assets/gltf.h"
"src/renderer/renderer.h" "src/renderer/render_types.h" "src/ui/ui.h" "src/error/error.h" "src/core/engine.h" "src/core/log.h" 
"src/core/scene.h" "src/core/arena.h" "src/dx_helpers/barrier_helpers.h" "src/dx_helpers/desc_helpers.h" "src/renderer/shader.h")

set(DEPS_FILES "deps/b64.h" "deps/cgltf.h" "deps/nuklear.h" "deps/stb_ds.h" "deps/stb_image.h")

set(SHADER_FILES "src/shaders/gltf/gltf.hlsl")

set_source_files_properties(${SHADER_FILES} PROPERTIES LANGUAGE HLSL)

source_group("Sources" FILES ${SOURCE_FILES})
source_group("Headers" FILES ${HEADER_FILES})
source_group("Shaders" FILES ${SHADER_FILES})
source_group("Libs" FILES ${DEPS_FILES})

set(ALL_PROJECT_FILES ${SOURCE_FILES} ${HEADER_FILES} ${SHADER_FILES} ${LIB_FILES})

# You need to add all files on the executable for them to be visible on 
# the source_group in Visual Studio
# https://stackoverflow.com/a/31653555/14815076
add_executable(${PROJECT_NAME} WIN32 ${ALL_PROJECT_FILES})

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/core/pch.h")
target_compile_options(${PROJECT_NAME} PRIVATE /W4)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

find_package(XMathC REQUIRED PATHS "C:/xmathc")
target_link_libraries(${PROJECT_NAME} PUBLIC d3d12.lib dxguid.lib dxgi.lib D3DCompiler.lib XMathC)
target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL:NO)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/gltf $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/gltf
    COMMENT "Copying triangle shaders" VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets" VERBATIM
)