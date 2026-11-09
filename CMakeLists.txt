cmake_minimum_required(VERSION 3.24)
project(flux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Widgets Quick QuickControls2 Qml DBus Svg Network)
find_package(KF6WindowSystem REQUIRED)
find_package(KF6StatusNotifierItem REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-hidraw)
pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)

qt_standard_project_setup(REQUIRES 6.7)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(flux
    src/main.cpp
)

file(GLOB_RECURSE FLUX_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml)
file(GLOB FLUX_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS resources/icons/*.svg)
file(GLOB_RECURSE FLUX_DATA RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS resources/data/*.json)

set_source_files_properties(qml/Theme.qml qml/Motion.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(flux
    URI Flux
    VERSION 1.0
    QML_FILES ${FLUX_QML}
    RESOURCES
        ${FLUX_ICONS}
        ${FLUX_DATA}
        resources/fonts/InterVariable.ttf
        resources/flux.svg
    SOURCES
        src/core/HidDevice.h src/core/HidDevice.cpp
        src/core/IoWorker.h src/core/IoWorker.cpp
        src/core/DeviceBase.h src/core/DeviceBase.cpp
        src/core/DeviceManager.h src/core/DeviceManager.cpp
        src/core/KeyNames.h src/core/KeyNames.cpp
        src/devices/lamzu/LamzuProtocol.h src/devices/lamzu/LamzuProtocol.cpp
        src/devices/lamzu/LamzuMouse.h src/devices/lamzu/LamzuMouse.cpp
        src/devices/mad/MadProtocol.h src/devices/mad/MadProtocol.cpp
        src/devices/mad/MadKeyboard.h src/devices/mad/MadKeyboard.cpp
        src/devices/mad/LightEngine.h src/devices/mad/LightEngine.cpp src/devices/mad/LightEngineExtra.cpp
        src/app/AudioTap.h src/app/AudioTap.cpp
        src/app/AppController.h src/app/AppController.cpp
        src/app/ProfileStore.h src/app/ProfileStore.cpp
        src/app/WindowChrome.h src/app/WindowChrome.cpp
        src/app/IconProvider.h src/app/IconProvider.cpp
)

# qmltyperegistrar includes headers by basename.
target_include_directories(flux PRIVATE src src/core src/app src/devices/lamzu src/devices/mad)
target_compile_definitions(flux PRIVATE FLUX_VERSION="${PROJECT_VERSION}")
target_compile_options(flux PRIVATE -Wall -Wextra -Wno-unused-parameter)

target_link_libraries(flux PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Qml Qt6::DBus Qt6::Svg
    KF6::WindowSystem KF6::StatusNotifierItem
    PkgConfig::HIDAPI PkgConfig::UDEV
)

# Small CLI for scripting / debugging the protocols.
add_executable(fluxctl
    src/cli/fluxctl.cpp
    src/core/HidDevice.cpp
    src/devices/lamzu/LamzuProtocol.cpp
    src/devices/mad/MadProtocol.cpp
    src/devices/mad/LightEngine.cpp
    src/devices/mad/LightEngineExtra.cpp
)
target_include_directories(fluxctl PRIVATE src)
target_link_libraries(fluxctl PRIVATE PkgConfig::HIDAPI)
set_target_properties(fluxctl PROPERTIES AUTOMOC OFF)

include(GNUInstallDirs)
install(TARGETS flux fluxctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/dev.flux.Flux.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/flux.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME dev.flux.Flux.svg)
install(FILES packaging/70-flux.rules DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/udev/rules.d)
install(FILES packaging/70-flux.rules DESTINATION ${CMAKE_INSTALL_DATADIR}/flux)
install(FILES resources/fonts/Inter-LICENSE.txt resources/icons/LUCIDE-LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/flux)

# Offline contact sheet of lighting effects (dev tool, not installed).
add_executable(fxpreview EXCLUDE_FROM_ALL tools/fxpreview.cpp src/devices/mad/LightEngine.cpp src/devices/mad/LightEngineExtra.cpp)
target_include_directories(fxpreview PRIVATE src)
target_link_libraries(fxpreview PRIVATE Qt6::Gui)
set_target_properties(fxpreview PROPERTIES AUTOMOC OFF)
