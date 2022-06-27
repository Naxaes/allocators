cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0048 NEW)

project(
    Allocator    # Project name
    VERSION 0.0.1.0
    DESCRIPTION "A bunch of efficient, composable and debuggable allocators."
    HOMEPAGE_URL "https://github.com/Naxaes/"
    LANGUAGES C
)


set(CMAKE_C_STANDARD 99)

# ---- Shared compiler flags ----
# -Wmissing-noreturn is due to a bug for switches.
set(WARNINGS_DEBUG   "-Wall -Wextra -Wpedantic -Weverything -Wno-padded -Wno-old-style-cast -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-zero-as-null-pointer-constant -Wno-alloca -Wno-missing-noreturn -Wno-missing-prototypes -Wno-unused-parameter -Wno-missing-variable-declarations -Wno-c11-extensions")
set(WARNINGS_RELEASE "${WARNINGS_DEBUG} -Wno-undef -Wno-unused-macros -Wno-unused-variable -Wno-unused-parameter -Wno-disabled-macro-expansion -Wno-missing-prototypes -Wno-missing-variable-declarations")
set(COMPILER_FLAGS         "")
set(COMPILER_FLAGS_DEBUG   "${WARNINGS_DEBUG} -O0 -g -DDEBUG -fsanitize=address,undefined,alignment,bounds,signed-integer-overflow,return,null -fno-omit-frame-pointer")
set(COMPILER_FLAGS_RELEASE "${WARNINGS_RELEASE} -O2")

# -fsanitize=thread,memory,address,undefined
# -fsanitize=alignment
# -fsanitize=bool
# -fsanitize=bounds
# -fsanitize=enum
# -fsanitize=float-cast-overflow
# -fsanitize=float-divide-by-zero
# -fsanitize=function
# -fsanitize=integer-divide-by-zero
# -fsanitize=null
# -fsanitize=returns-nonnull-attribute
# -fsanitize=shift
# -fsanitize=signed-integer-overflow
# -fsanitize=unreachable
# -fsanitize=unsigned-integer-overflow
# -fsanitize=vla-bound
# -fsanitize=object-size
# -fsanitize=return


set($ENV{ASAN_OPTIONS}=detect_stack_use_after_return=1)


# ---- Flags specific to the compiler ----
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Using Clang++
    message(STATUS "Building for Clang")
    set(COMPILER_FLAGS         "${COMPILER_FLAGS}")
    set(COMPILER_FLAGS_DEBUG   "${COMPILER_FLAGS_DEBUG}")
    set(COMPILER_FLAGS_RELEASE "${COMPILER_FLAGS_RELEASE}")
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Using Visual Studio C++
    message(STATUS "Building for MSVC")
    set(COMPILER_FLAGS         "${COMPILER_FLAGS}")
    set(COMPILER_FLAGS_DEBUG   "${COMPILER_FLAGS_DEBUG}")
    set(COMPILER_FLAGS_RELEASE "${COMPILER_FLAGS_RELEASE}")
else()
    message(STATUS "Building for unknown compiler; will not add any flags.")
endif()


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(COMPILER_FLAGS "${COMPILER_FLAGS} ${COMPILER_FLAGS_DEBUG}")
    message(STATUS "Building debug with compiler flags ${COMPILER_FLAGS}")
else()
    set(COMPILER_FLAGS "${COMPILER_FLAGS} ${COMPILER_FLAGS_RELEASE}")
    message(STATUS "Building release with compiler flags ${COMPILER_FLAGS}")
endif()

set(CMAKE_C_FLAGS "${COMPILER_FLAGS}")
add_definitions(${COMPILER_FLAGS})
add_executable(main main.c)




add_executable(test test.c)
