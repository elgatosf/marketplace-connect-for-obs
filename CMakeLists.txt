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
  find_qt(COMPONENTS Widgets Core Concurrent)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets Qt::Concurrent)
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

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-module.cpp
          src/elgato-styles.hpp
          src/zip_file.hpp
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
          src/elgato-widgets.cpp
          src/elgato-widgets.hpp)

add_executable(${CMAKE_PROJECT_NAME}-loader loader/main.cpp)
install(TARGETS ${CMAKE_PROJECT_NAME}-loader RUNTIME DESTINATION data/obs-plugins/${CMAKE_PROJECT_NAME})

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
