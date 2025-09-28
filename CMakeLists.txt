cmake_minimum_required (VERSION 3.8)

project(__sendai LANGUAGES C)
set(CMAKE_C_STANDARD 99)

set(SRC "src/win32/win_main.c" "src/renderer/renderer.c" "src/win32/window.c" "src/gui/gui.c" "src/error/error.c" "src/win32/win_path.c" "src/win32/window.c")

add_executable(${PROJECT_NAME} WIN32 ${SRC})

target_link_libraries(${PROJECT_NAME} PUBLIC d3d12.lib dxguid.lib dxgi.lib D3DCompiler.lib)
target_link_options(${PROJECT_NAME} PRIVATE /INCREMENTAL:NO)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/triangle $<TARGET_FILE_DIR:${PROJECT_NAME}>/src/shaders/triangle
    COMMENT "Copying triangle shaders" VERBATIM
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/nuklear $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying nuklear shaders" VERBATIM
)