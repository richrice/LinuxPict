cmake_minimum_required(VERSION 3.20)
project(LinuxPict VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)

add_library(linuxpict_core
    src/model.cpp
    src/render.cpp
)
target_include_directories(linuxpict_core PUBLIC include)
target_link_libraries(linuxpict_core PUBLIC PkgConfig::GTKMM)
target_compile_options(linuxpict_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(linuxpict
    src/main.cpp
    src/app.cpp
    src/capture.cpp
)
target_include_directories(linuxpict PRIVATE include)
target_link_libraries(linuxpict PRIVATE linuxpict_core PkgConfig::GTKMM)
target_compile_options(linuxpict PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(linuxpict_tests tests/model_tests.cpp)
    target_link_libraries(linuxpict_tests PRIVATE linuxpict_core)
    target_compile_options(linuxpict_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME linuxpict_tests COMMAND linuxpict_tests)
endif()

install(TARGETS linuxpict RUNTIME DESTINATION bin)
