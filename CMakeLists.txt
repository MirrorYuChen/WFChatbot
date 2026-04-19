cmake_minimum_required(VERSION 3.1...3.29)

project(WFChatbot LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(INC_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/source)
set(3RD_DIR ${CMAKE_CURRENT_LIST_DIR}/3rdLibs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build Shared library" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

include_directories(
  ${INC_DIR}
  ${SRC_DIR}
  ${3RD_DIR}/spdlog/include
)

# src files
file(GLOB_RECURSE SRC_FILES
  ${SRC_DIR}/*.h
  ${SRC_DIR}/*.cc
  ${SRC_DIR}/*.c
)

source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${SRC_FILES})

set(LIBRARIES_TO_LINK "")
find_package(Threads REQUIRED)
list(APPEND LIBRARIES_TO_LINK Threads::Threads)

find_library(LIBRT rt)
find_package(OpenSSL REQUIRED)
find_package(workflow REQUIRED CONFIG HINTS ..)
find_path(HIREDIS_HEADER hiredis/hiredis.h)
find_library(HIREDIS_LIB hiredis)
find_package(ZLIB REQUIRED)
include_directories(
  ${OPENSSL_INCLUDE_DIR}
  ${WORKFLOW_INCLUDE_DIR}
  ${HIREDIS_HEADER}
  ${ZLIB_INCLUDE_DIRS}
)
list(APPEND LIBRARIES_TO_LINK workflow ZLIB::ZLIB OpenSSL::SSL OpenSSL::Crypto ${HIREDIS_LIB})

if (BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} SHARED ${SRC_FILES} ${OBJECTS_TO_LINK})
else (BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME} STATIC ${SRC_FILES} ${OBJECTS_TO_LINK})
endif (BUILD_SHARED_LIBS)
target_link_libraries(${PROJECT_NAME} ${LIBRARIES_TO_LINK})

if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  target_compile_options(${PROJECT_NAME} PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor;/Zc:__cplusplus>
    $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
    $<$<COMPILE_LANGUAGE:CXX>:/diagnostics:classic>
    $<$<COMPILE_LANGUAGE:CXX>:/wd4251>
  )
else(MSVC)
  add_definitions(-fPIC -mfma -Wno-unused-function -pedantic)
  target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fvisibility=hidden>)
  target_link_libraries(${PROJECT_NAME} dl)
endif (MSVC)

file(GLOB EXAMPLES_SRC "examples/*.cc")
foreach(EXAMPLE_FILE ${EXAMPLES_SRC})
	get_filename_component(EXE_NAME ${EXAMPLE_FILE} NAME_WE)
	add_executable(${EXE_NAME} ${EXAMPLE_FILE})
	target_link_libraries(${EXE_NAME} ${PROJECT_NAME})
endforeach()