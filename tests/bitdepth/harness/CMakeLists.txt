# The bit-depth gate's Displays harness, built only by tests/run-bitdepth.sh: its own top-level project, which adds
# the repo as a subdirectory and links the harness with the panel's library (as tests/panel/ does).
cmake_minimum_required(VERSION 3.25)
project(riley-settings-bitdepth-displays LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick Test)
find_package(LibXml2 REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../.. riley-settings)

add_executable(tst_displays tst_displays.cpp)
target_link_libraries(tst_displays PRIVATE riley-settings-panel Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
target_compile_options(tst_displays PRIVATE -Wall -Wextra)
set_target_properties(tst_displays PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
