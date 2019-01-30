#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for eventhubclient_ll_ut
cmake_minimum_required(VERSION 2.8.11)

compileAsC99()
set(theseTestsName eventhubclient_ll_ut)
set(${theseTestsName}_cpp_files
    eventhubclient_ll_unittests.cpp
)

set(${theseTestsName}_c_files
    ../../src/eventhubclient_ll.c
    ${SHARED_UTIL_SRC_FOLDER}/xlogging.c
    ${SHARED_UTIL_SRC_FOLDER}/consolelogger.c
)

set(${theseTestsName}_h_files
)

build_test_artifacts(${theseTestsName} ON)
