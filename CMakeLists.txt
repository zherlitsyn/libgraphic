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
# shaders
#
# SPIR-V is embedded as a C array, so nothing is read from disk at run
# time. Two ways to get there, in order of preference:
#
#   glslangValidator   emits the whole header itself
#   glslc              emits the array, cmake/ wraps it
# --------------------------------------------------------------------

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(SHADER_COMPILER "")

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    HINTS "$ENV{VULKAN_SDK}/bin")
find_program(GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/bin")

# a compiler that is present but cannot run is worse than none,
# so it is tried once here rather than at build time
if(GLSLANG_VALIDATOR)
    execute_process(COMMAND "${GLSLANG_VALIDATOR}" --version
        RESULT_VARIABLE status
        OUTPUT_QUIET ERROR_QUIET)
    if(status EQUAL 0)
        set(SHADER_COMPILER glslang)
    else()
        message(WARNING
            "glslangValidator found but not runnable, "
            "trying glslc")
    endif()
endif()

if(NOT SHADER_COMPILER AND GLSLC)
    execute_process(COMMAND "${GLSLC}" --version
        RESULT_VARIABLE status
        OUTPUT_QUIET ERROR_QUIET)
    if(status EQUAL 0)
        set(SHADER_COMPILER glslc)
    endif()
endif()

if(NOT SHADER_COMPILER)
    message(FATAL_ERROR
        "no usable shader compiler: install the Vulkan SDK, which "
        "brings both glslangValidator and glslc")
else()
    message(STATUS "graphic: compiling shaders with ${SHADER_COMPILER}")

    set(SHADER_HEADERS "")

    function(graphic_compile_shader source stage)
        get_filename_component(name "${source}" NAME_WE)
        set(output "${GENERATED_DIR}/${name}.${stage}.h")
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/${source}")

        if(SHADER_COMPILER STREQUAL glslang)
            add_custom_command(
                OUTPUT "${output}"
                COMMAND "${GLSLANG_VALIDATOR}"
                    -V --vn ${name}_${stage}_spv
                    -o "${output}" "${input}"
                DEPENDS "${input}"
                COMMENT "compiling ${source}"
                VERBATIM)
        else()
            add_custom_command(
                OUTPUT "${output}"
                COMMAND ${CMAKE_COMMAND}
                    -DGLSLC=${GLSLC}
                    -DSOURCE=${input}
                    -DOUTPUT=${output}
                    -DVARIABLE=${name}_${stage}_spv
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/compile_shader_glslc.cmake"
                DEPENDS "${input}"
                COMMENT "compiling ${source}"
                VERBATIM)
        endif()

        set(SHADER_HEADERS ${SHADER_HEADERS} "${output}" PARENT_SCOPE)
    endfunction()

    graphic_compile_shader(shaders/default.vert vert)
    graphic_compile_shader(shaders/default.frag frag)

    add_custom_target(graphic_shaders DEPENDS ${SHADER_HEADERS})
endif()

# --------------------------------------------------------------------
# the library
# --------------------------------------------------------------------

add_library(graphic STATIC
    src/graphic_core.c
    src/graphic_backend.c
    src/graphic_pipeline.c
    src/graphic_platform_glfw.c)

add_dependencies(graphic graphic_shaders)

target_include_directories(graphic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${GENERATED_DIR}")

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
