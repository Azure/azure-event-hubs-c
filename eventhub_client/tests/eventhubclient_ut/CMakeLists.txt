#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.5)

compileAsC99()

set(theseTestsName eventhubclient_ut)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/eventhubclient.c
    ${SHARED_UTIL_SRC_FOLDER}/xlogging.c
    ${SHARED_UTIL_SRC_FOLDER}/consolelogger.c
)

set(${theseTestsName}_h_files
)

build_c_test_artifacts(${theseTestsName} ON "tests/azure_eventhubs_sdk_c_tests")
