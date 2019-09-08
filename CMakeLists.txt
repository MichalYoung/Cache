#
# Building Cache project with CMake ---
#   work in progress.   I will build this
#   up slowly, one target at a time.
#
#  Eventually we need to make this "staged" so that we can
#  build a Docker target without all the build infrastructure
#  (CMake etc), but for now I'll assume the deployment environment
#  has all the infrastructure for building.
#

CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
set(CMAKE_C_COMPILER gcc)

project(cache  VERSION 0.2
        DESCRIPTION "Cache pub/sub system; incremental conversion from autotools"
        LANGUAGES C
        )

# Out-of-source build
add_subdirectory(src "${CMAKE_SOURCE_DIR}/build")

