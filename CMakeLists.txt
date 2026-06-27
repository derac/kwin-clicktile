cmake_minimum_required(VERSION 3.16)

project(kwin_clicktile VERSION 0.6.6)

set(QT_MIN_VERSION "6.6.0")
set(QT_MAJOR_VERSION 6)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 REQUIRED COMPONENTS Config ConfigWidgets CoreAddons KCMUtils)
find_package(KWin REQUIRED COMPONENTS kwineffects)

set(kwin_clicktile_SOURCES
    src/effect.cpp
    src/effect.h
    src/input.cpp
    src/input_filter.cpp
    src/input_filter.h
    src/log.cpp
    src/overlay.cpp
    src/settings.cpp
    src/settings.h
    src/tiles.cpp
    src/main.cpp
)

add_library(kwin_clicktile MODULE ${kwin_clicktile_SOURCES})
set_property(TARGET kwin_clicktile APPEND PROPERTY
    AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED
)
target_link_libraries(kwin_clicktile PRIVATE
    KWin::kwin
    Qt6::Core
    Qt6::Gui
    KF6::ConfigCore
)
target_compile_options(kwin_clicktile PRIVATE -Wall -Wextra -Wpedantic)

install(
    TARGETS kwin_clicktile
    DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins
)

set(kwin_clicktile_config_SOURCES
    src/settings.cpp
    src/settings.h
    kcm/configmodule.cpp
)

add_library(kwin_clicktile_config MODULE ${kwin_clicktile_config_SOURCES})
target_include_directories(kwin_clicktile_config PRIVATE src)
target_link_libraries(kwin_clicktile_config PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::CoreAddons
    KF6::KCMUtils
)
target_compile_options(kwin_clicktile_config PRIVATE -Wall -Wextra -Wpedantic)

install(
    TARGETS kwin_clicktile_config
    DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs
)
