# Forked from plasma-desktop 6.8.2 applets/taskmanager (GPL-2.0-or-later): icons-only taskbar with an explicit icon size.
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Quick Qml)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS GlobalAccel I18n KIO Notifications XmlGui Service WindowSystem Config)
find_package(Plasma REQUIRED)
find_package(PlasmaActivities REQUIRED)
find_package(PlasmaActivitiesStats REQUIRED)
find_package(KSysGuard CONFIG REQUIRED)
find_package(LibNotificationManager CONFIG REQUIRED)
include(ECMQtDeclareLoggingCategory)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma_applet_org.lumen.tasks\")

plasma_add_applet(org.lumen.tasks
    QML_SOURCES
        qml/AudioStream.qml
        qml/ConfigAppearance.qml
        qml/ConfigBehavior.qml
        qml/config.qml
        qml/ContextMenu.qml
        qml/GroupDialog.qml
        qml/GroupExpanderOverlay.qml
        qml/main.qml
        qml/MouseHandler.qml
        qml/PipeWireThumbnail.qml
        qml/PlayerController.qml
        qml/PulseAudio.qml
        qml/ScrollableTextWrapper.qml
        qml/TaskBadgeOverlay.qml
        qml/TaskList.qml
        qml/TaskProgressOverlay.qml
        qml/Task.qml
        qml/ToolTipDelegate.qml
        qml/ToolTipInstance.qml
        qml/ToolTipWindowMouseArea.qml
        qml/code/LayoutMetrics.js
        qml/code/TaskTools.js
    CPP_SOURCES
        backend.cpp
        smartlauncherbackend.cpp
        smartlauncheritem.cpp
    RESOURCES
        main.xml
    GENERATE_APPLET_CLASS
)

kconfig_add_kcfg_files(org.lumen.tasks kactivitymanagerd_plugins_settings.kcfgc)

ecm_qt_declare_logging_category(org.lumen.tasks
    HEADER log_settings.h
    IDENTIFIER TASKMANAGER_DEBUG
    CATEGORY_NAME org.lumen.tasks)

target_link_libraries(org.lumen.tasks PRIVATE
    Qt::Core
    Qt::Qml
    Qt::Quick
    Plasma::Activities
    Plasma::ActivitiesStats
    KF6::ConfigGui
    KF6::GlobalAccel
    KF6::I18n
    KF6::KIOCore
    KF6::KIOGui
    KF6::KIOFileWidgets
    KF6::Notifications
    KF6::XmlGui
    KSysGuard::ProcessCore
    KF6::Service
    KF6::WindowSystem
    PW::LibNotificationManager)
