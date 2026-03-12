cmake_minimum_required(VERSION 3.11.0)
project(tictactoe VERSION 0.1.0 LANGUAGES C CXX)

# check for macOS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOS TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(LINUX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(WINDOWS TRUE)
endif()

# Set compiler-specific flags for g++ and MinGW
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(LINUX OR WINDOWS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-ignored-qualifiers")
    endif()
endif()

# for filesystem functionality from C++20
set(CMAKE_CXX_STANDARD 20)

if(MACOS)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    find_package(glfw3 REQUIRED)
    include_directories(${GLFW_INCLUDE_DIRS})
else()
    # Windows: Use modern Windows SDK libraries (no need to find them manually)
    # DirectX11 libraries are part of the Windows SDK
endif()

include(CTest)
enable_testing()

if(MACOS)
    set(MAIN_FILE "main_macos.cpp")
    set(IMPL_FILE "imgui/imgui_impl_glfw.cpp")
    set(BCKD_FILE "imgui/imgui_impl_opengl3.cpp")
elseif(WINDOWS)
    set(MAIN_FILE "main_win32.cpp")
    set(IMPL_FILE "imgui/imgui_impl_win32.cpp")
    set(BCKD_FILE "imgui/imgui_impl_dx11.cpp")
else() # Linux
    set(MAIN_FILE "main_macos.cpp")
    set(IMPL_FILE "imgui/imgui_impl_glfw.cpp")
    set(BCKD_FILE "imgui/imgui_impl_opengl3.cpp")
endif()

add_executable(demo Application.cpp
                          Command.cpp
                          Command.h
                          Logger.cpp
                          Logger.h
                          imgui/imgui_demo.cpp
                          imgui/imgui_draw.cpp
                          imgui/imgui_tables.cpp
                          imgui/imgui_widgets.cpp
                          imgui/imgui.cpp
                          classes/Bit.cpp
                          classes/BitHolder.cpp
                          classes/Game.cpp
                          classes/Sprite.cpp
                          classes/Square.cpp
                          classes/ChessSquare.cpp
                          classes/Grid.cpp
                          classes/TicTacToe.cpp
                          classes/Checkers.cpp
                          classes/Othello.cpp
                          classes/Chess.cpp
                          ${BCKD_FILE}
                          ${MAIN_FILE}
                          ${IMPL_FILE}
                )

if(MACOS OR LINUX)
    target_link_libraries(demo ${OPENGL_gl_LIBRARY} glfw)
elseif(WINDOWS)
    # Windows: Link DirectX11 and required Windows libraries
    target_link_libraries(demo 
        d3d11.lib 
        d3dcompiler.lib 
        dxgi.lib 
        user32.lib 
        gdi32.lib 
        winmm.lib
    )
endif()

# Copy resources to build directory
add_custom_command(
  TARGET demo POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/resources"
          "$<TARGET_FILE_DIR:demo>/resources"
  COMMENT "Copying resources to runtime output dir"
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

include(CPack)
