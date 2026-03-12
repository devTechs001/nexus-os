cmake_minimum_required(VERSION 3.20)
project(NEXUS-OS-GUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Core Gui Qml Quick QuickControls2 Widgets 
    Charts Multimedia Network Bluetooth Positioning
)

qt_standard_project_setup()

# Boot Manager GUI
qt_add_executable(nexus-boot-manager
    gui/boot-manager/main.cpp
    gui/boot-manager/bootconfig-model.cpp
    gui/boot-manager/virtmode-manager.cpp
)

# VM Manager GUI
qt_add_executable(nexus-vm-manager
    gui/vm-manager/main.cpp
    gui/vm-manager/vm-model.cpp
    gui/vm-manager/vm-controller.cpp
)

# System Settings
qt_add_executable(nexus-settings
    gui/system-settings/main.cpp
    gui/system-settings/settings-model.cpp
)

# Control Center
qt_add_executable(nexus-control-center
    gui/control-center/main.cpp
    gui/control-center/dashboard-model.cpp
)

# Link libraries
target_link_libraries(nexus-boot-manager PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

target_link_libraries(nexus-vm-manager PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Charts
)

target_link_libraries(nexus-settings PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
)

target_link_libraries(nexus-control-center PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Charts
)

# Install targets
install(TARGETS nexus-boot-manager nexus-vm-manager nexus-settings nexus-control-center
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
