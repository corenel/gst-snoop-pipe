cmake_minimum_required(VERSION 2.8.12)

project(gst_cv_pipeline CXX)

# set cmake compilation
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-Wall -Wextra -Wno-unused-parameter -O2 -g)

# set cmake options
option(DEBUG_MODE "Enable debug output" OFF)

# set cmake modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_SYSROOT
    $ENV{SDKTARGETSYSROOT}
    CACHE PATH "Path to sysroot")
# set (CMAKE_SKIP_RPATH true CACHE INTERNAL "")

# find dependencies
find_package(GStreamer REQUIRED)
find_package(GLIB REQUIRED)
find_package(OpenCV REQUIRED)

# find all source files
file(GLOB_RECURSE src_files ${PROJECT_SOURCE_DIR}/src/*.cc
     ${PROJECT_SOURCE_DIR}/src/*/*.cc)

# add common library
include_directories(include)
add_library(${PROJECT_NAME} SHARED ${src_files})

target_include_directories(${PROJECT_NAME} PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${GSTREAMER_LIBRARIES} ${OpenCV_LIBS})

# add executables with project library
macro(add_simple_app name)
  add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/app/${name}.cc)
  target_link_libraries(${name} ${PROJECT_NAME})
  install(TARGETS ${name} RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
endmacro()

macro(add_simple_apps)
  file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cc")
  foreach(file ${files})
    get_filename_component(name ${file} NAME_WE)
    add_simple_app(${name})
  endforeach()
endmacro()

add_simple_apps()
