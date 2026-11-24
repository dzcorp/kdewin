# Lumen fork of Dolphin (upstream version in UPSTREAM). Only the app is built: the stock package keeps
# providing libdolphinvcs (VCS plugins link it), the part, the KCMs, kcfg files and translations.
set(RELEASE_SERVICE_VERSION "26.08.2")
set(LUMEN_DOLPHIN_BINDIR "/usr/local/bin" CACHE PATH "Where the Lumen Dolphin binary shadows the stock one")
set(LUMEN_DOLPHIN_APPDIR "/usr/local/share/applications" CACHE PATH "Where the Lumen launcher shadows the stock one")

include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(GenerateExportHeader)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(KDEInstallDirs)

ecm_setup_version(${RELEASE_SERVICE_VERSION} VARIABLE_PREFIX DOLPHIN VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/dolphin_version.h")

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Concurrent Multimedia MultimediaWidgets WaylandClient)
find_package(Wayland REQUIRED COMPONENTS Client)
find_package(Qt6GuiPrivate ${QT_MIN_VERSION} REQUIRED NO_MODULE)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    KCMUtils NewStuff CoreAddons I18n DBusAddons Bookmarks Config KIO Parts Solid IconThemes Completion
    TextWidgets Notifications Crash WindowSystem WidgetsAddons Codecs GuiAddons ColorScheme FileMetaData Baloo BalooWidgets)
find_package(KF6UserFeedback ${KF_MIN_VERSION})
find_package(PackageKitQt6 QUIET)
find_package(DolphinVcs REQUIRED)

set(HAVE_BALOO TRUE)
set(HAVE_TERMINAL TRUE)
set(HAVE_X11 TRUE)
if(KF6UserFeedback_FOUND)
    set(HAVE_KUSERFEEDBACK TRUE)
endif()
if(PackageKitQt6_FOUND)
    set(HAVE_PACKAGEKIT TRUE)
endif()

check_include_files("sys/types.h;sys/stat.h;fts.h" HAVE_FTS_H)
set(FTS_LIB "")

add_subdirectory(src)
