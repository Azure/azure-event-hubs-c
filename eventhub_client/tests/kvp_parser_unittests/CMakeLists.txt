#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for kvp_parser_ut
cmake_minimum_required(VERSION 2.8.11)

compileAsC11()

set(theseTestsName kvp_parser_ut)

set(${theseTestsName}_test_files
    kvp_parser_unittests.c
)

set(${theseTestsName}_c_files
    ../../src/kvp_parser.c
)

set(${theseTestsName}_h_files
    ../../inc/kvp_parser.h
)

build_c_test_artifacts(${theseTestsName} ON "UnitTests")
