cmake_minimum_required(VERSION ${CMAKE_VERSION})

project(westeros_gl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra  -Wno-unused-parameter -Wno-unknown-pragmas -Wno-write-strings")

find_package(PkgConfig REQUIRED)
pkg_search_module(GLESv2 glesv2 >= 0.0)
pkg_search_module(EGL egl >= 0.0)

#############################################################################
# planecontrol_aidl static library (this client-side library would be
# better done as a separate component that is added as a bitbake DEPENDS)

set(PLANECONTROL_MODULE_NAME planecontrol_aidl)

message(STATUS "creating planecontrol_aidl static library")

aux_source_directory(${AIDL_GEN_DIR}/cpp/com/rdk/hal/planecontrol AIDL_PLANECONTROL_SOURCES)

add_library(planecontrol_aidl STATIC
    ${AIDL_PLANECONTROL_SOURCES}
    ${AIDL_GEN_DIR}/cpp/com/rdk/hal/PropertyValue.cpp
)

target_link_libraries(planecontrol_aidl PRIVATE
    binder
    utils_amlhal
)

target_include_directories(planecontrol_aidl PRIVATE
    ${AIDL_GEN_DIR}/h
)

############################################################################
# westeros-gl shared library
#

message(STATUS "creating westeros_gl_rdk shared library")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src WESTEROS_GL_SOURCES)

add_library(westeros_gl_rdk SHARED
    ${WESTEROS_GL_SOURCES}
)

set_target_properties(westeros_gl_rdk PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_link_libraries(westeros_gl_rdk PUBLIC
    ${GLESv2_LIBRARIES}
    ${EGL_LIBRARIES}
)

target_link_libraries(westeros_gl_rdk PRIVATE
    planecontrol_aidl
)

target_include_directories(westeros_gl_rdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(westeros_gl_rdk PRIVATE
    ${AIDL_GEN_DIR}/h
)

if(PLATFORM STREQUAL "DRM")

    message(WARNING "ADDING VENDOR DEPENDENCY LIBDRM TO MIDDLEWARE COMPONENT")

    target_compile_definitions(westeros_gl_rdk PRIVATE PLATFORM_DRM)
    
    target_link_libraries(westeros_gl_rdk PUBLIC
        ${DRM_LIBRARIES}
        gbm
    )

    target_include_directories(westeros_gl_rdk PRIVATE
        ${DRM_INCLUDE_DIRS}
    )

endif()

install(TARGETS westeros_gl_rdk LIBRARY)