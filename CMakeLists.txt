cmake_minimum_required(VERSION 3.20)
project(nexus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NEXUS_BUILD_TESTS "Build NEXUS unit tests" ON)
option(NEXUS_ENABLE_GTEST "Fetch and use GoogleTest when network access is available" OFF)

add_library(nexus_common INTERFACE)
target_include_directories(nexus_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(nexus_thermal
    core/thermal/thermal_solver.cpp
)
target_link_libraries(nexus_thermal PUBLIC nexus_common)

add_library(nexus_geometry
    core/geometry/geometry_parser.cpp
)
target_link_libraries(nexus_geometry PUBLIC nexus_common)

add_library(nexus_performance
    core/performance/performance_benchmark.cpp
)
target_link_libraries(nexus_performance PUBLIC nexus_common nexus_thermal)

add_library(nexus_neural
    core/neural/neural_regression.cpp
)
target_link_libraries(nexus_neural PUBLIC nexus_common)

add_library(nexus_hardware
    core/hardware/hardware_adapters.cpp
)
target_link_libraries(nexus_hardware PUBLIC nexus_common)

add_executable(nexus-api
    backend/api/main.cpp
)
target_link_libraries(nexus-api PRIVATE nexus_common nexus_geometry nexus_performance nexus_neural nexus_hardware)
if (WIN32)
    target_link_libraries(nexus-api PRIVATE ws2_32)
endif()

add_executable(nexus-thermal-export backend/thermal_export.cpp)
target_link_libraries(nexus-thermal-export PRIVATE nexus_thermal)

if (NEXUS_BUILD_TESTS)
    enable_testing()
    add_executable(nexus-core-tests tests/core_smoke_test.cpp)
    target_link_libraries(nexus-core-tests PRIVATE nexus_common nexus_geometry nexus_performance nexus_neural nexus_hardware)
    add_test(NAME nexus-core-smoke COMMAND nexus-core-tests)

    add_executable(nexus-thermal-tests tests/thermal_focused_test.cpp)
    target_link_libraries(nexus-thermal-tests PRIVATE nexus_common nexus_thermal)
    add_test(NAME nexus-thermal-focused COMMAND nexus-thermal-tests)
    add_executable(nexus-geometry-tests tests/geometry_focused_test.cpp)
    target_link_libraries(nexus-geometry-tests PRIVATE nexus_common nexus_geometry)
    add_test(NAME nexus-geometry-focused COMMAND nexus-geometry-tests)
    add_executable(nexus-performance-tests tests/performance_focused_test.cpp)
    target_link_libraries(nexus-performance-tests PRIVATE nexus_common nexus_performance)
    add_test(NAME nexus-performance-focused COMMAND nexus-performance-tests)
    add_executable(nexus-neural-tests tests/neural_focused_test.cpp)
    target_link_libraries(nexus-neural-tests PRIVATE nexus_common nexus_neural)
    add_test(NAME nexus-neural-focused COMMAND nexus-neural-tests)
    add_executable(nexus-hardware-tests tests/hardware_focused_test.cpp)
    target_link_libraries(nexus-hardware-tests PRIVATE nexus_common nexus_hardware)
    add_test(NAME nexus-hardware-focused COMMAND nexus-hardware-tests)
endif()

if (NEXUS_ENABLE_GTEST)
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GoogleTest is available for additional test targets.")
endif()

install(TARGETS nexus-api RUNTIME DESTINATION bin)
install(DIRECTORY frontend/dist/ DESTINATION frontend/dist OPTIONAL)
install(DIRECTORY docs/ DESTINATION docs OPTIONAL)
install(DIRECTORY data/ DESTINATION data OPTIONAL)
install(FILES README.md CMakeLists.txt config/nexus.example.json DESTINATION .)

if (WIN32)
    add_executable(nexus-launcher WIN32 packaging/windows/Launch.cpp)
    set_target_properties(nexus-launcher PROPERTIES OUTPUT_NAME "Launch")
    install(TARGETS nexus-launcher RUNTIME DESTINATION .)
endif()

set(CPACK_PACKAGE_NAME "NEXUS-Workbench")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "ZIP")
include(CPack)
