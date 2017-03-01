// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdbool>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif
#include <signal.h>

static void* TestHook_malloc(size_t size)
{
    return malloc(size);
}

static void TestHook_free(void* ptr)
{
    free(ptr);
}

#include "testrunnerswitcher.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umock_c_negative_tests.h"

#define ENABLE_MOCKS
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/string_tokenizer.h"
#undef  ENABLE_MOCKS

// interface under test
#include "kvp_parser.h"


//#################################################################################################
// KVP Parser Test Defines and Data types
//#################################################################################################
#define TEST_INPUT_STRING_HANDLE    (STRING_HANDLE)0x1000
#define TEST_KEY_STRING_HANDLE      (STRING_HANDLE)0x1001
#define TEST_VALUE_STRING_HANDLE    (STRING_HANDLE)0x1002

#define TEST_TOKENIZER_HANDLE       (STRING_TOKENIZER_HANDLE)0x2000

#define TEST_MAP_HANDLE             (MAP_HANDLE)0x2000

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

typedef struct KVPTag
{
    const char* key;
    const char* value;
} KVP;

typedef struct TestKVPTestDataTag
{
    size_t numPairs;
    const char* keyDelim;
    const char* valueDelim;
    KVP* kvpPairs;
} TestKVPTestData;

typedef struct TestKVPGlobalTag
{
    TestKVPTestData* currentKVPTestData;
    int currentKVPIndex;
    int rollOverIndex;
} TestKVPGlobal;

//#################################################################################################
// KVP Parser Test Data
//#################################################################################################
static TEST_MUTEX_HANDLE gTestByTest;
static TEST_MUTEX_HANDLE gDllByDll;
static TestKVPGlobal gTestData;

static const char KEY1[] = "key1";
static const char KEY2[] = "key2";
static const char VALUE1[] = "value1";
static const char VALUE2[] = "value2";
static const char UNUSED_KVP_STRING[] = "unused";

//#################################################################################################
// KVP Parser Test Hook Implementations
//#################################################################################################    
static void TestHook_OnUMockCError(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

//#################################################################################################
// KVP Parser Test Helper Implementations
//#################################################################################################
void TestHelper_ResetTestGlobalData(TestKVPTestData* testData)
{
    gTestData.currentKVPTestData = testData;
    gTestData.currentKVPIndex = -1;
}

const char* TestHook_STRING_c_str(STRING_HANDLE handle)
{
    const char* result;

    if (gTestData.currentKVPIndex < 0)
    {
        result = NULL;
    }
    else
    {
        if (handle == TEST_KEY_STRING_HANDLE)
        {
            result = gTestData.currentKVPTestData->kvpPairs[gTestData.currentKVPIndex].key;
        }
        else if (handle == TEST_VALUE_STRING_HANDLE)
        {
            result = gTestData.currentKVPTestData->kvpPairs[gTestData.currentKVPIndex].value;
        }
        else
        {
            result = NULL;
        }
    }

    return result;
}

STRING_TOKENIZER_HANDLE TestHook_STRING_TOKENIZER_create(STRING_HANDLE handle)
{
    (void)handle;

    gTestData.currentKVPIndex = 0;
    gTestData.rollOverIndex = 0;
    return TEST_TOKENIZER_HANDLE;
}

int TestHook_STRING_TOKENIZER_get_next_token(STRING_TOKENIZER_HANDLE tokenizer, STRING_HANDLE output, const char* delimiters)
{
    int result;
    (void)tokenizer;
    (void)delimiters;

    if (gTestData.currentKVPTestData->numPairs == 0)
    {
        result = -1;
    }
    else
    {
        if (output == TEST_VALUE_STRING_HANDLE)
        {
            gTestData.rollOverIndex = 1;
            result = 0;
        }
        else if (output == TEST_KEY_STRING_HANDLE)
        {
            if (gTestData.rollOverIndex == 1)
            {
                gTestData.rollOverIndex = 0;
                gTestData.currentKVPIndex++;
            }

            if (gTestData.currentKVPIndex >= (int)gTestData.currentKVPTestData->numPairs)
            {
                result = -1;
            }
            else
            {
                result = 0;
            }
        }
        else
        {
            result = -1;
        }
    }

    return result;
}

//**Tests_SRS_KVP_PARSER_29_002: \[**kvp_parser_parse shall create a STRING handle using API STRING_construct with parameter as input_string.**\]**
//**Tests_SRS_KVP_PARSER_29_003: \[**kvp_parser_parse shall create a STRING tokenizer to be used for parsing the input_string handle, by calling STRING_TOKENIZER_create.**\]**
//**Tests_SRS_KVP_PARSER_29_004: \[**kvp_parser_parse shall start scanning at the beginning of the input_string handle.**\]**
//**Tests_SRS_KVP_PARSER_29_005: \[**kvp_parser_parse shall return NULL if STRING_TOKENIZER_create fails.**\]**
//**Tests_SRS_KVP_PARSER_29_006: \[**kvp_parser_parse shall allocate 2 STRING handles to hold the to be parsed key and value tokens using API STRING_new.**\]**
//**Tests_SRS_KVP_PARSER_29_007: \[**kvp_parser_parse shall return NULL if allocating the STRING handles fail.**\]**
//**Tests_SRS_KVP_PARSER_29_008: \[**kvp_parser_parse shall create a Map to hold the to be parsed keys and values using API Map_Create.**\]**
//**Tests_SRS_KVP_PARSER_29_009: \[**kvp_parser_parse shall return NULL if Map_Create fails.**\]**
//**Tests_SRS_KVP_PARSER_29_010: \[**kvp_parser_parse shall perform the following actions until parsing is complete.**\]**
//**Tests_SRS_KVP_PARSER_29_011: \[**kvp_parser_parse shall find a token (the key of the key/value pair) delimited by the key_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**
//**Tests_SRS_KVP_PARSER_29_012: \[**kvp_parser_parse shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**
//**Tests_SRS_KVP_PARSER_29_013: \[**kvp_parser_parse shall find a token (the value of the key/value pair) delimited by the value_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**
//**Tests_SRS_KVP_PARSER_29_014: \[**kvp_parser_parse shall fail and return NULL if STRING_TOKENIZER_get_next_token fails (freeing the allocated result map).**\]**
//**Tests_SRS_KVP_PARSER_29_015: \[**kvp_parser_parse shall obtain the C string for the key from the previously parsed key STRING by using STRING_c_str.**\]**
//**Tests_SRS_KVP_PARSER_29_016: \[**kvp_parser_parse shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**
//**Tests_SRS_KVP_PARSER_29_017: \[**kvp_parser_parse shall fail and return NULL if the key length is zero (freeing the allocated result map).**\]**
//**Tests_SRS_KVP_PARSER_29_018: \[**kvp_parser_parse shall obtain the C string for the key from the previously parsed value STRING by using STRING_c_str.**\]**
//**Tests_SRS_KVP_PARSER_29_019: \[**kvp_parser_parse shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**
//**Tests_SRS_KVP_PARSER_29_020: \[**kvp_parser_parse shall add the key and value to the result map by using Map_Add.**\]**
//**Tests_SRS_KVP_PARSER_29_021: \[**kvp_parser_parse shall fail and return NULL if Map_Add fails (freeing the allocated result map).**\]**
//**Tests_SRS_KVP_PARSER_29_022: \[**kvp_parser_parse shall free up the allocated STRING handle for holding the parsed value string using API STRING_delete after parsing is complete.**\]**
//**Tests_SRS_KVP_PARSER_29_023: \[**kvp_parser_parse shall free up the allocated STRING handle for holding the parsed key string using API STRING_delete after parsing is complete.**\]**
//**Tests_SRS_KVP_PARSER_29_024: \[**kvp_parser_parse shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**
//**Tests_SRS_KVP_PARSER_29_025: \[**kvp_parser_parse shall free up the allocated input_string handle using API STRING_delete.**\]**
//**Tests_SRS_KVP_PARSER_29_026: \[**kvp_parser_parse shall return the created MAP_HANDLE.**\]**
uint64_t TestHelper_SetupKVPParserStack(TestKVPTestData* testData)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    size_t ctr;
    bool isError;

    // arrange
    TestHelper_ResetTestGlobalData(testData);
    umock_c_reset_all_calls();

    EXPECTED_CALL(STRING_construct(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_create(TEST_INPUT_STRING_HANDLE));
    failedFunctionBitmask |= ((uint64_t)1 << i++);
    
    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_KEY_STRING_HANDLE);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_VALUE_STRING_HANDLE);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_Create(NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    ctr = 0;
    isError = false;
    while ((isError == false) && (ctr < testData->numPairs))
    {
        size_t keySize = (testData->kvpPairs[ctr].key != NULL) ? strlen(testData->kvpPairs[ctr].key) : 0;
        size_t valueSize = (testData->kvpPairs[ctr].value != NULL) ? strlen(testData->kvpPairs[ctr].value) : 0;

        bool isKeyError = (testData->kvpPairs[ctr].key == NULL) ? true : ((keySize == 0) ? true : false);
        bool isValueError = (testData->kvpPairs[ctr].value == NULL) ? true : false;

        STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_TOKENIZER_HANDLE, TEST_KEY_STRING_HANDLE, IGNORED_PTR_ARG))
            .IgnoreArgument(3).ValidateArgumentBuffer(3, testData->keyDelim, strlen(testData->keyDelim));
        i++;

        STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_TOKENIZER_HANDLE, TEST_VALUE_STRING_HANDLE, IGNORED_PTR_ARG))
            .IgnoreArgument(3).ValidateArgumentBuffer(3, testData->valueDelim, strlen(testData->valueDelim));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_KEY_STRING_HANDLE));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        if (isKeyError == false)
        {
            STRICT_EXPECTED_CALL(STRING_c_str(TEST_VALUE_STRING_HANDLE));
            failedFunctionBitmask |= ((uint64_t)1 << i++);
        }

        if ((isKeyError == false) && (isValueError == false))
        {
            STRICT_EXPECTED_CALL(Map_Add(TEST_MAP_HANDLE, testData->kvpPairs[ctr].key, testData->kvpPairs[ctr].value));
            failedFunctionBitmask |= ((uint64_t)1 << i++);
        }

        ctr++;
        isError = (isKeyError || isValueError) ? true : false;
    }

    if (isError == false)
    {
        STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_TOKENIZER_HANDLE, TEST_KEY_STRING_HANDLE, IGNORED_PTR_ARG))
            .IgnoreArgument(3).ValidateArgumentBuffer(3, testData->keyDelim, strlen(testData->keyDelim));
        i++;
    }
    else
    {
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE));
        i++;
    }

    STRICT_EXPECTED_CALL(STRING_delete(TEST_VALUE_STRING_HANDLE));
    i++;

    STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
    i++;

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_destroy(TEST_TOKENIZER_HANDLE));
    i++;

    STRICT_EXPECTED_CALL(STRING_delete(TEST_INPUT_STRING_HANDLE));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE_WITH_MSG((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//#################################################################################################
// KVP Parser Tests
//#################################################################################################
BEGIN_TEST_SUITE(kvp_parser_unittests)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    TEST_INITIALIZE_MEMORY_DEBUG(gDllByDll);
    gTestByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(gTestByTest);

    umock_c_init(TestHook_OnUMockCError);
    
    REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(STRING_TOKENIZER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_FILTER_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_RESULT, int);

    REGISTER_GLOBAL_MOCK_HOOK(STRING_TOKENIZER_create, TestHook_STRING_TOKENIZER_create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_TOKENIZER_create, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_TOKENIZER_get_next_token, TestHook_STRING_TOKENIZER_get_next_token);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_TOKENIZER_get_next_token, -1);

    REGISTER_GLOBAL_MOCK_RETURN(STRING_construct, TEST_INPUT_STRING_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_construct, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_c_str, TestHook_STRING_c_str);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_c_str, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(Map_Create, TEST_MAP_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_Create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(Map_Add, (MAP_RESULT)0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_Add, (MAP_RESULT)-1);
}

TEST_SUITE_CLEANUP(TestClassCleanup)
{
    umock_c_deinit();
    TEST_MUTEX_DESTROY(gTestByTest);
    TEST_DEINITIALIZE_MEMORY_DEBUG(gDllByDll);
}

TEST_FUNCTION_INITIALIZE(TestMethodInitialize)
{
    if (TEST_MUTEX_ACQUIRE(gTestByTest))
    {
        ASSERT_FAIL("Mutex is ABANDONED. Failure in test framework");
    }
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(gTestByTest);
}

//#################################################################################################
// kvp_parser_parse Tests
//#################################################################################################

//**Tests_SRS_KVP_PARSER_29_001: \[**kvp_parser_parse shall return NULL if either input_string, key_delim or value_delim is NULL.**\]**
TEST_FUNCTION(kvp_parse_NULLParam_input)
{
    // arrange
    MAP_HANDLE map;

    // act
    map = kvp_parser_parse(NULL, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");
}

//**Tests_SRS_KVP_PARSER_29_001: \[**kvp_parser_parse shall return NULL if either input_string, key_delim or value_delim is NULL.**\]**
TEST_FUNCTION(kvp_parse_NULLParam_key_delim)
{
    // arrange
    MAP_HANDLE map;

    // act
    map = kvp_parser_parse(UNUSED_KVP_STRING, NULL, ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");
}

//**Tests_SRS_KVP_PARSER_29_001: \[**kvp_parser_parse shall return NULL if either input_string, key_delim or value_delim is NULL.**\]**
TEST_FUNCTION(kvp_parse_NULLParam_value_delim)
{
    // arrange
    MAP_HANDLE map;

    // act
    map = kvp_parser_parse(UNUSED_KVP_STRING, "=", NULL);

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");
}

TEST_FUNCTION(kvp_parse_ZeroKVP_Success)
{
    // arrange
    KVP *pairs = NULL;
    size_t numPairs = 0;
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, TEST_MAP_HANDLE, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_OneKVP_Success)
{
    // arrange
    KVP pairs[] = { { KEY1, VALUE1 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, TEST_MAP_HANDLE, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_Success)
{
    // arrange
    KVP pairs[] = { { KEY1, VALUE1 },{ KEY2, VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "==", "&&", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "==", "&&");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, TEST_MAP_HANDLE, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_OneKVP_InvalidNULLKeyToken)
{
    // arrange
    KVP pairs[] = { { NULL, VALUE1 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_OneKVP_InvalidZeroLenKeyToken)
{
    // arrange
    KVP pairs[] = { { "", VALUE1 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidFirstNULLKeyToken)
{
    // arrange
    KVP pairs[] = { { NULL, VALUE1 },{ KEY2, VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidFirstZeroLenKeyToken)
{
    // arrange
    KVP pairs[] = { { "", VALUE1 },{ KEY2, VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidSecondNULLKeyToken)
{
    // arrange
    KVP pairs[] = { { KEY1, VALUE1 },{ NULL, VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidSecondZeroLenKeyToken)
{
    // arrange
    KVP pairs[] = { { KEY1, VALUE1 },{ "", VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_OneKVP_InvalidNULLValueToken)
{
    // arrange
    KVP pairs[] = { { KEY1, NULL } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidFirstNULLValueToken)
{
    // arrange
    KVP pairs[] = { { KEY1, NULL },{ KEY2, VALUE2 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_TwoKVP_InvalidSecondNULLValueToken)
{
    // arrange
    KVP pairs[] = { { KEY1, VALUE1 },{ KEY2, NULL } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    (void)TestHelper_SetupKVPParserStack(&testData);

    // act
    MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");

    // assert
    ASSERT_ARE_EQUAL_WITH_MSG(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");

    // cleanup
}

TEST_FUNCTION(kvp_parse_NegativeTest)
{
    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);
    KVP pairs[] = { { KEY1, VALUE1 } };
    size_t numPairs = sizeof(pairs) / sizeof(pairs[0]);
    TestKVPTestData testData = { numPairs, "=", ";", pairs };
    uint64_t failedCallBitmask = TestHelper_SetupKVPParserStack(&testData);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            MAP_HANDLE map = kvp_parser_parse(UNUSED_KVP_STRING, "=", ";");
            // assert
            ASSERT_ARE_EQUAL_WITH_MSG(void_ptr, NULL, map, "Failed MAP Handle Test");
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

END_TEST_SUITE(kvp_parser_unittests)
