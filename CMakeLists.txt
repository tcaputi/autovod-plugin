cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

# is this the best way to do this?
# why does leptonica not find the regular 'include' dir?
# why do i get warnings about libarchive search path not found
find_package(PkgConfig REQUIRED)

# Locate Tesseract and Leptonica using pkg-config
find_package(PNG REQUIRED)
pkg_search_module(TESSERACT REQUIRED tesseract)
pkg_search_module(LEPTONICA REQUIRED lept)

# Include and link directories
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${TESSERACT_INCLUDE_DIRS})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LEPTONICA_INCLUDE_DIRS}/../)
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${TESSERACT_LIBRARY_DIRS})
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${LEPTONICA_LIBRARY_DIRS})

message("-------------------------------------------------------")
message("TESSERACT_INCLUDE_DIRS: ${TESSERACT_INCLUDE_DIRS}")
message("TESSERACT_LIBRARY_DIRS: ${TESSERACT_LIBRARY_DIRS}")
message("TESSERACT_LIBRARIES: ${TESSERACT_LIBRARIES}")
message("")
message("LEPTONICA_INCLUDE_DIRS: ${LEPTONICA_INCLUDE_DIRS}")
message("LEPTONICA_LIBRARY_DIRS: ${LEPTONICA_LIBRARY_DIRS}")
message("LEPTONICA_LIBRARIES: ${LEPTONICA_LIBRARIES}")
message("-------------------------------------------------------")

find_package(libobs REQUIRED)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    PRIVATE OBS::libobs
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
    PNG::PNG)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_qt(COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  src/game-detect/smash-ultimate.c
  src/img-utils.c 
  src/ocr.c 
  src/plugin-main.c 
  src/string-utils.c)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
