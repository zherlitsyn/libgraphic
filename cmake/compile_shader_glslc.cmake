# Wraps glslc for the case where glslangValidator is unusable. glslc
# can emit a C initialiser but not a whole header, so the declaration
# is added here.
#
# Run in script mode with GLSLC, SOURCE, OUTPUT and VARIABLE set.

execute_process(
    COMMAND "${GLSLC}" -mfmt=c -o "${OUTPUT}.inc" "${SOURCE}"
    RESULT_VARIABLE status
    ERROR_VARIABLE errors)

if(NOT status EQUAL 0)
    message(FATAL_ERROR "glslc failed on ${SOURCE}: ${errors}")
endif()

file(READ "${OUTPUT}.inc" payload)
file(WRITE "${OUTPUT}"
    "/* generated from ${SOURCE} by glslc */\n"
    "#pragma once\n"
    "#include <stdint.h>\n\n"
    "const uint32_t ${VARIABLE}[] = ${payload};\n")
file(REMOVE "${OUTPUT}.inc")