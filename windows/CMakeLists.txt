# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "webview_cef")
project(${PROJECT_NAME} LANGUAGES CXX)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  message(STATUS "${CMAKE_BUILD_TYPE}")
endif()

# wait for unified version cef source code and prebuilt files
include(${CMAKE_CURRENT_SOURCE_DIR}/../third/download.cmake)
prepare_local_file(${CMAKE_CURRENT_SOURCE_DIR}/../third/prebuilts)

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
set(USE_SANDBOX FALSE)
set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING "" FORCE)
find_package(CEF REQUIRED)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "webview_cef_plugin")

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "webview_cef_plugin.cpp"
  "webview_cef_plugin.h"
  "webview_cef_keyevent.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_app.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_app.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_handler.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_handler.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_plugin.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_plugin.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_value.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_value.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_js_handler.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_js_handler.h"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_cookieVisitor.cc"
  "${CMAKE_CURRENT_LIST_DIR}/../common/webview_cookieVisitor.h"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/webview_cef/webview_cef_plugin_c_api.h"
  "webview_cef_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
)

set(CEF_TARGET ${PLUGIN_NAME})
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
SET_CEF_TARGET_OUT_DIR()
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
add_dependencies(${CEF_TARGET} libcef_dll_wrapper)

target_link_libraries(${CEF_TARGET} PRIVATE flutter flutter_wrapper_plugin
  PUBLIC libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL
  PUBLIC NOMINMAX)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME}
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third/cef)

PRINT_CEF_CONFIG()

set(cef_library_list "")

foreach(FILE ${CEF_BINARY_FILES})
  list(APPEND cef_library_list ${CEF_BINARY_DIR}/${FILE})
endforeach()

foreach(FILE ${CEF_RESOURCE_FILES})
  list(APPEND cef_library_list ${CEF_RESOURCE_DIR}/${FILE})
endforeach(FILE)

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(webview_cef_bundled_libraries
  ""
  ${cef_library_list}
  PARENT_SCOPE
)