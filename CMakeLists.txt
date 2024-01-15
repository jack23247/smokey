cmake_minimum_required(VERSION 3.8)
project(smokey)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(OpenGL_GL_PREFERENCE GLVND)

# find dependencies
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Werror -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -march=native -DNDEBUG")
set(CMAKE_LINKER_FLAGS_DEBUG "-fsanitize=address")

include_directories(${SDL2_INCLUDE_DIRS} 
                    ${OPENGL_INCLUDE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.90)

add_executable(smokey src/main.cpp 
                imgui-1.90/imgui_draw.cpp
                imgui-1.90/imgui_tables.cpp
                imgui-1.90/imgui_widgets.cpp
                imgui-1.90/imgui.cpp
                imgui-1.90/backends/imgui_impl_opengl3.cpp 
                imgui-1.90/backends/imgui_impl_sdl2.cpp)

target_link_libraries(smokey ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})