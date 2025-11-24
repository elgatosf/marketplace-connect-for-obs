cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_qt(COMPONENTS Widgets Core Concurrent Svg)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets Qt::Concurrent Qt::Svg)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)

### --- libzip (static) -----------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libs" FORCE)
set(DO_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_BZIP2 OFF CACHE BOOL "" FORCE)
set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_OSSFUZZ OFF CACHE BOOL "" FORCE)
set(BUILD_REGRESS OFF CACHE BOOL "" FORCE)

# Add libzip
add_subdirectory(deps/libzip)

if(MSVC AND TARGET zip)
    target_compile_options(zip PRIVATE /wd4244)
endif()

# Add libzip include directory
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/libzip/lib
    ${CMAKE_CURRENT_BINARY_DIR}/deps/libzip  # for config headers like zipconf.h
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl nlohmann_json::nlohmann_json ZLIB::ZLIB zip)
target_link_options(zip PRIVATE $<$<CONFIG:DEBUG>:/NODEFAULTLIB:LIBCMT>)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-module.cpp
          src/elgato-styles.hpp
          src/zip_file.hpp
          src/zip-handle.hpp
          src/zip-archive.cpp
          src/zip-archive.hpp
          src/api.cpp
          src/api.hpp
          src/platform.cpp
          src/platform.h
          src/util.cpp
          src/util.h
          src/plugins.cpp
          src/plugins.hpp
          src/obs-utils.cpp
          src/obs-utils.hpp
          src/display-helpers.hpp
          src/qt-display.cpp
          src/qt-display.hpp
          src/downloader.cpp
          src/downloader.h
          src/flowlayout.cpp
          src/flowlayout.h
          src/scene-bundle.cpp
          src/scene-bundle.hpp
          src/setup-wizard.cpp
          src/setup-wizard.hpp
          src/scene-collection-info.cpp
          src/scene-collection-info.hpp
          src/export-wizard.cpp
          src/export-wizard.hpp
          src/elgato-product.cpp
          src/elgato-product.hpp
          src/elgato-cloud-data.cpp
          src/elgato-cloud-data.hpp
          src/elgato-cloud-config.cpp
          src/elgato-cloud-config.hpp
          src/elgato-cloud-window.cpp
          src/elgato-cloud-window.hpp
          src/elgato-stream-deck-widgets.cpp
          src/elgato-stream-deck-widgets.hpp
          src/elgato-update-modal.cpp
          src/elgato-update-modal.hpp
          src/elgato-widgets.cpp
          src/elgato-widgets.hpp)

add_executable(${CMAKE_PROJECT_NAME}-loader loader/main.cpp)
install(TARGETS ${CMAKE_PROJECT_NAME}-loader RUNTIME DESTINATION helper-app)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
