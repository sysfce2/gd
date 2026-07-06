# Fuzzer harnesses for libgd codec decode paths.
# Built only when both root-level BUILD_TESTS and ENABLE_FUZZERS are on.
# ENABLE_FUZZERS forces clang + static libgd (see root CMakeLists.txt);
# each harness links the static lib with -fsanitize=fuzzer so only the
# harness pulls in libFuzzer main.

include_directories(BEFORE ${GD_INCLUDE_DIR})

# Helper: define a fuzzer target.
# bmp/gif are always compiled into libgd; others depend on their ENABLE_* option.
# ${enable_var} is the gating CMake option for this codec.
macro(add_fuzz_target codec enable_var)
    if (${enable_var} OR "${codec}" STREQUAL "bmp" OR "${codec}" STREQUAL "gif")
        set(target fuzz_${codec}_decode)
        add_executable(${target} fuzz_${codec}_decode.c)
        target_link_libraries(${target} ${GD_LIB_STATIC})
        target_compile_options(${target} PRIVATE ${FUZZ_CFLAGS})
        target_link_options(${target} PRIVATE ${FUZZ_LDFLAGS})
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Bin"
        )
    endif()
endmacro()

add_fuzz_target(jxl    ENABLE_JXL)
add_fuzz_target(uhdr   ENABLE_ULTRAHDR)
add_fuzz_target(tiff   ENABLE_TIFF)
add_fuzz_target(bmp    ENABLE_GD_FORMATS)
add_fuzz_target(png    ENABLE_PNG)
add_fuzz_target(webp   ENABLE_WEBP)
add_fuzz_target(gif    ENABLE_GD_FORMATS)
add_fuzz_target(jpeg   ENABLE_JPEG)
