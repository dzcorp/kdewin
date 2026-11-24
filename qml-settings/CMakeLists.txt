find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Qml)
include(ECMQmlModule)

ecm_add_qml_module(lumenqmlsettings URI "org.lumen.settings" GENERATE_PLUGIN_SOURCE)
target_sources(lumenqmlsettings PRIVATE stylesettings.cpp stylesettings.h)
target_link_libraries(lumenqmlsettings PRIVATE lumencommon Qt6::Qml)
ecm_finalize_qml_module(lumenqmlsettings DESTINATION ${KDE_INSTALL_QMLDIR})
