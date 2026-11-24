find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Qml)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS KIO Service)
include(ECMQmlModule)

ecm_add_qml_module(lumennotifications URI "org.lumen.notifications" GENERATE_PLUGIN_SOURCE)
target_sources(lumennotifications PRIVATE notificationstore.cpp notificationlist.cpp)
target_link_libraries(lumennotifications PRIVATE Qt6::Qml KF6::KIOGui KF6::Service)
ecm_finalize_qml_module(lumennotifications DESTINATION ${KDE_INSTALL_QMLDIR})
