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

#include <time.h>

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
#include "azure_c_shared_utility/agenttime.h"
#include "azure_c_shared_utility/base64.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/sastoken.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/string_tokenizer.h"
#include "azure_c_shared_utility/urlencode.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"

#include "kvp_parser.h"
#undef  ENABLE_MOCKS

// interface under test
#include "eventhubauth.h"

//#################################################################################################
// EventHubAuth Test Defines and Data types
//#################################################################################################
DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

#define AUTH_WAITTIMEOUT_SECS 1
#define AUTH_EXPIRATION_SECS  4
#define AUTH_REFRESH_SECS     2

#define TEST_STRING_HANDLE_AUTO_HOSTNAME                (STRING_HANDLE)0x100
#define TEST_STRING_HANDLE_AUTO_EVENTHUBPATH            (STRING_HANDLE)0x101
#define TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME     (STRING_HANDLE)0x102
#define TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME_CLONE (STRING_HANDLE)0x103
#define TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEY         (STRING_HANDLE)0x104
#define TEST_STRING_HANDLE_AUTO_CONSUMERGROUP           (STRING_HANDLE)0x105
#define TEST_STRING_HANDLE_AUTO_PARTITIONID             (STRING_HANDLE)0x106
#define TEST_STRING_HANDLE_AUTO_URIENCODED              (STRING_HANDLE)0x107
#define TEST_STRING_HANDLE_AUTO_KEYBASE64ENCODED        (STRING_HANDLE)0x108
#define TEST_STRING_HANDLE_AUTO_PUBLISHER_VALUE         (STRING_HANDLE)0x109
#define TEST_STRING_HANDLE_AUTO_SENDER_URI              (STRING_HANDLE)0x110
#define TEST_STRING_HANDLE_AUTO_RECEIVER_URI            (STRING_HANDLE)0x111
#define TEST_STRING_HANDLE_AUTO_URI                     (STRING_HANDLE)0x112
#define TEST_STRING_HANDLE_AUTO_SASTOKEN                (STRING_HANDLE)0x113

#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME         (STRING_HANDLE)0x201
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH     (STRING_HANDLE)0x202
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PUBLISHER_VALUE  (STRING_HANDLE)0x203
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_CONSUMER_GROUP_VALUE (STRING_HANDLE)0x204
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE (STRING_HANDLE)0x205
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_SENDER           (STRING_HANDLE)0x206
#define TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_SENDER          (STRING_HANDLE)0x207
#define TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE            (STRING_HANDLE)0x208
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER         (STRING_HANDLE)0x209
#define TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_RECEIVER        (STRING_HANDLE)0x210
#define TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE          (STRING_HANDLE)0x211
#define TEST_STRING_HANDLE_EXT_SHAREDACCESSKEYNAME              (STRING_HANDLE)0x212
#define TEST_STRING_HANDLE_EXT_SHAREDACCESSKEYNAME_CLONE        (STRING_HANDLE)0x213
#define TEST_STRING_HANDLE_EXT_SENDER_URI                       (STRING_HANDLE)0x214
#define TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE                 (STRING_HANDLE)0x215
#define TEST_STRING_HANDLE_EXT_RECEIVER_URI                     (STRING_HANDLE)0x216
#define TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE               (STRING_HANDLE)0x217
#define TEST_STRING_HANDLE_EXT_PUBLISHER_VALUE                  (STRING_HANDLE)0x218

#define TEST_DUMMY_STRING_HANDLE                                (STRING_HANDLE)0x700


#define TEST_SESSION_HANDLE_VALID                       (SESSION_HANDLE)0x300

#define TEST_BUFFER_HANDLE_VALID_SHAREDACCESSKEY        (BUFFER_HANDLE)0x400

#define TEST_CBS_HANDLE_VALID                           (CBS_HANDLE)0x500

#define TEST_MAP_HANDLE_VALID                           (MAP_HANDLE)0x600

#define TEST_TIME_T_INVALID                             (time_t)-1

#define TEST_STRING_TOKENIZER_HANDLE                    (STRING_TOKENIZER_HANDLE)0x700
#define TEST_STRING_KEY_TOKEN_HANDLE                    (STRING_HANDLE)0x701

typedef struct TEST_HOOK_MAP_KVP_STRUCT_TAG
{
    const char*   key;
    STRING_HANDLE handle;
    STRING_HANDLE cloneHandle;
} TEST_HOOK_MAP_KVP_STRUCT;

typedef struct TESTGLOBAL_TAG
{
    ON_CBS_OPERATION_COMPLETE sasTokenPutCB;
    void* sasTokenPutCBContext;
    ON_CBS_OPERATION_COMPLETE sasTokenDelCB;
    void* sasTokenDelCBContext;
} TESTGLOBAL;

//#################################################################################################
// EventHubAuth Test Data
//#################################################################################################
static TEST_MUTEX_HANDLE g_testByTest;
static TESTGLOBAL g_TestGlobal;

#define HOSTNAME_VALUE_DEF 		    "servicebusName.servicebus.windows.net"
#define EVENTHUB_PATH_VALUE_DEF 	"eventHubName"
#define URI_KEY_DEF                 "sr"
#define SKN_KEY_DEF                 "skn"
#define SKN_VALUE_DEF               "RootManageSharedAccessKey"
#define EXPIRATION_KEY_DEF          "se"
#define EXPIRATION_VALUE_DEF        "1000"
#define SIGNATURE_KEY_DEF           "sig"
#define SIGNATURE_VALUE_DEF         "0123456789ABCDEF0123456789ABCDEF0123456789AB"
#define CONSUMER_GROUP_KEY_DEF      "ConsumerGroups"
#define CONSUMER_GROUP_VALUE_DEF    "$Default"
#define PARTITION_ID_KEY_DEF        "Partitions"
#define PARTITION_ID_VALUE_DEF      "0"
#define PUBLISHER_KEY_DEF           "publishers"
#define PUBLISHER_VALUE_DEF         "sender"
#define SAS_TOKEN_START_DEF         "SharedAccessSignature "
#define SB_STRING_DEF               "sb://"
#define SB_STRING_ENCODED_DEF       "sb%3a%2f%2f"
#define SAS_TOKEN_ENC_DELIM_DEF     "%2f"

#define SASTOKEN_EXT_EXPIRATION_TIMESTAMP (uint64_t)1000

static const char HOSTNAME[]            = HOSTNAME_VALUE_DEF;
static const char EVENTHUB_PATH[]       = EVENTHUB_PATH_VALUE_DEF;
static const char CONSUMER_GROUP[]      = CONSUMER_GROUP_VALUE_DEF;
static const char PARTITION_ID[]        = PARTITION_ID_VALUE_DEF;
static const char SHAREDACCESSKEY[]     = SIGNATURE_VALUE_DEF;
static const char SHAREDACCESSKEYNAME[] = SKN_VALUE_DEF;
static const char PUBLISHER_VALUE[]     = PUBLISHER_VALUE_DEF;
static const char PARTITION_VALUE[]     = PUBLISHER_VALUE_DEF;
static const char SAS_TOKEN_TYPE[]      = "servicebus.windows.net:sastoken";
static const char URI_AUTO_START[]      = SB_STRING_DEF;
static const char URI_AUTO_SENDER[]     = SB_STRING_DEF HOSTNAME_VALUE_DEF "/" EVENTHUB_PATH_VALUE_DEF "/" PUBLISHER_KEY_DEF "/" PUBLISHER_VALUE_DEF;
static const char URI_AUTO_RECEIVER[]   = SB_STRING_DEF HOSTNAME_VALUE_DEF "/" EVENTHUB_PATH_VALUE_DEF "/" CONSUMER_GROUP_KEY_DEF "/" CONSUMER_GROUP_VALUE_DEF "/" PARTITION_ID_KEY_DEF "/" PARTITION_ID_VALUE_DEF;
static const char SASTOKEN_AUTO[]       = "Sample SASToken Auto";

static const char SASTOKEN_EXT_SENDER[]   = SAS_TOKEN_START_DEF URI_KEY_DEF "=" SB_STRING_ENCODED_DEF HOSTNAME_VALUE_DEF "%2f" EVENTHUB_PATH_VALUE_DEF "%2f" PUBLISHER_KEY_DEF "%2f" PUBLISHER_VALUE_DEF "&" SIGNATURE_KEY_DEF "=" SIGNATURE_VALUE_DEF "&" EXPIRATION_KEY_DEF "=" EXPIRATION_VALUE_DEF "&" SKN_KEY_DEF "=" SKN_VALUE_DEF;
static const char SASTOKEN_EXT_RECEIVER[] = SAS_TOKEN_START_DEF URI_KEY_DEF "=" SB_STRING_ENCODED_DEF HOSTNAME_VALUE_DEF "%2f" EVENTHUB_PATH_VALUE_DEF "%2f" CONSUMER_GROUP_KEY_DEF "%2f" CONSUMER_GROUP_VALUE_DEF "%2f" PARTITION_ID_KEY_DEF "%2f" PARTITION_ID_VALUE_DEF "&" SIGNATURE_KEY_DEF "=" SIGNATURE_VALUE_DEF "&" EXPIRATION_KEY_DEF "=" EXPIRATION_VALUE_DEF "&" SKN_KEY_DEF "=" SKN_VALUE_DEF;
static const char URI_EXT_SENDER[]        = SB_STRING_ENCODED_DEF HOSTNAME_VALUE_DEF "%2f" EVENTHUB_PATH_VALUE_DEF "%2f" PUBLISHER_KEY_DEF "%2f" PUBLISHER_VALUE_DEF;
static const char URI_EXT_RECEIVER[]      = SB_STRING_ENCODED_DEF HOSTNAME_VALUE_DEF "%2f" EVENTHUB_PATH_VALUE_DEF "%2f" CONSUMER_GROUP_KEY_DEF "%2f" CONSUMER_GROUP_VALUE_DEF "%2f" PARTITION_ID_KEY_DEF "%2f" PARTITION_ID_VALUE_DEF;

static const char TEST_DUMMY_STRING[] = "DUMMY STRING";

static TEST_HOOK_MAP_KVP_STRUCT stringKVPTable[] =
{
    { HOSTNAME, TEST_STRING_HANDLE_AUTO_HOSTNAME, NULL },
    { EVENTHUB_PATH, TEST_STRING_HANDLE_AUTO_EVENTHUBPATH, NULL },
    { SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME_CLONE },
    { SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME_CLONE, NULL },
    { SHAREDACCESSKEY, TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEY, NULL },
    { CONSUMER_GROUP, TEST_STRING_HANDLE_AUTO_CONSUMERGROUP, NULL },
    { PARTITION_ID, TEST_STRING_HANDLE_AUTO_PARTITIONID, NULL },
    { URI_AUTO_START, TEST_STRING_HANDLE_AUTO_URI, NULL },
    { URI_AUTO_SENDER, TEST_STRING_HANDLE_AUTO_SENDER_URI, NULL },
    { URI_AUTO_RECEIVER, TEST_STRING_HANDLE_AUTO_RECEIVER_URI, NULL },
    { SASTOKEN_AUTO, TEST_STRING_HANDLE_AUTO_SASTOKEN, NULL },

    { SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_EXT_SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_EXT_SHAREDACCESSKEYNAME_CLONE },
    { SHAREDACCESSKEYNAME, TEST_STRING_HANDLE_EXT_SHAREDACCESSKEYNAME_CLONE, NULL },
    { SASTOKEN_EXT_SENDER, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_SENDER, TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE },
    { SASTOKEN_EXT_SENDER, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_SENDER, TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE },
    { SASTOKEN_EXT_SENDER, TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE, NULL },
    { SASTOKEN_EXT_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE },
    { SASTOKEN_EXT_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE },
    { SASTOKEN_EXT_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE, NULL },
    { URI_EXT_SENDER, TEST_STRING_HANDLE_EXT_SENDER_URI, TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE },
    { URI_EXT_SENDER, TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE, NULL },
    { URI_EXT_RECEIVER, TEST_STRING_HANDLE_EXT_RECEIVER_URI, TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE },
    { URI_EXT_RECEIVER, TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE, NULL },

    { TEST_DUMMY_STRING, TEST_STRING_KEY_TOKEN_HANDLE, NULL },
    { NULL, NULL, NULL }
};

//#################################################################################################
// EventHubAuth Test Helper Implementations
//#################################################################################################
STRING_HANDLE TestHelper_Map_GetStringHandle(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return stringKVPTable[index].handle;
}

int TestHelper_Map_GetIndexByKey(const char* key)
{
    bool found = false;
    int idx = 0, result;

    while (stringKVPTable[idx].key != NULL)
    {
        if (strcmp(stringKVPTable[idx].key, key) == 0)
        {
            found = true;
            break;
        }
        idx++;
    }

    if (!found)
    {
        printf("Test Map, could not find by key:%s \r\n", key);
        result = -1;
    }
    else
    {
        result = idx;
    }

    return result;
}

int TestHelper_Map_GetIndexByStringHandle(STRING_HANDLE h)
{
    bool found = false;
    int idx = 0, result;

    if (h)
    {
        while (stringKVPTable[idx].key != NULL)
        {
            if (stringKVPTable[idx].handle == h)
            {
                found = true;
                break;
            }
            idx++;
        }
    }

    if (!found)
    {
        printf("Test Map, could not find by string handle:%p \r\n", h);
        result = -1;
    }
    else
    {
        result = idx;
    }
    return result;
}

const char* TestHelper_Map_GetKey(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return stringKVPTable[index].key;
}

static STRING_HANDLE TestHelper_Map_GetStringClone(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return stringKVPTable[index].cloneHandle;
}

static void TestHelper_InitEventhHubAuthConfigCommonAuto(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    pCfg->hostName = TEST_STRING_HANDLE_AUTO_HOSTNAME;
    pCfg->eventHubPath = TEST_STRING_HANDLE_AUTO_EVENTHUBPATH;
    pCfg->sharedAccessKeyName = TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME;
    pCfg->sharedAccessKey = TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEY;
    pCfg->sasTokenAuthFailureTimeoutInSecs = AUTH_WAITTIMEOUT_SECS;
    pCfg->sasTokenExpirationTimeInSec = AUTH_EXPIRATION_SECS;
    pCfg->sasTokenRefreshPeriodInSecs = AUTH_REFRESH_SECS;
    pCfg->extSASToken = NULL;
    pCfg->extSASTokenURI = NULL;
    pCfg->extSASTokenExpTSInEpochSec = 0;
    pCfg->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO;
}

static void TestHelper_InitEventhHubAuthConfigReceiverAuto(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    TestHelper_InitEventhHubAuthConfigCommonAuto(pCfg);
    pCfg->mode = EVENTHUBAUTH_MODE_RECEIVER;
    pCfg->receiverConsumerGroup = TEST_STRING_HANDLE_AUTO_CONSUMERGROUP;
    pCfg->receiverPartitionId = TEST_STRING_HANDLE_AUTO_PARTITIONID;
    pCfg->senderPublisherId = NULL;
}

static void TestHelper_InitEventhHubAuthConfigSenderAuto(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    TestHelper_InitEventhHubAuthConfigCommonAuto(pCfg);
    pCfg->mode = EVENTHUBAUTH_MODE_SENDER;
    pCfg->receiverConsumerGroup = NULL;
    pCfg->receiverPartitionId = NULL;
    pCfg->senderPublisherId = TEST_STRING_HANDLE_AUTO_PUBLISHER_VALUE;
}

static void TestHelper_InitEventhHubAuthConfigCommonExt(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    pCfg->hostName = NULL;
    pCfg->eventHubPath = NULL;
    pCfg->sharedAccessKeyName = NULL;
    pCfg->sharedAccessKey = NULL;
    pCfg->sasTokenAuthFailureTimeoutInSecs = 0;
    pCfg->sasTokenExpirationTimeInSec = 0;
    pCfg->sasTokenRefreshPeriodInSecs = 0;
    pCfg->extSASTokenExpTSInEpochSec = SASTOKEN_EXT_EXPIRATION_TIMESTAMP;
    pCfg->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;
}

static void TestHelper_InitEventhHubAuthConfigReceiverExt(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    TestHelper_InitEventhHubAuthConfigCommonExt(pCfg);
    pCfg->mode = EVENTHUBAUTH_MODE_RECEIVER;
    pCfg->extSASToken = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER;
    pCfg->extSASTokenURI = TEST_STRING_HANDLE_EXT_RECEIVER_URI;
    pCfg->receiverConsumerGroup = TEST_STRING_HANDLE_AUTO_CONSUMERGROUP;
    pCfg->receiverPartitionId = TEST_STRING_HANDLE_AUTO_PARTITIONID;
    pCfg->senderPublisherId = NULL;
}

static void TestHelper_InitEventhHubAuthConfigSenderExt(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    TestHelper_InitEventhHubAuthConfigCommonExt(pCfg);
    pCfg->mode = EVENTHUBAUTH_MODE_SENDER;
    pCfg->extSASToken = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_SENDER;
    pCfg->extSASTokenURI = TEST_STRING_HANDLE_EXT_SENDER_URI;
    pCfg->receiverConsumerGroup = NULL;
    pCfg->receiverPartitionId = NULL;
    pCfg->senderPublisherId = TEST_STRING_HANDLE_EXT_PUBLISHER_VALUE;
}

static void TestHelper_ResetTestGlobal(TESTGLOBAL* gTestGlobal)
{
    memset(gTestGlobal, 0, sizeof(TESTGLOBAL));
}

#ifdef __cplusplus
extern "C"
{
#endif
    unsigned long long strtoull_s(const char* nptr, char** endPtr, int base)
    {
        return SASTOKEN_EXT_EXPIRATION_TIMESTAMP;
    }
#ifdef __cplusplus
}
#endif

//#################################################################################################
// EventHubAuth Test Hook Implementations
//#################################################################################################
static void TestHook_OnUMockCError(UMOCK_C_ERROR_CODE errorCode)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, errorCode));
    ASSERT_FAIL(temp_str);
}

static STRING_HANDLE TestHook_STRING_construct(const char* psz)
{
    return TestHelper_Map_GetStringHandle(TestHelper_Map_GetIndexByKey(psz));
}

static const char* TestHook_STRING_c_str(STRING_HANDLE handle)
{
    return TestHelper_Map_GetKey(TestHelper_Map_GetIndexByStringHandle(handle));
}

static STRING_HANDLE TestHook_STRING_clone(STRING_HANDLE handle)
{
    return TestHelper_Map_GetStringClone(TestHelper_Map_GetIndexByStringHandle(handle));
}

static int TestHook_cbs_put_token(CBS_HANDLE cbs, const char* type, const char* audience, const char* token, ON_CBS_OPERATION_COMPLETE on_operation_complete, void* context)
{
    g_TestGlobal.sasTokenPutCB = on_operation_complete;
    g_TestGlobal.sasTokenPutCBContext = context;
    return 0;
}

static time_t TestHook_get_time(time_t* p)
{
    return time(p);
}

static double TestHook_get_difftime(time_t stopTime, time_t startTime)
{
    return difftime(stopTime, startTime);
}

//#################################################################################################
// EventHubAuth Common Callstack Test Setup Functions
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_301: \[**EventHubAuthCBS_SASTokenParse shall return NULL if sasTokenData is NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_302: \[**EventHubAuthCBS_SASTokenParse shall construct a STRING using sasToken as data using API STRING_construct.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_303: \[**EventHubAuthCBS_SASTokenParse shall construct a new STRING uriFromSASToken to hold the sasToken URI substring using API STRING_new.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_304: \[**EventHubAuthCBS_SASTokenParse shall call GetURIAndExpirationFromSASToken' and pass in sasToken, uriFromSASToken and a pointer to a uint64_t to hold the token expiration time.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_305: \[**EventHubAuthCBS_SASTokenParse shall allocate the EVENTHUBAUTH_CBS_CONFIG structure using malloc.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_306: \[**EventHubAuthCBS_SASTokenParse shall return NULL if memory allocation fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_307: \[**EventHubAuthCBS_SASTokenParse shall create a token STRING handle using API STRING_new for handling the to be parsed tokens.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_308: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_new fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_309: \[**EventHubAuthCBS_SASTokenParse shall create a STRING tokenizer using API STRING_TOKENIZER_create and pass the URI scope as argument.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_310: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_TOKENIZER_create fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_311: \[**EventHubAuthCBS_SASTokenParse shall perform the following actions until parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_312: \[**EventHubAuthCBS_SASTokenParse shall find a token delimited by the delimiter string "%2f" by calling STRING_TOKENIZER_get_next_token.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_313: \[**EventHubAuthCBS_SASTokenParse shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_314: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated resources if any failure is encountered.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_315: \[**EventHubAuthCBS_SASTokenParse shall obtain the C strings for the key and value from the previously parsed STRINGs by using STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_316: \[**EventHubAuthCBS_SASTokenParse shall fail if STRING_c_str returns NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_317: \[**EventHubAuthCBS_SASTokenParse shall fail if the token length is zero.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_318: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the hostName after parsing the first token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_319: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the eventHubPath after parsing the second token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_320: \[**EventHubAuthCBS_SASTokenParse shall parse the third token and determine if the SAS token is meant for a EventHub sender or receiver. For senders the token value should be "publishers" and "ConsumerGroups" for receivers. In all other cases these checks fail.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_321: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the senderPublisherId if the SAS token is a EventHub sender using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_322: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the receiverConsumerGroup if the SAS token is a EventHub receiver using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_323: \[**EventHubAuthCBS_SASTokenParse shall parse the fifth token and check if the token value should be "Partitions". In all other cases this check fails. **\]**
//**Tests_SRS_EVENTHUB_AUTH_29_324: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the receiverPartitionId after parsing the sixth token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_325: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated token STRING handle using API STRING_delete after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_326: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_327: \[**EventHubAuthCBS_SASTokenParse shall return the allocated EVENTHUBAUTH_CBS_CONFIG structure on success.**\]**

//**Tests_SRS_EVENTHUB_AUTH_29_350: \[**GetURIAndExpirationFromSASToken shall return a non zero value immediately if sasToken does not begins with substring "SharedAccessSignature ".**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_353: \[**GetURIAndExpirationFromSASToken shall call kvp_parser_parse and pass in the SAS token STRING handle and "=" and ";" as key and value delimiters.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_354: \[**GetURIAndExpirationFromSASToken shall return a non zero value if kvp_parser_parse returns a NULL MAP handle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_355: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token URI using key "sr" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_356: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the URI value is NULL or the length of the URI is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_357: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if the URI does not begin with substring "sb%3a%2f%2f".**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_358: \[**GetURIAndExpirationFromSASToken shall populate the URI STRING using the substring of the URI after "sb%3a%2f%2f" using API STRING_copy.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_359: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if STRING_copy fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_360: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token signed hash key using key "sig" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_361: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the sig string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_362: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token node using key "skn" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_363: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the skn string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_364: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token expiration using key "se" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_365: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_366: \[**GetURIAndExpirationFromSASToken shall convert the expiration timestamp to a decimal number by calling API strtoull_s.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_367: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if calling strtoull_s fails when converting the expiration string to the expirationTimestamp.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_368: \[**GetURIAndExpirationFromSASToken shall free up the MAP handle created as a result of calling kvp_parser_parse after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_369: \[**GetURIAndExpirationFromSASToken shall return 0 on success.**\]**
static uint64_t TestSetupCallStack_ReceiverEventHubAuthCBS_SASTokenParse(const char* sasToken)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    STRING_HANDLE uriHandle;
    const char* uriFromSasToken;

    umock_c_reset_all_calls();

    uriHandle = TEST_STRING_HANDLE_EXT_RECEIVER_URI;
    uriFromSasToken = URI_EXT_RECEIVER;

    STRICT_EXPECTED_CALL(STRING_construct(sasToken))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_new()).SetReturn(uriHandle).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(kvp_parser_parse(sasToken + strlen(SAS_TOKEN_START_DEF), IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, "=", 1)
        .IgnoreArgument(3).ValidateArgumentBuffer(3, "&", 1)
        .SetReturn(TEST_MAP_HANDLE_VALID).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, URI_KEY_DEF, strlen(URI_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_copy(uriHandle, uriFromSasToken + strlen(SB_STRING_ENCODED_DEF)));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SIGNATURE_KEY_DEF, strlen(SIGNATURE_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SKN_KEY_DEF, strlen(SKN_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, EXPIRATION_KEY_DEF, strlen(EXPIRATION_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE_VALID));
    i++;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_create(uriHandle))
        .SetReturn(TEST_STRING_TOKENIZER_HANDLE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_STRING_KEY_TOKEN_HANDLE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(HOSTNAME);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(HOSTNAME, strlen(HOSTNAME)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(EVENTHUB_PATH);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(EVENTHUB_PATH, strlen(EVENTHUB_PATH)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(CONSUMER_GROUP_KEY_DEF);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(CONSUMER_GROUP);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(CONSUMER_GROUP, strlen(CONSUMER_GROUP)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PUBLISHER_VALUE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(PARTITION_ID_KEY_DEF);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(PARTITION_VALUE);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(PARTITION_VALUE, strlen(PARTITION_VALUE)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_KEY_TOKEN_HANDLE));
    i++;

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_destroy(TEST_STRING_TOKENIZER_HANDLE));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_301: \[**EventHubAuthCBS_SASTokenParse shall return NULL if sasTokenData is NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_302: \[**EventHubAuthCBS_SASTokenParse shall construct a STRING using sasToken as data using API STRING_construct.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_303: \[**EventHubAuthCBS_SASTokenParse shall construct a new STRING uriFromSASToken to hold the sasToken URI substring using API STRING_new.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_304: \[**EventHubAuthCBS_SASTokenParse shall call GetURIAndExpirationFromSASToken' and pass in sasToken, uriFromSASToken and a pointer to a uint64_t to hold the token expiration time.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_305: \[**EventHubAuthCBS_SASTokenParse shall allocate the EVENTHUBAUTH_CBS_CONFIG structure using malloc.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_306: \[**EventHubAuthCBS_SASTokenParse shall return NULL if memory allocation fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_307: \[**EventHubAuthCBS_SASTokenParse shall create a token STRING handle using API STRING_new for handling the to be parsed tokens.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_308: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_new fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_309: \[**EventHubAuthCBS_SASTokenParse shall create a STRING tokenizer using API STRING_TOKENIZER_create and pass the URI scope as argument.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_310: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_TOKENIZER_create fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_311: \[**EventHubAuthCBS_SASTokenParse shall perform the following actions until parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_312: \[**EventHubAuthCBS_SASTokenParse shall find a token delimited by the delimiter string "%2f" by calling STRING_TOKENIZER_get_next_token.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_313: \[**EventHubAuthCBS_SASTokenParse shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_314: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated resources if any failure is encountered.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_315: \[**EventHubAuthCBS_SASTokenParse shall obtain the C strings for the key and value from the previously parsed STRINGs by using STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_316: \[**EventHubAuthCBS_SASTokenParse shall fail if STRING_c_str returns NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_317: \[**EventHubAuthCBS_SASTokenParse shall fail if the token length is zero.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_318: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the hostName after parsing the first token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_319: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the eventHubPath after parsing the second token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_320: \[**EventHubAuthCBS_SASTokenParse shall parse the third token and determine if the SAS token is meant for a EventHub sender or receiver. For senders the token value should be "publishers" and "ConsumerGroups" for receivers. In all other cases these checks fail.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_321: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the senderPublisherId if the SAS token is a EventHub sender using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_322: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the receiverConsumerGroup if the SAS token is a EventHub receiver using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_323: \[**EventHubAuthCBS_SASTokenParse shall parse the fifth token and check if the token value should be "Partitions". In all other cases this check fails. **\]**
//**Tests_SRS_EVENTHUB_AUTH_29_324: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the receiverPartitionId after parsing the sixth token using API STRING_construct_n.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_325: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated token STRING handle using API STRING_delete after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_326: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_327: \[**EventHubAuthCBS_SASTokenParse shall return the allocated EVENTHUBAUTH_CBS_CONFIG structure on success.**\]**

//**Tests_SRS_EVENTHUB_AUTH_29_350: \[**GetURIAndExpirationFromSASToken shall return a non zero value immediately if sasToken does not begins with substring "SharedAccessSignature ".**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_353: \[**GetURIAndExpirationFromSASToken shall call kvp_parser_parse and pass in the SAS token STRING handle and "=" and ";" as key and value delimiters.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_354: \[**GetURIAndExpirationFromSASToken shall return a non zero value if kvp_parser_parse returns a NULL MAP handle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_355: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token URI using key "sr" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_356: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the URI value is NULL or the length of the URI is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_357: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if the URI does not begin with substring "sb%3a%2f%2f".**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_358: \[**GetURIAndExpirationFromSASToken shall populate the URI STRING using the substring of the URI after "sb%3a%2f%2f" using API STRING_copy.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_359: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if STRING_copy fails.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_360: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token signed hash key using key "sig" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_361: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the sig string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_362: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token node using key "skn" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_363: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the skn string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_364: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token expiration using key "se" in API Map_GetValueFromKey.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_365: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_366: \[**GetURIAndExpirationFromSASToken shall convert the expiration timestamp to a decimal number by calling API strtoull_s.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_367: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if calling strtoull_s fails when converting the expiration string to the expirationTimestamp.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_368: \[**GetURIAndExpirationFromSASToken shall free up the MAP handle created as a result of calling kvp_parser_parse after parsing is complete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_369: \[**GetURIAndExpirationFromSASToken shall return 0 on success.**\]**
static uint64_t TestSetupCallStack_SenderEventHubAuthCBS_SASTokenParse(const char* sasToken)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    STRING_HANDLE uriHandle;
    const char* uriFromSasToken;

    umock_c_reset_all_calls();

    uriHandle = TEST_STRING_HANDLE_EXT_SENDER_URI;
    uriFromSasToken = URI_EXT_SENDER;

    STRICT_EXPECTED_CALL(STRING_construct(sasToken))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_SENDER).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_new()).SetReturn(uriHandle).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(kvp_parser_parse(sasToken + strlen(SAS_TOKEN_START_DEF), IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, "=", 1)
        .IgnoreArgument(3).ValidateArgumentBuffer(3, "&", 1)
        .SetReturn(TEST_MAP_HANDLE_VALID).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, URI_KEY_DEF, strlen(URI_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_copy(uriHandle, uriFromSasToken + strlen(SB_STRING_ENCODED_DEF)));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SIGNATURE_KEY_DEF, strlen(SIGNATURE_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SKN_KEY_DEF, strlen(SKN_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, EXPIRATION_KEY_DEF, strlen(EXPIRATION_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE_VALID));
    i++;

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_create(uriHandle))
        .SetReturn(TEST_STRING_TOKENIZER_HANDLE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_new()).SetReturn(TEST_STRING_KEY_TOKEN_HANDLE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(HOSTNAME);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(HOSTNAME, strlen(HOSTNAME)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(EVENTHUB_PATH);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(EVENTHUB_PATH, strlen(EVENTHUB_PATH)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(PUBLISHER_KEY_DEF);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_get_next_token(TEST_STRING_TOKENIZER_HANDLE, TEST_STRING_KEY_TOKEN_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(3).ValidateArgumentBuffer(3, SAS_TOKEN_ENC_DELIM_DEF, strlen(SAS_TOKEN_ENC_DELIM_DEF))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_KEY_TOKEN_HANDLE)).SetReturn(PUBLISHER_VALUE);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct_n(PUBLISHER_VALUE, strlen(PUBLISHER_VALUE)))
        .SetReturn(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PUBLISHER_VALUE).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_KEY_TOKEN_HANDLE));
    i++;

    STRICT_EXPECTED_CALL(STRING_TOKENIZER_destroy(TEST_STRING_TOKENIZER_HANDLE));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_010: \[**EventHubAuthCBS_Create shall allocate new memory to store the specified configuration data by using API malloc.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**

//**Tests_SRS_EVENTHUB_AUTH_29_012: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Create shall clone the external SAS token using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_013: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Create shall clone the external SAS token URI using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_014: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall construct a URI of the format "sb://" using API STRING_construct.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_015: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->hostName using API STRING_concat_with_STRING.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_016: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with "/" using API STRING_concat.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_017: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->eventHubPath using API STRING_concat_with_STRING.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_019: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with "/ConsumerGroups/" using API STRING_concat.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_020: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->receiverConsumerGroup using API STRING_concat_with_STRING.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_021: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with "/Partitions/" using API STRING_concat.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_022: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->receiverPartitionId using API STRING_concat_with_STRING.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_023: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, EventHubAuthCBS_Create shall further concatenate the URI with "/publishers/" using API STRING_concat.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_024: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->senderPublisherId using API STRING_concat_with_STRING.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_025: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall construct a STRING_HANDLE to store eventHubAuthConfig->sharedAccessKeyName using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_026: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a new STRING_HANDLE by encoding the URI using API URL_Encode.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_027: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall obtain the underlying C string buffer of eventHubAuthConfig->sharedAccessKey using API STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_028: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a BUFFER_HANDLE using API BUFFER_create and pass in the eventHubAuthConfig->sharedAccessKey buffer and its length.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_029: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a new STRING_HANDLE by Base64 encoding the buffer handle created above by using API Base64_Encoder.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_030: \[**EventHubAuthCBS_Create shall initialize a CBS handle by calling API cbs_create.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_031: \[**EventHubAuthCBS_Create shall open the CBS handle by calling API `cbs_open_async.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_032: \[**EventHubAuthCBS_Create shall initialize its internal data structures using the configuration data passed in.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_033: \[**EventHubAuthCBS_Create shall return a non-NULL handle encapsulating the storage of the data provided.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Create(EVENTHUBAUTH_MODE mode, EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    STRING_HANDLE URI;
    // arrange
    umock_c_reset_all_calls();

    // internal data structure allocation
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        if (mode == EVENTHUBAUTH_MODE_RECEIVER)
        {
            URI = TEST_STRING_HANDLE_AUTO_RECEIVER_URI;
        }
        else
        {
            URI = TEST_STRING_HANDLE_AUTO_SENDER_URI;
        }

        EXPECTED_CALL(STRING_construct(IGNORED_PTR_ARG)).SetReturn(URI);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(URI, TEST_STRING_HANDLE_AUTO_HOSTNAME));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_concat(URI, IGNORED_PTR_ARG)).IgnoreArgument(2).ValidateArgumentBuffer(2, "/", 1);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(URI, TEST_STRING_HANDLE_AUTO_EVENTHUBPATH));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        if (mode == EVENTHUBAUTH_MODE_RECEIVER)
        {
            STRICT_EXPECTED_CALL(STRING_concat(URI, IGNORED_PTR_ARG)).IgnoreArgument(2).ValidateArgumentBuffer(2, "/" CONSUMER_GROUP_KEY_DEF "/", strlen("/" CONSUMER_GROUP_KEY_DEF "/"));
            failedFunctionBitmask |= ((uint64_t)1 << i++);

            STRICT_EXPECTED_CALL(STRING_concat_with_STRING(URI, TEST_STRING_HANDLE_AUTO_CONSUMERGROUP));
            failedFunctionBitmask |= ((uint64_t)1 << i++);

            STRICT_EXPECTED_CALL(STRING_concat(URI, IGNORED_PTR_ARG)).IgnoreArgument(2).ValidateArgumentBuffer(2, "/" PARTITION_ID_KEY_DEF "/", strlen("/" PARTITION_ID_KEY_DEF "/"));
            failedFunctionBitmask |= ((uint64_t)1 << i++);

            STRICT_EXPECTED_CALL(STRING_concat_with_STRING(URI, TEST_STRING_HANDLE_AUTO_PARTITIONID));
            failedFunctionBitmask |= ((uint64_t)1 << i++);
        }
        else
        {
            STRICT_EXPECTED_CALL(STRING_concat(URI, IGNORED_PTR_ARG)).IgnoreArgument(2).ValidateArgumentBuffer(2, "/" PUBLISHER_KEY_DEF "/", strlen("/" PUBLISHER_KEY_DEF "/"));
            failedFunctionBitmask |= ((uint64_t)1 << i++);

            STRICT_EXPECTED_CALL(STRING_concat_with_STRING(URI, TEST_STRING_HANDLE_AUTO_PUBLISHER_VALUE));
            failedFunctionBitmask |= ((uint64_t)1 << i++);
        }

        STRICT_EXPECTED_CALL(STRING_clone(TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(URL_Encode(URI));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEY));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(BUFFER_create((const unsigned char*)SHAREDACCESSKEY, strlen(SHAREDACCESSKEY)));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(Base64_Encoder(TEST_BUFFER_HANDLE_VALID_SHAREDACCESSKEY));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(BUFFER_delete(TEST_BUFFER_HANDLE_VALID_SHAREDACCESSKEY));
        i++;
    }
    else
    {
        STRING_HANDLE sasTokenHandle;
        if (mode == EVENTHUBAUTH_MODE_RECEIVER)
        {
            sasTokenHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER;
            URI = TEST_STRING_HANDLE_EXT_RECEIVER_URI;
        }
        else
        {
            sasTokenHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_SENDER;
            URI = TEST_STRING_HANDLE_EXT_SENDER_URI;
        }

        STRICT_EXPECTED_CALL(STRING_clone(sasTokenHandle));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(STRING_clone(URI));
        failedFunctionBitmask |= ((uint64_t)1 << i++);
    }


    STRICT_EXPECTED_CALL(cbs_create(TEST_SESSION_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(cbs_open_async(TEST_CBS_HANDLE_VALID, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_071: \[**EventHubAuthCBS_Destroy shall destroy the CBS handle if not NULL by using API cbs_destroy.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_072: \[**EventHubAuthCBS_Destroy shall destroy the SAS token if not NULL by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_073: \[**EventHubAuthCBS_Destroy shall destroy the Base64 encoded shared access key by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_074: \[**EventHubAuthCBS_Destroy shall destroy the encoded URI by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_075: \[**EventHubAuthCBS_Destroy shall destroy the shared access key name by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_076: \[**EventHubAuthCBS_Destroy shall destroy the ext SAS token by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_077: \[**EventHubAuthCBS_Destroy shall destroy the ext SAS token URI by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_078: \[**EventHubAuthCBS_Destroy shall destroy the URI by calling API STRING_delete if not NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_079: \[**EventHubAuthCBS_Destroy shall free the internal data structure allocated earlier using API free.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Destroy(bool isSASTokenAuthAttempted, EVENTHUBAUTH_MODE mode, EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();
    
    STRICT_EXPECTED_CALL(cbs_destroy(TEST_CBS_HANDLE_VALID));
    i++;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        if (isSASTokenAuthAttempted)
        {
            STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_AUTO_SASTOKEN));
            i++;
        }

        STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_AUTO_KEYBASE64ENCODED));
        i++;

        STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_AUTO_URIENCODED));
        i++;

        STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME_CLONE));
        i++;

        STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_AUTO_URI));
        i++;
    }
    else
    {
        STRING_HANDLE sasTokenCloneHandle, extSASTokenCloneHandle, URIClone;

        if (mode == EVENTHUBAUTH_MODE_RECEIVER)
        {
            sasTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE;
            extSASTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE;
            URIClone = TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE;
        }
        else
        {
            sasTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE;
            extSASTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE;
            URIClone = TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE;
        }

        if (isSASTokenAuthAttempted)
        {
            STRICT_EXPECTED_CALL(STRING_delete(sasTokenCloneHandle));
            i++;
        }

        STRICT_EXPECTED_CALL(STRING_delete(extSASTokenCloneHandle));
        i++;

        STRICT_EXPECTED_CALL(STRING_delete(URIClone));
        i++;
    }

    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_103: \[**EventHubAuthCBS_Authenticate shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_104: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Authenticate shall check if the ext token has expired using the seconds from epoch.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_105: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Authenticate shall create a new SAS token STRING_HANDLE using API SASToken_Create and passing in Base64 encoded shared access key STRING_HANDLE, encoded URI STRING_HANDLE, shared access key name STRING_HANDLE and expiration time in seconds from epoch.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_106: \[**EventHubAuthCBS_Authenticate shall delete the existing SAS token STRING_HANDLE if not NULL by calling STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_107: \[**EventHubAuthCBS_Authenticate shall obtain the underlying C string buffer of the uri STRING_HANDLE calling STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_108: \[**EventHubAuthCBS_Authenticate shall obtain the underlying C string buffer of STRING_HANDLE SAS token by calling STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_109: \[**EventHubAuthCBS_Authenticate shall establish (put) the new token by calling API cbs_put_token and passing in the CBS handle, "servicebus.windows.net:sastoken", URI string buffer, SAS token string buffer, OnCBSPutTokenOperationComplete and eventHubAuthHandle.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Authenticate_Common
(
    uint64_t failedFunctionBitmask,
    int startIndex,
    bool isPriorSASTokenAuthAttempted,
    EVENTHUBAUTH_MODE mode,
    EVENTHUBAUTH_CREDENTIAL_TYPE credential
)
{
    int i = startIndex;
    STRING_HANDLE sasToken, URI;

    STRICT_EXPECTED_CALL(get_time(NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(get_difftime(IGNORED_NUM_ARG, (time_t)(0))).IgnoreArgument(1);
    i++;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        if (mode == EVENTHUBAUTH_MODE_RECEIVER)
        {
            sasToken = TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE;
            URI = TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE;
        }
        else
        {
            sasToken = TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE;
            URI = TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE;
        }
    }
    else
    {
        sasToken = TEST_STRING_HANDLE_AUTO_SASTOKEN;
        URI = TEST_STRING_HANDLE_AUTO_URI;

        STRICT_EXPECTED_CALL(SASToken_Create(TEST_STRING_HANDLE_AUTO_KEYBASE64ENCODED, TEST_STRING_HANDLE_AUTO_URIENCODED, TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME_CLONE, IGNORED_NUM_ARG))
            .IgnoreArgument(4);
        failedFunctionBitmask |= ((uint64_t)1 << i++);
    }

    if (isPriorSASTokenAuthAttempted)
    {
        STRICT_EXPECTED_CALL(STRING_delete(sasToken));
        i++;
    }

    STRICT_EXPECTED_CALL(STRING_c_str(URI));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(sasToken));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(cbs_put_token_async(TEST_CBS_HANDLE_VALID, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

static uint64_t TestSetupCallStack_EventHubAuthCBS_Authenticate(bool isPriorSASTokenAuthAttempted, EVENTHUBAUTH_MODE mode, EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t failedFunctionBitmask = 0;

    // arrange
    TestHelper_ResetTestGlobal(&g_TestGlobal);
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate_Common(failedFunctionBitmask, 0, isPriorSASTokenAuthAttempted, mode, credential);

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_150: \[**OnCBSPutTokenOperationComplete shall obtain seconds from epoch by calling APIs get_time and get_difftime if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Authenticate_PutCallback(CBS_OPERATION_RESULT result)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    
    // arrange
    umock_c_reset_all_calls();

    if (result == CBS_OPERATION_RESULT_OK)
    {
        STRICT_EXPECTED_CALL(get_time(NULL));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(get_difftime(IGNORED_NUM_ARG, (time_t)(0))).IgnoreArgument(1);
        i++;
    }

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(EVENTHUBAUTH_MODE mode)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(get_time(NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(get_difftime(IGNORED_NUM_ARG, (time_t)(0))).IgnoreArgument(1);
    i++;

    return failedFunctionBitmask;
}

static uint64_t TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithRefreshRequired(EVENTHUBAUTH_MODE mode)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(get_time(NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(get_difftime(IGNORED_NUM_ARG, (time_t)(0))).IgnoreArgument(1);
    i++;

    failedFunctionBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate_Common(failedFunctionBitmask, i, true, mode, EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUB_AUTH_29_202: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall clone the extSASToken using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_203: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall create a new temp STRING using API STRING_new to hold the refresh ext SAS token URI.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_204: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall obtain the underlying C string buffer of cloned extSASToken using API STRING_c_str.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_205: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall obtain the expiration time and the URI from the cloned extSASToken.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_206: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall compare the refresh token URI and the existing ext token URI using API STRING_compare. If a mismatch is observed, EVENTHUBAUTH_RESULT_ERROR shall be returned and any resources shall be deallocated.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_207: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall delete the prior ext SAS Token using API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_208: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall delete the temp refresh ext SAS token URI using API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
static uint64_t TestSetupCallStack_EventHubAuthCBS_Refresh_Ext(bool isPriorSASTokenAuthAttempted, EVENTHUBAUTH_MODE mode)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;
    STRING_HANDLE refreshSASTokenHandle, refreshSASTokenCloneHandle, uriHandle;
    const char* refreshSASTokenBuffer, *uriFromSasToken;
    // arrange
    TestHelper_ResetTestGlobal(&g_TestGlobal);
    umock_c_reset_all_calls();

    if (mode == EVENTHUBAUTH_MODE_RECEIVER)
    {
        refreshSASTokenHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_RECEIVER;
        refreshSASTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE;
        refreshSASTokenBuffer = SASTOKEN_EXT_RECEIVER;
        uriFromSasToken = URI_EXT_RECEIVER;
        uriHandle = TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE;
    }
    else
    {
        refreshSASTokenHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_SENDER;
        refreshSASTokenCloneHandle = TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE;
        refreshSASTokenBuffer = SASTOKEN_EXT_SENDER;
        uriFromSasToken = URI_EXT_SENDER;
        uriHandle = TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE;
    }

    STRICT_EXPECTED_CALL(STRING_clone(refreshSASTokenHandle));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_new()).SetReturn(uriHandle).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(refreshSASTokenCloneHandle));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(kvp_parser_parse(refreshSASTokenBuffer + strlen(SAS_TOKEN_START_DEF), IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, "=", 1)
        .IgnoreArgument(3).ValidateArgumentBuffer(3, "&", 1)
        .SetReturn(TEST_MAP_HANDLE_VALID).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, URI_KEY_DEF, strlen(URI_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_copy(uriHandle, uriFromSasToken + strlen(SB_STRING_ENCODED_DEF)))
        .SetReturn(0).SetFailReturn(-1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SIGNATURE_KEY_DEF, strlen(SIGNATURE_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, SKN_KEY_DEF, strlen(SKN_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2).ValidateArgumentBuffer(2, EXPIRATION_KEY_DEF, strlen(EXPIRATION_KEY_DEF))
        .SetReturn(uriFromSasToken).SetFailReturn(NULL);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE_VALID));
    i++;

    STRICT_EXPECTED_CALL(STRING_compare(IGNORED_PTR_ARG, uriHandle))
        .IgnoreArgument(1).SetReturn(0).SetFailReturn(1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_delete(IGNORED_PTR_ARG));
    i++;

    STRICT_EXPECTED_CALL(STRING_delete(uriHandle));
    i++;

    failedFunctionBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate_Common(failedFunctionBitmask, i, isPriorSASTokenAuthAttempted, mode, EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

    return failedFunctionBitmask;
}

//#################################################################################################
// EventHubAuth Tests
//#################################################################################################

BEGIN_TEST_SUITE(eventhubauth_unittests)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(TestHook_OnUMockCError);

    REGISTER_UMOCK_ALIAS_TYPE(time_t, unsigned int);
    REGISTER_UMOCK_ALIAS_TYPE(uint64_t, unsigned long long);

    REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SESSION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(BUFFER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CBS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(STRING_TOKENIZER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_AMQP_MANAGEMENT_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_CBS_OPEN_COMPLETE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_CBS_ERROR, void*);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, TestHook_malloc);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(gballoc_malloc, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, TestHook_free);

    //REGISTER_GLOBAL_MOCK_HOOK(strtoull_s, TestHook_strtoull_s);

    REGISTER_GLOBAL_MOCK_HOOK(STRING_construct, TestHook_STRING_construct);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_construct, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_concat, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_concat, -1);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_concat_with_STRING, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_concat_with_STRING, -1);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_c_str, TestHook_STRING_c_str);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_c_str, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_clone, TestHook_STRING_clone);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_clone, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_copy, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_copy, -1);

    REGISTER_GLOBAL_MOCK_RETURN(URL_Encode, TEST_STRING_HANDLE_AUTO_URIENCODED);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(URL_Encode, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(BUFFER_create, TEST_BUFFER_HANDLE_VALID_SHAREDACCESSKEY);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(BUFFER_create, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(Base64_Encoder, TEST_STRING_HANDLE_AUTO_KEYBASE64ENCODED);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Base64_Encoder, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(SASToken_Create, TEST_STRING_HANDLE_AUTO_SASTOKEN);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(SASToken_Create, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(cbs_create, TEST_CBS_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(cbs_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(cbs_open_async, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(cbs_open_async, -1);
    REGISTER_UMOCK_ALIAS_TYPE(ON_CBS_OPERATION_COMPLETE, void*);
    REGISTER_GLOBAL_MOCK_HOOK(cbs_put_token_async, TestHook_cbs_put_token);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(cbs_put_token_async, -1);

    REGISTER_GLOBAL_MOCK_HOOK(get_time, TestHook_get_time);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(get_time, TEST_TIME_T_INVALID);

    REGISTER_GLOBAL_MOCK_HOOK(get_difftime, TestHook_get_difftime);
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
        ASSERT_FAIL("Mutex is ABANDONED. Failure in test framework");
    }
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    TEST_MUTEX_RELEASE(g_testByTest);
}

//#################################################################################################
// EventHubAuthCBS_SASTokenParse Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_301: \[**EventHubAuthCBS_SASTokenParse shall return NULL if sasTokenData is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_SASTokenExtParse_NULL_Param_extSASToken)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg;
    // act
    cfg = EventHubAuthCBS_SASTokenParse(NULL);

    // assert
    ASSERT_IS_NULL(cfg);
}

TEST_FUNCTION(EventHubAuthCBS_SASTokenExtParseSender_Success)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg;
    (void)TestSetupCallStack_SenderEventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_SENDER);

    // act
    cfg = EventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_SENDER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(cfg, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Config_Destroy(cfg);
}

TEST_FUNCTION(EventHubAuthCBS_SASTokenExtParseSender_Negative)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_SenderEventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_SENDER);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            EVENTHUBAUTH_CBS_CONFIG* cfg;
            // act
            cfg = EventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_SENDER);
            // assert
            ASSERT_IS_NULL(cfg);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

TEST_FUNCTION(EventHubAuthCBS_SASTokenExtParseReceiver_Success)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg;
    (void)TestSetupCallStack_ReceiverEventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_RECEIVER);

    // act
    cfg = EventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_RECEIVER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(cfg, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Config_Destroy(cfg);
}

TEST_FUNCTION(EventHubAuthCBS_SASTokenExtParseReceiver_Negative)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_ReceiverEventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_RECEIVER);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            EVENTHUBAUTH_CBS_CONFIG* cfg;
            // act
            cfg = EventHubAuthCBS_SASTokenParse(SASTOKEN_EXT_RECEIVER);
            // assert
            ASSERT_IS_NULL(cfg);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}


//#################################################################################################
// EventHubAuthCBS_Config_Destroy Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_401: \[**EventHubAuthCBS_Config_Destroy shall return immediately if cfg is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBKPS_Config_Destroy_NULL_Param_eventHubAuthConfig)
{
    // arrange

    // act
    EventHubAuthCBS_Config_Destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_402: \[**EventHubAuthCBS_Config_Destroy shall destroy the hostName STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_403: \[**EventHubAuthCBS_Config_Destroy shall destroy the eventHubPath STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_404: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverConsumerGroup STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_405: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverPartitionId STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_406: \[**EventHubAuthCBS_Config_Destroy shall destroy the senderPublisherId STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_407: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKeyName STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_408: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKey STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_409: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASToken STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_410: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASTokenURI STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_402: \[**EventHubAuthCBS_Config_Destroy shall destroy the hostName STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_Hostname)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_HOSTNAME;
    cfg->hostName = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_403: \[**EventHubAuthCBS_Config_Destroy shall destroy the eventHubPath STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_EventHubPath)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_EVENTHUBPATH;
    cfg->eventHubPath = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_404: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverConsumerGroup STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_ConsumerGroup)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_CONSUMERGROUP;
    cfg->receiverConsumerGroup = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_405: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverPartitionId STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_PartitionID)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_PARTITIONID;
    cfg->receiverPartitionId = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_406: \[**EventHubAuthCBS_Config_Destroy shall destroy the senderPublisherId STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_PublisherID)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_PUBLISHER_VALUE;
    cfg->senderPublisherId = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_407: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKeyName STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_SharedAccessKeyName)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEYNAME;
    cfg->sharedAccessKeyName = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_408: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKey STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_SharedAccessKey)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_AUTO_SHAREDACCESSKEY;
    cfg->sharedAccessKey = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_409: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASToken STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_ExtSASToken)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_EXT_SASTOKEN_SENDER_CLONE;
    cfg->extSASToken = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_410: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASTokenURI STRING handle if not null by calling API STRING_delete.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
TEST_FUNCTION(EventHubAuthCBS_Config_Destroy_eventHubAuthConfig_NULL_All_Valid_ExtSASTokenURI)
{
    // arrange
    EVENTHUBAUTH_CBS_CONFIG* cfg = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    memset(cfg, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));

    STRING_HANDLE h = TEST_STRING_HANDLE_EXT_SENDER_URI_CLONE;
    cfg->extSASTokenURI = h;

    STRICT_EXPECTED_CALL(STRING_delete(h));

    STRICT_EXPECTED_CALL(gballoc_free(cfg));

    // act
    EventHubAuthCBS_Config_Destroy(cfg);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//#################################################################################################
// EventHubAuthCBS_Create Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_001: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig or cbsSessionHandle is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_Create_NULL_Param_eventHubAuthConfig)
{
    // arrange
    EVENTHUBAUTH_CBS_HANDLE h;

    // act
    h = EventHubAuthCBS_Create(NULL, TEST_SESSION_HANDLE_VALID);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_001: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig or cbsSessionHandle is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_Create_NULL_Param_cbsSessionHandle)
{
    // arrange
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // act
    h = EventHubAuthCBS_Create(&cfg, NULL);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_002: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_003: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO or EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_004: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->hostName or eventHubAuthConfig->eventHubPath are NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_005: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sasTokenExpirationTimeInSec or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is zero or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is greater than eventHubAuthConfig->sasTokenExpirationTimeInSec.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_006: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sharedAccessKeyName or eventHubAuthConfig->sharedAccessKey are NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_007: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->mode is EVENTHUBAUTH_MODE_RECEIVER and eventHubAuthConfig->receiverConsumerGroup or eventHubAuthConfig->receiverPartitionId is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_Create_ReceiverAuto_InvalidConfigParams)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.hostName = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL HostName");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.eventHubPath = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL EventHubPath");    

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sharedAccessKeyName = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL SharedAccessKeyName");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sharedAccessKey = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL SharedAccessKey");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.receiverConsumerGroup = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL ConsumerGroup");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.receiverPartitionId = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL PartitionId");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sasTokenExpirationTimeInSec = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sasTokenExpirationTimeInSec = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sasTokenRefreshPeriodInSecs = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Refresh Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.sasTokenRefreshPeriodInSecs = 2;
    cfg.sasTokenExpirationTimeInSec = 1;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Refresh Time Greater than Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.credential = EVENTHUBAUTH_CREDENTIAL_TYPE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Credential");

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    cfg.mode = EVENTHUBAUTH_MODE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Mode #1");
}

//**Tests_SRS_EVENTHUB_AUTH_29_002: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_003: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO or EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_004: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->hostName or eventHubAuthConfig->eventHubPath are NULL.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_005: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sasTokenExpirationTimeInSec or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is zero or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is greater than eventHubAuthConfig->sasTokenExpirationTimeInSec.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_006: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sharedAccessKeyName or eventHubAuthConfig->sharedAccessKey are NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_Create_SenderAuto_InvalidConfigParams)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.hostName = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL HostName");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.eventHubPath = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL EventHubPath");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sharedAccessKeyName = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL SharedAccessKeyName");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sharedAccessKey = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL SharedAccessKey");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sasTokenExpirationTimeInSec = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sasTokenExpirationTimeInSec = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sasTokenRefreshPeriodInSecs = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Zero Refresh Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.sasTokenRefreshPeriodInSecs = 2;
    cfg.sasTokenExpirationTimeInSec = 1;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Refresh Time Greater than Expiration Time");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.credential = EVENTHUBAUTH_CREDENTIAL_TYPE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Credential");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.mode = EVENTHUBAUTH_MODE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Mode #1");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    cfg.mode = EVENTHUBAUTH_MODE_RECEIVER;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Mode #2");
}

TEST_FUNCTION(EventHubAuthCBS_CreateReceiverAuto_Success)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    (void)TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuthCBS_CreateSenderAuto_Success)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    (void)TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuthCBS_CreateReceiverAuto_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBAUTH_CBS_HANDLE h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

TEST_FUNCTION(EventHubAuthCBS_CreateSenderAuto_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBAUTH_CBS_HANDLE h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//**Tests_SRS_EVENTHUB_AUTH_29_002: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_003: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO or EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_009: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if eventHubAuthConfig->extSASToken is NULL or eventHubAuthConfig->extSASTokenURI is NULL or eventHubAuthConfig->extSASTokenExpTSInEpochSec equals 0.**\]**
TEST_FUNCTION(EventHubAuthCBS_Create_Ext_InvalidConfigParams)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    cfg.extSASToken = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL Ext SAS Token Handle");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    cfg.extSASTokenURI = NULL;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL Ext SAS Token URI Handle");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    cfg.extSASTokenExpTSInEpochSec = 0;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With NULL Ext SAS Token Expiration Timestamp");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    cfg.credential = EVENTHUBAUTH_CREDENTIAL_TYPE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Credential");

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    cfg.mode = EVENTHUBAUTH_MODE_UNKNOWN;
    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // assert
    ASSERT_IS_NULL(h, "Unexpected Non NULL Data Handle With Invalid Mode");
}

TEST_FUNCTION(EventHubAuthCBS_CreateReceiverExt_Success)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverExt(&cfg);
    (void)TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuthCBS_CreateSenderExt_Success)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    (void)TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    // act
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuthCBS_CreateReceiverExt_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverExt(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBAUTH_CBS_HANDLE h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

TEST_FUNCTION(EventHubAuthCBS_CreateSenderExt_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Create(cfg.mode, cfg.credential);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBAUTH_CBS_HANDLE h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//#################################################################################################
// EventHubAuthCBS_Destroy Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_070: \[**EventHubAuthCBS_Destroy shall return immediately eventHubAuthHandle is NULL.**\]**
TEST_FUNCTION(EventHubAuthCBS_Destroy_NULLParam)
{
    // arrange

    // act
    EventHubAuthCBS_Destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

TEST_FUNCTION(EventHubAuthCBS_Destroy_ReceiverAutoSuccess)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    (void)TestSetupCallStack_EventHubAuthCBS_Destroy(false, cfg.mode, cfg.credential);

    // act
    EventHubAuthCBS_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

TEST_FUNCTION(EventHubAuthCBS_Destroy_SenderAutoSuccess)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    
    (void)TestSetupCallStack_EventHubAuthCBS_Destroy(false, cfg.mode, cfg.credential);

    // act
    EventHubAuthCBS_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

TEST_FUNCTION(EventHubAuthCBS_Destroy_ReceiverExtSuccess)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    (void)TestSetupCallStack_EventHubAuthCBS_Destroy(false, cfg.mode, cfg.credential);

    // act
    EventHubAuthCBS_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

TEST_FUNCTION(EventHubAuthCBS_Destroy_SenderExtSuccess)
{
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    (void)TestSetupCallStack_EventHubAuthCBS_Destroy(false, cfg.mode, cfg.credential);

    // act
    EventHubAuthCBS_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//#################################################################################################
// EventHubAuthCBS_Authenticate Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_101: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_NULLParam_eventHubAuthHandle)
{
    // arrange
    EVENTHUBAUTH_RESULT result;

    // act
    result = EventHubAuthCBS_Authenticate(NULL);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_INVALID_ARG, result);

    // cleanup
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate(false, cfg.mode, cfg.credential);

    // act 1
    result = EventHubAuthCBS_Authenticate(h);

    // assert 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // act 2
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // assert 2
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_Negative)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate(false, cfg.mode, cfg.credential);
    
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS priorStatus, currentStatus;
        
        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
        (void)EventHubAuthCBS_GetStatus(h, &priorStatus);
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            result = EventHubAuthCBS_Authenticate(h);
            (void)EventHubAuthCBS_GetStatus(h, &currentStatus);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");
            ASSERT_ARE_EQUAL(int, priorStatus, currentStatus, "Failed Status Should Remain Unchanged Test");
        }

        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_MultipleAuthSuccess)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication operation
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate(true, cfg.mode, cfg.credential);

    // act 1
    result = EventHubAuthCBS_Authenticate(h);

    // assert 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // act 2
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // assert 2
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_MultipleNegative)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate(true, cfg.mode, cfg.credential);
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS priorStatus, currentStatus;

        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
        
        // authenticate once
        (void)EventHubAuthCBS_Authenticate(h);
        
        // complete authentication operation
        g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

        // obtain status before the authenticate operation
        (void)EventHubAuthCBS_GetStatus(h, &priorStatus);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            result = EventHubAuthCBS_Authenticate(h);
            (void)EventHubAuthCBS_GetStatus(h, &currentStatus);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");
            ASSERT_ARE_EQUAL(int, priorStatus, currentStatus, "Failed Status Should Remain Unchanged Test");
        }
        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Auto_Success)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate(false, cfg.mode, cfg.credential);

    // act 1
    result = EventHubAuthCBS_Authenticate(h);

    // assert 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // act 2
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // assert 2
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Auto_Negative)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate(false, cfg.mode, cfg.credential);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS priorStatus, currentStatus;

        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

        // obtain status before the authenticate operation
        (void)EventHubAuthCBS_GetStatus(h, &priorStatus);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            result = EventHubAuthCBS_Authenticate(h);
            (void)EventHubAuthCBS_GetStatus(h, &currentStatus);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");
            ASSERT_ARE_EQUAL(int, priorStatus, currentStatus, "Failed Status Should Remain Unchanged Test");
        }
        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Auto_MultipleSuccess)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication operation
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate(true, cfg.mode, cfg.credential);

    // act 1
    result = EventHubAuthCBS_Authenticate(h);

    // assert 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // act 2
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // assert 2
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

// @note additional requirements captured at TestSetupCallStack_EventHubAuthCBS_Authenticate
//**Tests_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Auto_MultipleNegative)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Authenticate(true, cfg.mode, cfg.credential);
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS priorStatus, currentStatus;

        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
        // authenticate once
        (void)EventHubAuthCBS_Authenticate(h);
        // complete authentication operation
        g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

        // obtain status before the authenticate operation
        (void)EventHubAuthCBS_GetStatus(h, &priorStatus);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            result = EventHubAuthCBS_Authenticate(h);
            (void)EventHubAuthCBS_GetStatus(h, &currentStatus);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");
        }
        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//**Tests_SRS_EVENTHUB_AUTH_29_102: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_NOT_PERMITED immediately if the status is in EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_StatusTest_Success)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    
    // act 1, authenticate once
    (void)EventHubAuthCBS_Authenticate(h);
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed Return Status Value Test");

    // authenticate again before the put operation completed
    result = EventHubAuthCBS_Authenticate(h);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_102: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_NOT_PERMITED immediately if the status is in EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Ext_MultipleNotPermitted)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);

    // act
    // authenticate again before put operation completed
    result = EventHubAuthCBS_Authenticate(h);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_102: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_NOT_PERMITED immediately if the status is in EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_Auto_MultipleNotPermitted)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);

    // act
    // authenticate again before put operation completed
    result = EventHubAuthCBS_Authenticate(h);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_OnCBSPutTokenOperationComplete_Ext_Success)
{
    // arrange
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate_PutCallback(CBS_OPERATION_RESULT_OK);

    // act
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_151: \[**OnCBSPutTokenOperationComplete shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_OK if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_OnCBSPutTokenOperationComplete_StatusTestOnPutOk)
{
    // arrange
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);

    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate_PutCallback(CBS_OPERATION_RESULT_OK);
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Failed Return Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_152: \[**OnCBSPutTokenOperationComplete shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_FAILURE if cbs_operation_result is not CBS_OPERATION_RESULT_OK and a message shall be logged.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Authenticate_OnCBSPutTokenOperationComplete_StatusTestOnPutFail)
{
    // arrange
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate once
    (void)EventHubAuthCBS_Authenticate(h);
    (void)TestSetupCallStack_EventHubAuthCBS_Authenticate_PutCallback(CBS_OPERATION_RESULT_OK);
    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_CBS_ERROR, 0, NULL);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_FAILURE, status, "Failed Return Status Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//#################################################################################################
// EventHubAuthCBS_Refresh Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_200: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_NULLParam_eventHubAuthHandle)
{
    // arrange
    EVENTHUBAUTH_RESULT result;

    // act
    result = EventHubAuthCBS_Refresh(NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_INVALID_ARG, result);

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_201: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if extSASToken is NULL, EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_INVALID_ARG.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ExtSASTokenNULL)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_INVALID_ARG, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverExtNoAuthSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_Ext(false, EVENTHUBAUTH_MODE_RECEIVER);

    // act
    result = EventHubAuthCBS_Refresh(h, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_RECEIVER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverExtWithAuthSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigReceiverExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_Ext(true, EVENTHUBAUTH_MODE_RECEIVER);

    // act
    result = EventHubAuthCBS_Refresh(h, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_RECEIVER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderExtNoAuthSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_Ext(false, EVENTHUBAUTH_MODE_SENDER);

    // act
    result = EventHubAuthCBS_Refresh(h, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_SENDER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderExtWithAuthSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_Ext(true, EVENTHUBAUTH_MODE_SENDER);

    // act
    result = EventHubAuthCBS_Refresh(h, TEST_STRING_HANDLE_EXT_SASTOKEN_REFRESH_SENDER);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermittedNoAuthSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IDLE, status, "Could Not Execute Test Because Status Is Not Valid");

    // arrange
    umock_c_reset_all_calls();
    
    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermittedAuthCompleteSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Could Not Execute Test Because Status Is Not Valid");

    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermittedAuthCompleteSASToken_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            EVENTHUBAUTH_RESULT result;
            EVENTHUBAUTH_CBS_HANDLE h;

            h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

            // authenticate
            (void)EventHubAuthCBS_Authenticate(h);

            // complete authentication
            g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            result = EventHubAuthCBS_Refresh(h, NULL);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");
            
            // cleanup
            EventHubAuthCBS_Destroy(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermittedAuthPutErrorSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication failure
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_CBS_ERROR, 0, NULL);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermittedWithAuthInProgressAndTimeoutSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Could Not Execute Test Because Status Is Not In Progress");
    
    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack In Progress Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value In Progress Test");

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status == EVENTHUBAUTH_STATUS_IN_PROGRESS);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_TIMEOUT, status, "Could Not Execute Test Because Status Is Not In Timeout");

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Timeout Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Timeout Test");
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoNotPermitedExpiredSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status != EVENTHUBAUTH_STATUS_EXPIRED);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoAuthWithRefreshRequiredSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);
    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status == EVENTHUBAUTH_STATUS_OK);
    
    ASSERT_ARE_EQUAL(int, status, EVENTHUBAUTH_STATUS_REFRESH_REQUIRED);

    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_ReceiverAutoAuthWithRefreshRequiredSASToken_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigReceiverAuto(&cfg);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithRefreshRequired(cfg.mode);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS status;
        EVENTHUBAUTH_RESULT result;
        EVENTHUBAUTH_CBS_HANDLE h;

        TestHelper_ResetTestGlobal(&g_TestGlobal);
        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
        
        // authenticate
        result = EventHubAuthCBS_Authenticate(h);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_NOT_PERMITED");
        
        // complete authentication
        g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            do
            {
                (void)EventHubAuthCBS_GetStatus(h, &status);
            } while (status == EVENTHUBAUTH_STATUS_OK);
            
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            result = EventHubAuthCBS_Refresh(h, NULL);

            // assert
            if (result != EVENTHUBAUTH_RESULT_ERROR)
            {
                ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_NOT_PERMITED");
            }
            if (result != EVENTHUBAUTH_RESULT_NOT_PERMITED)
            {
                ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_ERROR");
            }
        }

        // cleanup
        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermittedNoAuthSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IDLE, status, "Could Not Execute Test Because Status Is Not Valid");

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermittedAuthCompleteSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Could Not Execute Test Because Status Is Not Valid");

    // arrange
    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermittedAuthCompleteSASToken_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            EVENTHUBAUTH_RESULT result;
            EVENTHUBAUTH_CBS_HANDLE h;

            h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

            // authenticate
            (void)EventHubAuthCBS_Authenticate(h);

            // complete authentication
            g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            result = EventHubAuthCBS_Refresh(h, NULL);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test");

            // cleanup
            EventHubAuthCBS_Destroy(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermittedAuthPutErrorSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication failure
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_CBS_ERROR, 0, NULL);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermittedWithAuthInProgressAndTimeoutSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    (void)EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Could Not Execute Test Because Status Is Not In Progress");

    // arrange
    TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithNoRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack In Progress Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value In Progress Test");

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status == EVENTHUBAUTH_STATUS_IN_PROGRESS);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_TIMEOUT, status, "Could Not Execute Test Because Status Is Not In Timeout");

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Timeout Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Timeout Test");
    (void)EventHubAuthCBS_GetStatus(h, &status);

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoNotPermitedExpiredSASTokenSuccess)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status != EVENTHUBAUTH_STATUS_EXPIRED);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoAuthWithRefreshRequiredSASToken_Success)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;
    EVENTHUBAUTH_STATUS status;

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);
    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    do
    {
        (void)EventHubAuthCBS_GetStatus(h, &status);
    } while (status == EVENTHUBAUTH_STATUS_OK);

    ASSERT_ARE_EQUAL(int, status, EVENTHUBAUTH_STATUS_REFRESH_REQUIRED);

    (void)TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithRefreshRequired(cfg.mode);

    // act
    result = EventHubAuthCBS_Refresh(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_Refresh_SenderAutoAuthWithRefreshRequiredSASToken_Negative)
{
    EVENTHUBAUTH_CBS_CONFIG cfg;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_EventHubAuthCBS_Refresh_AutoWithRefreshRequired(cfg.mode);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBAUTH_STATUS status;
        EVENTHUBAUTH_RESULT result;
        EVENTHUBAUTH_CBS_HANDLE h;

        TestHelper_ResetTestGlobal(&g_TestGlobal);
        h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);
        // authenticate
        result = EventHubAuthCBS_Authenticate(h);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_NOT_PERMITED");
        // complete authentication
        g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            do
            {
                (void)EventHubAuthCBS_GetStatus(h, &status);
            } while (status == EVENTHUBAUTH_STATUS_OK);

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);

            // act
            result = EventHubAuthCBS_Refresh(h, NULL);

            // assert
            if (result != EVENTHUBAUTH_RESULT_ERROR)
            {
                ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_NOT_PERMITED, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_NOT_PERMITED");
            }
            if (result != EVENTHUBAUTH_RESULT_NOT_PERMITED)
            {
                ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_ERROR, result, "Failed Return Value Test For EVENTHUBAUTH_RESULT_ERROR");
            }
        }

        // cleanup
        EventHubAuthCBS_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//#################################################################################################
// EventHubAuthCBS_GetStatus Tests
//#################################################################################################

//**Tests_SRS_EVENTHUB_AUTH_29_250: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle or returnStatus is NULL.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_NULLParam_eventHubAuthHandle)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;

    // act
    result = EventHubAuthCBS_GetStatus(NULL, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_INVALID_ARG, result, "Failed Return Test");

    // cleanup
}

//**Tests_SRS_EVENTHUB_AUTH_29_250: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle or returnStatus is NULL.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_NULLParam_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderExt(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // act
    result = EventHubAuthCBS_GetStatus(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_INVALID_ARG, result, "Failed Return Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_StatusIdleNonAuth_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IDLE, status, "Failed Status Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_GetStatus shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_253: \[**EventHubAuthCBS_GetStatus shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_254: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_255: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_StatusInProgressAndTimeoutAuth_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for In Progress Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed In Progress Status Test. Line:" TOSTRING(__LINE__));

    do
    {
        result = EventHubAuthCBS_GetStatus(h, &status);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for In Progress Test. Line:" TOSTRING(__LINE__));
    } while (status == EVENTHUBAUTH_STATUS_IN_PROGRESS);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_TIMEOUT, status, "Failed Timeout Status Test. Line:" TOSTRING(__LINE__));

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_GetStatus shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_253: \[**EventHubAuthCBS_GetStatus shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_254: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_255: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_StatusFailedAuth_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // fail authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_CBS_ERROR, 0, NULL);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_FAILURE, status, "Failed Status Test");

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_GetStatus shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_253: \[**EventHubAuthCBS_GetStatus shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_254: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_255: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_StatusOKToRefreshRequiredToOkAuth_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    result = EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for In Progress Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed In Progress Status Test. Line:" TOSTRING(__LINE__));

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for EVENTHUBAUTH_STATUS_OK Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Failed EVENTHUBAUTH_STATUS_OK Status Test. Line:" TOSTRING(__LINE__));

    do
    {
        result = EventHubAuthCBS_GetStatus(h, &status);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for EVENTHUBAUTH_STATUS_OK Test. Line:" TOSTRING(__LINE__));
    } while (status == EVENTHUBAUTH_STATUS_OK);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, status, "Failed Refresh Required Status Test. Line:" TOSTRING(__LINE__));

    result = EventHubAuthCBS_Refresh(h, NULL);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed EventHubAuthCBS_Refresh Return Test. Line:" TOSTRING(__LINE__));

    result = EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for In Progress Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_IN_PROGRESS, status, "Failed In Progress Status Test. Line:" TOSTRING(__LINE__));

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    result = EventHubAuthCBS_GetStatus(h, &status);
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for EVENTHUBAUTH_STATUS_OK Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Failed EVENTHUBAUTH_STATUS_OK Status Test. Line:" TOSTRING(__LINE__));

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

//**Tests_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_GetStatus shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_253: \[**EventHubAuthCBS_GetStatus shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_254: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**
//**Tests_SRS_EVENTHUB_AUTH_29_255: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
TEST_FUNCTION(EventHubAuth_EventHubAuthCBS_GetStatus_StatusOKToRefreshRequiredToExpiredAuth_status)
{
    // arrange
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_STATUS status;
    EVENTHUBAUTH_CBS_HANDLE h;
    EVENTHUBAUTH_CBS_CONFIG cfg;

    TestHelper_ResetTestGlobal(&g_TestGlobal);
    TestHelper_InitEventhHubAuthConfigSenderAuto(&cfg);
    h = EventHubAuthCBS_Create(&cfg, TEST_SESSION_HANDLE_VALID);

    // authenticate
    (void)EventHubAuthCBS_Authenticate(h);

    // complete authentication
    g_TestGlobal.sasTokenPutCB(g_TestGlobal.sasTokenPutCBContext, CBS_OPERATION_RESULT_OK, 0, NULL);

    // act
    result = EventHubAuthCBS_GetStatus(h, &status);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for EVENTHUBAUTH_STATUS_OK Test. Line:" TOSTRING(__LINE__));
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_OK, status, "Failed EVENTHUBAUTH_STATUS_OK Status Test. Line:" TOSTRING(__LINE__));

    EVENTHUBAUTH_STATUS prevStatus = status;
    do
    {
        result = EventHubAuthCBS_GetStatus(h, &status);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Return Value for EVENTHUBAUTH_STATUS_OK Test. Line:" TOSTRING(__LINE__));
    } while (status == EVENTHUBAUTH_STATUS_OK);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, status, "Failed Refresh Required Status Test. Line:" TOSTRING(__LINE__));

    do
    {
        result = EventHubAuthCBS_GetStatus(h, &status);
        ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_RESULT_OK, result, "Failed Refresh Required Return Value Test. Line:" TOSTRING(__LINE__));
    } while (status == EVENTHUBAUTH_STATUS_REFRESH_REQUIRED);

    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_STATUS_EXPIRED, status, "Failed Expired Status Test. Line:" TOSTRING(__LINE__));

    // cleanup
    EventHubAuthCBS_Destroy(h);
}

END_TEST_SUITE(eventhubauth_unittests)
