# Forked from plasma-workspace 6.8.3 applets/notifications (GPL-2.0-or-later): notification server host with Windows-style toasts.
enable_language(C) # wayland-scanner glue
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Quick Qml WaylandClient)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS ConfigWidgets CoreAddons GlobalAccel I18n KIO Notifications Service)
find_package(Plasma REQUIRED)
find_package(PlasmaQuick REQUIRED)
find_package(LibNotificationManager CONFIG REQUIRED)
find_package(PlasmaWaylandProtocols REQUIRED)
find_package(Wayland REQUIRED COMPONENTS Client)
include(ECMQtDeclareLoggingCategory)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.lumen.notifier\")

set(notificationapplet_SRCS
    notificationapplet.cpp
    notificationwindow.cpp
    draghelper.cpp
    fileinfo.cpp
    filemenu.cpp
    globalshortcuts.cpp
    jobaggregator.cpp
    thumbnailer.cpp
    notificationapplet.h
    notificationwindow.h
    draghelper.h
    fileinfo.h
    filemenu.h
    globalshortcuts.h
    jobaggregator.h
    thumbnailer.h
)

plasma_add_applet(org.lumen.notifier QML_ARGS DEPENDENCIES QtCore QtQuick org.kde.plasma.core)

target_sources(org.lumen.notifier PRIVATE ${notificationapplet_SRCS})

ecm_target_qml_sources(org.lumen.notifier SOURCES
    components/ActionContainer.qml
    components/Body.qml
    components/DraggableFileArea.qml
    components/EditContextMenu.qml
    components/FooterLoader.qml
    components/HeadingButtons.qml
    components/Icon.qml
    components/JobDetails.qml
    components/JobIconItem.qml
    components/JobItem.qml
    components/ModelInterface.qml
    components/NotificationHeader.qml
    components/NotificationReplyField.qml
    components/SpeedChart.qml
    components/Summary.qml
    components/ThumbnailStrip.qml
    PATH components
)

ecm_target_qml_sources(org.lumen.notifier SOURCES
    delegates/BaseDelegate.qml
    delegates/DelegateHistoryGrouped.qml
    delegates/DelegateHistory.qml
    delegates/DelegatePopup.qml
    PATH delegates
)

set_source_files_properties(global/Globals.qml PROPERTIES QT_QML_SINGLETON_TYPE ON)

ecm_target_qml_sources(org.lumen.notifier SOURCES
    global/Globals.qml
    global/PulseAudio.qml
    PATH global
)

set_source_files_properties(${LUMEN_GEN_ROOT}/qml/LumenTokens.qml PROPERTIES GENERATED TRUE QT_RESOURCE_ALIAS LumenTokens.qml)
ecm_target_qml_sources(org.lumen.notifier SOURCES ${LUMEN_GEN_ROOT}/qml/LumenTokens.qml)

ecm_target_qml_sources(org.lumen.notifier SOURCES
    CompactRepresentation.qml
    DraggableDelegate.qml
    FullRepresentation.qml
    main.qml
    NotificationPopup.qml
)

qt_generate_wayland_protocol_client_sources(org.lumen.notifier
    PRIVATE_CODE
    FILES
        "${PLASMA_WAYLAND_PROTOCOLS_DIR}/plasma-shell.xml"
        "${Wayland_DATADIR}/wayland.xml"
)

target_link_libraries(org.lumen.notifier PRIVATE
                      Qt::Gui
                      Qt::Quick # for QQmlParserStatus
                      KF6::ConfigGui # for KStandardActions
                      KF6::I18n
                      Plasma::Plasma
                      Plasma::PlasmaQuick
                      KF6::CoreAddons
                      KF6::GlobalAccel
                      KF6::KIOGui
                      KF6::KIOWidgets # for PreviewJob
                      KF6::Notifications # for KNotificationJobUiDelegate
                      KF6::Service # KSycoca
                      PW::LibNotificationManager
                      Wayland::Client
                      )

ecm_qt_declare_logging_category(org.lumen.notifier
    HEADER notifications_debug.h
    IDENTIFIER PLASMA_APPLET_NOTIFICATIONS_DEBUG
    CATEGORY_NAME lumen.notifier
    DESCRIPTION "Lumen notifications"
)
