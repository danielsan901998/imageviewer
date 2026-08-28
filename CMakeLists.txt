cmake_minimum_required(VERSION 3.16)

project(imageviewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
qt_standard_project_setup()

option(MEMORY_PROFILER "Build the memory profiler" ON)

if (MEMORY_PROFILER AND NOT TARGET build_all)
    add_custom_target(build_all ALL DEPENDS imageviewer memory_profiler)
endif()

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
	add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()


add_executable(imageviewer
    src/main.cpp
    src/dirstore.cpp
    src/autoscrollwidget.cpp
    inc/autoscrollwidget.hpp
    inc/dirstore.hpp
)

target_include_directories(imageviewer PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(imageviewer PRIVATE Qt6::Widgets Qt6::Concurrent)

add_executable(memory_profiler src/memory_profiler.cpp)
find_package(Qt6 REQUIRED COMPONENTS Core Gui)
target_link_libraries(memory_profiler PRIVATE Qt6::Core Qt6::Gui)
install(TARGETS memory_profiler RUNTIME DESTINATION bin/)
