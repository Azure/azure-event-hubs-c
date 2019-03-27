// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#else // __cplusplus
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#endif // __cplusplus

#include "testrunnerswitcher.h"
#include "umock_c.h"

void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/map.h"

#undef ENABLE_MOCKS

#include "eventdata.h"

#include "real_strings.h"

#define NUMBER_OF_CHAR      8

TEST_DEFINE_ENUM_TYPE(EVENTDATA_RESULT, EVENTDATA_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTDATA_RESULT, EVENTDATA_RESULT_VALUES);

static const BUFFER_HANDLE TEST_BUFFER_HANDLE = (BUFFER_HANDLE)0x42;

static TEST_MUTEX_HANDLE g_testByTest;

static const char* PARTITION_KEY_VALUE = "EventDataPartitionKey";
static const char* PARTITION_KEY_ZERO_VALUE = "";
static const char* PROPERTY_NAME = "EventDataPropertyName";

#define TEST_STRING_HANDLE (STRING_HANDLE)0x46
#define TEST_BUFFER_SIZE 6
#define INITIALIZE_BUFFER_SIZE 256

static unsigned char TEST_BUFFER_VALUE[] = { 0x42, 0x43, 0x44, 0x45, 0x46, 0x47 };

static MAP_FILTER_CALLBACK g_mapFilterFunc;

typedef struct EVENT_PROPERTY_TEST_TAG
{
    STRING_HANDLE key;
    BUFFER_HANDLE value;
} EVENT_PROPERTY_TEST;

typedef struct LOCK_TEST_STRUCT_TAG
{
    char* dummy;
} LOCK_TEST_STRUCT;

static BUFFER_HANDLE mock_BUFFER_new(void)
{
    return (BUFFER_HANDLE)malloc(1);
}

static void mock_BUFFER_delete(BUFFER_HANDLE handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

static BUFFER_HANDLE mock_BUFFER_clone(BUFFER_HANDLE handle)
{
    (void)handle;
    return (BUFFER_HANDLE)malloc(1);
}

static MAP_HANDLE mock_Map_Create(MAP_FILTER_CALLBACK mapFilterFunc)
{
    g_mapFilterFunc = mapFilterFunc;
    return (MAP_HANDLE)malloc(1);
}

static void mock_Map_Destroy(MAP_HANDLE handle)
{
    free(handle);
}

static MAP_HANDLE mock_Map_Clone(MAP_HANDLE handle)
{
    (void)handle;
    return (MAP_HANDLE)malloc(1);
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

BEGIN_TEST_SUITE(eventdata_unittests)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    int result;

    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);

    REGISTER_GLOBAL_MOCK_HOOK(BUFFER_new, mock_BUFFER_new);
    REGISTER_GLOBAL_MOCK_HOOK(BUFFER_delete, mock_BUFFER_delete);
    REGISTER_GLOBAL_MOCK_RETURN(BUFFER_build, 0);
    REGISTER_GLOBAL_MOCK_RETURN(BUFFER_content, 0);
    REGISTER_GLOBAL_MOCK_RETURN(BUFFER_size, 0);
    REGISTER_GLOBAL_MOCK_HOOK(BUFFER_clone, mock_BUFFER_clone);

    REGISTER_GLOBAL_MOCK_HOOK(Map_Create, mock_Map_Create);
    REGISTER_GLOBAL_MOCK_HOOK(Map_Destroy, mock_Map_Destroy);
    REGISTER_GLOBAL_MOCK_HOOK(Map_Clone, mock_Map_Clone);

    REGISTER_UMOCK_ALIAS_TYPE(MAP_FILTER_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(BUFFER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);

    REGISTER_STRING_GLOBAL_MOCK_HOOK;
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

    g_mapFilterFunc = NULL;
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

/* EventData_CreateWithNewMemory */

/* Tests_SRS_EVENTDATA_03_008: [EventData_CreateWithNewMemory shall allocate new memory to store the specified data.] */
/* Tests_SRS_EVENTDATA_03_003: [EventData_Create shall return a NULL value if length is not zero and data is NULL.] */
/* Tests_SRS_EVENTDATA_03_002: [EventData_CreateWithNewMemory shall provide a none-NULL handle encapsulating the storage of the data provided.] */
TEST_FUNCTION(EventData_CreateWithNewMemory_with_zero_length_and_null_data_Succeeds)
{
    // arrange

    // act
    EVENTDATA_HANDLE result = EventData_CreateWithNewMemory(NULL, 0);

    // assert
    ASSERT_IS_NOT_NULL(result);

    // cleanup
    EventData_Destroy(result);
}

/* Tests_SRS_EVENTDATA_03_008: [EventData_CreateWithNewMemory shall allocate new memory to store the specified data.] */
/* Tests_SRS_EVENTDATA_03_003: [If data is not NULL and length is zero, EventData_Create shall return a NULL value.]  */
/* Tests_SRS_EVENTDATA_03_002: [EventData_CreateWithNewMemory shall provide a none-NULL handle encapsulating the storage of the data provided.] */
TEST_FUNCTION(EventData_CreateWithNewMemory_with_zero_length_and_none_null_data_Succeeds)
{
    // arrange
    unsigned char myData[] = { 0x42, 0x43, 0x44 };

    // act
    EVENTDATA_HANDLE result = EventData_CreateWithNewMemory(myData, 0);

    // assert
    ASSERT_IS_NOT_NULL(result);

    // cleanup
    EventData_Destroy(result);
}

/* Tests_SRS_EVENTDATA_03_008: [EventData_CreateWithNewMemory shall allocate new memory to store the specified data.] */
/* Tests_SRS_EVENTDATA_03_003: [EventData_Create shall return a NULL value if length is not zero and data is NULL.] */
/* Tests_SRS_EVENTDATA_03_002: [EventData_CreateWithNewMemory shall provide a none-NULL handle encapsulating the storage of the data provided.] */
TEST_FUNCTION(EventData_CreateWithNewMemory_with_none_zero_length_and_none_null_data_Succeeds)
{
    // arrange
    unsigned char myData[] = { 0x42, 0x43, 0x44 };
    size_t length = sizeof(myData);

    // act
    EVENTDATA_HANDLE result = EventData_CreateWithNewMemory(myData, length);

    // assert
    ASSERT_IS_NOT_NULL(result);

    // cleanup
    EventData_Destroy(result);
}

/* Tests_SRS_EVENTDATA_03_003: [EventData_Create shall return a NULL value if length is not zero and data is NULL.] */
TEST_FUNCTION(EventData_CreateWithNewMemory_with_none_zero_length_and_null_data_Fails)
{
    // arrange

    // act
    EVENTDATA_HANDLE result = EventData_CreateWithNewMemory(NULL, (size_t)25);

    // assert
    ASSERT_IS_NULL(result);
}

/* Tests_SRS_EVENTDATA_03_008: [EventData_CreateWithNewMemory shall allocate new memory to store the specified data.] */
/* Tests_SRS_EVENTDATA_03_002: [EventData_CreateWithNewMemory shall provide a none-NULL handle encapsulating the storage of the data provided.] */
TEST_FUNCTION(EventData_CreateWithNewMemory_with_none_null_data_and_none_matching_length_Succeeds)
{
    // arrange
    unsigned char myData[] = { 0x42, 0x43, 0x44 };
    size_t length = 1;

    // act
    EVENTDATA_HANDLE result = EventData_CreateWithNewMemory(myData, length);

    // assert
    ASSERT_IS_NOT_NULL(result);

    // cleanup
    EventData_Destroy(result);
}

/* EventData_Destroy */

/* Tests_SRS_EVENTDATA_03_005: [EventData_Destroy shall deallocate all resources related to the eventDataHandle specified.] */
TEST_FUNCTION(EvenData_Destroy_with_valid_handle_Succeeds)
{
    // arrange
    unsigned char myData[] = { 0x42, 0x43, 0x44 };
    size_t length = sizeof(myData);
    EVENTDATA_HANDLE eventHandle = EventData_CreateWithNewMemory(myData, length);

    // act
    EventData_Destroy(eventHandle);

    // assert
    // Implicit - No crash
}

/* Tests_SRS_EVENTDATA_03_006: [EventData_Destroy shall not do anything if eventDataHandle is NULL.] */
TEST_FUNCTION(EvenData_Destroy_with_NULL_handle_NoAction)
{
    // arrange

    // act
    EventData_Destroy(NULL);

    // assert
    // Implicit - No crash
}

/* EventData_GetData */

/* Tests_SRS_EVENTDATA_03_022: [If any of the arguments passed to EventData_GetData is NULL, EventData_GetData shall return EVENTDATA_INVALID_ARG.] */
TEST_FUNCTION(EventData_GetData_with_NULL_handle_fails)
{
    // arrange
    const unsigned char* buffer;
    size_t size;

    // act
    EVENTDATA_RESULT result = EventData_GetData(NULL, &buffer, &size);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG, result);

    // cleanup
}

/* Tests_SRS_EVENTDATA_03_022: [If any of the arguments passed to EventData_GetData is NULL, EventData_GetData shall return EVENTDATA_INVALID_ARG.] */
TEST_FUNCTION(EventData_GetData_with_NULL_buffer_fails)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    size_t actualSize;
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    // act
    EVENTDATA_RESULT result = EventData_GetData(eventDataHandle, NULL, &actualSize);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_03_022: [If any of the arguments passed to EventData_GetData is NULL, EventData_GetData shall return EVENTDATA_INVALID_ARG.] */
TEST_FUNCTION(EventData_GetData_with_NULL_size_fails)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    const unsigned char* actualDdata;
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    // act
    EVENTDATA_RESULT result = EventData_GetData(eventDataHandle, &actualDdata, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_03_019: [EventData_GetData shall provide a pointer and size for the data associated with the eventDataHandle.] */
/* Tests_SRS_EVENTDATA_03_020: [The pointer shall be obtained by using BUFFER_content and it shall be copied in the buffer argument. The size of the associated data shall be obtained by using BUFFER_size and it shall be copied to the size argument.] */
/* Tests_SRS_EVENTDATA_03_021: [On success, EventData_GetData shall return EVENTDATA_OK.] */
TEST_FUNCTION(EventData_GetData_with_valid_args_retrieves_data_and_size_from_the_underlying_BUFFER_Successfully)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    unsigned char* testBuffer = expectedData;
    size_t expectedSize = sizeof(expectedData);
    const unsigned char* actualDdata;
    size_t actualSize;
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(BUFFER_content(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &testBuffer, sizeof(testBuffer));
    STRICT_EXPECTED_CALL(BUFFER_size(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &expectedSize, sizeof(expectedSize));

    // act
    EVENTDATA_RESULT result = EventData_GetData(eventDataHandle, &actualDdata, &actualSize);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);
    ASSERT_ARE_EQUAL(void_ptr, testBuffer, actualDdata);
    ASSERT_ARE_EQUAL(size_t, expectedSize, actualSize);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_03_023: [If EventData_GetData fails because of any other error it shall return EVENTDATA_ERROR.]*/
TEST_FUNCTION(When_BUFFER_size_fails_EventData_GetData_fails)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    unsigned char* testBuffer = expectedData;
    size_t expectedSize = sizeof(expectedData);
    const unsigned char* actualDdata;
    size_t actualSize;
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(BUFFER_content(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &testBuffer, sizeof(testBuffer));
    STRICT_EXPECTED_CALL(BUFFER_size(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &expectedSize, sizeof(expectedSize))
        .SetReturn(1);

    // act
    EVENTDATA_RESULT result = EventData_GetData(eventDataHandle, &actualDdata, &actualSize);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_03_023: [If EventData_GetData fails because of any other error it shall return EVENTDATA_ERROR.]*/
TEST_FUNCTION(When_BUFFER_content_fails_EventData_GetData_fails)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    unsigned char* testBuffer = expectedData;
    size_t expectedSize = sizeof(expectedData);
    const unsigned char* actualDdata;
    size_t actualSize;
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(BUFFER_content(IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .CopyOutArgumentBuffer(2, &testBuffer, sizeof(testBuffer))
        .SetReturn(1);

    // act
    EVENTDATA_RESULT result = EventData_GetData(eventDataHandle, &actualDdata, &actualSize);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_ERROR, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_031: [EventData_SetPartitionKey shall return EVENTDATA_INVALID_ARG if eventDataHandle parameter is NULL.] */
TEST_FUNCTION(EventData_SetPartitionKey_EVENTDATA_HANDLE_NULL_FAIL)
{
    // arrange
    EVENTDATA_RESULT result;
    umock_c_reset_all_calls();

    // act
    result = EventData_SetPartitionKey(NULL, PARTITION_KEY_VALUE);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

/* Tests_SRS_EVENTDATA_07_029: [if the partitionKey parameter is NULL EventData_SetPartitionKey shall not assign any value and return EVENTDATA_OK.] */
TEST_FUNCTION(EventData_SetPartitionKey_Partition_key_NULL_FAIL)
{
    // arrange
    EVENTDATA_RESULT result;
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    // act
    result = EventData_SetPartitionKey(eventDataHandle, NULL);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_030: [On Success EventData_SetPartitionKey shall return EVENTDATA_OK.] */
/* Tests_SRS_EVENTDATA_07_028: [On success EventData_SetPartitionKey shall store the const char* partitionKey parameter in the EVENTDATA_HANDLE data structure partitionKey variable.] */
TEST_FUNCTION(EventData_SetPartitionKey_Partition_Key_SUCCEED)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    EVENTDATA_RESULT result;

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_construct(PARTITION_KEY_VALUE));

    // act
    result = EventData_SetPartitionKey(eventDataHandle, PARTITION_KEY_VALUE);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_029: [if the partitionKey parameter is a zero length string EventData_SetPartitionKey shall return a nonzero value and will remove an existing partition Key value.] */
TEST_FUNCTION(EventData_SetPartitionKey_Partition_key_Zero_String_SUCCEED)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    EVENTDATA_RESULT result;

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_construct(PARTITION_KEY_ZERO_VALUE));

    // act
    result = EventData_SetPartitionKey(eventDataHandle, PARTITION_KEY_ZERO_VALUE);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_027: [If the partitionKey variable contained in the eventDataHandle parameter is not NULL then EventData_SetPartitionKey shall delete the partitionKey STRING_HANDLE.] */
TEST_FUNCTION(EventData_SetPartitionKey_Partition_key_NOT_NULL_SUCCEED)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    EVENTDATA_RESULT result;

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_construct(PARTITION_KEY_VALUE));
    STRICT_EXPECTED_CALL(STRING_delete(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(STRING_construct(PARTITION_KEY_VALUE));

    // act
    result = EventData_SetPartitionKey(eventDataHandle, PARTITION_KEY_VALUE);
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    result = EventData_SetPartitionKey(eventDataHandle, PARTITION_KEY_VALUE);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_024: [EventData_GetPartitionKey shall return NULL if the eventDataHandle parameter is NULL.] */
TEST_FUNCTION(EventData_GetPartitionKey_EVENTDATA_HANDLE_NULL_FAIL)
{
    // arrange

    // act
    const char* result = EventData_GetPartitionKey(NULL);

    // assert
    ASSERT_IS_NULL(result);

    // cleanup
}

/* Tests_SRS_EVENTDATA_07_025: [EventData_GetPartitionKey shall return NULL if the partitionKey is in the EVENTDATA_HANDLE is NULL.] */
TEST_FUNCTION(EventData_GetPartitionKey_PartitionKey_NULL_FAIL)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    const char* result;

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    // act
    result = EventData_GetPartitionKey(eventDataHandle);

    // assert
    ASSERT_IS_NULL(result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_026: [On success EventData_GetPartitionKey shall return a const char* variable that is pointing to the Partition Key value that is stored in the EVENTDATA_HANDLE.] */
TEST_FUNCTION(EventData_GetPartitionKey_SUCCEED)
{
    // arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    int partitionKeyRet = EventData_SetPartitionKey(eventDataHandle, PARTITION_KEY_VALUE);
    ASSERT_ARE_EQUAL(int, 0, partitionKeyRet);

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_c_str(IGNORED_PTR_ARG));

    // act
    const char* result = EventData_GetPartitionKey(eventDataHandle);

    // assert
    ASSERT_IS_NOT_NULL(result);
    ASSERT_ARE_EQUAL(char_ptr, result, PARTITION_KEY_VALUE);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

/* Tests_SRS_EVENTDATA_07_034: [if eventDataHandle is NULL then EventData_Properties shall return NULL.] */
TEST_FUNCTION(EventData_Properties_with_NULL_handle_returns_NULL)
{
    ///arrange

    ///act
    MAP_HANDLE result = EventData_Properties(NULL);

    ///assert
    ASSERT_IS_NULL(result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    ///cleanup
}

TEST_FUNCTION(EventData_Map_Filter_Succeed)
{
    ///arrange
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    ///act
    const char validNameChar[] = "validNameChar";
    const char validValueChar[] = "validValueChar";

    char invalidNameChar[NUMBER_OF_CHAR];
    char invalidValueChar[NUMBER_OF_CHAR];
    for (size_t index = 0; index < NUMBER_OF_CHAR; index++)
    {
        invalidNameChar[index] = (char)index+2;
        invalidValueChar[index] = (char)index+2;
    }

    int result1 = g_mapFilterFunc(validNameChar, validValueChar);
    int result2 = g_mapFilterFunc(invalidNameChar, invalidValueChar);
    int result3 = g_mapFilterFunc(invalidNameChar, validValueChar);
    int result4 = g_mapFilterFunc(validNameChar, invalidValueChar);
    int result5 = g_mapFilterFunc(NULL, validValueChar);
    int result6 = g_mapFilterFunc(validNameChar, NULL);
    int result7 = g_mapFilterFunc(NULL, NULL);

    ///assert
    ASSERT_ARE_EQUAL(int, 0, result1);
    ASSERT_ARE_NOT_EQUAL(int, 0, result2);
    ASSERT_ARE_NOT_EQUAL(int, 0, result3);
    ASSERT_ARE_NOT_EQUAL(int, 0, result4);
    ASSERT_ARE_EQUAL(int, 0, result5);
    ASSERT_ARE_EQUAL(int, 0, result6);
    ASSERT_ARE_EQUAL(int, 0, result7);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    ///cleanup
    EventData_Destroy(eventDataHandle);
}

//**Tests_SRS_EVENTDATA_29_060: \[**`EventData_SetEnqueuedTimestampUTCInMs` shall return EVENTDATA_INVALID_ARG if eventDataHandle parameter is NULL.**\]**
TEST_FUNCTION(EventData_EventData_SetEnqueuedTimestampUTC_NULL_Param_Handle)
{
    // arrange
    EVENTDATA_RESULT result;

    // act
    result = EventData_SetEnqueuedTimestampUTCInMs(NULL, 10);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG, result);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
}

//**Tests_SRS_EVENTDATA_29_061: \[**On success `EventData_SetEnqueuedTimestampUTCInMs` shall store the timestamp parameter in the EVENTDATA_HANDLE data structure.**\]**
//**Tests_SRS_EVENTDATA_29_062: \[**On Success `EventData_SetEnqueuedTimestampUTCInMs` shall return EVENTDATA_OK.**\]**
TEST_FUNCTION(EventData_EventData_SetEnqueuedTimestampUTC_Success)
{
    // arrange
    EVENTDATA_RESULT result;
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);
    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    umock_c_reset_all_calls();

    // act
    result = EventData_SetEnqueuedTimestampUTCInMs(eventDataHandle, 10);

    // assert
    ASSERT_ARE_EQUAL(EVENTDATA_RESULT, EVENTDATA_OK, result);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

//**Tests_SRS_EVENTDATA_07_070: \[**`EventData_GetEnqueuedTimestampUTCInMs` shall return 0 if the eventDataHandle parameter is NULL.**\]**
TEST_FUNCTION(EventData_EventData_GetEnqueuedTimestampUTC_NULL_Param_Handle)
{
    //arrange
    uint64_t timestamp;

    // act
    timestamp = EventData_GetEnqueuedTimestampUTCInMs(NULL);

    //assert
    ASSERT_ARE_EQUAL(uint64_t, 0, timestamp);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    //cleanup
}

//**Tests_SRS_EVENTDATA_07_071: \[**If eventDataHandle is not null, `EventData_GetEnqueuedTimestampUTCInMs` shall return the timestamp value stored in the EVENTDATA_HANDLE.**\]**
TEST_FUNCTION(EventData_EventData_GetEnqueuedTimestampUTC_Success)
{
    // arrange
    uint64_t timestamp = 0;
    unsigned char expectedData[] = { 0x42, 0x43, 0x44 };
    size_t expectedSize = sizeof(expectedData);

    EVENTDATA_HANDLE eventDataHandle = EventData_CreateWithNewMemory(expectedData, expectedSize);
    timestamp = EventData_GetEnqueuedTimestampUTCInMs(eventDataHandle);
    ASSERT_ARE_EQUAL(uint64_t, (uint64_t)0, timestamp, "Initial Timestamp Expected To Be Zero.");

    (void)EventData_SetEnqueuedTimestampUTCInMs(eventDataHandle, 10);
    umock_c_reset_all_calls();

    // act
    timestamp = EventData_GetEnqueuedTimestampUTCInMs(eventDataHandle);

    // assert
    ASSERT_ARE_EQUAL(uint64_t, 10, timestamp, "Invalid timestamp value seen.");
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    EventData_Destroy(eventDataHandle);
}

END_TEST_SUITE(eventdata_unittests)
