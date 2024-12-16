cmake_minimum_required(VERSION 3.16...3.26)

# probably best to move this to a seperate file
function(setup_obs_lib_dependency target)
  if(BUILD_OUT_OF_TREE)
    find_package(libobs)
    if(libobs_FOUND AND NOT LIBOBS_LIB)
      target_link_libraries(${target} PUBLIC OBS::libobs)
    else()
      if(NOT LIBOBS_LIB)
        message(FATAL_ERROR "obs library not found - please set LIBOBS_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_LIB})
      if(NOT LIBOBS_INCLUDE_DIR)
        message(FATAL_ERROR "obs.hpp header not found - please set LIBOBS_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_INCLUDE_DIR})
    endif()
    find_package(obs-frontend-api)
    if(obs-frontend-api_FOUND AND NOT LIBOBS_FRONTEND_API_LIB)
      target_link_libraries(${target} PUBLIC OBS::obs-frontend-api)
    else()
      if(NOT LIBOBS_FRONTEND_API_LIB)
        message(FATAL_ERROR "libobs frontend-api library not found - please set LIBOBS_FRONTEND_API_LIB")
      endif()
      target_link_libraries(${target} PUBLIC ${LIBOBS_FRONTEND_API_LIB})
      if(NOT LIBOBS_FRONTEND_INCLUDE_DIR)
        message(FATAL_ERROR " obs-frontend-api.h not found - please set LIBOBS_FRONTEND_INCLUDE_DIR")
      endif()
      target_include_directories(${target} PRIVATE ${LIBOBS_FRONTEND_INCLUDE_DIR})
    endif()
  else()
    target_link_libraries(${target} PUBLIC OBS::libobs OBS::frontend-api)
  endif()
endfunction()

project(elgato-marketplace VERSION 0.0.3)

if(${CMAKE_PROJECT_NAME} STREQUAL "obs-studio")
  set(OBS_FRONTEND_API_NAME "frontend-api")
  if(NOT DEFINED BUILD_OUT_OF_TREE)
    message(STATUS "${PROJECT_NAME} configured for in-tree build")
  endif()
else()
  set(BUILD_OUT_OF_TREE ON)
  set(OBS_FRONTEND_API_NAME "obs-frontend-api")
  message(STATUS "${PROJECT_NAME} configured for out-of-tree build")
endif()

if(BUILD_OUT_OF_TREE)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
  include(compilerconfig)
  include(defaults)
endif()
include(helpers)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

add_library(${PROJECT_NAME} MODULE)

if(BUILD_OUT_OF_TREE)
  find_package(${OBS_FRONTEND_API_NAME} REQUIRED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::${OBS_FRONTEND_API_NAME})

find_package(Qt6 COMPONENTS Widgets Core Concurrent)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Concurrent)
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

find_package(CURL REQUIRED)
# find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

target_sources(
  ${PROJECT_NAME}
  PRIVATE src/plugin-module.cpp
          src/elgato-styles.hpp
          src/zip_file.hpp
          src/api.cpp
          src/api.hpp
          src/plugin-support.c
          src/plugin-support.h
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

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(${PROJECT_NAME}-loader loader/main.cpp)

install(TARGETS ${PROJECT_NAME}-loader RUNTIME DESTINATION data/obs-plugins/${PROJECT_NAME})

setup_obs_lib_dependency(${PROJECT_NAME})
if(BUILD_OUT_OF_TREE)
  set_target_properties_plugin(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
else()
  set_target_properties_obs(${PROJECT_NAME} PROPERTIES FOLDER "plugins/elgato" PREFIX "")
endif()

if(BUILD_OUT_OF_TREE)
  if(OS_WINDOWS)
    set(OBS_PLUGIN_DESTINATION "obs-plugins/64bit")
  else()
    set(OBS_PLUGIN_DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
  endif()
endif()
