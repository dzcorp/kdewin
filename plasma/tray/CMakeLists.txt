# C++ core forked from plasma-workspace 6.8.3 applets/systemtray (GPL-2.0-or-later) and libdbusmenuqt (LGPL-2.0-or-later).
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Quick Qml)
find_package(Qt6GuiPrivate ${QT_MIN_VERSION} REQUIRED)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS I18n IconThemes KIO JobWidgets Service ItemModels XmlGui StatusNotifierItem)
find_package(Plasma REQUIRED)
find_package(PlasmaQuick REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.lumen.tray\")

set(dbusmenu_SRCS
    dbusmenuqt/dbusmenuimporter.cpp
    dbusmenuqt/dbusmenushortcut_p.cpp
    dbusmenuqt/dbusmenutypes_p.cpp
    dbusmenuqt/utils.cpp
)
set_source_files_properties(dbusmenuqt/com.canonical.dbusmenu.xml PROPERTIES
    NO_NAMESPACE true
    INCLUDE "dbusmenutypes_p.h"
    CLASSNAME DBusMenuInterface
)
qt_add_dbus_interface(dbusmenu_SRCS dbusmenuqt/com.canonical.dbusmenu.xml dbusmenu_interface)
add_library(lumendbusmenuqt STATIC ${dbusmenu_SRCS})
target_include_directories(lumendbusmenuqt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dbusmenuqt ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(lumendbusmenuqt PUBLIC Qt::DBus Qt::Widgets)
set_property(TARGET lumendbusmenuqt PROPERTY POSITION_INDEPENDENT_CODE ON)

set(tray_SRCS
    src/dbusserviceobserver.cpp
    src/plasmoidregistry.cpp
    src/statusnotifieritemhost.cpp
    src/statusnotifieritemsource.cpp
    src/systemtraysettings.cpp
    src/systemtraytypes.cpp
    src/systemtraymodel.cpp
    src/sortedsystemtraymodel.cpp
)
qt_add_dbus_interface(tray_SRCS ${KSTATUSNOTIFIERITEM_DBUS_INTERFACES_DIR}/kf6_org.kde.StatusNotifierWatcher.xml statusnotifierwatcher_interface)
qt_add_dbus_interface(tray_SRCS dbus/org.freedesktop.DBus.Properties.xml dbusproperties)
set(statusnotifieritem_xml ${KSTATUSNOTIFIERITEM_DBUS_INTERFACES_DIR}/kf6_org.kde.StatusNotifierItem.xml)
set_source_files_properties(${statusnotifieritem_xml} PROPERTIES
    NO_NAMESPACE false
    INCLUDE "systemtraytypes.h"
    CLASSNAME OrgKdeStatusNotifierItem
)
qt_add_dbus_interface(tray_SRCS ${statusnotifieritem_xml} statusnotifieritem_interface)

add_library(lumentray_static STATIC ${tray_SRCS})
target_include_directories(lumentray_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(lumentray_static
    PUBLIC
        Qt::DBus
        Qt::Gui
        Qt::GuiPrivate
        Qt::Quick
        KF6::ConfigCore
        KF6::IconThemes
        KF6::I18n
        KF6::KIOGui
        KF6::JobWidgets
        KF6::Service
        KF6::ItemModels
        KF6::WindowSystem
        KF6::XmlGui
        Plasma::Plasma
        Plasma::PlasmaQuick
        lumendbusmenuqt
)
set_property(TARGET lumentray_static PROPERTY POSITION_INDEPENDENT_CODE ON)

# Shared "debug.h" for both forked trees.
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug.h CONTENT "#pragma once\n#include <QLoggingCategory>\nQ_DECLARE_LOGGING_CATEGORY(SYSTEM_TRAY)\nQ_DECLARE_LOGGING_CATEGORY(DBUSMENUQT)\n")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug.cpp CONTENT "#include \"debug.h\"\nQ_LOGGING_CATEGORY(SYSTEM_TRAY, \"lumen.tray\", QtInfoMsg)\nQ_LOGGING_CATEGORY(DBUSMENUQT, \"lumen.tray.dbusmenu\", QtInfoMsg)\n")
target_sources(lumendbusmenuqt PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/debug.cpp)

set_source_files_properties(${LUMEN_GEN_ROOT}/qml/LumenTokens.qml PROPERTIES GENERATED TRUE)

plasma_add_applet(org.lumen.tray
    CPP_SOURCES
        src/systemtray.cpp
    QML_SOURCES
        qml/main.qml
        qml/TrayState.qml
        qml/TrayItem.qml
        qml/ItemLoader.qml
        qml/PlasmoidTrayItem.qml
        qml/SniTrayItem.qml
        qml/TrayIcon.qml
        qml/BackgroundAppTrayItem.qml
        qml/CompactApplet.qml
        qml/OverflowChevron.qml
        qml/OverflowFlyout.qml
        qml/QuickSettingsButton.qml
        qml/IconReorderArea.qml
        qml/StatusGlyph.qml
        qml/FlyoutContent.qml
        qml/QuickSettings.qml
        qml/QuickTile.qml
        qml/QuickSlider.qml
        qml/AppletPage.qml
        qml/QsWifi.qml
        qml/QsBluetooth.qml
        qml/QsAirplane.qml
        qml/QsPowerProfile.qml
        qml/QsNightLight.qml
        qml/QsDnd.qml
        qml/QsDarkMode.qml
        qml/QsVolume.qml
        qml/QsBrightness.qml
        qml/QsMedia.qml
        qml/QsBattery.qml
        qml/config.qml
        qml/ConfigEntries.qml
        ${LUMEN_GEN_ROOT}/qml/LumenTokens.qml
    RESOURCES
        src/main.xml
)
target_link_libraries(org.lumen.tray PRIVATE lumentray_static)
