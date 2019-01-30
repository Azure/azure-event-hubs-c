#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

if(${use_installed_dependencies})
    if (NOT azure_c_shared_utility_FOUND)
        find_package(azure_c_shared_utility REQUIRED CONFIG)
    endif ()

    if (NOT uamqp_FOUND)
        find_package(uamqp REQUIRED CONFIG)
    endif ()
endif()