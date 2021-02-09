cmake_minimum_required(VERSION 3.12)

project(
    gstvimbasrc
    VERSION 0.0.1
    LANGUAGES C
)

configure_file(
    ${PROJECT_SOURCE_DIR}/src/config.h.in config.h
)

# add local cmake modules to simplify detection of dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

add_library(${PROJECT_NAME} SHARED
    src/gstvimbasrc.c
)

# Defines used in gstplugin.c
target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        PACKAGE=\"${PROJECT_NAME}\"
        PACKAGE_VERSION=\"${PROJECT_VERSION}\"
        GST_LICENSE=\"LGPL\"
        GST_API_VERSION=\"1.14\"
        GST_PACKAGE_NAME=\"GStreamer_template_Plug-ins\"
        GST_PACKAGE_ORIGIN=\"https://gstreamer.freedesktop.org\"
    PRIVATE
        HAVE_CONFIG_H
)

find_package(GStreamer REQUIRED COMPONENTS base)
find_package(GLIB2 REQUIRED)
find_package(GObject REQUIRED)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_BINARY_DIR}
        ${GSTREAMER_INCLUDE_DIR}
        ${GLIB2_INCLUDE_DIR}
)

target_link_libraries (${PROJECT_NAME}
    ${GLIB2_LIBRARIES}
    ${GOBJECT_LIBRARIES}
    ${GSTREAMER_LIBRARY}
    ${GSTREAMER_BASE_LIBRARY}
)
