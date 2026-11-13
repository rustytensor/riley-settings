cmake_minimum_required(VERSION 3.25)
project(riley-settings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The M1 gate's planted fault: the shared atomic output drops the second line of what a writer writes.
option(RILEY_PLANT_FAULT "Plant the gate's fault in the writers' output (tests only)" OFF)
# The M2 gate's planted fault: the second option's control is bound to the third option's key.
option(RILEY_PLANT_FAULT_PANEL "Plant the gate's fault in the panel's bindings (tests only)" OFF)

include(GNUInstallDirs)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_policy(SET QTP0001 NEW)   # QML modules under qrc:/qt/qml/
qt_policy(SET QTP0004 NEW)
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

# ---- The panel (M2): C++ models plus the QML module, as one static library -----------------------------------
set(RILEY_PANEL_QML
    qml/Main.qml
    qml/SectionBox.qml
    qml/OptionRow.qml
    qml/SwitchControl.qml
    qml/ChoiceControl.qml
    qml/SliderControl.qml
    qml/NumberControl.qml
    qml/TextControl.qml
    qml/ColourControl.qml
    qml/FontControl.qml
    qml/PathField.qml
    qml/FileControl.qml
    qml/FolderControl.qml
    qml/KeyBindingControl.qml
    qml/ListControl.qml
)
foreach(f ${RILEY_PANEL_QML})
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_library(riley-settings-panel STATIC
    src/panel/panel.cpp
    src/panel/panel.h
)
set_target_properties(riley-settings-panel PROPERTIES AUTOMOC ON)   # before qt_add_qml_module: its type registration needs it
qt_add_qml_module(riley-settings-panel
    URI Riley.Settings
    VERSION 1.0
    QML_FILES ${RILEY_PANEL_QML}
    SOURCES src/panel/models.cpp src/panel/models.h
)
target_include_directories(riley-settings-panel PUBLIC src PRIVATE src/panel)   # src/panel: for the generated type registration
target_link_libraries(riley-settings-panel
    PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick riley-settings-panelplugin
    PRIVATE riley-config-core Qt6::QuickControls2)
target_compile_definitions(riley-settings-panel PRIVATE RILEY_SETTINGS_SHARE_DIR="${RILEY_SETTINGS_SHARE_DIR}")
if(RILEY_PLANT_FAULT_PANEL)
    message(WARNING "RILEY_PLANT_FAULT_PANEL is ON: the second option is bound to the third option's key (gate builds only)")
    target_compile_definitions(riley-settings-panel PRIVATE RILEY_PLANT_FAULT_PANEL)
endif()

qt_add_executable(riley-settings-qt src/app/main.cpp)
set_target_properties(riley-settings-qt PROPERTIES AUTOMOC ON)
target_link_libraries(riley-settings-qt PRIVATE riley-settings-panel Qt6::Gui Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(riley-settings-qt PRIVATE QT_NO_KEYWORDS QT_NO_CAST_FROM_ASCII)
set_target_properties(riley-settings-qt PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

foreach(t riley-config-core riley-config)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

install(TARGETS riley-config riley-settings-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY share/riley-settings/ DESTINATION ${CMAKE_INSTALL_DATADIR}/riley-settings)
