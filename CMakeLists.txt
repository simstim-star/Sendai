cmake_minimum_required (VERSION 3.8)

project(__sendai LANGUAGES C)
set(CMAKE_C_STANDARD 99)

set(SRC_CORE "src/core/camera.c" "src/core/timer.c")
set(SRC_WIN "src/win32/win_main.c" "src/win32/win_path.c")
set(SRC ${SRC_WIN} ${SRC_CORE} "src/renderer/renderer.c" "src/gui/gui.c" "src/error/error.c" "src/core/engine.c" "src/core/log.h" "src/core/log.c")

add_executable(${PROJECT_NAME} WIN32 ${SRC} "src/core/engine.c" "src/assets/gltf.c"  "src/core/log.c")


target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        UNICODE
        _UNICODE
)

find_package(XMathC REQUIRED PATHS "C:/xmathc")
target_link_libraries(${PROJECT_NAME} PUBLIC d3d12.lib dxguid.lib dxgi.lib D3DCompiler.lib XMathC)
target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL:NO)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/gltf $<TARGET_FILE_DIR:${PROJECT_NAME}>/src/shaders/gltf
    COMMENT "Copying triangle shaders" VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/nuklear $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying nuklear shaders" VERBATIM
)