find_package(KDecoration3 REQUIRED)

add_library(org.lumen.decoration MODULE lumendecoration.cpp lumenbutton.cpp)
target_link_libraries(org.lumen.decoration PRIVATE lumencommon KDecoration3::KDecoration Qt6::Core Qt6::Gui KF6::CoreAddons KF6::ConfigCore)
install(TARGETS org.lumen.decoration DESTINATION ${KDE_INSTALL_PLUGINDIR}/org.kde.kdecoration3)
