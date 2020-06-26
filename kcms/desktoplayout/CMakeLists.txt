# KI18N Translation Domain for this library
add_definitions(-DTRANSLATION_DOMAIN=\"kcm_desktoplayout\")

if(X11_Xcursor_FOUND)
    set(HAVE_XCURSOR TRUE)
endif()
configure_file (config-kcm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kcm.h )

set(kcm_desktoplayout_SRCS
    kcmmain.cpp
    kcm.cpp
    ../krdb/krdb.cpp
)

if (X11_Xcursor_FOUND)
    set(kcm_desktoplayout_SRCS
        ${kcm_desktoplayout_SRCS}
        ../cursortheme/xcursor/cursortheme.cpp
        ../cursortheme/xcursor/xcursortheme.cpp
    )
endif ()

kconfig_add_kcfg_files(kcm_desktoplayout_SRCS desktoplayoutsettings.kcfgc GENERATE_MOC)

add_library(kcm_desktoplayout MODULE ${kcm_desktoplayout_SRCS})

target_link_libraries(kcm_desktoplayout
  KF5::KIOWidgets
  KF5::CoreAddons
  KF5::GuiAddons
  KF5::KCMUtils
  KF5::I18n
  KF5::Plasma
  KF5::PlasmaQuick
  KF5::KDELibs4Support
  KF5::Declarative
  KF5::QuickAddons
  PW::KWorkspace

  Qt5::DBus
  Qt5::Widgets
  Qt5::QuickWidgets
  ${X11_LIBRARIES}
)

if(X11_FOUND)
    target_link_libraries(kcm_desktoplayout Qt5::X11Extras)
endif()
if (X11_Xcursor_FOUND)
   target_link_libraries(kcm_desktoplayout ${X11_Xcursor_LIB})
endif ()
if (X11_Xfixes_FOUND)
   target_link_libraries(kcm_desktoplayout ${X11_Xfixes_LIB})
endif ()

kcoreaddons_desktop_to_json(kcm_desktoplayout "kcm_desktoplayout.desktop" SERVICE_TYPES kcmodule.desktop)

#this desktop file is installed only for retrocompatibility with sycoca
install(FILES desktoplayoutsettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES kcm_desktoplayout.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(TARGETS kcm_desktoplayout DESTINATION ${KDE_INSTALL_PLUGINDIR}/kcms)

if(BUILD_TESTING)
   find_package(Qt5Test ${QT_MIN_VERSION} CONFIG REQUIRED)
   add_subdirectory(autotests)
endif()

kpackage_install_package(package kcm_desktoplayout kcms)

###### Command line tool

set(desktoplayouttool_SRCS
    dltool.cpp
    # TODO: load kcm plugin instead of using code copy
    kcm.cpp
    ../krdb/krdb.cpp
)

if (X11_Xcursor_FOUND)
    set(desktoplayouttool_SRCS
        ${desktoplayouttool_SRCS}
        ../cursortheme/xcursor/cursortheme.cpp
        ../cursortheme/xcursor/xcursortheme.cpp
    )
endif ()

kconfig_add_kcfg_files(desktoplayouttool_SRCS desktoplayoutsettings.kcfgc GENERATE_MOC)

add_executable(desktoplayouttool ${desktoplayouttool_SRCS})

target_link_libraries(desktoplayouttool
  KF5::KIOWidgets
  KF5::CoreAddons
  KF5::KCMUtils
  KF5::I18n
  #TODO:kpackage
  KF5::Plasma
  KF5::PlasmaQuick
  KF5::KDELibs4Support
  KF5::Declarative
  KF5::QuickAddons
  KF5::NewStuff
  PW::KWorkspace

  Qt5::DBus
  Qt5::Widgets
  Qt5::QuickWidgets
  ${X11_LIBRARIES}
)

if(X11_FOUND)
    target_link_libraries(desktoplayouttool Qt5::X11Extras)
endif()
if (X11_Xcursor_FOUND)
   target_link_libraries(desktoplayouttool ${X11_Xcursor_LIB})
endif ()
if (X11_Xfixes_FOUND)
   target_link_libraries(desktoplayouttool ${X11_Xfixes_LIB})
endif ()

install( TARGETS desktoplayouttool ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )

