#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

add_subdirectory(eventdata_ut)
add_subdirectory(eventhubclient_ut)
add_subdirectory(eventhubclient_ll_unittests)
add_subdirectory(eventhubreceiver_unittests)
add_subdirectory(eventhubreceiver_ll_unittests)
add_subdirectory(eventhubauth_unittests)
add_subdirectory(kvp_parser_unittests)
add_subdirectory(version_ut)

if (${run_e2e_tests})
    #add_subdirectory(eventhubclient_e2etests)
endif()
