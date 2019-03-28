#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

compileAsC11()
set(theseTestsName eventhubclient_e2e)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
)

set(${theseTestsName}_h_files
)

include_directories(${EVENTHUB_TEST_INC_FOLDER})

build_c_test_artifacts(${theseTestsName} OFF "tests/azure_eventhubs_sdk_c_tests" ADDITIONAL_LIBS eventhub_client eventhub_test aziotsharedutil uamqp testrunnerswitcher)
