add_library(lumen6 MODULE
    lumenstyle.cpp
    lumenstyle_draw.cpp
    lumenanimations.cpp
    lumenshadowhelper.cpp
    lumenwindowdrag.cpp
    lumenwindowmaterial.cpp
    lumenplugin.cpp
)
target_link_libraries(lumen6 PRIVATE lumencommon Qt6::Core Qt6::Gui Qt6::Widgets KF6::WindowSystem)
install(TARGETS lumen6 DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/styles)
install(FILES lumen.themerc DESTINATION ${KDE_INSTALL_DATADIR}/kstyle/themes)
