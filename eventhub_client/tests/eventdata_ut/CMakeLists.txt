#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.5)

compileAsC99()

set(theseTestsName eventdata_ut)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/eventdata.c
    ${SHARED_UTIL_SRC_FOLDER}/xlogging.c
    ${SHARED_UTIL_SRC_FOLDER}/consolelogger.c
    ${SHARED_UTIL_SRC_FOLDER}/../tests/real_test_files/real_strings.c
)

set(${theseTestsName}_h_files
    ${SHARED_UTIL_SRC_FOLDER}/../tests/real_test_files/real_strings.h
)

include_directories(${SHARED_UTIL_SRC_FOLDER}/../tests/real_test_files)

build_c_test_artifacts(${theseTestsName} ON "tests/azure_eventhubs_sdk_c_tests")
