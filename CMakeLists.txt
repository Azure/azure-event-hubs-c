#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.5)
project(azure_event_hubs_c)

#the following variables are project-wide and can be used with cmake-gui
option(run_e2e_tests "set run_e2e_tests to ON to run e2e tests (default is OFF) [if possible, they are always build]" OFF)
option(run_unittests "set run_unittests to ON to run unittests (default is OFF)" OFF)
option(compileOption_C "passes a string to the command line of the C compiler" OFF)
option(compileOption_CXX "passes a string to the command line of the C++ compiler" OFF)
option(use_installed_dependencies "set use_installed_dependencies to ON to use installed packages instead of building dependencies from submodules" OFF)

function(getEHSDKVersion)
    # First find the applicable line in the file
    file (STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/eventhub_client/inc/version.h" ehsdkverstr
        REGEX "^[\t ]*#[\t ]*define[\t ]*EVENT_HUB_SDK_VERSION[\t ]*\"([0-9]+)[\\.]([0-9]+)[\\.]([0-9]+)\"") 
        
    if (!MATCHES)
        message(FATAL_ERROR "Unable to find version in ${CMAKE_SOURCE_DIR}/eventhub_client/inc/version.h")
    else(!MATCHES)
        # Parse out the three version identifiers
        set(CMAKE_MATCH_3 "")

        string(REGEX MATCH "^[\t ]*#[\t ]*define[\t ]*EVENT_HUB_SDK_VERSION[\t ]*\"([0-9]+)[\\.]([0-9]+)[\\.]([0-9]+)\"" temp "${ehsdkverstr}")

        if (NOT "${CMAKE_MATCH_3}" STREQUAL "")
            set (EVENTHUB_SDK_VERION_MAJOR "${CMAKE_MATCH_1}" PARENT_SCOPE)
            set (EVENTHUB_SDK_VERION_MINOR "${CMAKE_MATCH_2}" PARENT_SCOPE)
            set (EVENTHUB_SDK_VERION_FIX "${CMAKE_MATCH_3}" PARENT_SCOPE)
            set (EVENTHUB_SDK_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}" PARENT_SCOPE)
        else ()
            message(FATAL_ERROR "Unable to find version in ${ehsdkverstr}")
        endif()
    endif(!MATCHES)
endfunction(getEHSDKVersion)

getEHSDKVersion()
message(STATUS "EH Client SDK Version = ${EVENTHUB_SDK_VERSION}")

#Use solution folders. 
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set CMAKE_INSTALL_LIBDIR if not defined
include(GNUInstallDirs)

add_definitions(-DREFCOUNT_ATOMIC_DONTCARE)
add_definitions(-D__STDC_NO_ATOMICS__=1)

if(IN_OPENWRT)
    ADD_DEFINITIONS("$ENV{TARGET_LDFLAGS}" "$ENV{TARGET_CPPFLAGS}" "$ENV{TARGET_CFLAGS}")
    INCLUDE_DIRECTORIES("$ENV{TOOLCHAIN_DIR}/usr/include" "$ENV{TARGET_LDFLAGS}" "$ENV{TARGET_CPPFLAGS}" "$ENV{TARGET_CFLAGS}")
endif()

#do not add or build any tests of the dependencies
set(original_run_e2e_tests ${run_e2e_tests})
set(original_run_int_tests ${run_int_tests})
set(original_run_unittests ${run_unittests})

set(run_e2e_tests OFF)
set(run_int_tests OFF)
set(run_unittests OFF)

if(NOT ${use_installed_dependencies})
    if ((NOT TARGET azure_macro_utils_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-macro-utils-c/CMakeLists.txt))
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/azure-macro-utils-c)
    endif()
    if ((NOT TARGET umock_c) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/umock-c/CMakeLists.txt))
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/umock-c)
    endif()

    if(${original_run_e2e_tests} OR ${original_run_unittests})
        if ((NOT TARGET testrunnerswitcher) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-testrunnerswitcher/CMakeLists.txt))
            add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-testrunnerswitcher)
        endif()
        if ((NOT TARGET ctest) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-ctest/CMakeLists.txt))
            add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/azure-ctest)
        endif()
    endif()
    if ((NOT TARGET aziotsharedutil) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-shared-utility/CMakeLists.txt))
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-shared-utility)
    endif()
    if ((NOT TARGET uamqp) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-uamqp-c/CMakeLists.txt))
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/azure-uamqp-c)
    endif()
endif()

include_directories(${MACRO_UTILS_INC_FOLDER})

set(run_e2e_tests ${original_run_e2e_tests})
set(run_int_tests ${original_run_int_tests})
set(run_unittests ${original_run_unittests})

include("dependencies.cmake")

# Include the common build rules for C shared utility
if(${use_installed_dependencies})
    include(deps/azure-c-shared-utility/configs/azure_iot_build_rules.cmake)
else()
    include(${SHARED_UTIL_FOLDER}/configs/azure_iot_build_rules.cmake)
endif()

set_platform_files(${SHARED_UTIL_FOLDER})

include_directories(${UAMQP_INC_FOLDER})
include_directories(${UMOCK_C_INC_FOLDER})

enable_testing()

#this project uses several other projects that are build not by these CMakeFiles
#this project also targets several OSes

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()
if (MSVC)
    detect_architecture("_M_AMD64" x86_64)
    detect_architecture("_M_IX86" x86)
    detect_architecture("_M_ARM" ARM)
else()
    detect_architecture("__x86_64__" x86_64)
    detect_architecture("__i386__" x86)
    detect_architecture("__arm__" ARM)
endif()
if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
    set(ARCHITECTURE "GENERIC")
endif()
message(STATUS "eventhub architecture: ${ARCHITECTURE}")

#if any compiler has a command line switch called "OFF" then it will need special care
if(NOT "${compileOption_C}" STREQUAL "OFF")
    set(CMAKE_C_FLAGS "${compileOption_C} ${CMAKE_C_FLAGS}")
endif()

if(NOT "${compileOption_CXX}" STREQUAL "OFF")
    set(CMAKE_CXX_FLAGS "${compileOption_CXX} ${CMAKE_CXX_FLAGS}")
endif()


#this project uses several other projects that are build not by these CMakeFiles
#this project also targets several OSes

function(linkSharedUtil whatIsBuilding)
    target_link_libraries(${whatIsBuilding} aziotsharedutil)
endfunction(linkSharedUtil)

macro(compileAsC99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=c99 ${CMAKE_C_FLAGS}")
    if (NOT IN_OPENWRT)
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    endif()
    endif()
  else()
    set (CMAKE_C_STANDARD 99)
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(compileAsC99)

macro(compileAsC11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=c11 ${CMAKE_C_FLAGS}")
      set (CMAKE_C_FLAGS "-D_POSIX_C_SOURCE=200112L ${CMAKE_C_FLAGS}")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
    endif()
  else()
    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(compileAsC11)

function(windows_unittests_add_dll whatIsBuilding)	
    link_directories(${whatIsBuilding}_dll ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)
    
    add_library(${whatIsBuilding}_dll SHARED 
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
    )
    
    #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_dll
               PROPERTIES
               FOLDER "UnitTests") 
               
    target_include_directories(${whatIsBuilding}_dll PUBLIC ${sharedutil_include_directories} $ENV{VCInstallDir}UnitTest/include)
    target_compile_definitions(${whatIsBuilding}_dll PUBLIC -DCPP_UNITTEST)
    target_link_libraries(${whatIsBuilding}_dll micromock_cpp_unittest ctest ${ARGN})
endfunction()

function(windows_unittests_add_exe whatIsBuilding)
    link_directories(${whatIsBuilding}_exe ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)
    
    add_executable(${whatIsBuilding}_exe
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
        ${CMAKE_CURRENT_LIST_DIR}/main.c
    )
    
    #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_exe
               PROPERTIES
               FOLDER "UnitTests")
    
    target_compile_definitions(${whatIsBuilding}_exe PUBLIC -DUSE_CTEST)
    target_include_directories(${whatIsBuilding}_exe PUBLIC ${sharedutil_include_directories})
    target_link_libraries(${whatIsBuilding}_exe micromock_ctest ctest ${ARGN})
    add_test(NAME ${whatIsBuilding} COMMAND ${whatIsBuilding}_exe)
endfunction()

function(windows_unittests_add_lib whatIsBuilding)	
    link_directories(${whatIsBuilding}_lib ${MICROMOCK_UNITTEST_LIB_DIR} ${CTEST_LIB_DIR} ${SHARED_UTIL_LIB_DIR} $ENV{VCInstallDir}UnitTest/lib)
    
    add_library(${whatIsBuilding}_lib STATIC 
        ${${whatIsBuilding}_cpp_files} 
        ${${whatIsBuilding}_h_files} 
        ${${whatIsBuilding}_c_files}
    )
    
        #Add UnitTests to their own folder
    set_target_properties(${whatIsBuilding}_lib
               PROPERTIES
               FOLDER "UnitTests")
               
    target_include_directories(${whatIsBuilding}_lib PUBLIC ${sharedutil_include_directories})
    target_compile_definitions(${whatIsBuilding}_lib PUBLIC -DUSE_CTEST)
    target_link_libraries(${whatIsBuilding}_lib micromock_ctest ctest ${ARGN})
endfunction()

if(${run_e2e_tests})
    add_subdirectory(testtools)
endif()
add_subdirectory(eventhub_client)

if(${use_installed_dependencies})
    # Install azure-event-hubs-c
    set(package_location "cmake")

    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${EVENTHUB_SDK_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_file("configs/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
        COPYONLY
    )

    install(EXPORT azure_event_hubs_cTargets
        FILE
            "${PROJECT_NAME}Targets.cmake"
        DESTINATION
            ${package_location}
    )

    install(
        FILES
            "configs/${PROJECT_NAME}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION
            ${package_location}
    )
else()
endif()
