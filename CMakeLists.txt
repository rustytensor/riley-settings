cmake_minimum_required(VERSION 3.25)
project(riley-settings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The M1 gate's planted fault: the shared atomic output drops the second line of what a writer writes.
option(RILEY_PLANT_FAULT "Plant the gate's fault in the writers' output (tests only)" OFF)

include(GNUInstallDirs)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)
find_package(LibXml2 REQUIRED)   # XSD validation: Qt 6 has none

set(RILEY_SETTINGS_SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/riley-settings")

add_library(riley-config-core STATIC
    src/config/atomic_file.cpp
    src/config/atomic_file.h
    src/config/config.cpp
    src/config/config.h
    src/config/description.cpp
    src/config/description.h
    src/config/values.cpp
    src/config/values.h
    src/config/writer.cpp
    src/config/writer.h
    src/config/xml.cpp
    src/config/xml.h
    src/config/writers/writers.h
    src/config/writers/hyprland-lua.cpp
    src/config/writers/hypridle.cpp
    src/config/writers/hyprpaper.cpp
    src/config/writers/greeter-sway.cpp
)
target_include_directories(riley-config-core PUBLIC src)
target_link_libraries(riley-config-core PUBLIC Qt6::Core PRIVATE LibXml2::LibXml2)
target_compile_definitions(riley-config-core PUBLIC QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII)
if(RILEY_PLANT_FAULT)
    message(WARNING "RILEY_PLANT_FAULT is ON: writers drop the second line of their output (gate builds only)")
    target_compile_definitions(riley-config-core PRIVATE RILEY_PLANT_FAULT)
endif()

add_executable(riley-config src/cli/main.cpp)
target_link_libraries(riley-config PRIVATE riley-config-core)
target_compile_definitions(riley-config PRIVATE RILEY_SETTINGS_SHARE_DIR="${RILEY_SETTINGS_SHARE_DIR}")
set_target_properties(riley-config PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

foreach(t riley-config-core riley-config)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

install(TARGETS riley-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY share/riley-settings/ DESTINATION ${CMAKE_INSTALL_DATADIR}/riley-settings)
