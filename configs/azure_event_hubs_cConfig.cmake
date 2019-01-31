#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

include("${CMAKE_CURRENT_LIST_DIR}/azure_event_hubs_cTargets.cmake")

get_target_property(EVENTHUBCLIENT_INCLUDES eventhub_client INTERFACE_INCLUDE_DIRECTORIES)

set(EVENTHUBCLIENT_INCLUDES ${EVENTHUBCLIENT_INCLUDES} CACHE INTERNAL "")