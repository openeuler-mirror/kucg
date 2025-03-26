#
# Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
#

# Build ucg_info
file(GLOB SRCS ./*.c)
add_executable(ucg_info ${SRCS})


if (SUPPORT_CMAKE3 MATCHES "ON")
    if (IS_DIRECTORY ${UCG_BUILD_WITH_UCX})
        target_link_directories(ucg_info PRIVATE ${UCG_BUILD_WITH_UCX}/lib)
    endif()

    if (UCG_BUILD_PLANC_UCX MATCHES "ON")
        add_definitions(-DUCG_BUILD_PLANC_UCX)
        target_include_directories(ucg_info PRIVATE
                                   ${CMAKE_SOURCE_DIR}/src/planc/ucx)
    endif()

    if (UCG_BUILD_PLANC_HCCL MATCHES "ON")
        add_definitions(-DUCG_BUILD_PLANC_HCCL)
        target_include_directories(ucg_info PRIVATE
                                   ${CMAKE_SOURCE_DIR}/src/planc/hccl)
        if (IS_DIRECTORY ${UCG_BUILD_WITH_HCCL})
            target_include_directories(ucg_info PRIVATE ${UCG_BUILD_WITH_HCCL}/include)
            target_link_directories(ucg_info PRIVATE ${UCG_BUILD_WITH_HCCL}/lib64)
        endif()
        target_link_libraries(ucg_info ascendcl)
    endif()
    target_link_libraries(ucg_info ucg ucs pthread)
else()
    if (IS_DIRECTORY ${UCG_BUILD_WITH_UCX})
        target_include_directories(ucg_info PRIVATE
                                   ${CMAKE_SOURCE_DIR}/src/planc/ucx)
        target_include_directories(ucg_info PRIVATE ${UCG_BUILD_WITH_UCX}/include)
    endif()

    if (UCG_BUILD_PLANC_UCX MATCHES "ON")
	    find_library(UCS ucs HINTS ${UCG_BUILD_WITH_UCX}/lib)
        add_definitions(-DUCG_BUILD_PLANC_UCX)
        include_directories(${CMAKE_SOURCE_DIR}/src/planc/ucx)
    endif()
    if (UCG_BUILD_PLANC_HCCL MATCHES "ON")
        add_definitions(-DUCG_BUILD_PLANC_HCCL)
        include_directories(${CMAKE_SOURCE_DIR}/src/planc/hccl)
        if (IS_DIRECTORY ${UCG_BUILD_WITH_HCCL})
            target_include_directories(ucg_info PRIVATE ${UCG_BUILD_WITH_HCCL}/include)
            find_library(AS ascendcl HINTS ${UCG_BUILD_WITH_HCCL}/lib64)
        endif()
        target_link_libraries(ucg_info ${AS})
    endif()
    target_link_libraries(ucg_info ${UCS} ucg pthread)
endif()
# Install
install(TARGETS ucg_info
        RUNTIME DESTINATION ${UCG_INSTALL_BINDIR})
