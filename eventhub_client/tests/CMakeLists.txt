#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

if(${run_unittests})
    add_subdirectory(eventdata_ut)
    add_subdirectory(eventhubclient_ut)
    add_subdirectory(eventhubclient_ll_ut)
    add_subdirectory(eventhubreceiver_ut)
    add_subdirectory(eventhubreceiver_ll_ut)
    add_subdirectory(eventhubauth_ut)
    add_subdirectory(kvp_parser_ut)
    add_subdirectory(version_ut)
endif()

if(${run_e2e_tests})
# test disabled for the moment
#    add_subdirectory(eventhubclient_e2e)
endif()
