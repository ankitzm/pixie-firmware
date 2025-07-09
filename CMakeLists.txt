# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Get the short git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set it as a global compile definition
add_compile_definitions(GIT_COMMIT=\"${GIT_COMMIT_HASH}\")

# You can also add a build timestamp
#string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
#add_compile_definitions(BUILD_TIMESTAMP=\"${BUILD_TIMESTAMP}\")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pixie)

