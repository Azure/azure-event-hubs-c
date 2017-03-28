// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef EVENTHUBCLIENT_LL_H
#define EVENTHUBCLIENT_LL_H

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/umock_c_prod.h"
#include "eventdata.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define EVENTHUBCLIENT_RESULT_VALUES            \
    EVENTHUBCLIENT_OK,                          \
    EVENTHUBCLIENT_INVALID_ARG,                 \
    EVENTHUBCLIENT_INVALID_CONNECTION_STRING,   \
    EVENTHUBCLIENT_URL_ENCODING_FAILURE,        \
    EVENTHUBCLIENT_EVENT_DATA_FAILURE,          \
    EVENTHUBCLIENT_PARTITION_KEY_MISMATCH,      \
    EVENTHUBCLIENT_DATA_SIZE_EXCEEDED,          \
    EVENTHUBCLIENT_ERROR                        \

DEFINE_ENUM(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES);

#define EVENTHUBCLIENT_CONFIRMATION_RESULT_VALUES     \
    EVENTHUBCLIENT_CONFIRMATION_OK,                   \
    EVENTHUBCLIENT_CONFIRMATION_DESTROY,              \
    EVENTHUBCLIENT_CONFIRMATION_EXCEED_MAX_SIZE,      \
    EVENTHUBCLIENT_CONFIRMATION_UNKNOWN,              \
    EVENTHUBCLIENT_CONFIRMATION_ERROR,                \
    EVENTHUBCLIENT_CONFIRMATION_TIMEOUT               \

DEFINE_ENUM(EVENTHUBCLIENT_CONFIRMATION_RESULT, EVENTHUBCLIENT_CONFIRMATION_RESULT_VALUES);

#define EVENTHUBCLIENT_STATE_VALUES                 \
    EVENTHUBCLIENT_CONN_AUTHENTICATED,              \
    EVENTHUBCLIENT_CONN_UNAUTHENTICATED             \

DEFINE_ENUM(EVENTHUBCLIENT_STATE, EVENTHUBCLIENT_STATE_VALUES);

#define EVENTHUBCLIENT_ERROR_RESULT_VALUES          \
    EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE,           \
    EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT,           \
    EVENTHUBCLIENT_SOCKET_SEND_FAILURE,             \
    EVENTHUBCLIENT_AMQP_INIT_FAILURE                \

DEFINE_ENUM(EVENTHUBCLIENT_ERROR_RESULT, EVENTHUBCLIENT_ERROR_RESULT_VALUES);

typedef struct EVENTHUBCLIENT_LL_TAG* EVENTHUBCLIENT_LL_HANDLE;
typedef void(*EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK)(EVENTHUBCLIENT_CONFIRMATION_RESULT result, void* userContextCallback);

typedef void(*EVENTHUB_CLIENT_STATECHANGE_CALLBACK)(EVENTHUBCLIENT_STATE eventhub_state, void* userContextCallback);
typedef void(*EVENTHUB_CLIENT_ERROR_CALLBACK)(EVENTHUBCLIENT_ERROR_RESULT eventhub_failure, void* userContextCallback);
typedef void(*EVENTHUB_CLIENT_TIMEOUT_CALLBACK)(EVENTDATA_HANDLE eventDataHandle, void* userContextCallback);

MOCKABLE_FUNCTION(, EVENTHUBCLIENT_LL_HANDLE, EventHubClient_LL_CreateFromConnectionString, const char*, connectionString, const char*, eventHubPath);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_LL_HANDLE, EventHubClient_LL_CreateFromSASToken, const char*, eventHubSasToken);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_RefreshSASTokenAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, const char*, eventHubSasToken);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SendAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTDATA_HANDLE, eventDataHandle, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK, sendAsyncConfirmationCallback, void*, userContextCallback);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SendBatchAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTDATA_HANDLE*, eventDataList, size_t, count, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK, sendAsyncConfirmationCallback, void*, userContextCallback);

MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SetStateChangeCallback, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTHUB_CLIENT_STATECHANGE_CALLBACK, state_change_cb, void*, userContextCallback);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SetErrorCallback, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTHUB_CLIENT_ERROR_CALLBACK, failure_cb, void*, userContextCallback);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_SetMessageTimeout, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, size_t, timeout_value);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_SetLogTrace, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, bool, log_trace_on);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_DoWork, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_Destroy, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle);

#ifdef __cplusplus
}
#endif

#endif /* EVENTHUBCLIENT_LL_H */
