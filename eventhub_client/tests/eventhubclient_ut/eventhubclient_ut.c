// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstddef>
#include <cstdlib>
#include <cstring>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_bool.h"

void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS

#include "version.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/gballoc.h"
#include "eventhubclient_ll.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/condition.h"

#undef ENABLE_MOCKS

#include "eventhubclient.h"

TEST_DEFINE_ENUM_TYPE(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES);
MU_DEFINE_ENUM_STRINGS(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES)
TEST_DEFINE_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(THREADAPI_RESULT, THREADAPI_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(LOCK_RESULT, LOCK_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(LOCK_RESULT, LOCK_RESULT_VALUES);

static TEST_MUTEX_HANDLE g_testByTest;

#define TEST_EVENTDATA_HANDLE (EVENTDATA_HANDLE)0x45
#define TEST_ENCODED_STRING_HANDLE (STRING_HANDLE)0x47
#define TEST_EVENTCLIENT_LL_HANDLE (EVENTHUBCLIENT_LL_HANDLE)0x49

static bool g_confirmationCall = true;
static EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK g_ConfirmationCallback;
static void* g_userContextCallback;
static EVENTHUBCLIENT_CONFIRMATION_RESULT g_callbackConfirmationResult;

static const char* CONNECTION_STRING = "Endpoint=sb://servicebusName.servicebus.windows.net/;SharedAccessKeyName=RootManageSharedAccessKey;SharedAccessKey=icT5kKsJr/Dw7oZveq7OPsSxu5Trmr6aiWlgI5zIT/8=";
static const char* EVENTHUB_PATH = "eventHubName";

static const char* TEXT_MESSAGE = "Hello From EventHubClient Unit Tests";
static const char* TEST_CHAR = "TestChar";
static const char* PARTITION_KEY_VALUE = "PartitionKeyValue";
static const char* PARTITION_KEY_EMPTY_VALUE = "";
static const char* PROPERTY_NAME = "PropertyName";
const unsigned char PROPERTY_BUFF_VALUE[] = { 0x7F, 0x40, 0x3F, 0x6D, 0x6A, 0x20, 0x05, 0x60 };
static const int BUFFER_SIZE = 8;
static bool g_lockInitFail = false;

#define EVENT_HANDLE_COUNT  3

static size_t g_currentlock_call;

static size_t g_currentcond_call;
static size_t g_whenShallcond_fail;

static void EventHubSendAsycConfirmCallback(EVENTHUBCLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    (void)result;
    bool* callbackNotified = (bool*)userContextCallback;
    *callbackNotified = true;
}

typedef struct LOCK_TEST_STRUCT_TAG
{
    char* dummy;
} LOCK_TEST_STRUCT;

typedef struct COND_TEST_STRUCT_TAG
{
    char* dummy;
} COND_TEST_STRUCT;

static THREADAPI_RESULT mock_ThreadAPI_Create(THREAD_HANDLE* threadHandle, THREAD_START_FUNC func, void* arg)
{
    (void)func;
    (void)arg;
    *threadHandle = (THREAD_HANDLE)my_gballoc_malloc(1);
    return THREADAPI_OK;
}

static THREADAPI_RESULT mock_ThreadAPI_Join(THREAD_HANDLE threadHandle, int* res)
{
    (void)res;
    my_gballoc_free(threadHandle);
    return THREADAPI_OK;
}

static EVENTHUBCLIENT_RESULT mock_EventHubClient_LL_SendAsync(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTDATA_HANDLE eventDataHandle, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK telemetryConfirmationCallback, void* userContextCallback)
{
    (void)eventDataHandle;
    (void)eventHubClientLLHandle;

    if (g_confirmationCall)
    {
        g_ConfirmationCallback = telemetryConfirmationCallback;
        g_userContextCallback = userContextCallback;
        g_ConfirmationCallback(g_callbackConfirmationResult, g_userContextCallback);
    }

    return EVENTHUBCLIENT_OK;
}

static EVENTHUBCLIENT_RESULT mock_EventHubClient_LL_SendBatchAsync(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTDATA_HANDLE* eventDataList, size_t count, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK telemetryConfirmationCallback, void* userContextCallback)
{
    (void)count;
    (void)eventDataList;
    (void)eventHubClientLLHandle;

    if (g_confirmationCall)
    {
        g_ConfirmationCallback = telemetryConfirmationCallback;
        g_userContextCallback = userContextCallback;
        g_ConfirmationCallback(g_callbackConfirmationResult, g_userContextCallback);
    }

    return EVENTHUBCLIENT_OK;
}
static void mock_EventHubClient_LL_DoWork(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle)
{
    (void)eventHubClientLLHandle;

    if (g_ConfirmationCallback)
    {
        g_ConfirmationCallback(g_callbackConfirmationResult, g_userContextCallback);
    }
}

static LOCK_HANDLE mock_Lock_Init(void)
{
    LOCK_HANDLE handle;
    if (g_lockInitFail)
    {
        handle = NULL;
    }
    else
    {
        LOCK_TEST_STRUCT* lockTest = (LOCK_TEST_STRUCT*)my_gballoc_malloc(sizeof(LOCK_TEST_STRUCT));
        handle = lockTest;
    }
    return handle;
}

static LOCK_RESULT mock_Lock(LOCK_HANDLE handle)
{
    if (handle != NULL)
    {
        LOCK_TEST_STRUCT* lockTest = (LOCK_TEST_STRUCT*)handle;
        lockTest->dummy = (char*)my_gballoc_malloc(1);
    }

    return LOCK_OK;
}

static LOCK_RESULT mock_Unlock(LOCK_HANDLE handle)
{
    if (handle != NULL)
    {
        LOCK_TEST_STRUCT* lockTest = (LOCK_TEST_STRUCT*)handle;
        my_gballoc_free(lockTest->dummy);
    }
    return LOCK_OK;
}

static LOCK_RESULT mock_Lock_Deinit(LOCK_HANDLE handle)
{
    my_gballoc_free(handle);
    return LOCK_OK;
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(eventhubclient_unittests)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    int result;

    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result);

    result = umocktypes_bool_register_types();
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);

    REGISTER_GLOBAL_MOCK_HOOK(ThreadAPI_Create, mock_ThreadAPI_Create);
    REGISTER_GLOBAL_MOCK_HOOK(ThreadAPI_Join, mock_ThreadAPI_Join);

    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_LL_CreateFromConnectionString, TEST_EVENTCLIENT_LL_HANDLE);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_LL_CreateFromSASToken, TEST_EVENTCLIENT_LL_HANDLE);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_LL_RefreshSASTokenAsync, EVENTHUBCLIENT_OK);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubClient_LL_SendAsync, mock_EventHubClient_LL_SendAsync);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubClient_LL_SendBatchAsync, mock_EventHubClient_LL_SendBatchAsync);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_LL_SetStateChangeCallback, EVENTHUBCLIENT_OK);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_LL_SetErrorCallback, EVENTHUBCLIENT_OK);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubClient_LL_DoWork, mock_EventHubClient_LL_DoWork);
    REGISTER_GLOBAL_MOCK_HOOK(Lock_Init, mock_Lock_Init);
    REGISTER_GLOBAL_MOCK_HOOK(Lock, mock_Lock);
    REGISTER_GLOBAL_MOCK_HOOK(Unlock, mock_Unlock);
    REGISTER_GLOBAL_MOCK_HOOK(Lock_Deinit, mock_Lock_Deinit);

    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBCLIENT_LL_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LOCK_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(THREAD_START_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTDATA_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(COND_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUB_CLIENT_STATECHANGE_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUB_CLIENT_ERROR_CALLBACK, void*);

    REGISTER_TYPE(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT);
    REGISTER_TYPE(THREADAPI_RESULT, THREADAPI_RESULT);
    REGISTER_TYPE(LOCK_RESULT, LOCK_RESULT);
}

TEST_SUITE_CLEANUP(TestClassCleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(g_testByTest);
}

TEST_FUNCTION_INITIALIZE(TestMethodInitialize)
{
    if (TEST_MUTEX_ACQUIRE(g_testByTest))
    {
        ASSERT_FAIL("our mutex is ABANDONED. Failure in test framework");
    }

    umock_c_reset_all_calls();

    g_currentlock_call = 0;

    g_confirmationCall = true;
    g_ConfirmationCallback = NULL;
    g_userContextCallback = NULL;
    g_callbackConfirmationResult = EVENTHUBCLIENT_CONFIRMATION_OK;
    g_lockInitFail = false;
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

static void eventhub_state_change_callback(EVENTHUBCLIENT_STATE eventhub_state, void* userContextCallback)
{
    (void)eventhub_state;
    (void)userContextCallback;
}

static void eventhub_error_callback(EVENTHUBCLIENT_ERROR_RESULT eventhub_failure, void* userContextCallback)
{
    (void)eventhub_failure;
    (void)userContextCallback;
}

/*** EventHubClient_CreateFromConnectionString ***/
/* Tests_SRS_EVENTHUBCLIENT_03_004: [EventHubClient_ CreateFromConnectionString shall pass the connectionString and eventHubPath variables to EventHubClient_CreateFromConnectionString_LL.] */
/* Tests_SRS_EVENTHUBCLIENT_03_006: [EventHubClient_ CreateFromConnectionString shall return a NULL value if EventHubClient_CreateFromConnectionString_LL  returns NULL.] */
TEST_FUNCTION(EventHubClient_CreateFromConnectionString_Lower_Layer_Fails)
{
    // arrange
    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH))
        .SetReturn((EVENTHUBCLIENT_LL_HANDLE)NULL);

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // assert
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_EVENTHUBCLIENT_03_002: [Upon Success of EventHubClient_CreateFromConnectionString_LL,  EventHubClient_CreateFromConnectionString shall allocate the internal structures required by this module.] */
/* Tests_SRS_EVENTHUBCLIENT_03_005: [Upon Success EventHubClient_CreateFromConnectionString shall return the EVENTHUBCLIENT_HANDLE.] */
TEST_FUNCTION(EventHubClient_CreateFromConnectionString_Succeeds)
{
    // arrange
    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH));
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);

    // assert
    ASSERT_IS_NOT_NULL(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(result);
}

TEST_FUNCTION(EventHubClient_CreateFromConnectionString_Lock_Init_Fails)
{
    // arrange
    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH));
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init())
        .SetReturn(NULL);
    STRICT_EXPECTED_CALL(EventHubClient_LL_Destroy(TEST_EVENTCLIENT_LL_HANDLE));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // assert
    ASSERT_IS_NULL(result);

    // cleanup
    EventHubClient_Destroy(result);
}

//**Tests_SRS_EVENTHUBCLIENT_29_101: \[**EventHubClient_CreateFromSASToken shall pass the eventHubSasToken argument to EventHubClient_LL_CreateFromSASToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_102: \[**EventHubClient_CreateFromSASToken shall return a NULL value if EventHubClient_LL_CreateFromSASToken returns NULL.**\]**
TEST_FUNCTION(EventHubClient_CreateFromSASToken_Lower_Layer_Fails)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromSASToken(sasToken))
        .SetReturn((EVENTHUBCLIENT_LL_HANDLE)NULL);

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromSASToken(sasToken);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // assert
    ASSERT_IS_NULL(result);
}

//**Tests_SRS_EVENTHUBCLIENT_29_101: \[**EventHubClient_CreateFromSASToken shall pass the eventHubSasToken argument to EventHubClient_LL_CreateFromSASToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_103: \[**Upon Success of EventHubClient_LL_CreateFromSASToken, EventHubClient_CreateFromSASToken shall allocate the internal structures as required by this module.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_104: \[**Upon Success of EventHubClient_LL_CreateFromSASToken, EventHubClient_CreateFromSASToken shall initialize a lock using API Lock_Init.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_105: \[**Upon Success EventHubClient_CreateFromSASToken shall return the EVENTHUBCLIENT_HANDLE.**\]**
TEST_FUNCTION(EventHubClient_CreateFromSASToken_Succeeds)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromSASToken(sasToken));
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromSASToken(sasToken);

    // assert
    ASSERT_IS_NOT_NULL(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(result);
}

//**Tests_SRS_EVENTHUBCLIENT_29_101: \[**EventHubClient_CreateFromSASToken shall pass the eventHubSasToken argument to EventHubClient_LL_CreateFromSASToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_102: \[**EventHubClient_CreateFromSASToken shall return a NULL value if EventHubClient_LL_CreateFromSASToken returns NULL.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_103: \[**Upon Success of EventHubClient_LL_CreateFromSASToken, EventHubClient_CreateFromSASToken shall allocate the internal structures as required by this module.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_104: \[**Upon Success of EventHubClient_LL_CreateFromSASToken, EventHubClient_CreateFromSASToken shall initialize a lock using API Lock_Init.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_106: \[**Upon Failure EventHubClient_CreateFromSASToken shall return NULL and free any allocations as needed.**\]**
TEST_FUNCTION(EventHubClient_CreateFromSASToken_Lock_Init_Fails)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    STRICT_EXPECTED_CALL(EventHubClient_LL_CreateFromSASToken(sasToken));
    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(EventHubClient_LL_Destroy(TEST_EVENTCLIENT_LL_HANDLE));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    g_lockInitFail = true;

    // act
    EVENTHUBCLIENT_HANDLE result = EventHubClient_CreateFromSASToken(sasToken);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // assert
    ASSERT_IS_NULL(result);

    // cleanup
    EventHubClient_Destroy(result);
}

//**Tests_SRS_EVENTHUBCLIENT_29_201: \[**EventHubClient_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG immediately if eventHubHandle or sasToken is NULL.**\]**
TEST_FUNCTION(EventHubClient_RefreshSASTokenAsync_NULLParam_eventHubHandle)
{
    const char sasToken[] = "Test String";

    // arrange

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_RefreshSASTokenAsync(NULL, sasToken);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBCLIENT_INVALID_ARG, result, "Failed Return Value Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBCLIENT_29_201: \[**EventHubClient_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG immediately if eventHubHandle or sasToken is NULL.**\]**
TEST_FUNCTION(EventHubClient_RefreshSASTokenAsync_NULLParam_eventHubSasToken)
{
    const char sasToken[] = "Test String";

    // arrange
    EVENTHUBCLIENT_HANDLE h = EventHubClient_CreateFromSASToken(sasToken);
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_RefreshSASTokenAsync(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBCLIENT_INVALID_ARG, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(h);
}

//**Tests_SRS_EVENTHUBCLIENT_29_202: \[**EventHubClient_RefreshSASTokenAsync shall Lock the EVENTHUBCLIENT_STRUCT lockInfo using API Lock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_203: \[**EventHubClient_RefreshSASTokenAsync shall call EventHubClient_LL_RefreshSASTokenAsync and pass the  EVENTHUBCLIENT_LL_HANDLE and the sasToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_204: \[**EventHubClient_RefreshSASTokenAsync shall unlock the EVENTHUBCLIENT_STRUCT lockInfo using API Unlock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_205: \[**EventHubClient_RefreshSASTokenAsync shall return the result of the EventHubClient_LL_RefreshSASTokenAsync.**\]**
TEST_FUNCTION(EventHubClient_RefreshSASTokenAsync_Succeeds)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    EVENTHUBCLIENT_HANDLE h = EventHubClient_CreateFromSASToken(sasToken);

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_RefreshSASTokenAsync(IGNORED_PTR_ARG, sasToken));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_RefreshSASTokenAsync(h, sasToken);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBCLIENT_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(h);
}

//**Tests_SRS_EVENTHUBCLIENT_29_202: \[**EventHubClient_RefreshSASTokenAsync shall Lock the EVENTHUBCLIENT_STRUCT lockInfo using API Lock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_203: \[**EventHubClient_RefreshSASTokenAsync shall call EventHubClient_LL_RefreshSASTokenAsync and pass the  EVENTHUBCLIENT_LL_HANDLE and the sasToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_204: \[**EventHubClient_RefreshSASTokenAsync shall unlock the EVENTHUBCLIENT_STRUCT lockInfo using API Unlock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_206: \[**EventHubClient_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR for any errors encountered.**\]**
TEST_FUNCTION(EventHubClient_RefreshSASTokenAsync_Lock_Fails)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    EVENTHUBCLIENT_HANDLE h = EventHubClient_CreateFromSASToken(sasToken);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG))
        .SetReturn(LOCK_ERROR);

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_RefreshSASTokenAsync(h, sasToken);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBCLIENT_ERROR, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(h);
}

//**Tests_SRS_EVENTHUBCLIENT_29_202: \[**EventHubClient_RefreshSASTokenAsync shall Lock the EVENTHUBCLIENT_STRUCT lockInfo using API Lock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_203: \[**EventHubClient_RefreshSASTokenAsync shall call EventHubClient_LL_RefreshSASTokenAsync and pass the  EVENTHUBCLIENT_LL_HANDLE and the sasToken.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_204: \[**EventHubClient_RefreshSASTokenAsync shall unlock the EVENTHUBCLIENT_STRUCT lockInfo using API Unlock.**\]**
//**Tests_SRS_EVENTHUBCLIENT_29_206: \[**EventHubClient_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR for any errors encountered.**\]**
TEST_FUNCTION(EventHubClient_RefreshSASTokenAsync_Refresh_Fails)
{
    // arrange
    const char sasToken[] = "Test SAS Token String";

    EVENTHUBCLIENT_HANDLE h = EventHubClient_CreateFromSASToken(sasToken);

    umock_c_reset_all_calls();
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_RefreshSASTokenAsync(IGNORED_PTR_ARG, sasToken)).SetReturn(EVENTHUBCLIENT_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_RefreshSASTokenAsync(h, sasToken);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBCLIENT_ERROR, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(h);
}

/*** EventHubClient_Destroy ***/
/* Tests_SRS_EVENTHUBCLIENT_03_018: [If the eventHubHandle is NULL, EventHubClient_Destroy shall not do anything.] */
TEST_FUNCTION(EventHubClient_Destroy_with_NULL_eventHubHandle_Does_Nothing)
{
    // arrange

    // act
    EventHubClient_Destroy(NULL);

    // assert
    // Implicit
}

/* Tests_SRS_EVENTHUBCLIENT_03_019: [EventHubClient_Destroy shall terminate the usage of this EventHubClient specified by the eventHubHandle and cleanup all associated resources.] */
/* Tests_SRS_EVENTHUBCLIENT_03_020: [EventHubClient_Destroy shall call EventHubClient_LL_Destroy with the lower level handle.] */
TEST_FUNCTION(EventHubClient_Destroy_Success)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubClient_LL_Destroy(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EventHubClient_Destroy(eventHubHandle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup implicit
}

/* Tests_SRS_EVENTHUBCLIENT_03_019: [EventHubClient_Destroy shall terminate the usage of this EventHubClient specified by the eventHubHandle and cleanup all associated resources.] */
/* Tests_SRS_EVENTHUBCLIENT_03_020: [EventHubClient_Destroy shall call EventHubClient_LL_Destroy with the lower level handle.] */
TEST_FUNCTION(EventHubClient_Destroy_With_ThreadJoin_Success)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    (void)EventHubClient_Send(eventHubHandle, TEST_EVENTDATA_HANDLE);

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(ThreadAPI_Join(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_Destroy(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EventHubClient_Destroy(eventHubHandle);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup implicit
}

/*** EventHubClient_Send ***/
/* Tests_SRS_EVENTHUBCLIENT_03_007: [EventHubClient_Send shall return EVENTHUBCLIENT_INVALID_ARG if either eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_Send_with_NULL_eventHubHandle_Fails)
{
    // arrange
    unsigned char testData[] = { 0x42, 0x43, 0x44 };
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(testData, sizeof(testData) / sizeof(testData[0]));
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_Send(NULL, eventDataHandle);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_03_007: [EventHubClient_Send shall return EVENTHUBCLIENT_INVALID_ARG if either eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_Send_with_NULL_eventDataHandle_Fails)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString("j", "b");
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_Send(eventHubHandle, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_03_008: [EventHubClient_Send shall call into the Execute_LowerLayerSendAsync function to send the eventDataHandle parameter to the EventHub.] */
/* Tests_SRS_EVENTHUBCLIENT_03_013: [EventHubClient_Send shall return EVENTHUBCLIENT_OK upon successful completion of the Execute_LowerLayerSendAsync and the callback function.] */
/* Tests_SRS_EVENTHUBCLIENT_03_010: [Upon success of Execute_LowerLayerSendAsync, then EventHubClient_Send wait until the EVENTHUB_CALLBACK_STRUCT callbackStatus variable is set to CALLBACK_NOTIFIED.] */
TEST_FUNCTION(EventHubClient_Send_Succeeds)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Post(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Wait(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_Send(eventHubHandle, TEST_EVENTDATA_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_012: [EventHubClient_Send shall return EVENTHUBCLIENT_ERROR if the EVENTHUB_CALLBACK_STRUCT confirmationResult variable does not equal EVENTHUBCLIENT_CONFIMRATION_OK.] */
TEST_FUNCTION(EventHubClient_Send_ConfirmationResult_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Post(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Wait(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    g_callbackConfirmationResult = EVENTHUBCLIENT_CONFIRMATION_ERROR;

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_Send(eventHubHandle, TEST_EVENTDATA_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_03_009: [EventHubClient_Send shall return EVENTHUBCLIENT_ERROR on any failure that is encountered.] */
TEST_FUNCTION(EventHubClient_Send_EventhubClient_LL_SendAsync_Fails)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(EVENTHUBCLIENT_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    g_confirmationCall = false;

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_Send(eventHubHandle, TEST_EVENTDATA_HANDLE);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/*** EventHubClient_SendAsync ***/
/* Tests_SRS_EVENTHUBCLIENT_07_037: [On Success EventHubClient_SendAsync shall return EVENTHUBCLIENT_OK.] */
/* Tests_SRS_EVENTHUBCLIENT_07_038: [Execute_LowerLayerSendAsync shall call EventHubClient_LL_SendAsync to send data to the Eventhub Endpoint.] */
/* Tests_SRS_EVENTHUBCLIENT_07_028: [If Execute_LowerLayerSendAsync is successful then it shall return 0.] */
/* Tests_SRS_EVENTHUBCLIENT_07_034: [Create_DoWorkThreadIfNeccesary shall use the ThreadAPI_Create API to create a thread and execute EventhubClientThread function.] */
/* Tests_SRS_EVENTHUBCLIENT_07_029: [Execute_LowerLayerSendAsync shall Lock on the EVENTHUBCLIENT_STRUCT lockInfo to protect calls to Lower Layer and Thread function calls.] */
/* Tests_SRS_EVENTHUBCLIENT_07_031: [Execute_LowerLayerSendAsync shall call into the Create_DoWorkThreadIfNeccesary function to create the DoWork thread.] */
TEST_FUNCTION(EventHubClient_SendAsync_Succeeds)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_03_021: [EventHubClient_SendAsync shall return EVENTHUBCLIENT_INVALID_ARG if either eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_SendAsync_EventHubHandle_NULL_Fail)
{
    // arrange
    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(NULL, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

/* Tests_SRS_EVENTHUBCLIENT_03_021: [EventHubClient_SendAsync shall return EVENTHUBCLIENT_INVALID_ARG if either eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_SendAsync_EVENTDATA_HANDLE_NULL_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, NULL, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_039: [If the EventHubClient_LL_SendAsync call fails then Execute_LowerLayerSendAsync shall return a nonzero value.] */
TEST_FUNCTION(EventHubClient_SendAsync_EventHubClient_LL_SendAsync_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(EVENTHUBCLIENT_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_035: [Create_DoWorkThreadIfNeccesary shall return a nonzero value if any failure is encountered.] */
/* Tests_SRS_EVENTHUBCLIENT_07_032: [If Create_DoWorkThreadIfNeccesary does not return 0 then Execute_LowerLayerSendAsync shall return a nonzero value.] */
/* Tests_SRS_EVENTHUBCLIENT_07_022: [EventHubClient_SendAsync shall call into Execute_LowerLayerSendAsync and return EVENTHUBCLIENT_ERROR on a nonzero return value.] */
TEST_FUNCTION(EventHubClient_SendAsync_ThreadApi_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(THREADAPI_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_033: [Create_DoWorkThreadIfNeccesary shall set return 0 if threadHandle parameter is not a NULL value.] */
TEST_FUNCTION(EventHubClient_SendAsync_2nd_Call_Succeeds)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendAsync(IGNORED_PTR_ARG, TEST_EVENTDATA_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);

    result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_028: [CreateThread_And_SendAsync shall return EVENTHUBCLIENT_ERROR on any error that occurs.] */
/* Tests_SRS_EVENTHUBCLIENT_07_030: [Execute_LowerLayerSendAsync shall return a nonzero value if it is unable to obtain the lock with the Lock function.]*/
TEST_FUNCTION(EventHubClient_SendAsync_Lock_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG))
        .SetReturn(LOCK_ERROR);

    bool callbackNotified = false;
    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/*** EventHubClient_SendBatchAsync ***/
/* Tests_SRS_EVENTHUBCLIENT_07_040: [EventHubClient_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL or count is zero.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_EventHandle_NULL_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    umock_c_reset_all_calls();

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(NULL, eventhandleList, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

/* Tests_SRS_EVENTHUBCLIENT_07_040: [EventHubClient_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL or count is zero.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_EventHandleList_NULL_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, NULL, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_040: [EventHubClient_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL or count is zero.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_EventHandle_Count_Zero_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, eventhandleList, 0, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_042: [On Success EventHubClient_SendBatchAsync shall return EVENTHUBCLIENT_OK.] */
/* Tests_SRS_EVENTHUBCLIENT_07_043: [Execute_LowerLayerSendBatchAsync shall Lock on the EVENTHUBCLIENT_STRUCT lockInfo to protect calls to Lower Layer and Thread function calls.] */
/* Tests_SRS_EVENTHUBCLIENT_07_045: [Execute_LowerLayerSendAsync shall call into the Create_DoWorkThreadIfNeccesary function to create the DoWork thread.] */
/* Tests_SRS_EVENTHUBCLIENT_07_047: [If Execute_LowerLayerSendAsync is successful then it shall return 0.] */
/* Tests_SRS_EVENTHUBCLIENT_07_048: [Execute_LowerLayerSendAsync shall call EventHubClient_LL_SendAsync to send data to the Eventhub Endpoint.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_Succeed)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendBatchAsync(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_043: [Execute_LowerLayerSendBatchAsync shall Lock on the EVENTHUBCLIENT_STRUCT lockInfo to protect calls to Lower Layer and Thread function calls.] */
/* Tests_SRS_EVENTHUBCLIENT_07_044: [Execute_LowerLayerSendBatchAsync shall return a nonzero value if it is unable to obtain the lock with the Lock function.] */
/* Tests_SRS_EVENTHUBCLIENT_07_049: [If the EventHubClient_LL_SendAsync call fails then Execute_LowerLayerSendAsync shall return a nonzero value.] */
/* Tests_SRS_EVENTHUBCLIENT_07_041: [EventHubClient_SendBatchAsync shall call into Execute_LowerLayerSendBatchAsync and return EVENTHUBCLIENT_ERROR on a nonzero return value.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_Lock_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG))
        .SetReturn(LOCK_ERROR);

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_046: [If Create_DoWorkThreadIfNeccesary does not return 0 then Execute_LowerLayerSendAsync shall return a nonzero value.] */
TEST_FUNCTION(EventHubClient_SendBatchAsync_ThreadApi_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(THREADAPI_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

TEST_FUNCTION(EventHubClient_SendBatchAsync_LL_SendBatchAsync_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendBatchAsync(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(EVENTHUBCLIENT_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    bool callbackNotified = false;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatchAsync(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT, EventHubSendAsycConfirmCallback, &callbackNotified);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/*** EventHubClient_SendBatch ***/

/* Tests_SRS_EVENTHUBCLIENT_07_050: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_SendBatch_EVENTHUBCLIENT_NULLL_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    umock_c_reset_all_calls();


    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(NULL, eventhandleList, EVENT_HANDLE_COUNT);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

/* Tests_SRS_EVENTHUBCLIENT_07_050: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_SendBatch_EVENTHANDLELIST_NULL_Fail)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(eventHubHandle, NULL, EVENT_HANDLE_COUNT);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_050: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_INVALID_ARG if eventHubHandle or eventDataHandle is NULL.] */
TEST_FUNCTION(EventHubClient_SendBatch_Handle_count_zero_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(eventHubHandle, eventhandleList, 0);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_051: [EventHubClient_SendBatch shall call into the Execute_LowerLayerSendBatchAsync function to send the eventDataHandle parameter to the EventHub.] */
/* Tests_SRS_EVENTHUBCLIENT_07_053: [Upon success of Execute_LowerLayerSendBatchAsync, then EventHubClient_SendBatch shall wait until the EVENTHUB_CALLBACK_STRUCT callbackStatus variable is set to CALLBACK_NOTIFIED.] */
/* Tests_SRS_EVENTHUBCLIENT_07_054: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_OK upon successful completion of the Execute_LowerLayerSendBatchAsync and the callback function.] */
TEST_FUNCTION(EventHubClient_SendBatch_Succeed)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendBatchAsync(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Post(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Wait(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_052: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_ERROR on any failure that is encountered.] */
TEST_FUNCTION(EventHubClient_SendBatch_LowerLayerSendBatch_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendBatchAsync(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .SetReturn(EVENTHUBCLIENT_ERROR);
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    g_confirmationCall = false;

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_052: [EventHubClient_SendBatch shall return EVENTHUBCLIENT_ERROR on any failure that is encountered.] */
TEST_FUNCTION(EventHubClient_SendBatch_Confirmation_Result_Fail)
{
    // arrange
    EVENTDATA_HANDLE eventhandleList[EVENT_HANDLE_COUNT];

    eventhandleList[0] = (EVENTDATA_HANDLE)1;
    eventhandleList[1] = (EVENTDATA_HANDLE)2;
    eventhandleList[2] = (EVENTDATA_HANDLE)4;

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Lock_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Init());
    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(ThreadAPI_Create(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SendBatchAsync(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Post(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Wait(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Lock_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Condition_Deinit(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    g_callbackConfirmationResult = EVENTHUBCLIENT_CONFIRMATION_ERROR;
    EVENTHUBCLIENT_RESULT result = EventHubClient_SendBatch(eventHubHandle, eventhandleList, EVENT_HANDLE_COUNT);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

//**Tests_SRS_EVENTHUBCLIENT_07_080: [** If eventHubHandle is NULL EventHubClient_Set_StateChangeCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
TEST_FUNCTION(EventHubClient_SetStateChangeCallback_eventhubclient_NULL_fail)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SetStateChangeCallback(NULL, eventhub_state_change_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

//**Tests_SRS_EVENTHUBCLIENT_07_081: [** If state_change_cb is non-NULL then EventHubClient_Set_StateChange_Callback shall call state_change_cb when a state changes is encountered. **]**
//**Tests_SRS_EVENTHUBCLIENT_07_082: [** If state_change_cb is NULL EventHubClient_Set_StateChange_Callback shall no longer call state_change_cb on state changes. **]**
//**Tests_SRS_EVENTHUBCLIENT_07_083: [** If EventHubClient_Set_StateChange_Callback succeeds it shall return EVENTHUBCLIENT_OK. **]**
TEST_FUNCTION(EventHubClient_SetStateChangeCallback_Succeeds)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SetStateChangeCallback(IGNORED_PTR_ARG, eventhub_state_change_callback, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SetStateChangeCallback(eventHubHandle, eventhub_state_change_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_056: [If eventHubHandle is NULL EventHubClient_SetErrorCallback shall return EVENTHUBCLIENT_INVALID_ARG. ] */
TEST_FUNCTION(EventHubClient_SetErrorCallback_eventhubclient_NULL_fail)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SetErrorCallback(NULL, eventhub_error_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_EVENTHUBCLIENT_07_057: [If error_cb is non-NULL EventHubClient_SetErrorCallback shall execute the error_cb on failures with a EVENTHUBCLIENT_FAILURE_RESULT.] */
/* Tests_SRS_EVENTHUBCLIENT_07_058: [If error_cb is NULL EventHubClient_SetErrorCallback shall no longer call error_cb on failure.] */
/* Tests_SRS_EVENTHUBCLIENT_07_059: [If EventHubClient_SetErrorCallback succeeds it shall return EVENTHUBCLIENT_OK.] */
TEST_FUNCTION(EventHubClient_SetErrorCallback_Succeeds)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SetErrorCallback(IGNORED_PTR_ARG, eventhub_error_callback, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EVENTHUBCLIENT_RESULT result = EventHubClient_SetErrorCallback(eventHubHandle, eventhub_error_callback, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventHubClient_Destroy(eventHubHandle);
}

/* Tests_SRS_EVENTHUBCLIENT_07_061: [If eventHubClientLLHandle is NULL EventHubClient_SetLogTrace shall do nothing.] */
TEST_FUNCTION(EventHubClient_SetLogTrace_EventHubClient_NULL_fails)
{
    // arrange

    // act
    EventHubClient_SetLogTrace(NULL, false);

    //assert
    umock_c_reset_all_calls();
}

/* Tests_SRS_EVENTHUBCLIENT_07_060: [If eventHubClientLLHandle is non-NULL EventHubClient_SetLogTrace shall call the uAmqp trace function with the log_trace_on.] */
TEST_FUNCTION(EventHubClient_SetLogTrace_succeed)
{
    // arrange

    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SetLogTrace(IGNORED_PTR_ARG, true));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EventHubClient_SetLogTrace(eventHubHandle, true);

    //assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    EventHubClient_Destroy(eventHubHandle);
}

TEST_FUNCTION(EventHubClient_SetMessageTimeout_EventHubClient_NULL_fails)
{
    // arrange

    // act
    EventHubClient_SetMessageTimeout(NULL, 10000);

    //assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

TEST_FUNCTION(EventHubClient_SetMessageTimeout_succeed)
{
    // arrange
    EVENTHUBCLIENT_HANDLE eventHubHandle = EventHubClient_CreateFromConnectionString(CONNECTION_STRING, EVENTHUB_PATH);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(Lock(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(EventHubClient_LL_SetMessageTimeout(IGNORED_PTR_ARG, 10000));
    STRICT_EXPECTED_CALL(Unlock(IGNORED_PTR_ARG));

    // act
    EventHubClient_SetMessageTimeout(eventHubHandle, 10000);

    //assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
    EventHubClient_Destroy(eventHubHandle);
}

END_TEST_SUITE(eventhubclient_unittests)
