set(CORE_SOURCES "core/camera.c" "core/memory.c" "core/timer.c" "core/engine.c" "core/log.c" "error/error.c" "core/grid.c")
set(RENDERER_SOURCES "renderer/renderer.c" "renderer/shader.c" "renderer/light.c" "renderer/texture.c"
"renderer/billboard.c" "renderer/cubemap.c" "renderer/block_compression.c")
set(WIN32_SOURCES "win32/win_main.c" "win32/win_path.c" "win32/file_dialog.c")
set(UI_SOURCES "ui/sendai_nk.c" "ui/ui.c" "ui/ui_bottom_panel.c" "ui/ui_top_bar.c" "ui/ui_tool_bar.c")
set(GLTF_SOURCES "assets/gltf.c" "assets/dds_loader.c")
set(PIX_SOURCES "pix/pix.c")
set(SOURCE_FILES ${CORE_SOURCES} ${WIN32_SOURCES} ${RENDERER_SOURCES} ${UI_SOURCES} ${GLTF_SOURCES} ${PIX_SOURCES})

set(CORE_HEADERS "core/camera.h" "core/memory.h" "core/timer.h" "core/engine.h" "core/log.h" "core/scene.h" "error/error.h" "core/grid.h")
set(RENDERER_HEADERS "renderer/renderer.h" "renderer/render_types.h" "renderer/shader.h" "renderer/light.h" "renderer/texture.h" 
"renderer/billboard.h" "renderer/cubemap.h" "renderer/block_compression.h")
set(WIN32_HEADERS "win32/win_path.h" "win32/str_helper.h" "win32/file_dialog.h")
set(UI_HEADERS "ui/sendai_nk.h" "ui/ui.h" "ui/ui_bottom_panel.h" "ui/ui_top_bar.h" "ui/ui_tool_bar.h")
set(DX_HELPERS "dx_helpers/barrier_helpers.h" "dx_helpers/desc_helpers.h")
set(GLTF_HEADERS "assets/gltf.h" "assets/dds_loader.h")
set(HEADER_FILES ${CORE_HEADERS} ${WIN32_HEADERS} ${RENDERER_HEADERS} ${UI_HEADERS} ${DX_HELPERS} ${GLTF_HEADERS})

set(DEPS_FILES "b64.h" "cgltf.h" "nuklear.h" "stb_ds.h" "stb_image.h" "stb_image_resize2.h" "stb_dxt.h")
list(TRANSFORM DEPS_FILES PREPEND "${PROJECT_SOURCE_DIR}/deps/")

source_group("Core" FILES ${CORE_SOURCES} ${CORE_HEADERS})
source_group("Renderer" FILES ${RENDERER_SOURCES} ${RENDERER_HEADERS})
source_group("Win32" FILES ${WIN32_SOURCES} ${WIN32_HEADERS})
source_group("UI" FILES ${UI_SOURCES} ${UI_HEADERS})
source_group("GLTF" FILES ${GLTF_SOURCES} ${GLTF_HEADERS})
source_group("DX Helpers" FILES ${DX_HELPERS})
source_group("Libs" FILES ${DEPS_FILES})

# You need to add all files on the executable for them to be visible on
# the source_group in Visual Studio
# https://stackoverflow.com/a/31653555/14815076
add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES} ${HEADER_FILES} ${DEPS_FILES})

target_precompile_headers(${PROJECT_NAME} PRIVATE "core/pch.h")
target_compile_options(${PROJECT_NAME} PRIVATE /W4)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS" COMPILE_WARNING_AS_ERROR ON)

set_source_files_properties("ui/sendai_nk.c" PROPERTIES COMPILE_OPTIONS "/W0")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/deps")

target_link_libraries(${PROJECT_NAME} PUBLIC d3d12.lib dxguid.lib dxgi.lib D3DCompiler.lib XMathC)
target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL:NO)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sendai $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/sendai
    COMMENT "Copying triangle shaders" VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets" VERBATIM
)