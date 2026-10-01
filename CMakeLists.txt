cmake_minimum_required(VERSION 3.16)

project(graphic
    VERSION 0.1.0
    DESCRIPTION "Immediate mode graphics API over Vulkan"
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

option(GRAPHIC_BUILD_EXAMPLES "Build the examples" ON)

# --------------------------------------------------------------------
# dependencies
#
# GLFW is linked dynamically and never built here: on Linux the system
# package is the only thing that knows whether the session is Wayland
# or X11, and that decision belongs to the user, not to us.
# --------------------------------------------------------------------

find_package(Vulkan REQUIRED)

find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLFW IMPORTED_TARGET glfw3)
    endif()
    if(NOT TARGET PkgConfig::GLFW)
        message(FATAL_ERROR
            "GLFW 3.3 or newer not found. Install the development "
            "package (libglfw3-dev, glfw, glfw3) or point CMake at "
            "it with -Dglfw3_DIR=<path>")
    endif()
    add_library(glfw ALIAS PkgConfig::GLFW)
endif()

# --------------------------------------------------------------------
# the library
# --------------------------------------------------------------------

add_library(graphic STATIC
    src/graphic_core.c
    src/graphic_backend.c
    src/graphic_platform_glfw.c)

target_include_directories(graphic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(graphic
    PUBLIC Vulkan::Vulkan
    PRIVATE glfw)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(graphic PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Wno-unused-parameter>)
endif()

# --------------------------------------------------------------------
# examples
# --------------------------------------------------------------------


if(GRAPHIC_BUILD_EXAMPLES)
    foreach(example window)
        add_executable(${example} examples/${example}.c)
        target_link_libraries(${example} PRIVATE graphic)
        set_target_properties(${example} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()

endif()
