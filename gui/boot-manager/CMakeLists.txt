cmake_minimum_required(VERSION 3.20)
project(NEXUS-Boot-Manager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Widgets)

# Boot Manager Executable
qt_add_executable(nexus-boot-manager
    # Main source files
    gui/boot-manager/main.cpp
    gui/boot-manager/bootconfig-model.h
    gui/boot-manager/bootconfig-model.cpp
    gui/boot-manager/virtmode-manager.h
    gui/boot-manager/virtmode-manager.cpp
    
    # QML files
    gui/boot-manager/qml/Main.qml
    gui/boot-manager/qml/Message.qml
    gui/boot-manager/qml/EntryDialog.qml
    
    # Resource file
    gui/boot-manager/resources.qrc
)

# Add version definitions
target_compile_definitions(nexus-boot-manager PRIVATE
    NEXUS_VERSION_MAJOR=1
    NEXUS_VERSION_MINOR=0
    NEXUS_VERSION_PATCH=0
    NEXUS_VERSION_STRING="1.0.0"
    NEXUS_CODENAME="Genesis"
)

# Include directories
target_include_directories(nexus-boot-manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/boot-manager
)

# Link libraries
target_link_libraries(nexus-boot-manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

# Install target
install(TARGETS nexus-boot-manager
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Print configuration
message(STATUS "NEXUS Boot Manager Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
