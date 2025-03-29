cmake_minimum_required(VERSION 3.16)

project(imageviewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
	add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()


add_executable(imageviewer
    src/main.cpp
    src/autoscrollwidget.cpp
)

target_include_directories(imageviewer PRIVATE ${CMAKE_SOURCE_DIR}/inc)
target_link_libraries(imageviewer PRIVATE Qt6::Widgets)
