#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.5)

compileAsC99()

set(eventhub_client_c_files
    ./src/eventdata.c
    ./src/eventhubauth.c
    ./src/eventhubclient.c
    ./src/eventhubreceiver.c
    ./src/eventhubreceiver_ll.c
    ./src/eventhubclient_ll.c
    ./src/kvp_parser.c
    ./src/version.c
)

set(eventhub_client_h_files
    ./inc/eventdata.h
    ./inc/eventhubauth.h
    ./inc/eventhubclient.h
    ./inc/eventhubclient_ll.h
    ./inc/eventhubreceiver.h
    ./inc/eventhubreceiver_ll.h
    ./inc/kvp_parser.h
    ./inc/version.h
)

#these are the include folders
#the following "set" statement exports across the project a global variable called SHARED_UTIL_INC_FOLDER that expands to whatever needs to included when using COMMON library

set(EVENTHUB_CLIENT_INC_FOLDER ${CMAKE_CURRENT_LIST_DIR}/inc CACHE INTERNAL "this is what needs to be included if using eventhub_client lib" FORCE)

include_directories(${SHARED_UTIL_INC_FOLDER})

include_directories(${EVENTHUB_CLIENT_INC_FOLDER})

IF(WIN32)
    #windows needs this define
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DGB_MEASURE_MEMORY_FOR_THIS -DGB_DEBUG_ALLOC)
ENDIF(WIN32)

add_library(eventhub_client 
    ${eventhub_client_c_files} 
    ${eventhub_client_h_files}
)

target_link_libraries(eventhub_client uamqp)
linkSharedUtil(eventhub_client)

add_subdirectory(samples)
add_subdirectory(tests)

if(${use_installed_dependencies})
    if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
        set(CMAKE_INSTALL_LIBDIR "lib")
    endif()

    if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
        set(CMAKE_INSTALL_INCLUDEDIR "include")
    endif()

    install(TARGETS eventhub_client EXPORT azure_event_hubs_cTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/../bin
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azure_event_hubs_c
    )
    install(FILES ${eventhub_client_h_files} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azure_event_hubs_c)

else()
    set(install_staticlibs
        eventhub_client
    )
    install(FILES ${eventhub_client_h_files}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/azure_event_hubs_c)
    install(TARGETS ${install_staticlibs}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
