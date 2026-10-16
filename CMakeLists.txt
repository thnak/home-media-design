cmake_minimum_required(VERSION 3.25)

project(home_media
    VERSION 0.0.1
    DESCRIPTION "Home media server ma hoa"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HM_BUILD_TESTS "Build unit tests" ON)
option(HM_BUILD_BENCH "Build benchmarks" ON)
option(HM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(BoundaryCheck)

find_package(unofficial-sodium CONFIG REQUIRED)

# Thu tu quan trong: thu vien truoc, ung dung sau.
add_subdirectory(shared)   # shared_protocol: khong giu khoa
add_subdirectory(crypto)   # crypto_core: giu khoa
add_subdirectory(server)   # media_api (khong giu khoa), media_worker (giu khoa)
add_subdirectory(client)   # media_client

if(HM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HM_BUILD_BENCH)
    add_subdirectory(bench)
endif()

# Muc 11 cua thiet ke: media_api khong duoc phu thuoc bac cau vao module giu khoa.
hm_forbid_dependency(media_api crypto_core)
