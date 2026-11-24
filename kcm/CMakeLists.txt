find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS KCMUtils Config CoreAddons I18n)
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_lumen\")

set_source_files_properties(${LUMEN_GEN_ROOT}/kcfg/lumensettings.kcfgc PROPERTIES GENERATED TRUE)
kcmutils_add_qml_kcm(kcm_lumen SOURCES lumenkcm.cpp lumenkcm.h)
kconfig_add_kcfg_files(kcm_lumen GENERATE_MOC ${LUMEN_GEN_ROOT}/kcfg/lumensettings.kcfgc)
target_include_directories(kcm_lumen PRIVATE ${LUMEN_GEN_DIR})
target_link_libraries(kcm_lumen PRIVATE KF6::KCMUtilsQuick KF6::ConfigCore KF6::ConfigGui KF6::CoreAddons KF6::I18n Qt6::DBus)
set_property(SOURCE lumenkcm.cpp APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kcm_lumen.json)
