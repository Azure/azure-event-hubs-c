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

#include "azure_c_shared_utility/connection_string_parser.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/sastoken.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/platform.h"

#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/message_receiver.h"
#include "azure_c_shared_utility/sastoken.h"

#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"
#include "azure_uamqp_c/sasl_plain.h"
#include "azure_uamqp_c/amqp_definitions_terminus_durability.h"
#include "azure_uamqp_c/amqp_definitions_terminus_expiry_policy.h"
#include "azure_uamqp_c/amqp_definitions_seconds.h"
#include "azure_uamqp_c/amqp_definitions_node_properties.h"
#include "azure_uamqp_c/amqp_definitions_filter_set.h"
#include "azure_uamqp_c/amqp_definitions_source.h"
#include "azure_uamqp_c/amqp_definitions_application_properties.h"
#include "eventdata.h"
#include "eventhubauth.h"
#include "version.h"
#undef  ENABLE_MOCKS

// interface under test
#include "eventhubreceiver_ll.h"

//#################################################################################################
// EventHubReceiver LL Test Defines and Data types
//#################################################################################################
#define TEST_EVENTHUB_RECEIVER_LL_VALID                 (EVENTHUBRECEIVER_LL_HANDLE)0x43
#define TEST_EVENTDATA_HANDLE_VALID                     (EVENTDATA_HANDLE)0x45

#define TEST_MAP_HANDLE_VALID                           (MAP_HANDLE)0x46
#define TEST_TICK_COUNTER_HANDLE_VALID                  (TICK_COUNTER_HANDLE)0x47
// ensure that this is always >= 1000
#define TEST_EVENTHUB_RECEIVER_TIMEOUT_MS               1000
#define TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP            10000

#define TEST_CONNECTION_STRING_HANDLE_VALID             (STRING_HANDLE)0x100
#define TEST_EVENTHUBPATH_STRING_HANDLE_VALID           (STRING_HANDLE)0x101
#define TEST_CONSUMERGROUP_STRING_HANDLE_VALID          (STRING_HANDLE)0x102
#define TEST_PARTITIONID_STRING_HANDLE_VALID            (STRING_HANDLE)0x103
#define TEST_SHAREDACCESSKEYNAME_STRING_HANDLE_VALID    (STRING_HANDLE)0x104
#define TEST_SHAREDACCESSKEY_STRING_HANDLE_VALID        (STRING_HANDLE)0x105
#define TEST_HOSTNAME_STRING_HANDLE_VALID               (STRING_HANDLE)0x106
#define TEST_TARGETADDRESS_STRING_HANDLE_VALID          (STRING_HANDLE)0x107
#define TEST_CONNECTION_ENDPOINT_HANDLE_VALID           (STRING_HANDLE)0x108
#define TEST_FILTER_QUERY_STRING_HANDLE_VALID           (STRING_HANDLE)0x109

#define TEST_SASL_INTERFACE_HANDLE                      (SASL_MECHANISM_INTERFACE_DESCRIPTION*)0x200
#define TEST_SASL_MECHANISM_HANDLE_VALID                (SASL_MECHANISM_HANDLE)0x201
#define TEST_TLS_IO_INTERFACE_DESCRPTION_HANDLE_VALID   (const IO_INTERFACE_DESCRIPTION*)0x202
#define TEST_SASL_CLIENT_IO_HANDLE_VALID                (const IO_INTERFACE_DESCRIPTION*)0x203
#define TEST_TLS_XIO_VALID_HANDLE                       (XIO_HANDLE)0x204
#define TEST_SASL_XIO_VALID_HANDLE                      (XIO_HANDLE)0x205
#define TEST_CONNECTION_HANDLE_VALID                    (CONNECTION_HANDLE)0x206
#define TEST_SESSION_HANDLE_VALID                       (SESSION_HANDLE)0x207
#define TEST_AMQP_VALUE_MAP_HANDLE_VALID                (AMQP_VALUE)0x208
#define TEST_AMQP_VALUE_SYMBOL_HANDLE_VALID             (AMQP_VALUE)0x209
#define TEST_AMQP_VALUE_FILTER_HANDLE_VALID             (AMQP_VALUE)0x210
#define TEST_AMQP_VALUE_DESCRIBED_HANDLE_VALID          (AMQP_VALUE)0x211
#define TEST_SOURCE_HANDLE_VALID                        (SOURCE_HANDLE)0x212
#define TEST_AMQP_SOURCE_HANDLE_VALID                   (AMQP_VALUE)0x213
#define TEST_MESSAGING_TARGET_VALID                     (AMQP_VALUE)0x214
#define TEST_LINK_HANDLE_VALID                          (LINK_HANDLE)0X215
#define TEST_MESSAGE_RECEIVER_HANDLE_VALID              (MESSAGE_RECEIVER_HANDLE)0x216
#define TEST_MESSAGE_HANDLE_VALID                       (MESSAGE_HANDLE)0x217
#define TEST_AMQP_DUMMY_KVP_KEY_VALUE                   (AMQP_VALUE)0x218
#define TEST_AMQP_DUMMY_KVP_VAL_VALUE                   (AMQP_VALUE)0x219
#define TEST_EVENTHUBCBSAUTH_HANDLE_VALID               (EVENTHUBAUTH_CBS_HANDLE)0x220

#define TEST_MESSAGE_APP_PROPS_HANDLE_VALID             (AMQP_VALUE)  0x300
#define TEST_EVENT_DATA_PROPS_MAP_HANDLE_VALID          (MAP_HANDLE)  0x301
#define TEST_AMQP_INPLACE_DESCRIPTOR                    (AMQP_VALUE)  0x302
#define TEST_AMQP_INPLACE_DESCRIBED_DESCRIPTOR          (AMQP_VALUE)  0x303
#define TEST_AMQP_MESSAGE_APP_PROPS_MAP_HANDLE_VALID    (AMQP_VALUE)  0x304
#define TEST_AMQP_MESSAGE_ANNOTATIONS_VALID             (annotations) 0x305
#define TEST_AMQP_MESSAGE_ANNOTATIONS_MAP_HANDLE_VALID  (AMQP_VALUE)  0x306
#define TEST_AMQP_MESSAGE_REJECTED_VALUE                (AMQP_VALUE)  0x307

#define SASTOKEN_EXT_EXPIRATION_TIMESTAMP               (uint64_t)1000
#define SASTOKEN_EXT_REFRESH_EXPIRATION_TIMESTAMP_1     (uint64_t)1001
#define SASTOKEN_EXT_REFRESH_EXPIRATION_TIMESTAMP_2     (uint64_t)1002

#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME         (STRING_HANDLE)0x401
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH     (STRING_HANDLE)0x402
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_CONSUMER_GROUP_VALUE (STRING_HANDLE)0x403
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE (STRING_HANDLE)0x404
#define TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER         (STRING_HANDLE)0x405
#define TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE          (STRING_HANDLE)0x406
#define TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1               (STRING_HANDLE)0x407
#define TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1_CLONE         (STRING_HANDLE)0x408
#define TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_2               (STRING_HANDLE)0x409
#define TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_2_CLONE         (STRING_HANDLE)0x410
#define TEST_STRING_HANDLE_EXT_RECEIVER_URI                     (STRING_HANDLE)0x411
#define TEST_STRING_HANDLE_EXT_RECEIVER_URI_CLONE               (STRING_HANDLE)0x412
#define TEST_STRING_HANDLE_EXT_REFRESH_1_RECEIVER_URI           (STRING_HANDLE)0x413
#define TEST_STRING_HANDLE_EXT_REFRESH_2_RECEIVER_URI           (STRING_HANDLE)0x414

#define TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE                   (EVENTHUBAUTH_STATUS)(100)

#define TESTHOOK_STRING_BUFFER_SZ                       256

#define AUTH_EXPIRATION_SECS                            3600
#define AUTH_EXPIRATION_SECS_EPOCH                      (1000 + 3600)
#define AUTH_REFRESH_SECS                               2880

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

typedef struct TEST_HOOK_MAP_KVP_STRUCT_TAG
{
    const char*   key;
    const char*   value;
    STRING_HANDLE handle;
    STRING_HANDLE handleClone;
} TEST_HOOK_MAP_KVP_STRUCT;

typedef struct TEST_EVENTHUB_RECEIVER_ASYNC_CALLBACK_TAG
{
    EVENTHUBRECEIVER_LL_HANDLE eventHubRxHandle;
    EVENTHUBRECEIVER_ASYNC_CALLBACK rxCallback;
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK rxErrorCallback;
    ON_MESSAGE_RECEIVER_STATE_CHANGED onMsgChangedCallback;
    ON_MESSAGE_RECEIVED onMsgReceivedCallback;
    void* rxCallbackCtxt;
    void* rxErrorCallbackCtxt;
    void* rxEndCallbackCtxt;
    const void* onMsgChangedCallbackCtxt;
    const void* onMsgReceivedCallbackCtxt;
    int rxCallbackCalled;
    int rxErrorCallbackCalled;
    int rxEndCallbackCalled;
    EVENTHUBRECEIVER_RESULT rxCallbackResult;
    EVENTHUBRECEIVER_RESULT rxErrorCallbackResult;
    EVENTHUBRECEIVER_RESULT rxEndCallbackResult;
    int messageApplicationPropertiesNULLTest;
    int messageAnnotationsNULLTest;
} TEST_EVENTHUB_RECEIVER_ASYNC_CALLBACK;

//#################################################################################################
// EventHubReceiver LL Test Data
//#################################################################################################
static TEST_MUTEX_HANDLE g_testByTest;

static int IS_INVOKED_STRING_construct_sprintf = 0;
static int STRING_construct_sprintf_Negative_Test = 0;
static int ConnectionDowWorkCalled = 0;
static int messagingDeliveryRejectedCalled = 0;
static EVENTHUBAUTH_CBS_CONFIG gAuthConfig;
static EVENTHUBAUTH_CBS_CONFIG *gDynamicParsedConfig = NULL;
static EVENTHUBAUTH_CBS_CONFIG *gRefreshToken1 = NULL;

static const char CONNECTION_STRING[]    = "Endpoint=sb://servicebusName.servicebus.windows.net/;SharedAccessKeyName=RootManageSharedAccessKey;SharedAccessKey=ICT5KKSJR/DW7OZVEQ7OPSSXU5TRMR6AIWLGI5ZIT/8=";
static const char EVENTHUB_PATH[]        = "eventHubName";
static const char CONSUMER_GROUP[]       = "ConsumerGroup";
static const char PARTITION_ID[]         = "0";
static const char TEST_HOST_NAME_KEY[]   = "servicebusName.servicebus.windows.net/";
static const char TEST_HOST_NAME_VALUE[] = "servicebusName.servicebus.windows.net";
static const size_t TEST_HOST_NAME_VALUE_LEN  = sizeof(TEST_HOST_NAME_VALUE) - 1;
static const char FILTER_BY_TIMESTAMP_VALUE[] = "amqp.annotation.x-opt-enqueuedtimeutc";
static const char SASTOKEN[] = "SAS TOKEN";
static const char SASTOKEN_REFRESH1[] = "SAS TOKEN REFRESH1";
static const char SASTOKEN_REFRESH2[] = "SAS TOKEN REFRESH2";

static const char* KEY_NAME_ENQUEUED_TIME = "x-opt-enqueued-time";

static TEST_HOOK_MAP_KVP_STRUCT connectionStringKVP[] =
{
    { "Endpoint", "sb://servicebusName.servicebus.windows.net/", TEST_CONNECTION_ENDPOINT_HANDLE_VALID, NULL },
    { "SharedAccessKeyName", "RootManageSharedAccessKey", TEST_SHAREDACCESSKEYNAME_STRING_HANDLE_VALID, NULL },
    { "SharedAccessKey", "ICT5KKSJR/DW7OZVEQ7OPSSXU5TRMR6AIWLGI5ZIT/8=", TEST_SHAREDACCESSKEY_STRING_HANDLE_VALID, NULL },
    { CONNECTION_STRING, CONNECTION_STRING, TEST_CONNECTION_STRING_HANDLE_VALID, NULL },
    { CONSUMER_GROUP, CONSUMER_GROUP, TEST_CONSUMERGROUP_STRING_HANDLE_VALID, NULL },
    { EVENTHUB_PATH, EVENTHUB_PATH, TEST_EVENTHUBPATH_STRING_HANDLE_VALID, TEST_TARGETADDRESS_STRING_HANDLE_VALID },
    { PARTITION_ID, PARTITION_ID, TEST_PARTITIONID_STRING_HANDLE_VALID, NULL },
    { TEST_HOST_NAME_KEY, TEST_HOST_NAME_VALUE, TEST_HOSTNAME_STRING_HANDLE_VALID, NULL },
    { "FilterByTimestamp", FILTER_BY_TIMESTAMP_VALUE, TEST_FILTER_QUERY_STRING_HANDLE_VALID, NULL },
    { "TargetAddress", "servicebusName.servicebus.windows.net", TEST_TARGETADDRESS_STRING_HANDLE_VALID, NULL },

    { TEST_HOST_NAME_KEY, TEST_HOST_NAME_VALUE, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME, TEST_HOSTNAME_STRING_HANDLE_VALID  },
    { EVENTHUB_PATH, EVENTHUB_PATH, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH, TEST_EVENTHUBPATH_STRING_HANDLE_VALID },
    { CONSUMER_GROUP, CONSUMER_GROUP, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_CONSUMER_GROUP_VALUE, TEST_CONSUMERGROUP_STRING_HANDLE_VALID },
    { PARTITION_ID, PARTITION_ID, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE, TEST_PARTITIONID_STRING_HANDLE_VALID },
    { SASTOKEN, SASTOKEN, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER, TEST_STRING_HANDLE_EXT_SASTOKEN_RECEIVER_CLONE },
    { SASTOKEN_REFRESH1, SASTOKEN_REFRESH1, TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1, TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1_CLONE },
    { NULL, NULL, NULL, NULL }
};

static TEST_EVENTHUB_RECEIVER_ASYNC_CALLBACK OnRxCBStruct;
static void* OnRxCBCtxt    = (void*)&OnRxCBStruct;
static void* OnRxEndCBCtxt = (void*)&OnRxCBStruct;
static void* OnErrCBCtxt   = (void*)&OnRxCBStruct;

//#################################################################################################
// EventHubReceiver LL Test Helper Implementations
//#################################################################################################
int TestHelper_Map_GetIndexByKey(const char* key)
{
    bool found = false;
    int idx = 0, result;

    while (connectionStringKVP[idx].key != NULL)
    {
        if (strcmp(connectionStringKVP[idx].key, key) == 0)
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

int TestHelper_Map_GetIndexByValue(const char* value)
{
    bool found = false;
    int idx = 0, result;

    while (connectionStringKVP[idx].key != NULL)
    {
        if (strcmp(connectionStringKVP[idx].value, value) == 0)
        {
            found = true;
            break;
        }
        idx++;
    }

    if (!found)
    {
        printf("Test Map, could not find by value:%s \r\n", value);
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
        while (connectionStringKVP[idx].key != NULL)
        {
            if (connectionStringKVP[idx].handle == h)
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
    return connectionStringKVP[index].key;
}

const char* TestHelper_Map_GetValue(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return connectionStringKVP[index].value;
}

STRING_HANDLE TestHelper_Map_GetStringHandle(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return connectionStringKVP[index].handle;
}

STRING_HANDLE TestHelper_Map_GetStringHandleClone(int index)
{
    ASSERT_ARE_NOT_EQUAL(int, -1, index);
    return connectionStringKVP[index].handleClone;
}

void TestHelper_SetNullMessageApplicationProperties(void)
{
    OnRxCBStruct.messageApplicationPropertiesNULLTest = 1;
}

int TestHelper_IsNullMessageApplicationProperties(void)
{
    return OnRxCBStruct.messageApplicationPropertiesNULLTest ? true : false;
}

void TestHelper_SetNullMessageAnnotations(void)
{
    OnRxCBStruct.messageAnnotationsNULLTest = 1;
}

int TestHelper_IsNullMessageAnnotations(void)
{
    return OnRxCBStruct.messageAnnotationsNULLTest ? true : false;
}

static void TestHelper_ResetTestGlobalData(void)
{
    memset(&OnRxCBStruct, 0, sizeof(TEST_EVENTHUB_RECEIVER_ASYNC_CALLBACK));
    IS_INVOKED_STRING_construct_sprintf = 0;
    STRING_construct_sprintf_Negative_Test = 0;
    ConnectionDowWorkCalled = 0;
    messagingDeliveryRejectedCalled = 0;
    memset(&gAuthConfig, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));
}

static int TestHelper_isSTRING_construct_sprintfInvoked(void)
{
    return IS_INVOKED_STRING_construct_sprintf;
}

void TestHelper_SetNegativeTestSTRING_construct_sprintf(void)
{
    STRING_construct_sprintf_Negative_Test = 1;
}

static int TestHelper_isConnectionDoWorkInvoked(void)
{
    return ConnectionDowWorkCalled;
}

static const EVENTHUBAUTH_CBS_CONFIG* TestHelper_GetCBSAuthConfig(void)
{
    return &gAuthConfig;
}

static void TestHelper_SetCBSAuthConfig(const EVENTHUBAUTH_CBS_CONFIG* cfg)
{
    memcpy(&gAuthConfig, cfg, sizeof(EVENTHUBAUTH_CBS_CONFIG));
}

static void TestHelper_InitEventhHubAuthConfigReceiverExt(EVENTHUBAUTH_CBS_CONFIG* pCfg)
{
    pCfg->hostName = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME;
    pCfg->eventHubPath = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH;
    pCfg->receiverConsumerGroup = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_CONSUMER_GROUP_VALUE;
    pCfg->receiverPartitionId = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE;
    pCfg->sharedAccessKeyName = NULL;
    pCfg->sharedAccessKey = NULL;
    pCfg->sasTokenAuthFailureTimeoutInSecs = 0;
    pCfg->sasTokenExpirationTimeInSec = 0;
    pCfg->sasTokenRefreshPeriodInSecs = 0;
    pCfg->extSASTokenExpTSInEpochSec = SASTOKEN_EXT_EXPIRATION_TIMESTAMP;
    pCfg->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;
    pCfg->mode = EVENTHUBAUTH_MODE_RECEIVER;
    pCfg->extSASToken = TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER;
    pCfg->extSASTokenURI = TEST_STRING_HANDLE_EXT_RECEIVER_URI;
    pCfg->senderPublisherId = NULL;
}
//#################################################################################################
// EventHubReceiver LL Test Hook Implementations
//#################################################################################################
static void TestHook_OnUMockCError(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

#ifdef __cplusplus
extern "C"
{
#endif
    STRING_HANDLE STRING_construct_sprintf(const char* format, ...)
    {
        (void)format;
        IS_INVOKED_STRING_construct_sprintf = 1;
        if (STRING_construct_sprintf_Negative_Test)
        {
            return NULL;
        }
        return TEST_FILTER_QUERY_STRING_HANDLE_VALID;
    }
#ifdef __cplusplus
}
#endif

static const char* TestHoook_Map_GetValueFromKey(MAP_HANDLE handle, const char* key)
{
    (void)handle;
    return TestHelper_Map_GetValue(TestHelper_Map_GetIndexByKey(key));
}

static STRING_HANDLE TestHook_STRING_construct(const char* psz)
{
    return TestHelper_Map_GetStringHandle(TestHelper_Map_GetIndexByValue(psz));
}

static STRING_HANDLE TestHook_STRING_construct_n(const char* psz, size_t n)
{
    int idx;
    char valueString[TESTHOOK_STRING_BUFFER_SZ];
    ASSERT_ARE_NOT_EQUAL(size_t, 0, n);
    // n + 1 because this API expects to allocate n + 1 bytes with the nth being null term char
    ASSERT_IS_FALSE(n + 1 > TESTHOOK_STRING_BUFFER_SZ);
    memcpy(valueString, psz, n);
    // always null term
    valueString[n] = 0;
    idx = TestHelper_Map_GetIndexByValue(valueString);
    size_t valueLen = strlen(TestHelper_Map_GetValue(idx));
    ASSERT_ARE_EQUAL(size_t, valueLen, n);
    return TestHelper_Map_GetStringHandle(idx);
}

static STRING_HANDLE TestHook_STRING_clone(STRING_HANDLE handle)
{
    return TestHelper_Map_GetStringHandleClone(TestHelper_Map_GetIndexByStringHandle(handle));
}

static const char* TestHook_STRING_c_str(STRING_HANDLE handle)
{
    return TestHelper_Map_GetValue(TestHelper_Map_GetIndexByStringHandle(handle));
}

static XIO_HANDLE TestHook_xio_create(const IO_INTERFACE_DESCRIPTION* io_interface_description, const void* xio_create_parameters)
{
    (void)xio_create_parameters;
    XIO_HANDLE result = NULL; 
    if (io_interface_description == TEST_TLS_IO_INTERFACE_DESCRPTION_HANDLE_VALID)
    {
        result = TEST_TLS_XIO_VALID_HANDLE;
    }
    else if (io_interface_description == TEST_SASL_CLIENT_IO_HANDLE_VALID)
    {
        result = TEST_SASL_XIO_VALID_HANDLE;
    }
    ASSERT_IS_NOT_NULL(result);

    return result;
}

static void TestHook_connection_dowork(CONNECTION_HANDLE h)
{
    (void)h;
    ConnectionDowWorkCalled = 1;
}

static MESSAGE_RECEIVER_HANDLE TestHook_messagereceiver_create(LINK_HANDLE link, ON_MESSAGE_RECEIVER_STATE_CHANGED on_message_receiver_state_changed, void* context)
{
    (void)link;
    OnRxCBStruct.onMsgChangedCallback = on_message_receiver_state_changed;
    OnRxCBStruct.onMsgChangedCallbackCtxt = context;
    return TEST_MESSAGE_RECEIVER_HANDLE_VALID;
}

static int TestHook_messagereceiver_open(MESSAGE_RECEIVER_HANDLE message_receiver, ON_MESSAGE_RECEIVED on_message_received, void* callback_context)
{
    (void)message_receiver;
    OnRxCBStruct.onMsgReceivedCallback = on_message_received;
    OnRxCBStruct.onMsgReceivedCallbackCtxt = callback_context;
    return 0;
}

static int TestHook_message_get_message_annotations(MESSAGE_HANDLE message, annotations* message_annotations)
{
    (void)message;
    if (OnRxCBStruct.messageAnnotationsNULLTest)
    {
        *message_annotations = NULL;
    }
    else
    {
        *message_annotations = TEST_AMQP_MESSAGE_ANNOTATIONS_VALID;
    }

    return 0;
}

static int TestHook_message_get_application_properties(MESSAGE_HANDLE message, AMQP_VALUE* application_properties)
{
    (void)message;
    if (OnRxCBStruct.messageApplicationPropertiesNULLTest)
    {
        *application_properties = NULL;
    }
    else
    {
        *application_properties = TEST_MESSAGE_APP_PROPS_HANDLE_VALID;
    }

    return 0;
}

static AMQP_VALUE TestHook_messaging_delivery_rejected(const char* error_condition, const char* error_description)
{
    (void)error_condition;
    (void)error_description;
    ASSERT_IS_NOT_NULL(error_condition, "error_condition should be non NULL");
    ASSERT_IS_NOT_NULL(error_condition, "error_description should be non NULL");
    messagingDeliveryRejectedCalled = 1;

    return TEST_AMQP_MESSAGE_REJECTED_VALUE;
}

static int TestHook_amqpvalue_get_map(AMQP_VALUE value, AMQP_VALUE* map_value)
{
    if (value == TEST_AMQP_INPLACE_DESCRIBED_DESCRIPTOR)
    {
        *map_value = TEST_AMQP_MESSAGE_APP_PROPS_MAP_HANDLE_VALID;
    }
    else
    {
        *map_value = TEST_AMQP_MESSAGE_ANNOTATIONS_MAP_HANDLE_VALID;
    }
    
    return 0;
}

static int TestHook_amqpvalue_get_map_pair_count(AMQP_VALUE map, uint32_t* pair_count)
{
    (void)map;
    *pair_count = 1;
    return 0;
}

static int TestHook_amqpvalue_get_map_key_value_pair(AMQP_VALUE map, uint32_t index, AMQP_VALUE* key, AMQP_VALUE* value)
{
    (void)map;
    (void)index;
    *key = TEST_AMQP_DUMMY_KVP_KEY_VALUE;
    *value = TEST_AMQP_DUMMY_KVP_VAL_VALUE;
    return 0;
}

static int TestHook_amqpvalue_get_symbol(AMQP_VALUE value, const char** symbol_value)
{
    (void)value;
    *symbol_value = KEY_NAME_ENQUEUED_TIME;
    return 0;
}

/**
    @note this function is truly only capable of 1 sec precision and thus is accurate only at second transitions.
    The implementation here prefers portability over accuracy as sub second precision is not a requirement for 
    these tests.
*/
static int TestHook_tickcounter_get_current_ms(TICK_COUNTER_HANDLE tick_counter, tickcounter_ms_t* current_ms)
{
    int result;
    if (tick_counter == TEST_TICK_COUNTER_HANDLE_VALID)
    {
        time_t nowTS = time(NULL);
        if (nowTS == (time_t)(-1))
        {
            result = -1;
        }
        else
        {
            *current_ms = ((tickcounter_ms_t)nowTS) * 1000;
            result = 0;
        }
    }
    else
    {
        result = -2;
    }

    return result;
}

static EVENTHUBAUTH_CBS_HANDLE TestHook_EventHubAuthCBS_Create(const EVENTHUBAUTH_CBS_CONFIG* eventHubAuthConfig, SESSION_HANDLE session_ignored)
{
    EVENTHUBAUTH_CBS_HANDLE result;
    (void)session_ignored;

    if (eventHubAuthConfig == NULL)
    {
        result = NULL;
    }
    else
    {
        TestHelper_SetCBSAuthConfig(eventHubAuthConfig);
        result = TEST_EVENTHUBCBSAUTH_HANDLE_VALID;
    }

    return result;
}

static EVENTHUBAUTH_RESULT TestHook_EventHubAuthCBS_GetStatus(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle, EVENTHUBAUTH_STATUS* returnStatus)
{
    EVENTHUBAUTH_RESULT result;

    if ((eventHubAuthHandle == NULL) || (returnStatus == NULL))
    {
        result = EVENTHUBAUTH_RESULT_INVALID_ARG;
    }
    else
    {
        *returnStatus = EVENTHUBAUTH_STATUS_OK;
        result = EVENTHUBAUTH_RESULT_OK;
    }

    return result;
}

static EVENTHUBAUTH_CBS_CONFIG* TestHook_EventHubAuthCBS_SASTokenParse(const char* sasToken)
{
    EVENTHUBAUTH_CBS_CONFIG* result;

    result = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    ASSERT_IS_NOT_NULL(result);
    memset(result, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));
    TestHelper_InitEventhHubAuthConfigReceiverExt(result);

    if (strstr(sasToken, "REFRESH1") != NULL)
    {
        result->extSASTokenExpTSInEpochSec = SASTOKEN_EXT_REFRESH_EXPIRATION_TIMESTAMP_1;
        result->extSASToken = TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1;
        result->extSASTokenURI = TEST_STRING_HANDLE_EXT_REFRESH_1_RECEIVER_URI;
        ASSERT_IS_NULL(gRefreshToken1, "gRefreshToken1 is non null");
        gRefreshToken1 = result;
    }
    else
    {
        ASSERT_IS_NULL(gDynamicParsedConfig, "gDynamicParsedConfig is non null");
        gDynamicParsedConfig = result;
    }

    return result;
}

static void TestHook_EventHubAuthCBS_Config_Destroy(EVENTHUBAUTH_CBS_CONFIG* cfg)
{
    if (cfg != NULL)
    {
        if (gDynamicParsedConfig == cfg)
        {
            TestHook_free(gDynamicParsedConfig);
            gDynamicParsedConfig = NULL;
        }
        else if (gRefreshToken1 == cfg)
        {
            TestHook_free(gRefreshToken1);
            gRefreshToken1 = NULL;
        }
        else
        {
            ASSERT_FAIL("Invalid EventHubAuthCBS Config Pointer");
        }
    }
}

//#################################################################################################
// EventHubReceiver LL Callback Implementations
//#################################################################################################
static void EventHubHReceiver_LL_OnRxCB(EVENTHUBRECEIVER_RESULT result, EVENTDATA_HANDLE eventDataHandle, void* userContext)
{
    (void)userContext;
    if (result == EVENTHUBRECEIVER_OK) ASSERT_IS_NOT_NULL(eventDataHandle, "Unexpected NULL Data Handle");
    if (result != EVENTHUBRECEIVER_OK) ASSERT_IS_NULL(eventDataHandle, "Unexpected Non NULL Data Handle");
    OnRxCBStruct.rxCallbackCalled = 1;
    OnRxCBStruct.rxCallbackResult = result;
}

static void EventHubHReceiver_LL_OnRxNoTimeoutCB(EVENTHUBRECEIVER_RESULT result, EVENTDATA_HANDLE eventDataHandle, void* userContext)
{
    (void)eventDataHandle;
    (void)userContext;
    ASSERT_ARE_NOT_EQUAL(int, EVENTHUBRECEIVER_TIMEOUT, result);
    OnRxCBStruct.rxCallbackCalled = 1;
    OnRxCBStruct.rxCallbackResult = result;
}

static void EventHubHReceiver_LL_OnErrCB(EVENTHUBRECEIVER_RESULT errorCode, void* userContext)
{
    (void)userContext;
    OnRxCBStruct.rxErrorCallbackCalled = 1;
    OnRxCBStruct.rxErrorCallbackResult = errorCode;
}

static void EventHubHReceiver_LL_OnRxEndCB(EVENTHUBRECEIVER_RESULT result, void* userContext)
{
    (void)userContext;
    OnRxCBStruct.rxEndCallbackCalled = 1;
    OnRxCBStruct.rxEndCallbackResult = result;
}

//#################################################################################################
// EventHubReceiver LL Common Callstack Test Setup Functions
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_101: \[**EventHubReceiver_LL_Create shall obtain the version string by a call to EVENTHUBRECEIVER_GetVersionString.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_102: \[**EventHubReceiver_LL_Create shall print the version string to standard output.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, EventHubReceiver_LL_Create shall return NULL and cleanup any allocated resources as needed.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_105: \[**EventHubReceiver_LL_Create shall allocate a new event hub receiver LL instance.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_106: \[**EventHubReceiver_LL_Create shall create a tickcounter using API tickcounter_create.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_107: \[**EventHubReceiver_LL_Create shall expect a service bus connection string in one of the following formats:
//Endpoint = sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
//Endpoint = sb ://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
//    **\] **
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_108: \[**EventHubReceiver_LL_Create shall create a temp connection STRING_HANDLE using connectionString as the parameter.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_109: \[**EventHubReceiver_LL_Create shall parse the connection string handle to a map of strings by using API connection_string_parser_parse.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_110: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument eventHubPath.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_111: \[**EventHubReceiver_LL_Create shall lookup the endpoint in the resulting map using API Map_GetValueFromKey and argument "Endpoint".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_112: \[**EventHubReceiver_LL_Create shall obtain the host name after parsing characters after substring "sb://".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_113: \[**EventHubReceiver_LL_Create shall create a host name STRING_HANDLE using API STRING_construct_n and the host name substring and its length obtained above as parameters.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_114: \[**EventHubReceiver_LL_Create shall lookup the SharedAccessKeyName in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKeyName".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_115: \[**EventHubReceiver_LL_Create shall create SharedAccessKeyName STRING_HANDLE using API STRING_construct and using the "SharedAccessKeyName" key's value obtained above as parameter.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_116: \[**EventHubReceiver_LL_Create shall lookup the SharedAccessKey in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKey".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_117: \[**EventHubReceiver_LL_Create shall create SharedAccessKey STRING_HANDLE using API STRING_construct and using the "SharedAccessKey" key's value obtained above as parameter.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_118: \[**EventHubReceiver_LL_Create shall destroy the map handle using API Map_Destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_119: \[**EventHubReceiver_LL_Create shall destroy the temp connection string handle using API STRING_delete.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_120: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument consumerGroup.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_121: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument partitionId.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_122: \[**EventHubReceiver_LL_Create shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_123: \[**EventHubReceiver_LL_Create shall return a non-NULL handle value upon success.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_124: \[**EventHubReceiver_LL_Create shall initialize connection tracing to false by default.**\]**
static uint64_t TestSetupCallStack_Create(void)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();

    EXPECTED_CALL(EventHubClient_GetVersionString());
    i++; // this function is not expected to fail and thus does not included in the bitmask

    // LL internal data structure allocation
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(tickcounter_create());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for temp connection string handling
    STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // parse connection string
    STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNECTION_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for event hub path 
    STRICT_EXPECTED_CALL(STRING_construct(EVENTHUB_PATH));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for (hostName) "Endpoint"
    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for host name TEST_HOST_NAME_VALUE
    STRICT_EXPECTED_CALL(STRING_construct_n(IGNORED_PTR_ARG, TEST_HOST_NAME_VALUE_LEN))
        .IgnoreArgument(1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for Key:SharedAccessKeyName value in connection string
    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_construct(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for Key:SharedAccessKey value in connection string
    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(STRING_construct(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // destroy map as connection string processing is done
    STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    // destroy temp connection string handle
    STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNECTION_STRING_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    // create string handle for consumer group
    STRICT_EXPECTED_CALL(STRING_construct(CONSUMER_GROUP));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // create string handle for partitionId
    STRICT_EXPECTED_CALL(STRING_construct(PARTITION_ID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // creation of string handle for partition URI 
    STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUBPATH_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGETADDRESS_STRING_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGETADDRESS_STRING_HANDLE_VALID, TEST_CONSUMERGROUP_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGETADDRESS_STRING_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGETADDRESS_STRING_HANDLE_VALID, TEST_PARTITIONID_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_151: \[**EventHubReceiver_LL_CreateFromSASToken shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_152: \[**EventHubReceiver_LL_CreateFromSASToken shall print the version string to standard output.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_154: \[**EventHubReceiver_LL_CreateFromSASToken parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_155: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL if EventHubAuthCBS_SASTokenParse fails and returns NULL.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_157: \[**EventHubReceiver_LL_CreateFromSASToken shall allocate memory to hold structure EVENTHUBRECEIVER_LL_STRUCT.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_158: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations on failures.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_159: \[**EventHubReceiver_LL_CreateFromSASToken shall create a tick counter handle using API tickcounter_create.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_160: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the hostName string using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_161: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the eventHubPath string using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_162: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the consumerGroup string using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_163: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the receiverPartitionId string using API STRING_clone.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_164: \[**EventHubReceiver_LL_CreateFromSASToken shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_165: \[**EventHubReceiver_LL_CreateFromSASToken shall initialize connection tracing to false by default.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_166: \[**EventHubReceiver_LL_CreateFromSASToken shall return the allocated EVENTHUBRECEIVER_LL_STRUCT on success.**\]**
static uint64_t TestSetupCallStack_CreateFromSASToken(const char* sasToken)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();

    EXPECTED_CALL(EventHubClient_GetVersionString());
    i++;

    EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(sasToken));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(tickcounter_create());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_clone(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_clone(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_clone(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_CONSUMER_GROUP_VALUE));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_clone(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_PARTITION_ID_VALUE));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // creation of string handle for partition URI 
    STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUBPATH_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGETADDRESS_STRING_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGETADDRESS_STRING_HANDLE_VALID, TEST_CONSUMERGROUP_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGETADDRESS_STRING_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGETADDRESS_STRING_HANDLE_VALID, TEST_PARTITIONID_STRING_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

static void TestSetupCallStack_ReceiveFromStartTimestampCommon(unsigned int waitTimeInMS)
{
    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    if (waitTimeInMS) STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TEST_TICK_COUNTER_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);

    //STRICT_EXPECTED_CALL(STRING_construct_sprintf(IGNORED_PTR_ARG, nowTS)).IgnoreArgument(1);
    // Since the above is not possible to do with the mocking framework 
    // use TestHelper_isSTRING_construct_sprintfInvoked() instead to see if STRING_construct_sprintf was invoked at all
    // Ex: ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");
}

static void TestSetupCallStack_ReceiveEndAsync(void)
{
    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_605: \[**The session shall be freed by calling session_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_606: \[**The connection shall be freed by calling connection_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_607: \[**The SASL client IO shall be freed by calling xio_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_608: \[**The TLS IO shall be freed by calling xio_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_609: \[**The SASL mechanism shall be freed by calling saslmechanism_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_610: \[**The filter string shall be freed by STRING_delete.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_613: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_TearDown_Common(uint64_t failedFunctionBitmask, int* currentIndex)
{
    int i = *currentIndex;

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Destroy(TEST_EVENTHUBCBSAUTH_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(session_destroy(TEST_SESSION_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(connection_destroy(TEST_CONNECTION_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(xio_destroy(TEST_SASL_XIO_VALID_HANDLE));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(xio_destroy(TEST_TLS_XIO_VALID_HANDLE));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(saslmechanism_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(STRING_delete(TEST_FILTER_QUERY_STRING_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    *currentIndex = i;

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_601: \[**`EventHubReceiver_LL_DoWork` shall do the work to tear down the AMQP stack when a user had called `EventHubReceiver_LL_ReceiveEndAsync`.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_602: \[**All pending message data not reported to the calling client shall be freed by calling messagereceiver_close and messagereceiver_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_603: \[**The link shall be freed by calling link_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_604: \[**`EventHubAuthCBS_Destroy` shall be called to destroy the event hub auth handle.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_605: \[**The session shall be freed by calling session_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_606: \[**The connection shall be freed by calling connection_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_607: \[**The SASL client IO shall be freed by calling xio_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_608: \[**The TLS IO shall be freed by calling xio_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_609: \[**The SASL mechanism shall be freed by calling saslmechanism_destroy.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_610: \[**The filter string shall be freed by STRING_delete.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_611: \[**Upon Success, `EventHubReceiver_LL_DoWork` shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_OK.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_612: \[**Upon failure, `EventHubReceiver_LL_DoWork` shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_ERROR.**\]**
static uint64_t TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_TearDown_Common(uint64_t failedFunctionBitmask, int* currentIndex)
{
    int i = *currentIndex;

    STRICT_EXPECTED_CALL(messagereceiver_close(TEST_MESSAGE_RECEIVER_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(messagereceiver_destroy(TEST_MESSAGE_RECEIVER_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_TearDown_Common(failedFunctionBitmask, &i);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    *currentIndex = i;

    return failedFunctionBitmask;
}

static uint64_t TestSetupCallStack_DoWork_PreAuth_ActiveTearDown(void)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_TearDown_Common(failedFunctionBitmask, &i);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

static uint64_t TestSetupCallStack_DoWork_PostAuth_ActiveTearDown(void)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_TearDown_Common(failedFunctionBitmask, &i);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_451: \[**`EventHubReceiver_LL_DoWork` shall initialize and bring up the uAMQP stack if it has not already brought up**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_501: \[**The SASL interface to be passed into saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_502: \[**A SASL mechanism shall be created by calling saslmechanism_create with the interface obtained above.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_503: \[**If saslmssbcbs_get_interface fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_504: \[**A TLS IO shall be created by calling xio_create using TLS port 5671 and host name obtained from the connection string**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_505: \[**The interface passed to xio_create shall be obtained by calling platform_get_default_tlsio.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_506: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_507: \[**The SASL client IO interface shall be obtained using `saslclientio_get_interface_description`**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_508: \[**A SASL client IO shall be created by calling xio_create using TLS IO interface created previously and the SASL  mechanism created earlier. The SASL client IO interface to be used will be the one obtained above.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_509: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_510: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle created previously, hostname, connection name and NULL for the new session handler end point and context.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_511: \[**If connection_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_512: \[**Connection tracing shall be called with the current value of the tracing flag**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_513: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_514: \[**If session_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_515: \[**Configure the session incoming window by calling `session_set_incoming_window` and set value to INTMAX.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_516: \[**If `saslclientio_get_interface_description` returns NULL, a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_521: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a `EVENTHUBAUTH_CBS_CONFIG` structure params hostName, eventHubPath, receiverConsumerGroup, receiverPartitionId, sharedAccessKeyName, sharedAccessKey using previously set values. Set senderPublisherId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_RECEIVER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_522: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the `EVENTHUBAUTH_CBS_CONFIG` obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_524: \[**`EventHubAuthCBS_Create` shall be invoked using the config structure reference and the session handle created earlier.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_525: \[**If `EventHubAuthCBS_Create` returns NULL, a log message will be logged and the function returns immediately.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Internal(uint64_t failedFunctionBitmask, int* currentIndex)
{
    int i = *currentIndex;

    STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_HANDLE, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // HostName
    STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(platform_get_default_tlsio());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(xio_create(TEST_TLS_IO_INTERFACE_DESCRPTION_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(xio_create(TEST_SASL_CLIENT_IO_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_create(TEST_SASL_XIO_VALID_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
        .IgnoreArgument(2).IgnoreArgument(3).IgnoreArgument(4).IgnoreArgument(5);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE_VALID, 0));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE_VALID, NULL, NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(session_set_incoming_window(TEST_SESSION_HANDLE_VALID, IGNORED_NUM_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    //if (wasRefreshTokenApplied)
    //{
    //    STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_RECEIVER));
    //    i++;
    //}

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Create(IGNORED_NUM_ARG, TEST_SESSION_HANDLE_VALID))
        .IgnoreArgument(1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    *currentIndex = i;

    return failedFunctionBitmask;
}

static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Common(uint64_t failedFunctionBitmask, int* currentIndex)
{
    return TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Internal(failedFunctionBitmask, currentIndex);
}

static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_WithRefreshToken_Common(uint64_t failedFunctionBitmask, int* currentIndex)
{
    return TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Internal(failedFunctionBitmask, currentIndex);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_544: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, `connection_dowork` shall be invoked to perform work to establish/refresh the SAS token.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusInProgress(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_IN_PROGRESS;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Common(failedFunctionBitmask, &i);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_543: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT and `EventHubReceiver_LL_DoWork` shall bring down AMQP stack so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Status_FailureOrExpiredOrTimeoutOrUnknown(EVENTHUBAUTH_STATUS status)
{
    const static EVENTHUBAUTH_STATUS statusFailure = EVENTHUBAUTH_STATUS_FAILURE;
    const static EVENTHUBAUTH_STATUS statusExpired = EVENTHUBAUTH_STATUS_EXPIRED;
    const static EVENTHUBAUTH_STATUS statusTimeout = EVENTHUBAUTH_STATUS_TIMEOUT;
    const static EVENTHUBAUTH_STATUS statusUnknown = TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE;

    const EVENTHUBAUTH_STATUS* statusTestInput;

    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    statusTestInput = (status == EVENTHUBAUTH_STATUS_FAILURE) ? &statusFailure : (status == EVENTHUBAUTH_STATUS_EXPIRED) ? &statusExpired : (status == EVENTHUBAUTH_STATUS_TIMEOUT) ? &statusTimeout : &statusUnknown;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Common(failedFunctionBitmask, &i);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, statusTestInput, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_TearDown_Common(failedFunctionBitmask, &i);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_543: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT and `EventHubReceiver_LL_DoWork` shall bring down AMQP stack so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_PostAuth_AMQP_Stack_Teardown_Status_FailureOrExpiredOrTimeoutOrUnknown(EVENTHUBAUTH_STATUS status)
{
    const static EVENTHUBAUTH_STATUS statusFailure = EVENTHUBAUTH_STATUS_FAILURE;
    const static EVENTHUBAUTH_STATUS statusExpired = EVENTHUBAUTH_STATUS_EXPIRED;
    const static EVENTHUBAUTH_STATUS statusTimeout = EVENTHUBAUTH_STATUS_TIMEOUT;
    const static EVENTHUBAUTH_STATUS statusUnknown = TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE;

    const EVENTHUBAUTH_STATUS* statusTestInput;

    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    statusTestInput = (status == EVENTHUBAUTH_STATUS_FAILURE) ? &statusFailure : (status == EVENTHUBAUTH_STATUS_EXPIRED) ? &statusExpired : (status == EVENTHUBAUTH_STATUS_TIMEOUT) ? &statusTimeout : &statusUnknown;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, statusTestInput, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    failedFunctionBitmask = TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_TearDown_Common(failedFunctionBitmask, &i);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_547: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusOk(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_OK;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Common(failedFunctionBitmask, &i);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_546: \[**If status is EVENTHUBAUTH_STATUS_IDLE, `EventHubAuthCBS_Authenticate` shall be invoked to create and install the SAS token.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_IDLE;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Common(failedFunctionBitmask, &i);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Authenticate(TEST_EVENTHUBCBSAUTH_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_546: \[**If status is EVENTHUBAUTH_STATUS_IDLE, `EventHubAuthCBS_Authenticate` shall be invoked to create and install the SAS token.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle_WithRefreshToken(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_IDLE;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    umock_c_reset_all_calls();

    failedFunctionBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_WithRefreshToken_Common(failedFunctionBitmask, &i);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Authenticate(TEST_EVENTHUBCBSAUTH_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_451: \[**`EventHubReceiver_LL_DoWork` shall initialize and bring up the uAMQP stack if it has not already brought up**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_453: \[**`EventHubReceiver_LL_DoWork` shall initialize the uAMQP Message Receiver stack if it has not already brought up. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_454: \[**`EventHubReceiver_LL_DoWork` shall create a message receiver if not already created. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_455: \[**`EventHubReceiver_LL_DoWork` shall invoke connection_dowork **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_561: \[**A filter_set shall be created and initialized using key "apache.org:selector-filter:string" and value as the query filter created previously.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_564: \[**If messaging_create_target fails, then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_565: \[**The message receiver 'source' shall be created using source_create.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_566: \[**An AMQP link for the to be created message receiver shall be created by calling link_create with role as role_receiver and name as "receiver-link"**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_567: \[**The message receiver link 'source' shall be created using API amqpvalue_create_source**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_568: \[**The message receiver link 'source' filter shall be initialized by calling source_set_filter and using the filter_set created earlier.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_569: \[**The message receiver link 'source' address shall be initialized using the partition target address created earlier.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_570: \[**The message receiver link target shall be created using messaging_create_target with address obtained from the partition target address created earlier.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_571: \[**If link_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_572: \[**Configure the link settle mode by calling link_set_rcv_settle_mode and set value to receiver_settle_mode_first.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_573: \[**If link_set_rcv_settle_mode fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_574: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_575: \[**If link_set_max_message_size fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_576: \[**A message receiver shall be created by calling messagereceiver_create and passing as arguments the link handle, a state changed callback and context.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_577: \[**If messagereceiver_create fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_578: \[**The created message receiver shall be transitioned to OPEN by calling messagereceiver_open.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_579: \[**If messagereceiver_open fails then a log message will be logged and the function returns immediately.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_620: \[**`EventHubReceiver_LL_DoWork` shall setup the message receiver_create by passing in `EHR_LL_OnStateChanged` as the ON_MESSAGE_RECEIVER_STATE_CHANGED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_create is called.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_621: \[**`EventHubReceiver_LL_DoWork` shall open the message receiver_create by passing in `EHR_LL_OnMessageReceived` as the ON_MESSAGE_RECEIVED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_open is called.**\]**
static uint64_t TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_Bringup(unsigned int waitTimeoutMs)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_OK;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    // arrange
    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(amqpvalue_create_map());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(amqpvalue_create_symbol(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(amqpvalue_create_symbol(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_FILTER_QUERY_STRING_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(amqpvalue_create_string(FILTER_BY_TIMESTAMP_VALUE));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(amqpvalue_create_described(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(amqpvalue_set_map_value(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGETADDRESS_STRING_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    STRICT_EXPECTED_CALL(source_create());
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(amqpvalue_create_string(IGNORED_PTR_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(source_set_address(TEST_SOURCE_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(source_set_filter(TEST_SOURCE_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(amqpvalue_create_source(TEST_SOURCE_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(messaging_create_target(IGNORED_PTR_ARG)).IgnoreAllArguments(); // TEST_MESSAGING_TARGET_VALID
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE_VALID, IGNORED_PTR_ARG, 1, TEST_AMQP_SOURCE_HANDLE_VALID, TEST_MESSAGING_TARGET_VALID))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(link_set_rcv_settle_mode(TEST_LINK_HANDLE_VALID, 0));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE_VALID, (256 * 1024)));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(messagereceiver_create(TEST_LINK_HANDLE_VALID, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreArgument(2).IgnoreArgument(3);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(messagereceiver_open(TEST_MESSAGE_RECEIVER_HANDLE_VALID, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreArgument(2).IgnoreArgument(3);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(source_destroy(TEST_SOURCE_HANDLE_VALID)); //TEST_AMQP_SOURCE_HANDLE_VALID
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    if (waitTimeoutMs > 0)
    {
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TEST_TICK_COUNTER_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);
    }

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    if (waitTimeoutMs > 0)
    {
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TEST_TICK_COUNTER_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);
        i++; // we skip this because this is covered in another test
    }

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_545: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusRefreshRequired(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_REFRESH_REQUIRED;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, NULL));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_404: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_405: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_406: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubReceiver_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_407: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR if eventHubRefreshSasToken is not compatible.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_408: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in EventHubReceiver_LL_DoWork.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_410: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_OK on success.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_411: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR on failure.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_412: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
static uint64_t TestSetupCallStack_EventHubReceiver_LL_RefreshSASTokenAsync(const char* sasToken)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(sasToken));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_compare(TEST_STRING_HANDLE_EXT_REFRESH_1_RECEIVER_URI, TEST_STRING_HANDLE_EXT_RECEIVER_URI)).IgnoreArgument(1);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_construct(sasToken));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(IGNORED_PTR_ARG));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_547: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
static uint64_t TestSetupCallStack_DoWork_PostAuthComplete_StatusOk_ExtRefreshTokenApplied(void)
{
    const static EVENTHUBAUTH_STATUS status = EVENTHUBAUTH_STATUS_OK;
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).CopyOutArgumentBuffer(2, &status, sizeof(EVENTHUBAUTH_STATUS));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1));
    i++;

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));
    i++;

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_641: \[**When `EHR_LL_OnMessageReceived` is invoked, message_get_body_amqp_data_in_place shall be called to obtain the data into a BINARY_DATA buffer.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_642: \[**`EHR_LL_OnMessageReceived` shall create a EVENT_DATA handle using EventData_CreateWithNewMemory and pass in the buffer data pointer and size as arguments.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_643: \[**`EHR_LL_OnMessageReceived` shall obtain the application properties using message_get_application_properties() and populate the EVENT_DATA handle map with these key value pairs.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_644: \[**`EHR_LL_OnMessageReceived` shall obtain event data specific properties using message_get_message_annotations() and populate the EVENT_DATA handle with these properties.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_646: \[**`EHR_LL_OnMessageReceived` shall invoke the user registered onMessageReceive callback with status code EVENTHUBRECEIVER_OK, the EVENT_DATA handle and the context passed in by the user.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_647: \[**After `EHR_LL_OnMessageReceived` invokes the user callback, messaging_delivery_accepted shall be called.**\]**
static uint64_t TestSetupCallStack_OnMessageReceived(void)
{
    uint64_t failedFunctionBitmask = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();

    //////////////////////////////////////////////////////////////////////////////////////////
    //   Event Data                                                                         //
    //////////////////////////////////////////////////////////////////////////////////////////
    STRICT_EXPECTED_CALL(message_get_body_amqp_data_in_place(TEST_MESSAGE_HANDLE_VALID, 0, IGNORED_PTR_ARG))
        .IgnoreArgument(3);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    EXPECTED_CALL(EventData_CreateWithNewMemory(IGNORED_PTR_ARG, IGNORED_NUM_ARG));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    //////////////////////////////////////////////////////////////////////////////////////////
    //   Event Partition Key                                                                //
    //////////////////////////////////////////////////////////////////////////////////////////
    STRICT_EXPECTED_CALL(message_get_message_annotations(TEST_MESSAGE_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    if (!TestHelper_IsNullMessageAnnotations())
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_map(TEST_AMQP_MESSAGE_ANNOTATIONS_VALID, IGNORED_PTR_ARG))
            .IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_map_pair_count(TEST_AMQP_MESSAGE_ANNOTATIONS_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
            .IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_map_key_value_pair(TEST_AMQP_MESSAGE_ANNOTATIONS_MAP_HANDLE_VALID, 0, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .IgnoreArgument(3).IgnoreArgument(4);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_get_symbol(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_get_timestamp(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(EventData_SetEnqueuedTimestampUTCInMs(TEST_EVENTDATA_HANDLE_VALID, IGNORED_NUM_ARG))
            .IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask

        EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask

        EXPECTED_CALL(annotations_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    //   Event Application Properties                                                       //
    //////////////////////////////////////////////////////////////////////////////////////////
    STRICT_EXPECTED_CALL(EventData_Properties(TEST_EVENTDATA_HANDLE_VALID));
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    STRICT_EXPECTED_CALL(message_get_application_properties(TEST_MESSAGE_HANDLE_VALID, IGNORED_PTR_ARG))
        .IgnoreArgument(2);
    failedFunctionBitmask |= ((uint64_t)1 << i++);

    if (!TestHelper_IsNullMessageApplicationProperties())
    {
        STRICT_EXPECTED_CALL(amqpvalue_get_inplace_descriptor(TEST_MESSAGE_APP_PROPS_HANDLE_VALID));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_inplace_described_value(TEST_MESSAGE_APP_PROPS_HANDLE_VALID));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_map(TEST_AMQP_INPLACE_DESCRIBED_DESCRIPTOR, IGNORED_PTR_ARG))
            .IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_map_pair_count(TEST_AMQP_MESSAGE_APP_PROPS_MAP_HANDLE_VALID, IGNORED_PTR_ARG))
            .IgnoreArgument(2);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(amqpvalue_get_map_key_value_pair(TEST_AMQP_MESSAGE_APP_PROPS_MAP_HANDLE_VALID, 0, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .IgnoreArgument(3).IgnoreArgument(4);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_get_string(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_get_string(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        STRICT_EXPECTED_CALL(Map_Add(TEST_EVENT_DATA_PROPS_MAP_HANDLE_VALID, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .IgnoreArgument(2).IgnoreArgument(3);
        failedFunctionBitmask |= ((uint64_t)1 << i++);

        EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask

        EXPECTED_CALL(amqpvalue_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask

        EXPECTED_CALL(application_properties_destroy(IGNORED_PTR_ARG));
        i++; // this function is not expected to fail and thus does not included in the bitmask
    }

    STRICT_EXPECTED_CALL(EventData_Destroy(TEST_EVENTDATA_HANDLE_VALID));
    i++; // this function is not expected to fail and thus does not included in the bitmask

    EXPECTED_CALL(messaging_delivery_accepted());
    i++; // this function is not expected to fail and thus does not included in the bitmask

    // ensure that we do not have more that 64 mocked functions
    ASSERT_IS_FALSE((i > 64), "More Mocked Functions than permitted bitmask width");

    return failedFunctionBitmask;
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE_VALID));

    STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUBPATH_STRING_HANDLE_VALID));

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        STRICT_EXPECTED_CALL(STRING_delete(TEST_SHAREDACCESSKEYNAME_STRING_HANDLE_VALID));

        STRICT_EXPECTED_CALL(STRING_delete(TEST_SHAREDACCESSKEY_STRING_HANDLE_VALID));
    }

    STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGETADDRESS_STRING_HANDLE_VALID));

    STRICT_EXPECTED_CALL(STRING_delete(TEST_CONSUMERGROUP_STRING_HANDLE_VALID));

    STRICT_EXPECTED_CALL(STRING_delete(TEST_PARTITIONID_STRING_HANDLE_VALID));

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(gDynamicParsedConfig));
    }

    STRICT_EXPECTED_CALL(tickcounter_destroy(TEST_TICK_COUNTER_HANDLE_VALID));

    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_NoActiveReceiver(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    // arrange
    umock_c_reset_all_calls();

    TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(credential);
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_NoAuth(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    // arrange
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_delete(TEST_FILTER_QUERY_STRING_HANDLE_VALID));

    TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(credential);
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_NoAuth_WithRefreshToken(void)
{
    // arrange
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(STRING_delete(TEST_FILTER_QUERY_STRING_HANDLE_VALID));
    //msr
    STRICT_EXPECTED_CALL(STRING_delete(TEST_STRING_HANDLE_EXT_REFRESH_SASTOKEN_1)).IgnoreAllArguments();

    TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_AuthOk_PreFullStack(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t tmp = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();

    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_TearDown_Common(tmp, &i);

    TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(credential);
}

static void TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_PostAuth(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t tmp = 0;
    int i = 0;

    // arrange
    umock_c_reset_all_calls();

    (void)TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_TearDown_Common(tmp, &i);

    TestSetupCallStack_EventHubReceiver_LL_Destroy_Common(credential);
}

//#################################################################################################
// EventHubReceiver LL Tests
//#################################################################################################
BEGIN_TEST_SUITE(eventhubreceiver_ll_unittests)

TEST_SUITE_INITIALIZE(TestClassInitialize)
{
    g_testByTest = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(g_testByTest);

    umock_c_init(TestHook_OnUMockCError);

    REGISTER_UMOCK_ALIAS_TYPE(bool, unsigned char);
    REGISTER_UMOCK_ALIAS_TYPE(uint32_t, unsigned int);
    REGISTER_UMOCK_ALIAS_TYPE(uint64_t, unsigned long long);

    REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(TICK_COUNTER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MAP_RESULT, unsigned int);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_RECEIVER_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(LINK_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SESSION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(IO_INTERFACE_DESCRIPTION*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CONNECTION_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(XIO_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SASL_MECHANISM_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(const IO_INTERFACE_DESCRIPTION*, void*);
    REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(filter_set, void*);
    REGISTER_UMOCK_ALIAS_TYPE(annotations, void*);
    REGISTER_UMOCK_ALIAS_TYPE(SOURCE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(role, unsigned char);
    REGISTER_UMOCK_ALIAS_TYPE(receiver_settle_mode, unsigned char);
    REGISTER_UMOCK_ALIAS_TYPE(ON_NEW_ENDPOINT, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_ATTACHED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_CONNECTION_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_RECEIVER_STATE_CHANGED, void*);
    REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_RECEIVED, void*);
    
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBAUTH_CBS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBAUTH_RESULT, unsigned int);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBAUTH_STATUS, unsigned int);

    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBRECEIVER_LL_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTDATA_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBRECEIVER_ASYNC_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, void*);
    REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBRECEIVER_ASYNC_END_CALLBACK, void*);

    REGISTER_UMOCK_ALIAS_TYPE(SASL_MECHANISM_INTERFACE_DESCRIPTION*, void*);

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, TestHook_malloc);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(gballoc_malloc, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, TestHook_free);

    REGISTER_GLOBAL_MOCK_RETURN(EventData_CreateWithNewMemory, TEST_EVENTDATA_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventData_CreateWithNewMemory, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(EventData_Clone, TEST_EVENTDATA_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventData_Clone, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_GetVersionString, "Version Test");
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubClient_GetVersionString, NULL);

    REGISTER_GLOBAL_MOCK_HOOK(STRING_construct, TestHook_STRING_construct);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_construct, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_construct_n, TestHook_STRING_construct_n);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_construct_n, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_clone, TestHook_STRING_clone);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_clone, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_concat, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_concat, -1);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_concat_with_STRING, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_concat_with_STRING, -1);
    REGISTER_GLOBAL_MOCK_HOOK(STRING_c_str, TestHook_STRING_c_str);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_c_str, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(STRING_compare, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(STRING_compare, -1);

    REGISTER_GLOBAL_MOCK_RETURN(connectionstringparser_parse, TEST_MAP_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(connectionstringparser_parse, NULL);

    REGISTER_GLOBAL_MOCK_HOOK(Map_GetValueFromKey, TestHoook_Map_GetValueFromKey);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_GetValueFromKey, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(Map_Add, (MAP_RESULT)0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(Map_Add, (MAP_RESULT)1);

    REGISTER_GLOBAL_MOCK_RETURN(tickcounter_create, TEST_TICK_COUNTER_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(tickcounter_create, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(tickcounter_get_current_ms, TestHook_tickcounter_get_current_ms);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(tickcounter_get_current_ms, -1);

    REGISTER_GLOBAL_MOCK_RETURN(saslmssbcbs_get_interface, TEST_SASL_INTERFACE_HANDLE);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(saslmssbcbs_get_interface, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(saslmechanism_create, TEST_SASL_MECHANISM_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(saslmechanism_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(platform_get_default_tlsio, TEST_TLS_IO_INTERFACE_DESCRPTION_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(platform_get_default_tlsio, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(xio_create, TestHook_xio_create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(saslclientio_get_interface_description, TEST_SASL_CLIENT_IO_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(saslclientio_get_interface_description, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(connection_create, TEST_CONNECTION_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(connection_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(session_create, TEST_SESSION_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(session_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(session_set_incoming_window, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(session_set_incoming_window, -1);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_map, TEST_AMQP_VALUE_MAP_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_map, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_symbol, TEST_AMQP_VALUE_SYMBOL_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_symbol, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_string, TEST_AMQP_VALUE_FILTER_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_string, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_described, TEST_AMQP_VALUE_DESCRIBED_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_described, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_set_map_value, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_set_map_value, -1);
    REGISTER_GLOBAL_MOCK_RETURN(source_create, TEST_SOURCE_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(source_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(source_set_address, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(source_set_address, -1);
    REGISTER_GLOBAL_MOCK_RETURN(source_set_filter, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(source_set_filter, -1);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_source, TEST_AMQP_SOURCE_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_create_source, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(messaging_create_target, TEST_MESSAGING_TARGET_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messaging_create_target, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(link_create, TEST_LINK_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_create, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(link_set_rcv_settle_mode, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_set_rcv_settle_mode, -1);
    REGISTER_GLOBAL_MOCK_RETURN(link_set_max_message_size, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(link_set_max_message_size, -1);

    REGISTER_GLOBAL_MOCK_HOOK(messagereceiver_create, TestHook_messagereceiver_create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagereceiver_create, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(messagereceiver_open, TestHook_messagereceiver_open);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagereceiver_open, -1);
    REGISTER_GLOBAL_MOCK_RETURN(messagereceiver_close, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(messagereceiver_close, -1);

    REGISTER_GLOBAL_MOCK_RETURN(message_get_body_amqp_data_in_place, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_body_amqp_data_in_place, -1);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_message_annotations, TestHook_message_get_message_annotations);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_message_annotations, -1);
    REGISTER_GLOBAL_MOCK_HOOK(message_get_application_properties, TestHook_message_get_application_properties);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(message_get_application_properties, -1);
    REGISTER_GLOBAL_MOCK_HOOK(messaging_delivery_rejected, TestHook_messaging_delivery_rejected);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_inplace_descriptor, TEST_AMQP_INPLACE_DESCRIPTOR);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_inplace_descriptor, NULL);

    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_inplace_described_value, TEST_AMQP_INPLACE_DESCRIBED_DESCRIPTOR);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_inplace_described_value, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_map, TestHook_amqpvalue_get_map);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_map, -1);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_map_pair_count, TestHook_amqpvalue_get_map_pair_count);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_map_pair_count, -1);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_map_key_value_pair, TestHook_amqpvalue_get_map_key_value_pair);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_map_key_value_pair, -1);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_string, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_string, -1);
    REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_timestamp, 0);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_timestamp, -1);
    REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_get_symbol, TestHook_amqpvalue_get_symbol);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(amqpvalue_get_symbol, -1);

    REGISTER_GLOBAL_MOCK_RETURN(EventData_CreateWithNewMemory, TEST_EVENTDATA_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventData_CreateWithNewMemory, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(EventData_Properties, TEST_EVENT_DATA_PROPS_MAP_HANDLE_VALID);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventData_Properties, NULL);
    REGISTER_GLOBAL_MOCK_RETURN(EventData_SetEnqueuedTimestampUTCInMs, EVENTDATA_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventData_SetEnqueuedTimestampUTCInMs, EVENTDATA_INVALID_ARG);

    REGISTER_GLOBAL_MOCK_HOOK(connection_dowork, TestHook_connection_dowork);

    REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_Create, TestHook_EventHubAuthCBS_Create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubAuthCBS_Create, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_GetStatus, TestHook_EventHubAuthCBS_GetStatus);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubAuthCBS_GetStatus, EVENTHUBAUTH_RESULT_ERROR);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubAuthCBS_Authenticate, EVENTHUBAUTH_RESULT_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubAuthCBS_Authenticate, EVENTHUBAUTH_RESULT_ERROR);
    REGISTER_GLOBAL_MOCK_RETURN(EventHubAuthCBS_Refresh, EVENTHUBAUTH_RESULT_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubAuthCBS_Refresh, EVENTHUBAUTH_RESULT_ERROR);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_SASTokenParse, TestHook_EventHubAuthCBS_SASTokenParse);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(EventHubAuthCBS_SASTokenParse, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_Config_Destroy, TestHook_EventHubAuthCBS_Config_Destroy);
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
// EventHubReceiver_LL_Create Tests
//#################################################################################################
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_103: \[**EventHubReceiver_LL_Create shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Create_NULL_Param_ConnectionString)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // act
    h = EventHubReceiver_LL_Create(NULL, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_103: \[**EventHubReceiver_LL_Create shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Create_NULL_Param_EventHubPath)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // act
    h = EventHubReceiver_LL_Create(CONNECTION_STRING, NULL, CONSUMER_GROUP, PARTITION_ID);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_103: \[**EventHubReceiver_LL_Create shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Create_NULL_Param_ConsumerGroup)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // act
    h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, NULL, PARTITION_ID);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_103: \[**EventHubReceiver_LL_Create shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Create_NULL_Param_PartitionId)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // act
    h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, NULL);

    // assert
    ASSERT_IS_NULL(h);
}

TEST_FUNCTION(EventHubReceiver_LL_Create_Success)
{
    // arrange
    (void)TestSetupCallStack_Create();

    // act
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #1");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, EventHubReceiver_LL_Create shall return NULL and cleanup any allocated resources as needed.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_107: \[**EventHubReceiver_LL_Create shall expect a service bus connection string in one of the following formats:
//  Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
//  Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
//  **\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_108: \[**EventHubReceiver_LL_Create shall create a temp connection STRING_HANDLE using connectionString as the parameter.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_109: \[**EventHubReceiver_LL_Create shall parse the connection string handle to a map of strings by using API connection_string_parser_parse.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_110: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument eventHubPath.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_111: \[**EventHubReceiver_LL_Create shall lookup the endpoint in the resulting map using API Map_GetValueFromKey and argument "Endpoint".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_112: \[**EventHubReceiver_LL_Create shall obtain the host name after parsing characters after substring "sb://".**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_113: \[**EventHubReceiver_LL_Create shall create a host name STRING_HANDLE using API STRING_construct_n and the host name substring and its length obtained above as parameters.**\]**
static void EventHubReceiver_LL_Create_Fails_With_Invalid_HostName(const char* invalidHostName)
{
    // arrange
    umock_c_reset_all_calls();

    EXPECTED_CALL(EventHubClient_GetVersionString());

    // LL internal data structure allocation
    EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));

    EXPECTED_CALL(tickcounter_create());

    // create string handle for temp connection string handling
    STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING));

    // parse connection string
    STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNECTION_STRING_HANDLE_VALID));

    // create string handle for event hub path
    STRICT_EXPECTED_CALL(STRING_construct(EVENTHUB_PATH));

    // create string handle for (hostName) "Endpoint"
    STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_MAP_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2).SetReturn(invalidHostName);

    // destroy map as connection string processing is done
    STRICT_EXPECTED_CALL(Map_Destroy(TEST_MAP_HANDLE_VALID));

    // destroy temp connection string handle
    STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNECTION_STRING_HANDLE_VALID));

    // destroy event hub string handle
    STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUBPATH_STRING_HANDLE_VALID));

    // destroy event hub string handle
    STRICT_EXPECTED_CALL(tickcounter_destroy(TEST_TICK_COUNTER_HANDLE_VALID));

    // free LL internal data structure allocation
    EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

    // act
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #1");
    ASSERT_IS_NULL(h, "Failed Return Value Test #2");

    // cleanup
}

TEST_FUNCTION(EventHubReceiver_LL_Create_Fails_With_ZeroLength_HostName)
{
    EventHubReceiver_LL_Create_Fails_With_Invalid_HostName("");
}

TEST_FUNCTION(EventHubReceiver_LL_Create_Fails_With_Invalid_Prefix_HostName)
{
    EventHubReceiver_LL_Create_Fails_With_Invalid_HostName("sb");
}

TEST_FUNCTION(EventHubReceiver_LL_Create_Negative)
{
    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    uint64_t failedCallBitmask = TestSetupCallStack_Create();

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//#################################################################################################
// EventHubReceiver_LL_CreateFromSASToken Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_153: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL if parameter eventHubSasToken is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_CreateFromSASToken_NULL_Param_eventHubSasToken)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // act
    h = EventHubReceiver_LL_CreateFromSASToken(NULL);

    // assert
    ASSERT_IS_NULL(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_156: \[**EventHubReceiver_LL_CreateFromSASToken shall check if sasTokenData mode is EVENTHUBAUTH_MODE_RECEIVER and if not, any de allocations shall be done and NULL is returned.**\]**
TEST_FUNCTION(EventHubReceiver_LL_CreateFromSASToken_InvalidMode)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    gDynamicParsedConfig = (EVENTHUBAUTH_CBS_CONFIG*)TestHook_malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG));
    TestHelper_InitEventhHubAuthConfigReceiverExt(gDynamicParsedConfig);
    gDynamicParsedConfig->mode = EVENTHUBAUTH_MODE_SENDER;

    // arrange
    umock_c_reset_all_calls();

    EXPECTED_CALL(EventHubClient_GetVersionString());

    STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(SASTOKEN)).SetReturn(gDynamicParsedConfig);
    // note gDynamicParsedConfig will be freed in TestHook_EventHubAuthCBS_Config_Destroy
    STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(gDynamicParsedConfig));

    // act
    h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_CreateFromSASToken_Success)
{
    // arrange
    EVENTHUBRECEIVER_LL_HANDLE h;

    // arrange
    (void)TestSetupCallStack_CreateFromSASToken(SASTOKEN);

    // act
    h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_IS_NOT_NULL(h, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_CreateFromSASToken_Negative)
{
    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    uint64_t failedCallBitmask = TestSetupCallStack_CreateFromSASToken(SASTOKEN);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
            // assert
            ASSERT_IS_NULL(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//#################################################################################################
// EventHubReceiver_LL_Destroy Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_200: \[**EventHubReceiver_LL_Destroy return immediately if eventHubReceiverHandle is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_NULL_Param_eventHubReceiverLLHandle)
{
    // arrange
    umock_c_reset_all_calls();

    // act
    EventHubReceiver_LL_Destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_AutoSASToken_NoActiveReceiver)
{
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_NoActiveReceiver(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_ExtSASToken_NoActiveReceiver)
{
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_NoActiveReceiver(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_AutoSASToken_ActiveReceiver)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_NoAuth(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_ExtSASToken_ActiveReceiver)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_NoAuth(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_ExtSASToken_ActiveReceiver_WithRefreshToken)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    (void)EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_NoAuth_WithRefreshToken();

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_AutoSASToken_ActiveReceiver_AuthOk_PreFullStack)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_AuthOk_PreFullStack(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_ExtSASToken_ActiveReceiver_AuthOk_PreFullStack)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_AuthOk_PreFullStack(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_AutoSASToken_ActiveReceiver_PostAuth)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_DoWork(h);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_PostAuth(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
TEST_FUNCTION(EventHubReceiver_LL_Destroy_ExtSASToken_ActiveReceiver_PostAuth)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_DoWork(h);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_EventHubReceiver_LL_Destroy_ActiveReceiver_PostAuth(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

    // act
    EventHubReceiver_LL_Destroy(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
}

//#################################################################################################
// EventHubReceiver_LL_SetConnectionTracing Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_800: \[**EventHubReceiver_LL_SetConnectionTracing shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverLLHandle.**\]**
TEST_FUNCTION(EventHubReceiver_LL_SetConnectionTracing_NULL_Params)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_SetConnectionTracing(NULL, false);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_801: \[**EventHubReceiver_LL_SetConnectionTracing shall save the value of tracingOnOff in eventHubReceiverLLHandle**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_803: \[**Upon success, EventHubReceiver_LL_SetConnectionTracing shall return EVENTHUBRECEIVER_OK**\]**
static void EventHubReceiver_LL_SetConnectionTracing_NoActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // arrange, 1
    umock_c_reset_all_calls();

    // act, 1
    result = EventHubReceiver_LL_SetConnectionTracing(h, false);

    // assert, 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #1");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test #1");

    // arrange, 2
    umock_c_reset_all_calls();

    // act, 2
    result = EventHubReceiver_LL_SetConnectionTracing(h, true);

    // assert, 2
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #2");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test #2");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_SetConnectionTracing_NoActiveConnection_Success)
{
    EventHubReceiver_LL_SetConnectionTracing_NoActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_SetConnectionTracing_NoActiveConnection_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_SetConnectionTracing_NoActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_801: \[**EventHubReceiver_LL_SetConnectionTracing shall save the value of tracingOnOff in eventHubReceiverLLHandle**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_802: \[**If an active connection has been setup, EventHubReceiver_LL_SetConnectionTracing shall be called with the value of connection_set_trace tracingOnOff**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_803: \[**Upon success, EventHubReceiver_LL_SetConnectionTracing shall return EVENTHUBRECEIVER_OK**\]**
static void EventHubReceiver_LL_SetConnectionTracing_ActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_DoWork(h);

    // arrange, 1
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE_VALID, 0));

    // act, 1
    result = EventHubReceiver_LL_SetConnectionTracing(h, false);

    // assert, 1
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #1");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test #1");

    // arrange, 2
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE_VALID, 1));

    // act, 2
    result = EventHubReceiver_LL_SetConnectionTracing(h, true);

    // assert, 2
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #2");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test #2");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_SetConnectionTracing_ActiveConnection_Success)
{
    EventHubReceiver_LL_SetConnectionTracing_ActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_SetConnectionTracing_ActiveConnection_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_SetConnectionTracing_ActiveConnection_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//#################################################################################################
// EventHubReceiver_LL_ReceiveFromStartTimestampAsync Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_301: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverHandle, onEventReceiveErrorCallback, onEventReceiveErrorCallback are NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_NULL_Param_Handle)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(NULL, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, 0);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_301: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverHandle, onEventReceiveErrorCallback, onEventReceiveErrorCallback are NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_NULL_Param_onEventReceiveCallback)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(TEST_EVENTHUB_RECEIVER_LL_VALID, NULL, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, 0);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_301: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverHandle, onEventReceiveErrorCallback, onEventReceiveErrorCallback are NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_NULL_Param_onEventReceiveErrorCallback)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(TEST_EVENTHUB_RECEIVER_LL_VALID, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, NULL, OnErrCBCtxt, 0);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_302: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall record the callbacks and contexts in the EVENTHUBRECEIVER_LL_STRUCT.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_304: \[**Create a filter string using format "apache.org:selector-filter:string" and "amqp.annotation.x-opt-enqueuedtimeutc > startTimestampInSec" using STRING_sprintf**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_306: \[**Initialize timeout value (zero if no timeout) and a current timestamp of now.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_308: \[**Otherwise EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall succeed and return EVENTHUBRECEIVER_OK.**\]**
static void EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;
    EVENTHUBRECEIVER_RESULT result;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(0);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_302: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall record the callbacks and contexts in the EVENTHUBRECEIVER_LL_STRUCT.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_304: \[**Create a filter string using format "apache.org:selector-filter:string" and "amqp.annotation.x-opt-enqueuedtimeutc > startTimestampInSec" using STRING_sprintf**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_306: \[**Initialize timeout value (zero if no timeout) and a current timestamp of now.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_308: \[**Otherwise EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall succeed and return EVENTHUBRECEIVER_OK.**\]**
static void EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(0);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_WithEnd_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_WithEnd_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_307: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall return an error code of EVENTHUBRECEIVER_NOT_ALLOWED if a user called EventHubReceiver_LL_Receive* more than once on the same handle.**\]**
static void EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;
    EVENTHUBRECEIVER_RESULT result;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(0);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 0, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_Failure)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_UsingExtSASToken_Mulitple_Receive_Failure)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_303: \[**If tickcounter_get_current_ms fails, EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_ERROR.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_305: \[**If filter string create fails then a log message will be logged and an error code of EVENTHUBRECEIVER_ERROR shall be returned.**\]**
static void EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)credential;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange, 1
    TestSetupCallStack_ReceiveFromStartTimestampCommon(0);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        // act, 1
        result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);
        // assert, 1
        ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_ERROR, result, "Failed Return Value Test #1");
    }

    // arrange, 2
    TestHelper_SetNegativeTestSTRING_construct_sprintf();

    // act, 2
    result = EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // assert, 2
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_ERROR, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    umock_c_negative_tests_deinit();
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Negative)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampAsync_UsingExtSASToken_Negative)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//#################################################################################################
// EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync Tests
//#################################################################################################
TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_NULL_Param_Handle)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(NULL, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, 0, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_NULL_Param_onEventReceiveCallback)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(TEST_EVENTHUB_RECEIVER_LL_VALID, NULL, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, 0, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_NULL_Param_onEventReceiveErrorCallback)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(TEST_EVENTHUB_RECEIVER_LL_VALID, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, NULL, OnErrCBCtxt, 0, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

static void EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_WithEnd_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_UsingExtSASToken_Mulitple_Receive_WithEnd_Success)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_WithEnd_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // arrange
    TestSetupCallStack_ReceiveFromStartTimestampCommon(TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // act
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 0, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_Failure)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_UsingExtSASToken_Mulitple_Receive_Failure)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Mulitple_Receive_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange, 1
    TestSetupCallStack_ReceiveFromStartTimestampCommon(TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        // act, 1
        result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
        // assert, 1
        ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_ERROR, result, "Failed Return Value Test #1");
    }

    // arrange, 2
    TestHelper_SetNegativeTestSTRING_construct_sprintf();

    // act, 2
    result = EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);

    // assert, 2
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_ERROR, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 1, TestHelper_isSTRING_construct_sprintfInvoked(), "Failed STRING_consturct_sprintf Value Test");

    // cleanup
    umock_c_negative_tests_deinit();
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Negative)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_UsingExtSASToken_Negative)
{
    EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//#################################################################################################
// EventHubReceiver_LL_ReceiveEndAsync Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_900: \[**EventHubReceiver_LL_ReceiveEndAsync shall validate arguments, in case they are invalid, error code EVENTHUBRECEIVER_INVALID_ARG will be returned.**\]**
TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_NULL_Param_Handle)
{
    // arrange
    EVENTHUBRECEIVER_RESULT result;

    // act
    result = EventHubReceiver_LL_ReceiveEndAsync(NULL, NULL, NULL);

    // assert
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_902: \[**EventHubReceiver_LL_ReceiveEndAsync save off the user callback and context and defer the UAMQP stack tear down to EventHubReceiver_LL_DoWork.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_903: \[**Upon Success, EventHubReceiver_LL_ReceiveEndAsync shall return EVENTHUBRECEIVER_OK.**\]**
static void EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    TestSetupCallStack_ReceiveEndAsync();

    // act
    result = EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiver_Success)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiver_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_901: \[**EventHubReceiver_LL_ReceiveEndAsync shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned and a message will be logged.**\]**
static void EventHubReceiver_LL_ReceiveEndAsync_WithInActiveReceiver_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // arrange
    TestSetupCallStack_ReceiveEndAsync();

    // act
    result = EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithInActiveReceiver_Failure)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithInActiveReceiver_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithInActiveReceiver_UsingExtSASToken_Failure)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithInActiveReceiver_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_901: \[**EventHubReceiver_LL_ReceiveEndAsync shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned and a message will be logged.**\]**
static void EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiverAndAsyncEnd_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestSetupCallStack_ReceiveEndAsync();

    // act
    result = EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiverAndAsyncEnd_Failure)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiverAndAsyncEnd_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiverAndAsyncEnd_UsingExtSASToken_Failure)
{
    EventHubReceiver_LL_ReceiveEndAsync_WithActiveReceiverAndAsyncEnd_Failure_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//#################################################################################################
// EventHubReceiver_LL_DoWork Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_450: \[**`EventHubReceiver_LL_DoWork` shall return immediately if the supplied handle is NULL**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_NULL_Param)
{
    // arrange

    // act
    EventHubReceiver_LL_DoWork(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

static void EventHubReceiver_LL_DoWork_NoActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // arrange
    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_NoActiveReceiver_Success)
{
    EventHubReceiver_LL_DoWork_NoActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_NoActiveReceiver_UsingExtSASToken_Success)
{
    EventHubReceiver_LL_DoWork_NoActiveReceiver_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void TestHelper_ValidateAndAssertCBSAuthConfig(const EVENTHUBAUTH_CBS_CONFIG* cfg, unsigned int waitTimeoutInMs)
{
    ASSERT_IS_NOT_NULL(cfg, "Failed EventHubCBS Auth Config Is NULL");
    ASSERT_ARE_EQUAL(int, EVENTHUBAUTH_MODE_RECEIVER, cfg->mode, "Invalid EventHubCBS Auth Config Param Mode");
    ASSERT_IS_NULL(cfg->senderPublisherId, "Invalid EventHubCBS Auth Config Param Publisher ID");
    ASSERT_ARE_EQUAL(int, waitTimeoutInMs, cfg->sasTokenAuthFailureTimeoutInSecs * 1000, "Invalid EventHubCBS Auth Config Timeout Param");

    if (cfg->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        ASSERT_ARE_EQUAL(void_ptr, TEST_HOSTNAME_STRING_HANDLE_VALID, cfg->hostName, "Invalid EventHubCBS Auth Config Param Hostname");
        ASSERT_ARE_EQUAL(void_ptr, TEST_EVENTHUBPATH_STRING_HANDLE_VALID, cfg->eventHubPath, "Invalid EventHubCBS Auth Config Param Event Hub Path");
        ASSERT_ARE_EQUAL(void_ptr, TEST_SHAREDACCESSKEYNAME_STRING_HANDLE_VALID, cfg->sharedAccessKeyName, "Invalid EventHubCBS Auth Config Param Shared Access Key Name");
        ASSERT_ARE_EQUAL(void_ptr, TEST_SHAREDACCESSKEY_STRING_HANDLE_VALID, cfg->sharedAccessKey, "Invalid EventHubCBS Auth Config Param Shared Access Key");
        ASSERT_ARE_EQUAL(void_ptr, TEST_CONSUMERGROUP_STRING_HANDLE_VALID, cfg->receiverConsumerGroup, "Invalid EventHubCBS Auth Config Param Consumer Group");
        ASSERT_ARE_EQUAL(void_ptr, TEST_PARTITIONID_STRING_HANDLE_VALID, cfg->receiverPartitionId, "Invalid EventHubCBS Auth Config Param Partition ID");
        ASSERT_ARE_EQUAL(int, AUTH_REFRESH_SECS, cfg->sasTokenRefreshPeriodInSecs, "Invalid EventHubCBS Auth Config Refresh Period Param");
        ASSERT_ARE_EQUAL(int, AUTH_EXPIRATION_SECS, cfg->sasTokenExpirationTimeInSec, "Invalid EventHubCBS Auth Config Expiration Time Param");
        ASSERT_IS_NULL(cfg->extSASToken, "Invalid EventHubCBS Auth Config Param Shared Access Key");
        ASSERT_ARE_EQUAL(uint64_t, 0, cfg->extSASTokenExpTSInEpochSec, "Invalid EventHubCBS Auth Config Expiration Time Param");
    }
    else if (cfg->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        ASSERT_ARE_EQUAL(void_ptr, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_HOSTNAME, cfg->hostName, "Invalid EventHubCBS Auth Config Param Hostname");
        ASSERT_ARE_EQUAL(void_ptr, TEST_STRING_HANDLE_EXT_SASTOKEN_PARSER_EVENTHUBPATH, cfg->eventHubPath, "Invalid EventHubCBS Auth Config Param Event Hub Path");
        ASSERT_IS_NULL(cfg->sharedAccessKeyName, "Invalid EventHubCBS Auth Config Param Shared Access Key Name");
        ASSERT_IS_NULL(cfg->sharedAccessKey, "Invalid EventHubCBS Auth Config Param Shared Access Key");
        ASSERT_ARE_EQUAL(int, 0, cfg->sasTokenRefreshPeriodInSecs, "Invalid EventHubCBS Auth Config Refresh Period Param");
        ASSERT_ARE_EQUAL(int, 0, cfg->sasTokenExpirationTimeInSec, "Invalid EventHubCBS Auth Config Expiration Time Param");
        ASSERT_IS_NOT_NULL(cfg->extSASToken, "Invalid EventHubCBS Auth Config Param External SAS Token");
        ASSERT_ARE_EQUAL(uint64_t, SASTOKEN_EXT_EXPIRATION_TIMESTAMP, cfg->extSASTokenExpTSInEpochSec, "Invalid EventHubCBS Auth Config Expiration Time Param");
    }
    else
    {
        ASSERT_FAIL("Invalid EventHubCBS Auth Config Param Credential Value");
    }
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential, unsigned int waitTimeoutInMs)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    TestHelper_ResetTestGlobalData();
    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, waitTimeoutInMs);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusInProgress();

    EventHubReceiver_LL_DoWork(h);

    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    TestHelper_ValidateAndAssertCBSAuthConfig(TestHelper_GetCBSAuthConfig(), waitTimeoutInMs);

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_WithTimout_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_WithTimout_ExtSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_NoTimout_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, 0);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_NoTimout_ExtSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, 0);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusInProgress();
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();
        EVENTHUBRECEIVER_LL_HANDLE h;

        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
        }
        else
        {
            h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
        }
        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_Uninitialized_AuthInProgress_AMQPStackBringup_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusOk();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_AuthOk_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_AuthOk_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusOk();
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();
        EVENTHUBRECEIVER_LL_HANDLE h;

        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
        }
        else
        {
            h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
        }
        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_AuthOk_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_AuthOk_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Success(EVENTHUBAUTH_CREDENTIAL_TYPE credential, unsigned int timeoutMs)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

    // pre auth stack setup
    EventHubReceiver_LL_DoWork(h);

    // arrange
    (void)TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_Bringup(timeoutMs);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthOk_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Success(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, 0);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthOk_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Success(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, 0);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthOk_WithTimeout_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Success(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthOk_WithTimeout_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Success(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential, unsigned int timeoutMs)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_PostAuthComplete_AMQP_Stack_Bringup(timeoutMs);
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();
        EVENTHUBRECEIVER_LL_HANDLE h;

        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
        }
        else
        {
            h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
        }

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

        // pre auth stack setup
        EventHubReceiver_LL_DoWork(h);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthOk_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, 0);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthOk_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, 0);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthOk_WithTimeout_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthOk_WithTimeout_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthOk_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, TEST_EVENTHUB_RECEIVER_TIMEOUT_MS);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestHelper_ResetTestGlobalData();
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);

    STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE_VALID));

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_RefreshRequiredStatus_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // setup a ext refresh token
    (void)EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // arrange
    (void)TestSetupCallStack_DoWork_PostAuthComplete_StatusOk_ExtRefreshTokenApplied();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_RefreshRequiredStatus_Negative)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_PostAuthComplete_StatusOk_ExtRefreshTokenApplied();
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        // pre auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        // post auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        // setup a ext refresh token
        (void)EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            TestHelper_ResetTestGlobalData();
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential, EVENTHUBAUTH_STATUS status)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    (void)TestSetupCallStack_DoWork_PostAuth_AMQP_Stack_Teardown_Status_FailureOrExpiredOrTimeoutOrUnknown(status);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    if (status == EVENTHUBAUTH_STATUS_TIMEOUT)
    {
        ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Invoked Test");
    }
    else
    {
        ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Invoked Test");
    }

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_FailedStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EVENTHUBAUTH_STATUS_FAILURE);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_FailedStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EVENTHUBAUTH_STATUS_FAILURE);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_TimeoutStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EVENTHUBAUTH_STATUS_TIMEOUT);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_TimeoutStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EVENTHUBAUTH_STATUS_TIMEOUT);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_ExpiredStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EVENTHUBAUTH_STATUS_EXPIRED);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_ExpiredStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EVENTHUBAUTH_STATUS_EXPIRED);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_UnknownStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_UnknownStatus)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_StatusFailureOrExpiredOrTimeoutOrUnknown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

        EVENTHUBRECEIVER_LL_HANDLE h;

        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
        }
        else
        {
            h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
        }

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        // pre auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        // post auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        TestHelper_ResetTestGlobalData();

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (i == 0)
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_PostAuthComplete_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_PostAuthComplete_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_PostAuthComplete_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_AuthIdle_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_AuthIdle_Success)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_WithRefreshToken_AMQPStackBringup_AuthIdle_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    TestHelper_ResetTestGlobalData();
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    (void)EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle_WithRefreshToken();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_WithRefreshToken_AMQPStackBringup_AuthIdle_Negative)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    umock_c_reset_all_calls();

    uint64_t failedCallBitmask   = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle_WithRefreshToken();

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();

        uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

        EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        (void)EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusIdle();

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

        EVENTHUBRECEIVER_LL_HANDLE h;

        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
        }
        else
        {
            h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
        }

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        TestHelper_ResetTestGlobalData();
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_AuthIdle_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_AuthIdle_Negative)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_AuthIdle_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsRefreshRequired_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // pre auth stack setup
    EventHubReceiver_LL_DoWork(h);

    // auth stack setup
    EventHubReceiver_LL_DoWork(h);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusRefreshRequired();

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsRefreshRequired_Negative)
{
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_StatusRefreshRequired();

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
        EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        // pre auth stack setup
        EventHubReceiver_LL_DoWork(h);

        // auth stack setup
        EventHubReceiver_LL_DoWork(h);

        TestHelper_ResetTestGlobalData();
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);

        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
    TestHelper_ResetTestGlobalData();
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsFailure_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Status_FailureOrExpiredOrTimeoutOrUnknown(EVENTHUBAUTH_STATUS_FAILURE);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsFailure_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsFailure_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_GetAuthStatusReturnsFailure_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsFailure_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsUnknownCode_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Status_FailureOrExpiredOrTimeoutOrUnknown(TEST_UNKNOWN_EVENTHUBAUTH_STATUS_CODE);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsUnknownCode_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsUnknownCode_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_GetAuthStatusReturnsUnknownCode_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsUnknownCode_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsTimeout_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Status_FailureOrExpiredOrTimeoutOrUnknown(EVENTHUBAUTH_STATUS_TIMEOUT);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsTimeout_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsTimeout_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_GetAuthStatusReturnsTimeout_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsTimeout_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsExpired_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_DoWork_Uninit_UnAuth_AMQP_Stack_Bringup_Status_FailureOrExpiredOrTimeoutOrUnknown(EVENTHUBAUTH_STATUS_EXPIRED);

    // act
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_AutoSASToken_AMQPStackBringup_GetAuthStatusReturnsExpired_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsExpired_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiver_ExtSASToken_AMQPStackBringup_GetAuthStatusReturnsExpired_WithTeardown)
{
    EventHubReceiver_LL_DoWork_ActiveReceiver_SASToken_AMQPStackBringup_GetAuthStatusReturnsExpired_WithTeardown_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPreAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // setup a receiver
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // request stack tear down
    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // arrange
    (void)TestSetupCallStack_DoWork_PreAuth_ActiveTearDown();

    // act (perform actual tear down)
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxEndCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_AutoSASToken_ActiveReceiverPreAuthStackTeardown_Success)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPreAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ExtSASToken_ActiveReceiverPreAuthStackTeardown_Success)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPreAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPostCompleteAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // setup a receiver
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // request stack tear down
    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // arrange
    (void)TestSetupCallStack_DoWork_PostAuth_ActiveTearDown();

    // act (perform actual tear down)
    EventHubReceiver_LL_DoWork(h);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxEndCallbackResult, "Failed Receive Error Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_Auto_ActiveReceiverPostCompleteAuthStackTeardown_Success)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPostCompleteAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ActiveReceiverPostCompleteAuthStackTeardown_Success)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverPostCompleteAuthStackTeardown_Success_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

static void EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverStackTeardown_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE credential)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h;

    if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    }
    else
    {
        h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);
    }

    // setup a receiver
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // request stack tear down
    (void)EventHubReceiver_LL_ReceiveEndAsync(h, EventHubHReceiver_LL_OnRxEndCB, OnRxEndCBCtxt);

    // arrange
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    uint64_t failedCallBitmask = TestSetupCallStack_DoWork_PostAuth_ActiveTearDown();
    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            EventHubReceiver_LL_DoWork(h);
            // assert
            ASSERT_ARE_EQUAL(int, 0, TestHelper_isConnectionDoWorkInvoked());
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_AutoSASToken_ActiveReceiverStackTeardown_Negative)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverStackTeardown_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_ExtSASToken_ActiveReceiverStackTeardown_Negative)
{
    EventHubReceiver_LL_DoWork_SASToken_ActiveReceiverStackTeardown_Negative_Common(EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);
}

//#################################################################################################
// EventHubReceiver_LL_DoWork OnStateChanged Callback Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_630: \[**When `EHR_LL_OnStateChanged` is invoked, obtain the EventHubReceiverLL handle from the context and update the message receiver state with the new state received in the callback.**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_631: \[**If the new state is MESSAGE_RECEIVER_STATE_ERROR, and previous state is not MESSAGE_RECEIVER_STATE_ERROR, `EHR_LL_OnStateChanged` shall invoke the user supplied error callback along with error callback context`**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnStateChanged_Callback_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    umock_c_reset_all_calls();

    // assert
    ASSERT_IS_TRUE(OnRxCBStruct.onMsgChangedCallback != NULL);
    ASSERT_IS_TRUE(OnRxCBStruct.onMsgReceivedCallback != NULL);

    // MSG_RECEIVER idle to MSG_RECEIVER open transition no error callback expected
    OnRxCBStruct.onMsgChangedCallback(OnRxCBStruct.onMsgChangedCallbackCtxt, MESSAGE_RECEIVER_STATE_OPEN, MESSAGE_RECEIVER_STATE_IDLE);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #1");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test #1");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test #1");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test #1");

    // MSG_RECEIVER open to MSG_RECEIVER error transition error callback expected
    OnRxCBStruct.onMsgChangedCallback(OnRxCBStruct.onMsgChangedCallbackCtxt, MESSAGE_RECEIVER_STATE_ERROR, MESSAGE_RECEIVER_STATE_OPEN);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #2");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test #2");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test #2");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test #2");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_CONNECTION_RUNTIME_ERROR, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value #2");

    // reset error callback data
    OnRxCBStruct.rxErrorCallbackCalled = 0;
    OnRxCBStruct.rxErrorCallbackResult = EVENTHUBRECEIVER_OK;

    // MSG_RECEIVER error to MSG_RECEIVER error transition no error callback expected
    OnRxCBStruct.onMsgChangedCallback(OnRxCBStruct.onMsgChangedCallbackCtxt, MESSAGE_RECEIVER_STATE_ERROR, MESSAGE_RECEIVER_STATE_ERROR);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #3");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test #3");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test #3");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test #3");

    // MSG_RECEIVER error to MSG_RECEIVER open transition no error callback expected
    OnRxCBStruct.onMsgChangedCallback(OnRxCBStruct.onMsgChangedCallbackCtxt, MESSAGE_RECEIVER_STATE_OPEN, MESSAGE_RECEIVER_STATE_ERROR);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #4");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test #4");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test #4");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test #4");

    // MSG_RECEIVER open to MSG_RECEIVER error transition error callback expected
    OnRxCBStruct.onMsgChangedCallback(OnRxCBStruct.onMsgChangedCallbackCtxt, MESSAGE_RECEIVER_STATE_ERROR, MESSAGE_RECEIVER_STATE_OPEN);
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test #5");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test #5");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test #5");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test #5");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_CONNECTION_RUNTIME_ERROR, OnRxCBStruct.rxErrorCallbackResult, "Failed Receive Error Callback Result Value #5");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

//#################################################################################################
// EventHubReceiver_LL_DoWork OnMessageReceived Callback Tests
//#################################################################################################

TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnMessageReceived_Callback_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    (void)TestSetupCallStack_OnMessageReceived();

    // act
    OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxCallbackResult, "Failed Receive Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnMessageReceived_Callback_NullApplicationProperties_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestHelper_SetNullMessageApplicationProperties();
    (void)TestSetupCallStack_OnMessageReceived();

    // act
    OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxCallbackResult, "Failed Receive Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnMessageReceived_Callback_NullMessageAnnotation_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestHelper_SetNullMessageAnnotations();
    (void)TestSetupCallStack_OnMessageReceived();

    // act
    OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxCallbackResult, "Failed Receive Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnMessageReceived_Callback_NullMessageAnnotationAndAppProps_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestHelper_SetNullMessageAnnotations();
    TestHelper_SetNullMessageApplicationProperties();
    (void)TestSetupCallStack_OnMessageReceived();

    // act
    OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls(), "Failed CallStack Test");
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, OnRxCBStruct.rxCallbackResult, "Failed Receive Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
    TestHelper_ResetTestGlobalData();
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen `EHR_LL_OnMessageReceived` shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_OnMessageReceived_Callback_Negative)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    // arrange
    uint64_t failedCallBitmask = TestSetupCallStack_OnMessageReceived();

    umock_c_negative_tests_snapshot();

    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        TestHelper_ResetTestGlobalData();

        EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

        (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

        // perform pre auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        // perform post auth stack bring up
        EventHubReceiver_LL_DoWork(h);

        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            // act
            OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);

            // assert
            ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
            ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
            ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
            ASSERT_ARE_EQUAL(int, 1, messagingDeliveryRejectedCalled, "Failed Message Reject API Invoked Test");
        }

        EventHubReceiver_LL_Destroy(h);
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

//#################################################################################################
// EventHubReceiver_LL_DoWork Message Timeout Callback Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_662: \[**`EventHubReceiver_LL_DoWork` shall check if a message was received, if so, reset the last activity time to the current time i.e. now**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_663: \[**If a message was not received, check if the time now minus the last activity time is greater than or equal to the user specified timeout. If greater, the user registered callback is invoked along with the user supplied context with status code EVENTHUBRECEIVER_TIMEOUT. Last activity time shall be updated to the current time i.e. now.**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_Msg_Timeout_Callback_Success)
{
    unsigned int timeoutMs = TEST_EVENTHUB_RECEIVER_TIMEOUT_MS, done = 0;
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;    
    time_t nowTimestamp, prevTimestamp;
    double timespan;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    prevTimestamp = time(NULL);
    do
    {
        nowTimestamp = time(NULL);
        timespan = difftime(nowTimestamp, prevTimestamp);
        if (timespan * 1000 >= (double)timeoutMs)
        {
            done = 1;
        }
        // act
        EventHubReceiver_LL_DoWork(h);
    } while (!done);

    // assert
    // ensure callback was invoked
    ASSERT_ARE_EQUAL(int, 1, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_TIMEOUT, OnRxCBStruct.rxCallbackResult, "Failed Receive Callback Result Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_661: \[**`EventHubReceiver_LL_DoWork` shall manage timeouts as long as the user specified timeout value is non zero **\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_Msg_Timeout_Callback_ZeroTimeoutValue_Success)
{
    unsigned int timeoutMs = 0, done = 0;
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    time_t nowTimestamp, prevTimestamp;
    double timespan;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    prevTimestamp = time(NULL);
    do
    {
        nowTimestamp = time(NULL);
        timespan = difftime(nowTimestamp, prevTimestamp);
        if (timespan * 1000 >= (double)timeoutMs)
        {
            done = 1;
        }
        // act
        EventHubReceiver_LL_DoWork(h);
    } while (!done);

    // assert
    // ensure no callback was invoked
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
    ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_662: \[**`EventHubReceiver_LL_DoWork` shall check if a message was received, if so, reset the last activity time to the current time i.e. now**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_663: \[**If a message was not received, check if the time now minus the last activity time is greater than or equal to the user specified timeout. If greater, the user registered callback is invoked along with the user supplied context with status code EVENTHUBRECEIVER_TIMEOUT. Last activity time shall be updated to the current time i.e. now.**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_Msg_Timeout_Callback_WithMessageReceive_Success)
{
    unsigned int timeoutMs = TEST_EVENTHUB_RECEIVER_TIMEOUT_MS, done = 0;
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    time_t nowTimestamp, prevTimestamp;
    double timespan;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    TestHelper_ResetTestGlobalData();

    // No timeout CB registered thus if a timeout is ever called we will assert
    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxNoTimeoutCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

    // required here to set up onMsgReceivedCallback
    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    prevTimestamp = time(NULL);
    do
    {
        nowTimestamp = time(NULL);
        timespan = difftime(nowTimestamp, prevTimestamp);
        if (timespan * 1000 >= (double)timeoutMs)
        {
            done = 1;
        }
        // act
        OnRxCBStruct.onMsgReceivedCallback(OnRxCBStruct.onMsgReceivedCallbackCtxt, TEST_MESSAGE_HANDLE_VALID);
        EventHubReceiver_LL_DoWork(h);
    } while (!done);

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_662: \[**`EventHubReceiver_LL_DoWork` shall check if a message was received, if so, reset the last activity time to the current time i.e. now**\]**
//**Tests_SRS_EVENTHUBRECEIVER_LL_29_663: \[**If a message was not received, check if the time now minus the last activity time is greater than or equal to the user specified timeout. If greater, the user registered callback is invoked along with the user supplied context with status code EVENTHUBRECEIVER_TIMEOUT. Last activity time shall be updated to the current time i.e. now.**\]**
TEST_FUNCTION(EventHubReceiver_LL_DoWork_Msg_Timeout_Callback_Negative)
{
    unsigned int timeoutMs = TEST_EVENTHUB_RECEIVER_TIMEOUT_MS;
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS, timeoutMs);

    // perform pre auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // perform post auth stack bring up
    EventHubReceiver_LL_DoWork(h);

    // arrange
    TestHelper_ResetTestGlobalData();
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TEST_TICK_COUNTER_HANDLE_VALID, IGNORED_PTR_ARG)).IgnoreArgument(2);

    umock_c_negative_tests_snapshot();
    // act
    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        umock_c_negative_tests_reset();
        umock_c_negative_tests_fail_call(i);
        // act
        EventHubReceiver_LL_DoWork(h);
        // assert
        ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxCallbackCalled, "Failed Receive Callback Invoked Test");
        ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxErrorCallbackCalled, "Failed Receive Error Callback Invoked Test");
        ASSERT_ARE_EQUAL(int, 0, OnRxCBStruct.rxEndCallbackCalled, "Failed Receive Async End Callback Invoked Test");
    }

    // cleanup
    umock_c_negative_tests_deinit();
    EventHubReceiver_LL_Destroy(h);
}

//#################################################################################################
// EventHubReceiver_LL_RefreshSASTokenAsync Tests
//#################################################################################################

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_400: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubReceiverLLHandle or eventHubRefreshSasToken is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_NULLParam_eventHubReceiverLLHandle)
{
    EVENTHUBRECEIVER_RESULT result;

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(NULL, "Test String");

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result, "Failed Return Value Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_400: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubReceiverLLHandle or eventHubRefreshSasToken is NULL.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_NULLParam_eventHubSasToken)
{
    EVENTHUBRECEIVER_LL_HANDLE h = (EVENTHUBRECEIVER_LL_HANDLE)0x1000;
    EVENTHUBRECEIVER_RESULT result;

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_INVALID_ARG, result, "Failed Return Value Test");

    // cleanup
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_402: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_NOT_ALLOWED if the token type is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_AutoSASToken_Should_NotBePermitted)
{
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    EVENTHUBRECEIVER_RESULT result;

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}


//**Tests_SRS_EVENTHUBRECEIVER_LL_29_402: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_NOT_ALLOWED if the token type is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_AutoSASTokenActiveReceiver_Should_NotBePermitted)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_Create(CONNECTION_STRING, EVENTHUB_PATH, CONSUMER_GROUP, PARTITION_ID);
    EVENTHUBRECEIVER_RESULT result;

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_401: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_NoactiveReceiver_Success)
{
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    TestHelper_ResetTestGlobalData();

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

//**Tests_SRS_EVENTHUBRECEIVER_LL_29_403: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall check if any prior refresh ext SAS token was applied, if so EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_MultipleRefresh_ShouldFail)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // arrange
    umock_c_reset_all_calls();

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_NOT_ALLOWED, result, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_Success)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

    TestHelper_ResetTestGlobalData();

    (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

    // arrange
    (void)TestSetupCallStack_EventHubReceiver_LL_RefreshSASTokenAsync(SASTOKEN_REFRESH1);

    // act
    result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_OK, result, "Failed Return Value Test");

    // cleanup
    EventHubReceiver_LL_Destroy(h);
}

TEST_FUNCTION(EventHubReceiver_LL_RefreshSASTokenAsync_Negative)
{
    uint64_t nowTS = TEST_EVENTHUB_RECEIVER_UTC_TIMESTAMP;

    // arrange
    TestHelper_ResetTestGlobalData();
    int testResult = umock_c_negative_tests_init();
    ASSERT_ARE_EQUAL(int, 0, testResult);

    uint64_t failedCallBitmask = TestSetupCallStack_EventHubReceiver_LL_RefreshSASTokenAsync(SASTOKEN_REFRESH1);

    umock_c_negative_tests_snapshot();

    // act
    for (size_t i = 0; i < umock_c_negative_tests_call_count(); i++)
    {
        if (failedCallBitmask & ((uint64_t)1 << i))
        {
            EVENTHUBRECEIVER_RESULT result;
            EVENTHUBRECEIVER_LL_HANDLE h = EventHubReceiver_LL_CreateFromSASToken(SASTOKEN);

            (void)EventHubReceiver_LL_ReceiveFromStartTimestampAsync(h, EventHubHReceiver_LL_OnRxCB, OnRxCBCtxt, EventHubHReceiver_LL_OnErrCB, OnErrCBCtxt, nowTS);

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(i);
            // act
            result = EventHubReceiver_LL_RefreshSASTokenAsync(h, SASTOKEN_REFRESH1);

            // assert
            ASSERT_ARE_EQUAL(int, EVENTHUBRECEIVER_ERROR, result, "Failed Return Value Test");

            EventHubReceiver_LL_Destroy(h);
        }
    }

    // cleanup
    umock_c_negative_tests_deinit();
}

END_TEST_SUITE(eventhubreceiver_ll_unittests)
