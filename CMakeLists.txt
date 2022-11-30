cmake_minimum_required(VERSION 3.12)

project(
    gstvimbaxsrc
    VERSION 0.0.1
    LANGUAGES C
    DESCRIPTION "GStreamer source plugin for VimbaX"
    HOMEPAGE_URL "https://alliedvision.com/"
)

# Turn on compiler warnings and treat them as errors
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# add local cmake modules to simplify detection of dependencies
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

add_library(${PROJECT_NAME} SHARED
    src/gstvimbaxsrc.c
    src/vimba_helpers.c
    src/pixelformats.c
)

# Defines used in gstplugin.c
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        HAVE_CONFIG_H
)

# fill configuration file with defined values
configure_file(
    ${PROJECT_SOURCE_DIR}/src/config.h.in config.h
)

find_package(GStreamer REQUIRED COMPONENTS base video)
find_package(GLIB2 REQUIRED)
find_package(GObject REQUIRED)
find_package(Vmb REQUIRED COMPONENTS C)

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
    ${GSTREAMER_VIDEO_LIBRARY}
    Vmb::C
)

install(
    TARGETS ${PROJECT_NAME}
)
