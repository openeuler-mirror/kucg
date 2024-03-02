#
# Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
#

# Support clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-stdlib=libc++ ${CMAKE_CXX_FLAGS}")
endif()

if (IS_DIRECTORY ${UCG_BUILD_WITH_GTEST})
    find_library(GTEST gtest HINT ${UCG_BUILD_WITH_GTEST}/lib)
    find_library(GTEST_MAIN gtest_main HINT ${UCG_BUILD_WITH_GTEST}/lib)
    include_directories(${UCG_BUILD_WITH_GTEST}/googletest/include)
    if ( ${GTEST} STREQUAL "GTEST-NOTFOUND" OR ${GTEST_MAIN} STREQUAL "GTEST_MAIN-NOTFOUND" )
        message(FATAL_ERROR "The directory of gtest you provide :${UCG_BUILD_WITH_GTEST} is wrong!
                            If you use cmake 3, you may not need to provide the directory as
                            the project will download it itself.")
    endif()
else()
    if(SUPPORT_CMAKE3 MATCHES "ON")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            # googletest tag release-1.11.0
            GIT_TAG        e2239ee6043f73722e7aa812a459f54a28552929
        )

        FetchContent_MakeAvailable(googletest)
        set(GTEST "gtest")
        set(GTEST_MAIN "gtest_main")
    else()
        message(FATAL_ERROR "cmake 2 only support googletest which you provide the directory.
                            Please use -DUCG_BUILD_WITH_GTEST.")
    endif()
endif()
# Not install gtest
set(INSTALL_GTEST "OFF" CACHE STRING "" FORCE)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DUCG_ENABLE_DEBUG)
endif()

add_definitions(-DUCG_ENABLE_CHECK_PARAMS)
if (UCG_ENABLE_CHECK_PARAMS MATCHES "OFF")
    remove_definitions(-DUCG_ENABLE_CHECK_PARAMS)
endif()

if (UCG_ENABLE_MT MATCHES "ON")
    add_definitions(-DUCG_ENABLE_MT)
endif()

add_compile_options("-Wno-sign-compare")
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Build test dependencies
include(${CMAKE_SOURCE_DIR}/cmake/test_component_dep.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/test_planc_dep.cmake)

# Build ucg_gtest
file(GLOB_RECURSE SRCS ./*.cpp)

set(LINK_LIB ${GTEST_MAIN} ucg ucg_planc_ucx ucg_planc_hccl pthread ${GTEST})
if (UCG_BUILD_PLANC_UCX MATCHES "OFF")
    foreach(var IN LISTS SRCS LINK_LIB)
        if (var MATCHES ".*planc/ucx.*")
            list(REMOVE_ITEM SRCS ${var})
        elseif (var MATCHES "ucg_planc_ucx")
            list(REMOVE_ITEM LINK_LIB ${var})
        endif()
    endforeach()
endif()
if (UCG_BUILD_PLANC_HCCL MATCHES "OFF")
    foreach(var IN LISTS SRCS LINK_LIB)
        if (var MATCHES ".*planc/hccl.*")
            list(REMOVE_ITEM SRCS ${var})
        elseif (var MATCHES "ucg_planc_hccl")
            list(REMOVE_ITEM LINK_LIB ${var})
        endif()
    endforeach()
elseif (IS_DIRECTORY ${UCG_BUILD_WITH_HCCL})
    link_directories(${UCG_BUILD_WITH_HCCL}/lib64)
    list(APPEND LINK_LIB ascendcl)
endif()
add_executable(ucg_gtest ${SRCS})
target_link_libraries(ucg_gtest ${LINK_LIB})

if(SUPPORT_CMAKE3 MATCHES "ON")
    include(GoogleTest)
    gtest_discover_tests(ucg_gtest)
endif()