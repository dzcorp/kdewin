add_library(lumencommon STATIC lumencolor.cpp lumenshadow.cpp lumenconfig.cpp lumenglyph.cpp)
set_target_properties(lumencommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lumencommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${LUMEN_GEN_DIR})
target_link_libraries(lumencommon PUBLIC Qt6::Gui KF6::ConfigCore)
