#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for eventhubauth_ut
cmake_minimum_required(VERSION 2.8.11)

compileAsC11()

set(theseTestsName eventhubauth_ut)

set(${theseTestsName}_test_files
    eventhubauth_unittests.c
)

set(${theseTestsName}_c_files
    ../../src/eventhubauth.c
)

set(${theseTestsName}_h_files
)

build_c_test_artifacts(${theseTestsName} ON "tests/azure_eventhubs_sdk_c_tests")
