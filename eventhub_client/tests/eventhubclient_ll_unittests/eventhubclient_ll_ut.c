// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#endif
#include <signal.h>

#include "testrunnerswitcher.h"
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umocktypes_bool.h"
#include "umocktypes_stdint.h"

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
#include "azure_uamqp_c/amqpvalue.h"
#include "azure_uamqp_c/async_operation.h"
#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/sasl_mechanism.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/amqp_definitions_data.h"
#include "azure_uamqp_c/amqp_definitions_application_properties.h"
#include "azure_c_shared_utility/connection_string_parser.h"
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/lock.h"
#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/xio.h"
#include "eventhubauth.h"
#include "version.h"
#include "eventdata.h"

#undef ENABLE_MOCKS

#include "eventhubclient_ll.h"
#include "../reals/real_doublylinkedlist.h"

TEST_DEFINE_ENUM_TYPE(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(EVENTHUBCLIENT_CONFIRMATION_RESULT, EVENTHUBCLIENT_CONFIRMATION_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTHUBCLIENT_CONFIRMATION_RESULT, EVENTHUBCLIENT_CONFIRMATION_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(EVENTHUBCLIENT_ERROR_RESULT, EVENTHUBCLIENT_ERROR_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTHUBCLIENT_ERROR_RESULT, EVENTHUBCLIENT_ERROR_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(EVENTHUBAUTH_RESULT, EVENTHUBAUTH_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTHUBAUTH_RESULT, EVENTHUBAUTH_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(EVENTDATA_RESULT, EVENTDATA_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(EVENTDATA_RESULT, EVENTDATA_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(MAP_RESULT, MAP_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(MAP_RESULT, MAP_RESULT_VALUES);

static TEST_MUTEX_HANDLE g_testByTest;
EVENTHUBCLIENT_CONFIRMATION_RESULT g_confirmationResult;
static tickcounter_ms_t g_tickcounter_value = (tickcounter_ms_t)1000;

#define TEST_CONNSTR_HANDLE         (STRING_HANDLE)0x46
#define DUMMY_STRING_HANDLE         (STRING_HANDLE)0x47
#define TEST_HOSTNAME_STRING_HANDLE (STRING_HANDLE)0x48
#define TEST_KEYNAME_STRING_HANDLE  (STRING_HANDLE)0x49
#define TEST_KEY_STRING_HANDLE      (STRING_HANDLE)0x4A
#define TEST_TARGET_STRING_HANDLE   (STRING_HANDLE)0x4B
#define TEST_EVENTHUB_STRING_HANDLE (STRING_HANDLE)0x4C
#define TEST_PUBLISHER_STRING_HANDLE (STRING_HANDLE)0x4D
#define TEST_CONNSTR_MAP_HANDLE     (MAP_HANDLE)0x50

static ASYNC_OPERATION_HANDLE test_async_operation = (ASYNC_OPERATION_HANDLE)0x51;

#define TEST_HOSTNAME_PARSER_STRING_HANDLE (STRING_HANDLE)0x60
#define TEST_EVENTHUB_PARSER_STRING_HANDLE (STRING_HANDLE)0x61
#define TEST_PUBLISHER_PARSER_STRING_HANDLE (STRING_HANDLE)0x62
#define TEST_SASTOKEN_PARSER_STRING_HANDLE (STRING_HANDLE)0x63
#define TEST_SASTOKEN_URI_PARSER_STRING_HANDLE (STRING_HANDLE)0x64
#define TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE (STRING_HANDLE)0x65
#define TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE (STRING_HANDLE)0x66


#define TEST_CLONED_EVENTDATA_HANDLE_1     (EVENTDATA_HANDLE)0x4240
#define TEST_CLONED_EVENTDATA_HANDLE_2     (EVENTDATA_HANDLE)0x4241

#define TEST_EVENTDATA_HANDLE     (EVENTDATA_HANDLE)0x4243
#define TEST_EVENTDATA_HANDLE_1   (EVENTDATA_HANDLE)0x4244
#define TEST_EVENTDATA_HANDLE_2   (EVENTDATA_HANDLE)0x4245

#define TEST_STRING_TOKENIZER_HANDLE (STRING_TOKENIZER_HANDLE)0x48
#define TEST_MAP_HANDLE (MAP_HANDLE)0x49
#define MICROSOFT_MESSAGE_FORMAT 0x80013700

#define TEST_EVENTHUBCBSAUTH_HANDLE_VALID (EVENTHUBAUTH_CBS_HANDLE)0x51

#define CONNECTION_STRING       "Endpoint=sb://servicebusName.servicebus.windows.net/;SharedAccessKeyName=RootManageSharedAccessKey;SharedAccessKey=icT5kKsJr/Dw7oZveq7OPsSxu5Trmr6aiWlgI5zIT/8="
#define TEST_EVENTHUB_PATH      "eventHubName"
#define TEST_PUBLISHER_ID       "sender"
#define TEST_ENDPOINT           "sb://servicebusName.servicebus.windows.net"
#define TEST_HOSTNAME           "servicebusName.servicebus.windows.net"
#define TEST_KEYNAME            "RootManageSharedAccessKey"
#define TEST_KEY                "icT5kKsJr/Dw7oZveq7OPsSxu5Trmr6aiWlgI5zIT/8="
#define TEST_SASTOKEN           "sas_token"
#define TEST_REFRESH_SASTOKEN   "refresh_sas_token"

#define SASTOKEN_EXT_EXPIRATION_TIMESTAMP               (uint64_t)1000

static ON_MESSAGE_SENDER_STATE_CHANGED saved_on_message_sender_state_changed;
static void* saved_message_sender_context;

static const AMQP_VALUE TEST_PROPERTY_1_KEY_AMQP_VALUE = (AMQP_VALUE)0x5001;
static const AMQP_VALUE TEST_PROPERTY_1_VALUE_AMQP_VALUE = (AMQP_VALUE)0x5002;
static const AMQP_VALUE TEST_PROPERTY_2_KEY_AMQP_VALUE = (AMQP_VALUE)0x5003;
static const AMQP_VALUE TEST_PROPERTY_2_VALUE_AMQP_VALUE = (AMQP_VALUE)0x5004;
static const AMQP_VALUE TEST_UAMQP_MAP = (AMQP_VALUE)0x5004;

static const AMQP_VALUE DUMMY_APPLICATION_PROPERTIES = (AMQP_VALUE)0x6001;
static const AMQP_VALUE TEST_APPLICATION_PROPERTIES_1 = (AMQP_VALUE)0x6002;
static const AMQP_VALUE TEST_APPLICATION_PROPERTIES_2 = (AMQP_VALUE)0x6003;
static const AMQP_VALUE DUMMY_DATA = (AMQP_VALUE)0x7001;
static const AMQP_VALUE TEST_DATA_1 = (AMQP_VALUE)0x7002;
static const AMQP_VALUE TEST_DATA_2 = (AMQP_VALUE)0x7003;
static const AMQP_VALUE PARTITION_MAP = (AMQP_VALUE)0x7004;
static const AMQP_VALUE PARTITION_NAME = (AMQP_VALUE)0x7005;
static const AMQP_VALUE PARTITION_STRING_VALUE = (AMQP_VALUE)0x7006;
static const AMQP_VALUE PARTITION_ANNOTATION = (AMQP_VALUE)0x7007;
static const AMQP_VALUE test_delivery_state = (AMQP_VALUE)0x8000;

static const char* const no_property_keys[] = { "test_property_key" };
static const char* const no_property_values[] = { "test_property_value" };
static const char* const* no_property_keys_ptr = no_property_keys;
static const char* const* no_property_values_ptr = no_property_values;
static size_t no_property_size = 0;

static unsigned char* g_expected_encoded_buffer[3];
static size_t g_expected_encoded_length[3];
static size_t g_expected_encoded_counter;

static const char* TEXT_MESSAGE = "Hello From EventHubClient Unit Tests";
static const char* TEST_CHAR = "TestChar";
static const char* PARTITION_KEY_VALUE = "PartitionKeyValue";
static const char* PARTITION_KEY_EMPTY_VALUE = "";
static const char* PARTITION_KEY_NAME = "x-opt-partition-key";
static const char* PROPERTY_NAME = "PropertyName";
static const char TEST_STRING_VALUE[] = "Property_String_Value_1";
static const char TEST_STRING_VALUE2[] = "Property_String_Value_2";

static const char* TEST_PROPERTY_KEY[] = {"Key1", "Key2"};
static const char* TEST_PROPERTY_VALUE[] = {"Value1", "Value2"};

static const int BUFFER_SIZE = 8;
static bool g_setProperty = false;
static bool g_includeProperties = false;
static DLIST_ENTRY* saved_pending_list;

static size_t g_currentEventClone_call;
static size_t g_whenShallEventClone_fail;

static EVENTHUBAUTH_STATUS g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_OK;

typedef struct CALLBACK_CONFIRM_INFO
{
    sig_atomic_t successCounter;
    sig_atomic_t totalCalls;
} CALLBACK_CONFIRM_INFO;

static ON_MESSAGE_SEND_COMPLETE saved_on_message_send_complete;
static void* saved_on_message_send_complete_context;

static const XIO_HANDLE DUMMY_IO_HANDLE = (XIO_HANDLE)0x4342;
static const XIO_HANDLE TEST_TLSIO_HANDLE = (XIO_HANDLE)0x4343;
static const XIO_HANDLE TEST_SASLCLIENTIO_HANDLE = (XIO_HANDLE)0x4344;
static const CONNECTION_HANDLE TEST_CONNECTION_HANDLE = (CONNECTION_HANDLE)0x4243;
static const SESSION_HANDLE TEST_SESSION_HANDLE = (SESSION_HANDLE)0x4244;
static const LINK_HANDLE TEST_LINK_HANDLE = (LINK_HANDLE)0x4245;
static const AMQP_VALUE TEST_SOURCE_AMQP_VALUE = (AMQP_VALUE)0x4246;
static const AMQP_VALUE TEST_TARGET_AMQP_VALUE = (AMQP_VALUE)0x4247;
static const MESSAGE_HANDLE TEST_MESSAGE_HANDLE = (MESSAGE_HANDLE)0x4248;
static const MESSAGE_SENDER_HANDLE TEST_MESSAGE_SENDER_HANDLE = (MESSAGE_SENDER_HANDLE)0x4249;
static const SASL_MECHANISM_HANDLE TEST_SASL_MECHANISM_HANDLE = (SASL_MECHANISM_HANDLE)0x4250;
static const IO_INTERFACE_DESCRIPTION* TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION = (const IO_INTERFACE_DESCRIPTION*)0x4251;
static const SASL_MECHANISM_INTERFACE_DESCRIPTION* TEST_SASL_INTERFACE_DESCRIPTION = (const SASL_MECHANISM_INTERFACE_DESCRIPTION*)0x4252;
static const IO_INTERFACE_DESCRIPTION* TEST_TLSIO_INTERFACE_DESCRIPTION = (const IO_INTERFACE_DESCRIPTION*)0x4253;
static const AMQP_VALUE TEST_MAP_AMQP_VALUE = (AMQP_VALUE)0x4254;
static const AMQP_VALUE TEST_STRING_AMQP_VALUE = (AMQP_VALUE)0x4255;
static TICK_COUNTER_HANDLE TICK_COUNT_HANDLE_TEST = (TICK_COUNTER_HANDLE)0x4256;

static TLSIO_CONFIG* saved_tlsio_parameters;
static SASLCLIENTIO_CONFIG* saved_saslclientio_parameters;
static EVENTHUBAUTH_CBS_CONFIG g_parsed_config;

static void setup_parse_sastoken(EVENTHUBAUTH_CBS_CONFIG* cfg)
{
    cfg->hostName = TEST_HOSTNAME_PARSER_STRING_HANDLE;
    cfg->eventHubPath = TEST_EVENTHUB_PARSER_STRING_HANDLE;
    cfg->sharedAccessKeyName = NULL;
    cfg->sharedAccessKey = NULL;
    cfg->sasTokenAuthFailureTimeoutInSecs = 0;
    cfg->sasTokenExpirationTimeInSec = 0;
    cfg->sasTokenRefreshPeriodInSecs = 0;
    cfg->extSASTokenExpTSInEpochSec = SASTOKEN_EXT_EXPIRATION_TIMESTAMP;
    cfg->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;
    cfg->mode = EVENTHUBAUTH_MODE_SENDER;
    cfg->extSASToken = TEST_SASTOKEN_PARSER_STRING_HANDLE;
    cfg->extSASTokenURI = TEST_SASTOKEN_URI_PARSER_STRING_HANDLE;
    cfg->receiverConsumerGroup = NULL;
    cfg->receiverPartitionId = NULL;
    cfg->senderPublisherId = TEST_PUBLISHER_PARSER_STRING_HANDLE;
}

#ifdef __cplusplus
extern "C" {
#endif

    static int hook_tickcounter_get_current_ms(TICK_COUNTER_HANDLE tick_counter, tickcounter_ms_t* current_ms)
    {
        (void)tick_counter;
        *current_ms = g_tickcounter_value;
        return 0;
    }

    static void hook_DList_InitializeListHead(PDLIST_ENTRY listHead)
    {
        saved_pending_list = listHead;
        real_DList_InitializeListHead(listHead);
    }

    static EVENTHUBAUTH_CBS_CONFIG* hook_EventHubAuthCBS_SASTokenParse(const char* sasToken)
    {
        (void)sasToken;
        setup_parse_sastoken(&g_parsed_config);
        return &g_parsed_config;
    }

    MOCK_FUNCTION_WITH_CODE(, void, sendAsyncConfirmationCallback, EVENTHUBCLIENT_CONFIRMATION_RESULT, result2, void*, userContextCallback)
    MOCK_FUNCTION_END()

    static XIO_HANDLE hook_xio_create(const IO_INTERFACE_DESCRIPTION* io_interface_description, const void* io_create_parameters)
    {
        XIO_HANDLE result;
        (void)io_interface_description;
        if (saved_tlsio_parameters == NULL)
        {
            // first call
            saved_tlsio_parameters = (TLSIO_CONFIG*)my_gballoc_malloc(sizeof(TLSIO_CONFIG));
            saved_tlsio_parameters->port = ((TLSIO_CONFIG*)io_create_parameters)->port;
            saved_tlsio_parameters->hostname = (char*)my_gballoc_malloc(strlen(((TLSIO_CONFIG*)io_create_parameters)->hostname) + 1);
            (void)strcpy((char*)saved_tlsio_parameters->hostname, ((TLSIO_CONFIG*)io_create_parameters)->hostname);
            result = TEST_TLSIO_HANDLE;
        }
        else
        {
            // second call
            saved_saslclientio_parameters = (SASLCLIENTIO_CONFIG*)my_gballoc_malloc(sizeof(SASLCLIENTIO_CONFIG));
            saved_saslclientio_parameters->sasl_mechanism = ((SASLCLIENTIO_CONFIG*)io_create_parameters)->sasl_mechanism;
            saved_saslclientio_parameters->underlying_io = ((SASLCLIENTIO_CONFIG*)io_create_parameters)->underlying_io;
            result = TEST_SASLCLIENTIO_HANDLE;
        }

        return result;
    }

    static MESSAGE_SENDER_HANDLE hook_messagesender_create(LINK_HANDLE link, ON_MESSAGE_SENDER_STATE_CHANGED on_message_sender_state_changed, void* context)
    {
        (void)link;
        saved_on_message_sender_state_changed = on_message_sender_state_changed;
        saved_message_sender_context = context;
        return TEST_MESSAGE_SENDER_HANDLE;
    }

    static ASYNC_OPERATION_HANDLE hook_messagesender_send_async(MESSAGE_SENDER_HANDLE message_sender, MESSAGE_HANDLE message, ON_MESSAGE_SEND_COMPLETE on_message_send_complete, void* callback_context, tickcounter_ms_t timeout)
    {
        (void)message;
        (void)message_sender;
        (void)timeout;
        saved_on_message_send_complete = on_message_send_complete;
        saved_on_message_send_complete_context = callback_context;
        return test_async_operation;
    }

    static EVENTHUBAUTH_RESULT hook_EventHubAuthCBS_GetStatus(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle, EVENTHUBAUTH_STATUS* returnStatus)
    {
        (void)eventHubAuthHandle;
        *returnStatus = g_eventhub_auth_get_status;
        return EVENTHUBAUTH_RESULT_OK;
    }

    MOCK_FUNCTION_WITH_CODE(, void, testHook_eventhub_error_callback, EVENTHUBCLIENT_ERROR_RESULT, eventhub_failure, void*, userContextCallback)
    MOCK_FUNCTION_END()

    static MAP_RESULT hook_Map_GetInternals(MAP_HANDLE handle, const char*const** keys, const char*const** values, size_t* count)
    {
        (void)handle;
        if (g_includeProperties)
        {
            *keys = TEST_PROPERTY_KEY;
            *values = TEST_PROPERTY_VALUE;
            *count = 2;
        }

        return MAP_OK;
    }

    static int hook_amqpvalue_encode(AMQP_VALUE value, AMQPVALUE_ENCODER_OUTPUT encoder_output, void* context)
    {
        (void)value;
        encoder_output(context, g_expected_encoded_buffer[g_expected_encoded_counter], g_expected_encoded_length[g_expected_encoded_counter]);
        g_expected_encoded_counter++;
        return 0;
    }

#ifdef __cplusplus
}
#endif

static int umocktypes_copy_BINARY_DATA(BINARY_DATA* destination, const BINARY_DATA* source)
{
    int result;

    destination->length = source->length;
    if (destination->length == 0)
    {
        destination->bytes = NULL;
        result = 0;
    }
    else
    {
        destination->bytes = (const unsigned char*)my_gballoc_malloc(destination->length);
        if (destination->bytes == NULL)
        {
            result = __LINE__;
        }
        else
        {
            (void)memcpy((void*)destination->bytes, source->bytes, destination->length);
            result = 0;
        }
    }

    return result;
}

static void umocktypes_free_BINARY_DATA(BINARY_DATA* value)
{
    my_gballoc_free((void*)value->bytes);
}

static char* umocktypes_stringify_BINARY_DATA(const BINARY_DATA* value)
{
    char* result = (char*)my_gballoc_malloc(3 + (5 * value->length));
    if (result != NULL)
    {
        size_t pos = 0;
        size_t i;

        result[pos++] = '[';
        for (i = 0; i < value->length; i++)
        {
            (void)sprintf(&result[pos], "0x%02X ", value->bytes[i]);
            pos += 5;
        }
        result[pos++] = ']';
        result[pos++] = '\0';
    }

    return result;
}

static int umocktypes_are_equal_BINARY_DATA(BINARY_DATA* left, BINARY_DATA* right)
{
    int result;

    if (left == right)
    {
        result = 1;
    }
    else
    {
        if (left->length != right->length)
        {
            result = 0;
        }
        else
        {
            if (left->length == 0)
            {
                result = 1;
            }
            else
            {
                result = (memcmp(left->bytes, right->bytes, left->length) == 0) ? 1 : 0;
            }
        }
    }

    return result;
}

static int umocktypes_copy_amqp_binary(amqp_binary* destination, const amqp_binary* source)
{
    int result;

    destination->length = source->length;
    if (destination->length == 0)
    {
        destination->bytes = NULL;
        result = 0;
    }
    else
    {
        destination->bytes = (const unsigned char*)my_gballoc_malloc(destination->length);
        if (destination->bytes == NULL)
        {
            result = __LINE__;
        }
        else
        {
            (void)memcpy((void*)destination->bytes, source->bytes, destination->length);
            result = 0;
        }
    }

    return result;
}

static void umocktypes_free_amqp_binary(amqp_binary* value)
{
    my_gballoc_free((void*)value->bytes);
}

static char* umocktypes_stringify_amqp_binary(const amqp_binary* value)
{
    char* result = (char*)my_gballoc_malloc(3 + (5 * value->length));
    if (result != NULL)
    {
        size_t pos = 0;
        size_t i;

        result[pos++] = '[';
        for (i = 0; i < value->length; i++)
        {
            (void)sprintf(&result[pos], "0x%02X ", ((uint8_t*)value->bytes)[i]);
            pos += 5;
        }
        result[pos++] = ']';
        result[pos++] = '\0';
    }

    return result;
}

static int umocktypes_are_equal_amqp_binary(amqp_binary* left, amqp_binary* right)
{
    int result;

    if (left == right)
    {
        result = 1;
    }
    else
    {
        if (left->length != right->length)
        {
            result = 0;
        }
        else
        {
            if (left->length == 0)
            {
                result = 1;
            }
            else
            {
                result = (memcmp(left->bytes, right->bytes, left->length) == 0) ? 1 : 0;
            }
        }
    }

    return result;
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

// ** End of Mocks **
BEGIN_TEST_SUITE(eventhubclient_ll_unittests)

    TEST_SUITE_INITIALIZE(TestClassInitialize)
    {
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        ASSERT_ARE_EQUAL(int, 0, umock_c_init(on_umock_c_error));
        ASSERT_ARE_EQUAL(int, 0, umocktypes_charptr_register_types());
        ASSERT_ARE_EQUAL(int, 0, umocktypes_bool_register_types());
        ASSERT_ARE_EQUAL(int, 0, umocktypes_stdint_register_types());

        REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
        REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);
        REGISTER_GLOBAL_MOCK_RETURN(EventHubClient_GetVersionString, "Version Test");
        REGISTER_GLOBAL_MOCK_RETURN(STRING_clone, DUMMY_STRING_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_construct, DUMMY_STRING_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_construct_n, DUMMY_STRING_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_concat, 0);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_concat_with_STRING, 0);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_c_str, TEST_CHAR);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_new, DUMMY_STRING_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(STRING_compare, 0);

        REGISTER_GLOBAL_MOCK_RETURN(connectionstringparser_parse, TEST_CONNSTR_MAP_HANDLE);

        REGISTER_GLOBAL_MOCK_RETURN(tickcounter_create, TICK_COUNT_HANDLE_TEST);
        REGISTER_GLOBAL_MOCK_HOOK(tickcounter_get_current_ms, hook_tickcounter_get_current_ms);

        REGISTER_GLOBAL_MOCK_RETURN(Map_GetValueFromKey, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(Map_GetInternals, hook_Map_GetInternals);

        REGISTER_GLOBAL_MOCK_FAIL_RETURN(gballoc_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_SASTokenParse, hook_EventHubAuthCBS_SASTokenParse);
        REGISTER_GLOBAL_MOCK_RETURN(EventHubAuthCBS_Create, TEST_EVENTHUBCBSAUTH_HANDLE_VALID);
        REGISTER_GLOBAL_MOCK_HOOK(EventHubAuthCBS_GetStatus, hook_EventHubAuthCBS_GetStatus);
        REGISTER_GLOBAL_MOCK_RETURN(EventHubAuthCBS_Authenticate, EVENTHUBAUTH_RESULT_OK);
        REGISTER_GLOBAL_MOCK_RETURN(EventHubAuthCBS_Refresh, EVENTHUBAUTH_RESULT_OK);

        REGISTER_GLOBAL_MOCK_RETURN(EventData_Clone, TEST_CLONED_EVENTDATA_HANDLE_1);
        REGISTER_GLOBAL_MOCK_RETURN(EventData_GetData, EVENTDATA_OK);
        REGISTER_GLOBAL_MOCK_RETURN(EventData_GetPartitionKey, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(EventData_Properties, TEST_MAP_HANDLE);

        REGISTER_DOUBLYLINKEDLIST_GLOBAL_MOCK_HOOKS();
        REGISTER_GLOBAL_MOCK_HOOK(DList_InitializeListHead, hook_DList_InitializeListHead);

        REGISTER_GLOBAL_MOCK_RETURN(saslmssbcbs_get_interface, TEST_SASL_INTERFACE_DESCRIPTION);

        REGISTER_GLOBAL_MOCK_RETURN(saslmechanism_create, TEST_SASL_MECHANISM_HANDLE);

        REGISTER_GLOBAL_MOCK_RETURN(platform_get_default_tlsio, TEST_TLSIO_INTERFACE_DESCRIPTION);

        REGISTER_GLOBAL_MOCK_RETURN(saslclientio_get_interface_description, TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION);

        REGISTER_GLOBAL_MOCK_HOOK(xio_create, hook_xio_create);

        REGISTER_GLOBAL_MOCK_RETURN(connection_create, TEST_CONNECTION_HANDLE);

        REGISTER_GLOBAL_MOCK_RETURN(session_create, TEST_SESSION_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(session_set_outgoing_window, 0);
        
        REGISTER_GLOBAL_MOCK_RETURN(link_create, TEST_LINK_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(link_set_snd_settle_mode, 0);
        REGISTER_GLOBAL_MOCK_RETURN(link_set_max_message_size, 0);

        REGISTER_GLOBAL_MOCK_RETURN(messaging_create_source, TEST_SOURCE_AMQP_VALUE);
        REGISTER_GLOBAL_MOCK_RETURN(messaging_create_target, TEST_TARGET_AMQP_VALUE);

        REGISTER_GLOBAL_MOCK_RETURN(message_create, TEST_MESSAGE_HANDLE);
        REGISTER_GLOBAL_MOCK_RETURN(message_set_application_properties, 0);
        REGISTER_GLOBAL_MOCK_RETURN(message_add_body_amqp_data, 0);
        REGISTER_GLOBAL_MOCK_RETURN(message_set_message_format, 0);

        REGISTER_GLOBAL_MOCK_HOOK(messagesender_create, hook_messagesender_create);
        REGISTER_GLOBAL_MOCK_RETURN(messagesender_open, 0);
        REGISTER_GLOBAL_MOCK_RETURN(messagesender_close, 0);
        REGISTER_GLOBAL_MOCK_HOOK(messagesender_send_async, hook_messagesender_send_async);

        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_map, TEST_MAP_AMQP_VALUE);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_symbol, TEST_STRING_AMQP_VALUE);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_string, TEST_STRING_AMQP_VALUE);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_message_annotations, TEST_STRING_AMQP_VALUE);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_set_map_value, 0);
        REGISTER_GLOBAL_MOCK_HOOK(amqpvalue_encode, hook_amqpvalue_encode);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_get_encoded_size, 0);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_application_properties, DUMMY_APPLICATION_PROPERTIES);
        REGISTER_GLOBAL_MOCK_RETURN(amqpvalue_create_data, DUMMY_DATA);
        REGISTER_GLOBAL_MOCK_RETURN(message_set_message_annotations, 0);

        REGISTER_UMOCK_ALIAS_TYPE(STRING_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(PDLIST_ENTRY, void*);
        REGISTER_UMOCK_ALIAS_TYPE(MAP_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(TICK_COUNTER_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(EVENTDATA_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(XIO_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_NEW_ENDPOINT, void*);
        REGISTER_UMOCK_ALIAS_TYPE(CONNECTION_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_LINK_ATTACHED, void*);
        REGISTER_UMOCK_ALIAS_TYPE(SESSION_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(EVENTHUBAUTH_CBS_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(AMQP_VALUE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(LINK_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(role, bool);
        REGISTER_UMOCK_ALIAS_TYPE(sender_settle_mode, uint8_t);
        REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_SENDER_STATE_CHANGED, void*);
        REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_SENDER_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(SASL_MECHANISM_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(MESSAGE_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_MESSAGE_SEND_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(tickcounter_ms_t, uint64_t);
        REGISTER_UMOCK_ALIAS_TYPE(ASYNC_OPERATION_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(annotations, AMQP_VALUE);
        REGISTER_UMOCK_ALIAS_TYPE(message_annotations, annotations);
        REGISTER_UMOCK_ALIAS_TYPE(AMQPVALUE_ENCODER_OUTPUT, void*);

        REGISTER_TYPE(EVENTHUBCLIENT_CONFIRMATION_RESULT, EVENTHUBCLIENT_CONFIRMATION_RESULT);
        REGISTER_TYPE(EVENTHUBCLIENT_ERROR_RESULT, EVENTHUBCLIENT_ERROR_RESULT);
        REGISTER_TYPE(EVENTHUBAUTH_RESULT, EVENTHUBAUTH_RESULT);
        REGISTER_TYPE(BINARY_DATA, BINARY_DATA);
        REGISTER_TYPE(amqp_binary, amqp_binary);
        REGISTER_TYPE(EVENTDATA_RESULT, EVENTDATA_RESULT);
        REGISTER_TYPE(MAP_RESULT, MAP_RESULT);

        REGISTER_UMOCK_ALIAS_TYPE(data, amqp_binary);
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

        saved_tlsio_parameters = NULL;
        saved_saslclientio_parameters = NULL;

        g_setProperty = false;

        g_currentEventClone_call = 0;
        g_whenShallEventClone_fail = 0;
        g_confirmationResult = EVENTHUBCLIENT_CONFIRMATION_ERROR;
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_OK;
        g_includeProperties = false;
    }

    TEST_FUNCTION_CLEANUP(TestMethodCleanup)
    {
        if (saved_tlsio_parameters != NULL)
        {
            free((char*)saved_tlsio_parameters->hostname);
            free(saved_tlsio_parameters);
        }

        if (saved_saslclientio_parameters != NULL)
        {
            free(saved_saslclientio_parameters);
        }

        saved_tlsio_parameters = NULL;
        saved_saslclientio_parameters = NULL;

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

    static void setup_createfromconnectionstring_success(void)
    {
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_construct(TEST_EVENTHUB_PATH))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));
    }

    static void setup_messenger_pre_auth_uamqp_stack_bringup_success(void)
    {
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL));
        STRICT_EXPECTED_CALL(session_set_outgoing_window(TEST_SESSION_HANDLE, 10));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Create(IGNORED_PTR_ARG, TEST_SESSION_HANDLE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_074: \[**The session shall be destroyed by calling session_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_075: \[**The connection shall be destroyed by calling connection_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_076: \[**The SASL IO shall be destroyed by calling xio_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_077: \[**The TLS IO shall be destroyed by calling xio_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_078: \[**The SASL mechanism shall be destroyed by calling saslmechanism_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_151: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_152: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    static void setup_messenger_pre_auth_uamqp_stack_teardown_success(void)
    {
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Destroy(TEST_EVENTHUBCBSAUTH_HANDLE_VALID));
        STRICT_EXPECTED_CALL(session_destroy(TEST_SESSION_HANDLE));
        STRICT_EXPECTED_CALL(connection_destroy(TEST_CONNECTION_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_SASLCLIENTIO_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));
    }

    static void setup_messenger_post_auth_complete_messenger_stack_bringup_success(void)
    {
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"));
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024));
        STRICT_EXPECTED_CALL(messagesender_create(TEST_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_open(TEST_MESSAGE_SENDER_HANDLE));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_072: \[**The message sender shall be destroyed by calling messagesender_destroy.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_073: \[**The link shall be destroyed by calling link_destroy.**\]**
    static void setup_messenger_post_auth_complete_messenger_stack_teardown_success(void)
    {
        STRICT_EXPECTED_CALL(messagesender_destroy(TEST_MESSAGE_SENDER_HANDLE));
        STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE));
    }

    static void setup_messenger_initialize_success(void)
    {
        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        setup_messenger_post_auth_complete_messenger_stack_bringup_success();
    }

    static void setup_createfromsastoken_success(void)
    {
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/publishers/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_PUBLISHER_ID));
    }

    /*** EventHubClient_LL_CreateFromConnectionString ***/

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_016: [EventHubClient_LL_CreateFromConnectionString shall return a non-NULL handle value upon success.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_002: [EventHubClient_LL_CreateFromConnectionString shall allocate a new event hub client LL instance.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_05_001: [EventHubClient_LL_CreateFromConnectionString shall obtain the version string by a call to EventHubClient_GetVersionString.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_05_002: [EventHubClient_LL_CreateFromConnectionString shall print the version string to standard output.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_017: [EventHubClient_LL expects a service bus connection string in one of the following formats:
    Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
    Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
    ] ]*/
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_065: [The connection string shall be parsed to a map of strings by using connection_string_parser_parse.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_067: [The endpoint shall be looked up in the resulting map and used to construct the host name to be used for connecting by removing the sb://.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_068: [The key name and key shall be looked up in the resulting map and they should be stored as is for later use in connecting.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_04_016: [EventHubClient_LL_CreateFromConnectionString shall initialize the pending list that will be used to send Events.] */
    TEST_FUNCTION(EventHubClient_LL_CreateFromConnectionString_with_valid_args_yields_a_valid_eventhub_client)
    {
        // arrange
        setup_createfromconnectionstring_success();

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);

        // assert
        ASSERT_IS_NOT_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_002: [EventHubClient_LL_CreateFromConnectionString shall allocate a new event hub client LL instance.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_05_001: [EventHubClient_LL_CreateFromConnectionString shall obtain the version string by a call to EventHubClient_GetVersionString.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_05_002: [EventHubClient_LL_CreateFromConnectionString shall print the version string to standard output.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_017: [EventHubClient_LL expects a service bus connection string in one of the following formats:
    Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
    Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
    ] ]*/
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_065: [The connection string shall be parsed to a map of strings by using connection_string_parser_parse.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_067: [The endpoint shall be looked up in the resulting map and used to construct the host name to be used for connecting by removing the sb://.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_068: [The key name and key shall be looked up in the resulting map and they should be stored as is for later use in connecting.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_016: [EventHubClient_LL_CreateFromConnectionString shall return a non-NULL handle value upon success.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_04_016: [EventHubClient_LL_CreateFromConnectionString shall initialize the pending list that will be used to send Events.] */
    TEST_FUNCTION(EventHubClient_LL_CreateFromConnectionString_with_valid_args_second_set_of_args_yields_a_valid_eventhub_client)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn("sb://Host2");
        STRICT_EXPECTED_CALL(STRING_construct_n("Host2", 5))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn("Mwahaha");
        STRICT_EXPECTED_CALL(STRING_construct("Mwahaha"))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn("Secret");
        STRICT_EXPECTED_CALL(STRING_construct("Secret"))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "AnotherOne"));
        STRICT_EXPECTED_CALL(STRING_construct("AnotherOne"))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, "AnotherOne");

        // assert
        ASSERT_IS_NOT_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_003: [EventHubClient_LL_CreateFromConnectionString shall return a NULL value if connectionString or eventHubPath is NULL.] */
    TEST_FUNCTION(EventHubClient_LL_CreateFromConnectionString_with_NULL_connectionString_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(NULL, TEST_EVENTHUB_PATH);

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_003: [EventHubClient_LL_CreateFromConnectionString shall return a NULL value if connectionString or eventHubPath is NULL.] */
    TEST_FUNCTION(EventHubClient_LL_CreateFromConnectionString_with_NULL_path_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString("connectionString", NULL);

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_066: [If connection_string_parser_parse fails then EventHubClient_LL_CreateFromConnectionString shall fail and return NULL.] */
    TEST_FUNCTION(when_creating_the_string_for_the_connection_string_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(NULL);

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_066: [If connection_string_parser_parse fails then EventHubClient_LL_CreateFromConnectionString shall fail and return NULL.] */
    TEST_FUNCTION(when_connection_string_parser_parse_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_allocating_memory_for_the_event_hub_client_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_creating_a_tickcounter_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_getting_the_endpoint_from_the_map_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_constructing_the_hostname_string_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));


        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_getting_the_keyname_from_the_map_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_constructing_the_keyname_string_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_getting_the_key_from_the_map_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_constructing_the_key_string_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_constructing_the_target_address_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_concatenating_the_hostname_in_the_target_address_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_concatenating_the_slash_in_the_target_address_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
    TEST_FUNCTION(when_concatenating_the_event_hub_path_in_the_target_address_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    TEST_FUNCTION(when_constructing_the_event_hub_path_fails_then_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT);
        STRICT_EXPECTED_CALL(STRING_construct_n(TEST_HOSTNAME, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_construct(TEST_EVENTHUB_PATH))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_the_endpoint_has_only_sb_slash_slash_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn("sb://");
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_the_endpoint_does_not_start_with_sb_slash_slash_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn("sb:/5test");
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_the_endpoint_ends_in_a_slash_the_slash_is_stripped)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT "/");
        STRICT_EXPECTED_CALL(STRING_construct_n(IGNORED_PTR_ARG, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn(TEST_KEY);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEY))
            .SetReturn(TEST_KEY_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_construct(TEST_EVENTHUB_PATH))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);

        // assert
        ASSERT_IS_NOT_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_the_key_is_empty_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT "/");
        STRICT_EXPECTED_CALL(STRING_construct_n(IGNORED_PTR_ARG, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn(TEST_KEYNAME);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_KEYNAME))
            .SetReturn(TEST_KEYNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKey"))
            .SetReturn("");
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);

        // assert
        ASSERT_IS_NULL(result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
    TEST_FUNCTION(when_the_keyname_is_empty_EventHubClient_LL_CreateFromConnectionString_fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(STRING_construct(CONNECTION_STRING))
            .SetReturn(TEST_CONNSTR_HANDLE);
        STRICT_EXPECTED_CALL(connectionstringparser_parse(TEST_CONNSTR_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "Endpoint"))
            .SetReturn(TEST_ENDPOINT "/");
        STRICT_EXPECTED_CALL(STRING_construct_n(IGNORED_PTR_ARG, strlen(TEST_HOSTNAME)))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(Map_GetValueFromKey(TEST_CONNSTR_MAP_HANDLE, "SharedAccessKeyName"))
            .SetReturn("");
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(Map_Destroy(TEST_CONNSTR_MAP_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_CONNSTR_HANDLE));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);

        // assert
        ASSERT_IS_NULL(result);
    }

    /*** EventHubClient_LL_CreateFromSASToken ***/

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_300: \[**EventHubClient_LL_CreateFromSASToken shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_301: \[**EventHubClient_LL_CreateFromSASToken shall print the version string to standard output.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_302: \[**EventHubClient_LL_CreateFromSASToken shall return NULL if eventHubSasToken is NULL.**\]**
    TEST_FUNCTION(EventHubClient_LL_CreateFromSASToken_with_NULL_arg_eventHubSasToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(NULL);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_300: \[**EventHubClient_LL_CreateFromSASToken shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_301: \[**EventHubClient_LL_CreateFromSASToken shall print the version string to standard output.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_302: \[**EventHubClient_LL_CreateFromSASToken shall return NULL if eventHubSasToken is NULL.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_303: \[**EventHubClient_LL_CreateFromSASToken parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_306: \[**EventHubClient_LL_CreateFromSASToken shall allocate a new event hub client LL instance.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_308: \[**EventHubClient_LL_CreateFromSASToken shall create a tick counter handle using API tickcounter_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_309: \[**EventHubClient_LL_CreateFromSASToken shall clone the hostName string using API STRING_Clone.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_310: \[**EventHubClient_LL_CreateFromSASToken shall clone the senderPublisherId and eventHubPath strings using API STRING_Clone.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_311: \[**EventHubClient_LL_CreateFromSASToken shall initialize sender target address using the eventHub and senderPublisherId with format amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_312: \[**EventHubClient_LL_CreateFromSASToken shall initialize connection tracing to false by default.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_313: \[**EventHubClient_LL_CreateFromSASToken shall return the allocated event hub client LL instance on success.**\]**
    TEST_FUNCTION(EventHubClient_LL_CreateFromSASToken_Success)
    {
        // arrange
        setup_createfromsastoken_success();

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NOT_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(result);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_304: \[**EventHubClient_LL_CreateFromSASToken shall fail if EventHubAuthCBS_SASTokenParse return NULL.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_SASTokenParse_returns_NULL_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN))
            .SetReturn(NULL);

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_305: \[**EventHubClient_LL_CreateFromSASToken shall check if sasTokenData mode is EVENTHUBAUTH_MODE_SENDER, if not, NULL is returned.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_SASTokenParse_returns_a_receiver_type_token_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_parse_sastoken(&cfg);
        cfg.mode = EVENTHUBAUTH_MODE_RECEIVER;

        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN))
            .SetReturn(&cfg);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&cfg));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_305: \[**EventHubClient_LL_CreateFromSASToken shall check if sasTokenData mode is EVENTHUBAUTH_MODE_SENDER, if not, NULL is returned.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_SASTokenParse_returns_a_unknown_type_token_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        EVENTHUBAUTH_CBS_CONFIG cfg;

        setup_parse_sastoken(&cfg);
        cfg.mode = EVENTHUBAUTH_MODE_UNKNOWN;

        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN))
            .SetReturn(&cfg);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&cfg));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_allocating_memory_using_malloc_returns_NULL_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_tickcounter_create_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_publisher_c_string_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_eventhubpath_c_string_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_publisherid_clone_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_eventhubpath_clone_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_hostname_clone_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_string_construct_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_concat_with_hostname_handle_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_concat_with_slash_string_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_concat_with_eventhubpath_handle_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_concat_with_publisher_string_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/publishers/"))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
    TEST_FUNCTION(when_targetaddress_concat_with_publisherid_handle_fails_EventHubClient_LL_CreateFromSASToken_Fails)
    {
        // arrange
        STRICT_EXPECTED_CALL(EventHubClient_GetVersionString());
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_SASTOKEN));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(DList_InitializeListHead(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(tickcounter_create());
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_ID);
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_PUBLISHER_PARSER_STRING_HANDLE))
            .SetReturn(TEST_PUBLISHER_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_EVENTHUB_PARSER_STRING_HANDLE))
            .SetReturn(TEST_EVENTHUB_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_clone(TEST_HOSTNAME_PARSER_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_construct("amqps://"))
            .SetReturn(TEST_TARGET_STRING_HANDLE);
        STRICT_EXPECTED_CALL(STRING_concat_with_STRING(TEST_TARGET_STRING_HANDLE, TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_EVENTHUB_PATH));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, "/publishers/"));
        STRICT_EXPECTED_CALL(STRING_concat(TEST_TARGET_STRING_HANDLE, TEST_PUBLISHER_ID))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&g_parsed_config));

        // act
        EVENTHUBCLIENT_LL_HANDLE result = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);

        // assert
        ASSERT_IS_NULL(result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    /* EventHubClient_LL_SendAsync */

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_011: [EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_INVALID_ARG if parameter eventHubClientLLHandle or eventDataHandle is NULL.] */
    TEST_FUNCTION(EventHubClient_LL_SendAsync_with_NULL_eventHubLLHandle_fails)
    {
        // arrange

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(NULL, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_011: [EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_INVALID_ARG if parameter eventHubClientLLHandle or eventDataHandle is NULL.] */
    TEST_FUNCTION(EventHubClient_LL_SendAsync_with_NULL_eventDataHandle_fails)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, NULL, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_012: [EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_INVALID_ARG if parameter sendAsyncConfirmationCallback is NULL and userContextCallBack is not NULL.] */
    TEST_FUNCTION(EventHubClient_LL_SendAsync_with_NULL_sendAsyncConfirmationCallbackandNonNullUSerContext_fails)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, NULL, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_013: [EventHubClient_LL_SendAsync shall add to the pending events list a new record cloning the information from eventDataHandle, sendAsyncConfirmationCallback and userContextCallBack.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_04_015: [Otherwise EventHubClient_LL_SendAsync shall succeed and return EVENTHUBCLIENT_OK.] */
    TEST_FUNCTION(EventHubClient_LL_SendAsync_succeeds)
    {
        ///arrange
        EVENTDATA_HANDLE dataEventHandle = (EVENTDATA_HANDLE)1;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(dataEventHandle));
        STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_PTR_ARG, IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_014: [If cloning and/or adding the information fails for any reason, EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
    TEST_FUNCTION(when_allocating_memory_for_the_pending_entry_fails_then_EventHubClient_LL_SendAsync_fails)
    {
        ///arrange
        EVENTDATA_HANDLE dataEventHandle = (EVENTDATA_HANDLE)1;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_014: [If cloning and/or adding the information fails for any reason, EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
    TEST_FUNCTION(when_allocating_memory_for_the_event_data_list_of_the_entry_fails_then_EventHubClient_LL_SendAsync_fails)
    {
        ///arrange
        EVENTDATA_HANDLE dataEventHandle = (EVENTDATA_HANDLE)1;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_014: [If cloning and/or adding the information fails for any reason, EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
    TEST_FUNCTION(when_cloning_the_payload_fails_then_EventHubClient_LL_SendAsync_fails)
    {
        ///arrange
        EVENTDATA_HANDLE dataEventHandle = (EVENTDATA_HANDLE)1;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(dataEventHandle))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* EventHubClient_LL_Destroy */

    //**Tests_SRS_EVENTHUBCLIENT_LL_03_010: \[**If the eventHubLLHandle is NULL, EventHubClient_Destroy shall not do anything.**\]**
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_NULL_eventHubHandle_Does_Nothing)
    {
        // arrange

        // act
        EventHubClient_LL_Destroy(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_valid_handle_auto_sastoken_no_auth_frees_all_the_resources)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));


        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_valid_handle_ext_sastoken_no_auth_frees_all_the_resources)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_valid_handle_auto_sastoken_post_auth_complete_frees_all_the_resources)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();

        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_valid_handle_ext_sastoken_post_auth_complete_frees_all_the_resources)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();

        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    void setup_refresh_sastoken_success(EVENTHUBAUTH_CBS_CONFIG* cfg)
    {
        setup_parse_sastoken(cfg);
        cfg->extSASTokenURI = TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE;
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_REFRESH_SASTOKEN))
            .SetReturn(cfg);
        STRICT_EXPECTED_CALL(STRING_compare(TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE, TEST_SASTOKEN_URI_PARSER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_construct(TEST_REFRESH_SASTOKEN)).SetReturn(TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(cfg));
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    TEST_FUNCTION(EventHubClient_LL_Destroy_with_valid_handle_ext_sastoken_post_auth_complete_with_refresh_token_frees_all_the_resources)
    {
        // arrange
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        setup_refresh_sastoken_success(&cfg);
        (void)EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);
        umock_c_reset_all_calls();

        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        STRICT_EXPECTED_CALL(STRING_delete(TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    static void EventHubClient_LL_Destroy_frees_2_pending_messages_common(EVENTHUBCLIENT_LL_HANDLE eventHubHandle, EVENTHUBAUTH_CREDENTIAL_TYPE credential)
    {
        EVENTDATA_HANDLE dataEventHandle = (EVENTDATA_HANDLE)1;
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, dataEventHandle, sendAsyncConfirmationCallback, (void*)0x4243);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
            STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        } else if (credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
        {
            STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        }
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));

        /* 1st item */
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_DESTROY, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        /* 2nd item */
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_DESTROY, (void*)0x4243));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_040: [All the pending messages shall be indicated as error by calling the associated callback with EVENTHUBCLIENT_CONFIRMATION_DESTROY.] */
    TEST_FUNCTION(EventHubClient_LL_Destroy_auto_sastoken_frees_2_pending_messages)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        EventHubClient_LL_Destroy_frees_2_pending_messages_common(eventHubHandle, EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO);

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_040: [All the pending messages shall be indicated as error by calling the associated callback with EVENTHUBCLIENT_CONFIRMATION_DESTROY.] */
    TEST_FUNCTION(EventHubClient_LL_Destroy_ext_sastoken_frees_2_pending_messages)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        EventHubClient_LL_Destroy_frees_2_pending_messages_common(eventHubHandle, EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT);

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_060: [When on_messagesender_state_changed is called with MESSAGE_SENDER_STATE_ERROR, the uAMQP stack shall be brough down so that it can be created again if needed in dowork:] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    TEST_FUNCTION(when_destroy_is_Called_after_the_messenger_and_uamqp_stack_has_been_brought_down_then_it_is_not_brought_down_again_using_auto_sas_token)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_ERROR, MESSAGE_SENDER_STATE_OPEN);
        EventHubClient_LL_DoWork(eventHubHandle); //bring down the stack
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEYNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_KEY_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_060: [When on_messagesender_state_changed is called with MESSAGE_SENDER_STATE_ERROR, the uAMQP stack shall be brough down so that it can be created again if needed in dowork:] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
    TEST_FUNCTION(when_destroy_is_called_after_the_messenger_and_uamqp_stack_has_been_brought_down_then_it_is_not_brought_down_again_using_auto_ext_sas_token)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_ERROR, MESSAGE_SENDER_STATE_OPEN);
        EventHubClient_LL_DoWork(eventHubHandle); //bring down the stack
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_delete(TEST_TARGET_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_HOSTNAME_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_EVENTHUB_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_PUBLISHER_STRING_HANDLE));
        STRICT_EXPECTED_CALL(tickcounter_destroy(TICK_COUNT_HANDLE_TEST));
        STRICT_EXPECTED_CALL(DList_RemoveHeadList(saved_pending_list));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EventHubClient_LL_Destroy(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    /* EventHubClient_LL_RefreshSASTokenAsync */

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_401: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubClientLLHandle or eventHubSasToken is NULL.**\]**
    TEST_FUNCTION(With_Null_eventHubClientLLHandle_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        EVENTHUBCLIENT_RESULT result;

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(NULL, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_INVALID_ARG);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_402: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if eventHubClientLLHandle credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
    TEST_FUNCTION(For_Handles_Not_Created_Using_EventHubClient_LL_CreateFromSASToken_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        EVENTHUBCLIENT_RESULT result;

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_403: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if AMQP stack is not fully initialized.**\]**
    TEST_FUNCTION(When_Inactive_AMQP_Stack_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        EVENTHUBCLIENT_RESULT result;

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_404: \[**EventHubClient_LL_RefreshSASTokenAsync shall check if any prior refresh ext SAS token was applied, if so EVENTHUBCLIENT_ERROR shall be returned.**\]**
    TEST_FUNCTION(When_Multiple_Back_To_Back_Refresh_Tokens_Are_Applied_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        const char* new_refresh_token = "blah";

        EVENTHUBCLIENT_RESULT result;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);
        umock_c_reset_all_calls();

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, new_refresh_token);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_405: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_406: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
    TEST_FUNCTION(When_EventHubAuthCBS_SASTokenParse_Fails_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        EVENTHUBCLIENT_RESULT result;
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();

        setup_parse_sastoken(&cfg);
        cfg.extSASTokenURI = TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE;
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_REFRESH_SASTOKEN))
            .SetReturn(NULL);

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_407: \[**EventHubClient_LL_RefreshSASTokenAsync shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubClient_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_408: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if eventHubRefreshSasToken is not compatible.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_412: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
    TEST_FUNCTION(When_EventHubAuthCBS_String_Compare_Fails_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        const char* new_refresh_token = "blah";

        EVENTHUBCLIENT_RESULT result;
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();

        setup_parse_sastoken(&cfg);
        cfg.extSASTokenURI = TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE;
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(new_refresh_token))
            .SetReturn(&cfg);
        STRICT_EXPECTED_CALL(STRING_compare(TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE, TEST_SASTOKEN_URI_PARSER_STRING_HANDLE))
            .SetReturn(1);
        //STRICT_EXPECTED_CALL(STRING_construct(TEST_REFRESH_SASTOKEN)).SetReturn(TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&cfg));

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, new_refresh_token);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_409: \[**EventHubClient_LL_RefreshSASTokenAsync shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in EventHubClient_LL_DoWork.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_411: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR on failure.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_412: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
    TEST_FUNCTION(When_EventHubAuthCBS_String_construct_Fails_EventHubClient_LL_RefreshSASTokenAsync_Fails)
    {
        // arrange
        EVENTHUBCLIENT_RESULT result;
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();

        setup_parse_sastoken(&cfg);
        cfg.extSASTokenURI = TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE;
        STRICT_EXPECTED_CALL(EventHubAuthCBS_SASTokenParse(TEST_REFRESH_SASTOKEN))
            .SetReturn(&cfg);
        STRICT_EXPECTED_CALL(STRING_compare(TEST_SASTOKEN_URI_PARSER_REFRESH_STRING_HANDLE, TEST_SASTOKEN_URI_PARSER_STRING_HANDLE))
            .SetReturn(0);
        STRICT_EXPECTED_CALL(STRING_construct(TEST_REFRESH_SASTOKEN)).SetReturn(NULL);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Config_Destroy(&cfg));

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_ERROR);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_405: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_407: \[**EventHubClient_LL_RefreshSASTokenAsync shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubClient_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_409: \[**EventHubClient_LL_RefreshSASTokenAsync shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in EventHubClient_LL_DoWork.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_410: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_OK on success.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_412: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
    TEST_FUNCTION(EventHubClient_LL_RefreshSASTokenAsync_Success)
    {
        // arrange
        EVENTHUBCLIENT_RESULT result;
        EVENTHUBAUTH_CBS_CONFIG cfg;

        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth do work
        EventHubClient_LL_DoWork(eventHubHandle); // post auth do work
        umock_c_reset_all_calls();
        setup_refresh_sastoken_success(&cfg);

        // act
        result = EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, result, EVENTHUBCLIENT_OK);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* EventHubClient_LL_DoWork */

    /* Tests_SRS_EVENTHUBCLIENT_LL_04_018: [if parameter eventHubClientLLHandle is NULL EventHubClient_LL_DoWork shall immediately return.]  */
    TEST_FUNCTION(EventHubClient_LL_DoWork_with_NullHandle_Do_Not_Work)
    {
        // arrange

        // act
        EventHubClient_LL_DoWork(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_079: \[**EventHubClient_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already been brought up**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_006: \[**If saslmssbcbs_get_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_03_030: \[**A TLS IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_002: \[**The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_012: \[**A SASL client IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_013: \[**The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_014: \[**If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_015: \[**The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_016: \[**The underlying_io members shall be set to the previously created TLS IO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_017: \[**The sasl_mechanism shall be set to the previously created SASL mechanism.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_019: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_028: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_030: \[**The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_110: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a EVENTHUBAUTH_CBS_CONFIG structure params hostName, eventHubPath, sharedAccessKeyName, sharedAccessKey using the values set previously. Set senderPublisherId to "sender". Set receiverConsumerGroup, receiverPartitionId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_SENDER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_115: \[**EventHubClient_LL_DoWork shall invoke connection_set_trace using the current value of the trace on boolean.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    TEST_FUNCTION(EventHubClient_LL_DoWork_Auto_SASToken_PreAuth_Stack_Bringup_Success)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, 5671, saved_tlsio_parameters->port);
        ASSERT_ARE_EQUAL(void_ptr, TEST_SASL_MECHANISM_HANDLE, saved_saslclientio_parameters->sasl_mechanism);
        ASSERT_ARE_EQUAL(void_ptr, TEST_TLSIO_HANDLE, saved_saslclientio_parameters->underlying_io);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_079: \[**EventHubClient_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already been brought up**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_006: \[**If saslmssbcbs_get_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_03_030: \[**A TLS IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_002: \[**The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_012: \[**A SASL client IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_013: \[**The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_014: \[**If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_015: \[**The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_016: \[**The underlying_io members shall be set to the previously created TLS IO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_017: \[**The sasl_mechanism shall be set to the previously created SASL mechanism.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_019: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_028: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_030: \[**The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_111: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the EVENTHUBAUTH_CBS_CONFIG obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_115: \[**EventHubClient_LL_DoWork shall invoke connection_set_trace using the current value of the trace on boolean.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    TEST_FUNCTION(EventHubClient_LL_DoWork_Ext_SASToken_PreAuth_Stack_Bringup_Success)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(int, 5671, saved_tlsio_parameters->port);
        ASSERT_ARE_EQUAL(void_ptr, TEST_SASL_MECHANISM_HANDLE, saved_saslclientio_parameters->sasl_mechanism);
        ASSERT_ARE_EQUAL(void_ptr, TEST_TLSIO_HANDLE, saved_saslclientio_parameters->underlying_io);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_079: \[**EventHubClient_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already been brought up**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_03_030: \[**A TLS IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_002: \[**The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_012: \[**A SASL client IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_013: \[**The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_014: \[**If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_015: \[**The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_016: \[**The underlying_io members shall be set to the previously created TLS IO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_017: \[**The sasl_mechanism shall be set to the previously created SASL mechanism.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_019: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_028: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_030: \[**The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_110: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a EVENTHUBAUTH_CBS_CONFIG structure params hostName, eventHubPath, sharedAccessKeyName, sharedAccessKey using the values set previously. Set senderPublisherId to "sender". Set receiverConsumerGroup, receiverPartitionId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_SENDER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_115: \[**EventHubClient_LL_DoWork shall invoke connection_set_trace using the current value of the trace on boolean.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_021: \[**A source AMQP value shall be created by calling messaging_create_source.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_022: \[**The source address shall be "ingress".**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_023: \[**A target AMQP value shall be created by calling messaging_create_target.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_024: \[**The target address shall be amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_025: \[**If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_026: \[**An AMQP link shall be created by calling link_create and passing as arguments the session handle, "sender-link" as link name, role_sender and the previously created source and target values.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_027: \[**If creating the link fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_032: \[**The link sender settle mode shall be set to unsettled by calling link_set_snd_settle_mode.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_033: \[**If link_set_snd_settle_mode fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_034: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_036: \[**A message sender shall be created by calling messagesender_create and passing as arguments the link handle, a state changed callback, a context and NULL for the logging function.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_038: \[**EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.**\]**
    TEST_FUNCTION(EventHubClient_LL_DoWork_Ext_SASToken_PostAuthComplete_Stack_Bringup_Success)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);

        umock_c_reset_all_calls();

        setup_messenger_initialize_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, 5671, saved_tlsio_parameters->port);
        ASSERT_ARE_EQUAL(void_ptr, TEST_SASL_MECHANISM_HANDLE, saved_saslclientio_parameters->sasl_mechanism);
        ASSERT_ARE_EQUAL(void_ptr, TEST_TLSIO_HANDLE, saved_saslclientio_parameters->underlying_io);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_201: \[**`EventHubClient_LL_DoWork` shall perform SAS token handling. **\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_202: \[**`EventHubClient_LL_DoWork` shall initialize the uAMQP Message Sender stack if it has not already brought up. **\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_079: \[**EventHubClient_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already been brought up**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_03_030: \[**A TLS IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_002: \[**The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_012: \[**A SASL client IO shall be created by calling xio_create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_013: \[**The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_014: \[**If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_015: \[**The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_016: \[**The underlying_io members shall be set to the previously created TLS IO.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_017: \[**The sasl_mechanism shall be set to the previously created SASL mechanism.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_019: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_028: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_030: \[**The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_111: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the EVENTHUBAUTH_CBS_CONFIG obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_115: \[**EventHubClient_LL_DoWork shall invoke connection_set_trace using the current value of the trace on boolean.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_021: \[**A source AMQP value shall be created by calling messaging_create_source.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_022: \[**The source address shall be "ingress".**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_023: \[**A target AMQP value shall be created by calling messaging_create_target.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_024: \[**The target address shall be amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_025: \[**If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_026: \[**An AMQP link shall be created by calling link_create and passing as arguments the session handle, "sender-link" as link name, role_sender and the previously created source and target values.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_027: \[**If creating the link fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_032: \[**The link sender settle mode shall be set to unsettled by calling link_set_snd_settle_mode.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_033: \[**If link_set_snd_settle_mode fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_034: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_036: \[**A message sender shall be created by calling messagesender_create and passing as arguments the link handle, a state changed callback, a context and NULL for the logging function.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_01_038: \[**EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.**\]**
    TEST_FUNCTION(EventHubClient_LL_DoWork_Auto_SASToken_PostAuthComplete_Stack_Bringup_Success)
    {
        // arrange
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        umock_c_reset_all_calls();

        setup_messenger_initialize_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        ASSERT_ARE_EQUAL(int, 5671, saved_tlsio_parameters->port);
        ASSERT_ARE_EQUAL(void_ptr, TEST_SASL_MECHANISM_HANDLE, saved_saslclientio_parameters->sasl_mechanism);
        ASSERT_ARE_EQUAL(void_ptr, TEST_TLSIO_HANDLE, saved_saslclientio_parameters->underlying_io);

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_201: \[**`EventHubClient_LL_DoWork` shall perform SAS token handling. **\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_202: \[**`EventHubClient_LL_DoWork` shall initialize the uAMQP Message Sender stack if it has not already brought up. **\]**
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_038: [EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_064: [EventHubClient_LL_DoWork shall call connection_dowork while passing as argument the connection handle obtained in EventHubClient_LL_Create.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_079: [EventHubClient_LL_DoWork shall bring up the uAMQP stack if it has not already brought up:] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_03_030: [A TLS IO shall be created by calling xio_create.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_002: [The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_004: [A SASL mechanism shall be created by calling saslmechanism_create.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_005: [The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.] */
    /* Codes_SRS_EVENTHUBCLIENT_LL_01_006: [If saslmssbcbs_get_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_012: [A SASL client IO shall be created by calling xio_create.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_013: [The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_015: [The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_016: [The underlying_io members shall be set to the previously created TLS IO.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_017: [The sasl_mechanism shall be set to the previously created SASL mechanism.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_019: [An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_028: [An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_030: [The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_021: [A source AMQP value shall be created by calling messaging_create_source.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_022: [The source address shall be "ingress".] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_023: [A target AMQP value shall be created by calling messaging_create_target.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_024: [The target address shall be amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_026: [An AMQP link shall be created by calling link_create and passing as arguments the session handle, "sender-link" as link name, role_sender and the previously created source and target values.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_032: [The link sender settle mode shall be set to unsettled by calling link_set_snd_settle_mode.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_034: [The message size shall be set to 256K by calling link_set_max_message_size.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_036: [A message sender shall be created by calling messagesender_create and passing as arguments the link handle, a state changed callback, a context and NULL for the logging function.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_079: [EventHubClient_LL_DoWork shall bring up the uAMQP stack if it has not already brought up:] */
    TEST_FUNCTION(EventHubClient_LL_DoWork_when_message_sender_is_not_open_opens_the_message_sender_different_args)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn("Host2");
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, "Host2", "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL));
        STRICT_EXPECTED_CALL(session_set_outgoing_window(TEST_SESSION_HANDLE, 10));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Create(IGNORED_PTR_ARG, TEST_SESSION_HANDLE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" "Host2" "/" "AnotherOne");
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"));
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" "Host2" "/" "AnotherOne"));
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024));
        STRICT_EXPECTED_CALL(messagesender_create(TEST_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_open(TEST_MESSAGE_SENDER_HANDLE));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle); //pre auth
        EventHubClient_LL_DoWork(eventHubHandle); //post auth

        // assert
        //ASSERT_ARE_EQUAL(char_ptr, "Host2", saved_tlsio_parameters->hostname);
        ASSERT_ARE_EQUAL(int, 5671, saved_tlsio_parameters->port);
        ASSERT_ARE_EQUAL(void_ptr, TEST_SASL_MECHANISM_HANDLE, saved_saslclientio_parameters->sasl_mechanism);
        ASSERT_ARE_EQUAL(void_ptr, TEST_TLSIO_HANDLE, saved_saslclientio_parameters->underlying_io);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_080: [If any other error happens while bringing up the uAMQP stack, EventHubClient_LL_DoWork shall not attempt to open the message_sender and return without sending any messages.] */
    TEST_FUNCTION(when_getting_the_hostname_raw_string_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(NULL);

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_080: [If any other error happens while bringing up the uAMQP stack, EventHubClient_LL_DoWork shall not attempt to open the message_sender and return without sending any messages.] */
    TEST_FUNCTION(when_getting_the_sasl_interface_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface())
            .SetReturn(NULL);

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_011: [If sasl_mechanism_create fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_SASL_mechanism_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG))
            .SetReturn(NULL);

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_001: [If platform_get_default_tlsio_interface fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages. ] */
    TEST_FUNCTION(when_getting_the_default_TLS_IO_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_003: [If xio_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_TLS_IO_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_014: [If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_getting_the_SASL_client_IO_interface_description_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG))
            .SetReturn(TEST_TLSIO_HANDLE);
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_018: [If xio_create fails creating the SASL client IO then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_saslclientio_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_020: [If connection_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_connection_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(xio_destroy(TEST_SASLCLIENTIO_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_029: [If session_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_sesion_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(connection_destroy(TEST_CONNECTION_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_SASLCLIENTIO_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_031: [If setting the outgoing window fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_setting_the_outgoing_window_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL));
        STRICT_EXPECTED_CALL(session_set_outgoing_window(TEST_SESSION_HANDLE, 10))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(session_destroy(TEST_SESSION_HANDLE));
        STRICT_EXPECTED_CALL(connection_destroy(TEST_CONNECTION_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_SASLCLIENTIO_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_114: \[**If EventHubAuthCBS_Create returns NULL, a log message will be logged and the function returns immediately.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_Create_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL));
        STRICT_EXPECTED_CALL(session_set_outgoing_window(TEST_SESSION_HANDLE, 10))
            .SetReturn(0);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Create(IGNORED_PTR_ARG, TEST_SESSION_HANDLE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(session_destroy(TEST_SESSION_HANDLE));
        STRICT_EXPECTED_CALL(connection_destroy(TEST_CONNECTION_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_SASLCLIENTIO_HANDLE));
        STRICT_EXPECTED_CALL(xio_destroy(TEST_TLSIO_HANDLE));
        STRICT_EXPECTED_CALL(saslmechanism_destroy(TEST_SASL_MECHANISM_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(STRING_c_str(TEST_HOSTNAME_STRING_HANDLE))
            .SetReturn(TEST_HOSTNAME);
        STRICT_EXPECTED_CALL(saslmssbcbs_get_interface());
        STRICT_EXPECTED_CALL(saslmechanism_create(TEST_SASL_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(platform_get_default_tlsio());
        STRICT_EXPECTED_CALL(xio_create(TEST_TLSIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(saslclientio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SASLCLIENTIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_create(TEST_SASLCLIENTIO_HANDLE, TEST_HOSTNAME, "eh_client_connection", NULL, NULL));
        STRICT_EXPECTED_CALL(connection_set_trace(TEST_CONNECTION_HANDLE, false));
        STRICT_EXPECTED_CALL(session_create(TEST_CONNECTION_HANDLE, NULL, NULL));
        STRICT_EXPECTED_CALL(session_set_outgoing_window(TEST_SESSION_HANDLE, 10))
            .SetReturn(0);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Create(IGNORED_PTR_ARG, TEST_SESSION_HANDLE))
            .SetReturn(TEST_EVENTHUBCBSAUTH_HANDLE_VALID);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG))
            .SetReturn(EVENTHUBAUTH_RESULT_ERROR);
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_Failure_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_error_callback_and_destroys_uamqp_stack)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_FAILURE;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_Expired_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_error_callback_and_destroys_uamqp_stack)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_EXPIRED;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_122: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT and EventHubClient_LL_DoWork shall bring down AMQP stack so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_Timeout_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_error_callback_and_destroys_uamqp_stack)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_TIMEOUT;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT, (void*)0x1234));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_123: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, connection_dowork shall be invoked to perform work to establish/refresh the SAS token.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_Inprogress_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_do_work)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_IN_PROGRESS;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_125: \[**If status is EVENTHUBAUTH_STATUS_IDLE, EventHubAuthCBS_Authenticate shall be invoked to create and install the SAS token.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_idle_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_EventHubAuthCBS_Authenticate_and_dowork)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_IDLE;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Authenticate(TEST_EVENTHUBCBSAUTH_HANDLE_VALID));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_125: \[**If status is EVENTHUBAUTH_STATUS_IDLE, EventHubAuthCBS_Authenticate shall be invoked to create and install the SAS token.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_idle_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_EventHubAuthCBS_Authenticate_Fails_invokes_error_callback_and_destroys_uamqp_stack)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_IDLE;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Authenticate(TEST_EVENTHUBCBSAUTH_HANDLE_VALID))
            .SetReturn(EVENTHUBAUTH_RESULT_ERROR);
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_124: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_refreshrequired_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_EventHubAuthCBS_Refresh)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_REFRESH_REQUIRED;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, NULL));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_124: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_refreshrequired_during_pre_auth_then_EventHubClient_LL_DoWork_invokes_EventHubAuthCBS_Refresh_Fails_invokes_error_callback_and_destroys_uamqp_stack)
    {
        // arrange
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_REFRESH_REQUIRED;
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, NULL)).SetReturn(EVENTHUBAUTH_RESULT_ERROR);
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_C_String_For_Target_Address_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn(NULL);
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_025: [If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_source_for_the_link_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(NULL);
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_025: [If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_target_for_the_link_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_027: [If creating the link fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_link_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_033: [If link_set_snd_settle_mode fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_link_set_snd_settle_mode_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_035: [If link_set_max_message_size fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_link_set_max_message_size_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_037: [If creating the message sender fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
    TEST_FUNCTION(when_creating_the_messagesender_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024));
        STRICT_EXPECTED_CALL(messagesender_create(TEST_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_post_auth_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024));
        STRICT_EXPECTED_CALL(messagesender_create(TEST_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG))
            .SetReturn(EVENTHUBAUTH_RESULT_ERROR);
        STRICT_EXPECTED_CALL(messagesender_destroy(TEST_MESSAGE_SENDER_HANDLE));
        STRICT_EXPECTED_CALL(link_destroy(TEST_LINK_HANDLE));
        setup_messenger_pre_auth_uamqp_stack_teardown_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth

        // act
        EventHubClient_LL_DoWork(eventHubHandle); // post auth

                                                  // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_01_039: \[**If messagesender_open fails, no further actions shall be carried out.**\]**
    TEST_FUNCTION(when_messagesender_open_fails_then_EventHubClient_LL_DoWork_does_not_proceed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        setup_messenger_pre_auth_uamqp_stack_bringup_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        STRICT_EXPECTED_CALL(STRING_c_str(TEST_TARGET_STRING_HANDLE))
            .SetReturn("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH);
        STRICT_EXPECTED_CALL(messaging_create_source("ingress"))
            .SetReturn(TEST_SOURCE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(messaging_create_target("amqps://" TEST_HOSTNAME "/" TEST_EVENTHUB_PATH))
            .SetReturn(TEST_TARGET_AMQP_VALUE);
        STRICT_EXPECTED_CALL(link_create(TEST_SESSION_HANDLE, "sender-link", role_sender, TEST_SOURCE_AMQP_VALUE, TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(link_set_snd_settle_mode(TEST_LINK_HANDLE, sender_settle_mode_unsettled));
        STRICT_EXPECTED_CALL(link_set_max_message_size(TEST_LINK_HANDLE, 256 * 1024));
        STRICT_EXPECTED_CALL(messagesender_create(TEST_LINK_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_SOURCE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_TARGET_AMQP_VALUE));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_open(TEST_MESSAGE_SENDER_HANDLE))
            .SetReturn(1);
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_open(TEST_MESSAGE_SENDER_HANDLE))
            .SetReturn(1);

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_038: [EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.] */
    TEST_FUNCTION(EventHubClient_LL_DoWork_when_message_sender_is_already_opening_no_messagesender_open_is_issued_again)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPENING, MESSAGE_SENDER_STATE_IDLE);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_failure_postauthcomplete_withpendingmsgs_then_EventHubClient_LL_DoWork_tearsdown_amqp_stack)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_FAILURE;
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_expired_postauthcomplete_withpendingmsgs_then_EventHubClient_LL_DoWork_tearsdown_amqp_stack)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_EXPIRED;
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_timeout_postauthcomplete_withpendingmsgs_then_EventHubClient_LL_DoWork_tearsdown_amqp_stack)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_TIMEOUT;
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT, (void*)0x1234));
        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_123: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, connection_dowork shall be invoked to perform work to establish/refresh the SAS token.**\]**
    TEST_FUNCTION(when_EventHubAuthCBS_GetStatus_returns_status_inprogress_postauthcomplete_with_pendingmsgs_then_EventHubClient_LL_DoWork_invokes_connection_dowork)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        g_eventhub_auth_get_status = EVENTHUBAUTH_STATUS_IN_PROGRESS;
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_126: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
    TEST_FUNCTION(using_ext_sastoken_with_refresh_when_EventHubAuthCBS_GetStatus_returns_status_ok_postauthcomplete_then_EventHubClient_LL_DoWork_invokes_AuthRefresh)
    {
        // arrange
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        setup_refresh_sastoken_success(&cfg);
        (void)EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE));
        STRICT_EXPECTED_CALL(STRING_delete(TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_126: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
    TEST_FUNCTION(using_ext_sastoken_with_refresh_when_EventHubAuthCBS_GetStatus_returns_status_ok_postauthcomplete_andauthrefresh_fails_EventHubClient_LL_DoWork_tearsdown_amqp_stack)
    {
        // arrange
        EVENTHUBAUTH_CBS_CONFIG cfg;
        setup_createfromsastoken_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromSASToken(TEST_SASTOKEN);
        (void)EventHubClient_LL_SetErrorCallback(eventHubHandle, testHook_eventhub_error_callback, (void*)0x1234);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle); // pre auth
        EventHubClient_LL_DoWork(eventHubHandle); // post auth
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        setup_refresh_sastoken_success(&cfg);
        (void)EventHubClient_LL_RefreshSASTokenAsync(eventHubHandle, TEST_REFRESH_SASTOKEN);
        // todo enable messages
        //(void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(EventHubAuthCBS_Refresh(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE))
            .SetReturn(EVENTHUBAUTH_RESULT_ERROR);
        STRICT_EXPECTED_CALL(STRING_delete(TEST_SASTOKEN_PARSER_REFRESH_STRING_HANDLE));
        STRICT_EXPECTED_CALL(testHook_eventhub_error_callback(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, (void*)0x1234));
        setup_messenger_post_auth_complete_messenger_stack_teardown_success();
        setup_messenger_pre_auth_uamqp_stack_teardown_success();

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_203: \[**EventHubClient_LL_DoWork shall perform message send handling **\]**
    /* Tests_SRS_EVENTHUBCLIENT_LL_07_028: [If the message idle time is greater than the msg_timeout, EventHubClient_LL_DoWork shall call callback with EVENTHUBCLIENT_CONFIRMATION_TIMEOUT.] */
    TEST_FUNCTION(EventHubClient_LL_DoWork_when_message_timeout)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        EventHubClient_LL_SetMessageTimeout(eventHubHandle, 2);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        g_tickcounter_value += (2000*2);
        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_TIMEOUT, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_29_203: \[**EventHubClient_LL_DoWork shall perform message send handling **\]**
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_049: [If the message has not yet been given to uAMQP then a new message shall be created by calling message_create.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_050: [If the number of event data entries for the message is 1 (not batched) then the message body shall be set to the event data payload by calling message_add_body_amqp_data.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_051: [The pointer to the payload and its length shall be obtained by calling EventData_GetData.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_069: [The AMQP message shall be given to uAMQP by calling messagesender_send, while passing as arguments the message sender handle, the message handle, a callback function and its context.] */
    TEST_FUNCTION(messages_that_are_pending_are_given_to_uAMQP_by_EventHubClient_LL_DoWork)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_049: [If the message has not yet been given to uAMQP then a new message shall be created by calling message_create.] */
    TEST_FUNCTION(two_messages_that_are_pending_are_given_to_uAMQP_by_EventHubClient_LL_DoWork)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4243);
        umock_c_reset_all_calls();

        unsigned char test_data_1[] = { 0x42 };
        unsigned char test_data_2[] = { 0x43, 0x44 };
        unsigned char* buffer = test_data_1;
        size_t length = sizeof(test_data_1);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data_1;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        buffer = test_data_2;
        length = sizeof(test_data_2);
        binary_data.bytes = test_data_2;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_052: [If EventData_GetData fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_EventData_GetData_fails_then_the_message_is_indicated_as_ERROR_and_removed_from_pending_list)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length))
            .SetReturn(EVENTDATA_ERROR);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_071: [If message_add_body_amqp_data fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_message_add_body_amqp_data_send_fails_then_the_message_is_indicated_as_ERROR_and_removed_from_pending_list)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_053: [If messagesender_send failed then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_messagesender_send_fails_then_the_message_is_indicated_as_ERROR_and_removed_from_pending_list)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_070: [If creating the message fails, then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_message_create_fails_then_the_message_is_indicated_as_ERROR_and_removed_from_pending_list)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_070: [If creating the message fails, then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_the_first_message_fails_the_second_is_given_to_uAMQP)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_Clone(IGNORED_PTR_ARG))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4243);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_055: [A map shall be created to hold the application properties by calling amqpvalue_create_map.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(one_property_is_added_to_a_non_batched_message)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(message_set_application_properties(TEST_MESSAGE_HANDLE, TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(add_partition_key_to_message_succeed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        size_t no_properties_size = 0;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1)).SetReturn(PARTITION_KEY_VALUE);

        STRICT_EXPECTED_CALL(amqpvalue_create_map()).SetReturn(PARTITION_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_symbol(PARTITION_KEY_NAME)).SetReturn(PARTITION_NAME);
        STRICT_EXPECTED_CALL(amqpvalue_create_string(PARTITION_KEY_VALUE)).SetReturn(PARTITION_STRING_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(PARTITION_MAP, PARTITION_NAME, PARTITION_STRING_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_message_annotations(PARTITION_MAP)).SetReturn(PARTITION_ANNOTATION);
        STRICT_EXPECTED_CALL(message_set_message_annotations(TEST_MESSAGE_HANDLE, PARTITION_ANNOTATION));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_STRING_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_NAME));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_MAP));

        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(4, &no_properties_size, sizeof(no_properties_size));

        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(add_partition_key_to_message_amqpvalue_create_map_NULL_fail)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        AMQP_VALUE NULL_VALUE = NULL;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1)).SetReturn(PARTITION_KEY_VALUE);

        STRICT_EXPECTED_CALL(amqpvalue_create_map()).SetReturn(NULL_VALUE);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(add_partition_key_to_message_amqpvalue_create_symbol_NULL_fail)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        AMQP_VALUE NULL_VALUE = NULL;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1)).SetReturn(PARTITION_KEY_VALUE);

        STRICT_EXPECTED_CALL(amqpvalue_create_map()).SetReturn(PARTITION_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_symbol(PARTITION_KEY_NAME)).SetReturn(NULL_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_MAP));

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(add_partition_key_to_message_amqpvalue_create_string_NULL_fail)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        AMQP_VALUE NULL_VALUE = NULL;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1)).SetReturn(PARTITION_KEY_VALUE);

        STRICT_EXPECTED_CALL(amqpvalue_create_map()).SetReturn(PARTITION_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_symbol(PARTITION_KEY_NAME)).SetReturn(PARTITION_NAME);
        STRICT_EXPECTED_CALL(amqpvalue_create_string(PARTITION_KEY_VALUE)).SetReturn(NULL_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_NAME));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_MAP));

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(add_partition_key_to_message_amqpvalue_set_map_value_return_line_fail)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1)).SetReturn(PARTITION_KEY_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_map()).SetReturn(PARTITION_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_symbol(PARTITION_KEY_NAME)).SetReturn(PARTITION_NAME);
        STRICT_EXPECTED_CALL(amqpvalue_create_string(PARTITION_KEY_VALUE)).SetReturn(PARTITION_STRING_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(PARTITION_MAP, PARTITION_NAME, PARTITION_STRING_VALUE)).SetReturn(__LINE__);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_STRING_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_NAME));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(PARTITION_MAP));

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_055: [A map shall be created to hold the application properties by calling amqpvalue_create_map.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
    TEST_FUNCTION(two_properties_are_added_to_a_non_batched_message)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(TEST_PROPERTY_2_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_2_KEY_AMQP_VALUE, TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(message_set_application_properties(TEST_MESSAGE_HANDLE, TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_properties_from_the_event_data_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_properties_details_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size))
            .SetReturn(MAP_ERROR);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_AMQP_map_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_AMQP_property_key_value_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_AMQP_property_value_value_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_setting_the_property_in_the_uAMQP_map_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_setting_the_message_annotations_on_the_message_fails_the_event_is_indicated_as_errored)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        const char* const one_property_keys[] = { "test_property_key" };
        const char* const one_property_values[] = { "test_property_value" };
        const char* const* one_property_keys_ptr = one_property_keys;
        const char* const* one_property_values_ptr = one_property_values;
        size_t one_property_size = 1;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &one_property_keys_ptr, sizeof(one_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &one_property_values_ptr, sizeof(one_property_values_ptr))
            .CopyOutArgumentBuffer(4, &one_property_size, sizeof(one_property_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(message_set_application_properties(TEST_MESSAGE_HANDLE, TEST_UAMQP_MAP))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* on_message_send_complete */

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_061: [When on_message_send_complete is called with MESSAGE_SEND_OK the pending message shall be indicated as sent correctly by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_OK.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_062: [The pending message shall be removed from the pending list.] */
    TEST_FUNCTION(on_message_send_complete_with_OK_on_one_message_triggers_the_user_callback_and_removes_the_pending_event)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        EventHubClient_LL_DoWork(eventHubHandle);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_OK, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        saved_on_message_send_complete(saved_on_message_send_complete_context, MESSAGE_SEND_OK, test_delivery_state);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_063: [When on_message_send_complete is called with a result code different than MESSAGE_SEND_OK the pending message shall be indicated as having an error by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_ERROR.]  */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_062: [The pending message shall be removed from the pending list.] */
    TEST_FUNCTION(on_message_send_complete_with_ERROR_on_one_message_triggers_the_user_callback_and_removes_the_pending_event)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        (void)EventHubClient_LL_SendAsync(eventHubHandle, TEST_EVENTDATA_HANDLE, sendAsyncConfirmationCallback, (void*)0x4242);
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys_ptr, sizeof(no_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &no_property_values_ptr, sizeof(no_property_values_ptr))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        EventHubClient_LL_DoWork(eventHubHandle);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        size_t length = sizeof(test_data);
        BINARY_DATA binary_data;
        binary_data.bytes = test_data;
        binary_data.length = length;

        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        saved_on_message_send_complete(saved_on_message_send_complete_context, MESSAGE_SEND_ERROR, test_delivery_state);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* on_messagesender_state_changed */

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_060: [When on_messagesender_state_changed is called with MESSAGE_SENDER_STATE_ERROR] */
    TEST_FUNCTION(when_the_messagesender_state_changes_to_ERROR_then_the_uAMQP_stack_is_brought_down)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        umock_c_reset_all_calls();

        // act
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_ERROR, MESSAGE_SENDER_STATE_OPEN);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_082: [If the number of event data entries for the message is greater than 1 (batched) then the message format shall be set to 0x80013700 by calling message_set_message_format.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_084: [For each event in the batch:] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_085: [The event shall be added to the message by into a separate data section by calling message_add_body_amqp_data.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_086: [The buffer passed to message_add_body_amqp_data shall contain the properties and the binary event payload serialized as AMQP values.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_088: [The event payload shall be serialized as an AMQP message data section.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_090: [Enough memory shall be allocated to hold the properties and binary payload for each event part of the batch.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_091: [The size needed for the properties and data section shall be obtained by calling amqpvalue_get_encoded_size.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_092: [The properties and binary data shall be encoded by calling amqpvalue_encode and passing an encoding function that places the encoded data into the memory allocated for the event.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_093: [If the property count is 0 for an event part of the batch, then no property map shall be serialized for that event.] */
    TEST_FUNCTION(a_batched_message_with_2_events_and_no_properties_is_added_to_the_message_body)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size));
        g_expected_encoded_buffer[0] = encoded_data_1;
        g_expected_encoded_length[0] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        BINARY_DATA binary_data;
        binary_data.bytes = encoded_data_1;
        binary_data.length = data_encoded_size;
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));

        //2nd event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_2);
        unsigned char encoded_data_2[] = { 0x43, 0x44 };
        data_encoded_size = sizeof(encoded_data_2);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_2, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size));
        g_expected_encoded_buffer[1] = encoded_data_2;
        g_expected_encoded_length[1] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        binary_data.bytes = encoded_data_2;
        binary_data.length = data_encoded_size;
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_2));

        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_083: [If message_set_message_format fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_message_set_message_format_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT))
            .SetReturn(1);
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_event_data_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length))
            .SetReturn(EVENTDATA_ERROR);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_event_data_properties_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_properties_map_keys_and_values_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size))
            .SetReturn(MAP_ERROR);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_data_section_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_encoded_length_of_the_data_section_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        (void)encoded_data_1;
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_allocating_memory_for_the_data_section_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        (void)encoded_data_1;
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_encoding_the_event_in_a_batch_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size));
        g_expected_encoded_buffer[0] = encoded_data_1;
        g_expected_encoded_length[0] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_089: [If message_add_body_amqp_data fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_message_add_body_amqp_data_for_an_event_in_a_batch_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size));
        g_expected_encoded_buffer[0] = encoded_data_1;
        g_expected_encoded_length[0] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        BINARY_DATA binary_data;
        binary_data.bytes = encoded_data_1;
        binary_data.length = data_encoded_size;
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_086: [The buffer passed to message_add_body_amqp_data shall contain the properties and the binary event payload serialized as AMQP values.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_090: [Enough memory shall be allocated to hold the properties and binary payload for each event part of the batch.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_091: [The size needed for the properties and data section shall be obtained by calling amqpvalue_get_encoded_size.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_092: [The properties and binary data shall be encoded by calling amqpvalue_encode and passing an encoding function that places the encoded data into the memory allocated for the event.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_087: [The properties shall be serialized as AMQP application_properties.] */
    TEST_FUNCTION(a_batched_event_with_2_properties_gets_the_properties_added_to_the_payload)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(TEST_PROPERTY_2_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_2_KEY_AMQP_VALUE, TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_VALUE_AMQP_VALUE));

        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);

        STRICT_EXPECTED_CALL(amqpvalue_create_application_properties(TEST_UAMQP_MAP))
            .SetReturn(TEST_APPLICATION_PROPERTIES_1);
        unsigned char properties_encoded_data[] = { 0x42, 0x43, 0x44 };
        size_t properties_encoded_size = sizeof(properties_encoded_data);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_APPLICATION_PROPERTIES_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &properties_encoded_size, sizeof(properties_encoded_size));
        unsigned char encoded_data_1[] = { 0x42 };
        size_t data_encoded_size = sizeof(encoded_data_1);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size + properties_encoded_size));
        g_expected_encoded_buffer[0] = properties_encoded_data;
        g_expected_encoded_length[0] = properties_encoded_size;
        g_expected_encoded_buffer[1] = encoded_data_1;
        g_expected_encoded_length[1] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_APPLICATION_PROPERTIES_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        unsigned char combined_encoded_data[] = { 0x42, 0x43, 0x44, 0x42 };
        BINARY_DATA binary_data;
        binary_data.bytes = combined_encoded_data;
        binary_data.length = data_encoded_size + properties_encoded_size;
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_APPLICATION_PROPERTIES_1));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));

        //2nd event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &no_property_keys, sizeof(no_property_keys))
            .CopyOutArgumentBuffer(3, &no_property_values, sizeof(no_property_values))
            .CopyOutArgumentBuffer(4, &no_property_size, sizeof(no_property_size));
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_2);
        unsigned char encoded_data_2[] = { 0x43, 0x44 };
        data_encoded_size = sizeof(encoded_data_2);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_DATA_2, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &data_encoded_size, sizeof(data_encoded_size));
        STRICT_EXPECTED_CALL(gballoc_malloc(data_encoded_size));
        g_expected_encoded_buffer[2] = encoded_data_2;
        g_expected_encoded_length[2] = data_encoded_size;
        STRICT_EXPECTED_CALL(amqpvalue_encode(TEST_DATA_2, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        binary_data.bytes = encoded_data_2;
        binary_data.length = data_encoded_size;
        STRICT_EXPECTED_CALL(message_add_body_amqp_data(TEST_MESSAGE_HANDLE, binary_data));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_2));

        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(messagesender_send_async(TEST_MESSAGE_SENDER_HANDLE, TEST_MESSAGE_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, 0));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_properties_map_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_key_for_the_first_property_string_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_value_for_the_first_property_string_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_setting_the_first_property_in_the_properties_map_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_key_for_the_second_property_in_the_properties_map_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_value_for_the_second_property_in_the_properties_map_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_adding_the_second_property_to_the_map_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(TEST_PROPERTY_2_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_2_KEY_AMQP_VALUE, TEST_PROPERTY_2_VALUE_AMQP_VALUE))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_creating_the_application_properties_object_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(TEST_PROPERTY_2_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_2_KEY_AMQP_VALUE, TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        STRICT_EXPECTED_CALL(amqpvalue_create_application_properties(TEST_UAMQP_MAP))
            .SetReturn(NULL);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
    TEST_FUNCTION(when_getting_the_encoded_size_for_the_properties_fails_then_the_callback_is_triggered_with_ERROR)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        setup_messenger_initialize_success();
        EventHubClient_LL_DoWork(eventHubHandle);
        EventHubClient_LL_DoWork(eventHubHandle);
        saved_on_message_sender_state_changed(saved_message_sender_context, MESSAGE_SENDER_STATE_OPEN, MESSAGE_SENDER_STATE_IDLE);
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        umock_c_reset_all_calls();
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        (void)EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(EVENTDATA_HANDLE), sendAsyncConfirmationCallback, (void*)0x4242);
        umock_c_reset_all_calls();

        unsigned char test_data[] = { 0x42 };
        unsigned char* buffer = test_data;
        size_t length = sizeof(test_data);
        g_expected_encoded_counter = 0;

        const char* const two_property_keys[] = { "test_property_key", "prop_key_2" };
        const char* const two_property_values[] = { "test_property_value", "prop_value_2" };
        const char* const* two_property_keys_ptr = two_property_keys;
        const char* const* two_property_values_ptr = two_property_values;
        size_t two_properties_size = 2;

        STRICT_EXPECTED_CALL(EventHubAuthCBS_GetStatus(TEST_EVENTHUBCBSAUTH_HANDLE_VALID, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(message_create());
        STRICT_EXPECTED_CALL(message_set_message_format(TEST_MESSAGE_HANDLE, MICROSOFT_MESSAGE_FORMAT));

        // 1st event
        STRICT_EXPECTED_CALL(EventData_GetData(TEST_CLONED_EVENTDATA_HANDLE_1, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &buffer, sizeof(buffer))
            .CopyOutArgumentBuffer(3, &length, sizeof(length));
        STRICT_EXPECTED_CALL(EventData_Properties(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(Map_GetInternals(TEST_MAP_HANDLE, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &two_property_keys_ptr, sizeof(two_property_keys_ptr))
            .CopyOutArgumentBuffer(3, &two_property_values_ptr, sizeof(two_property_values_ptr))
            .CopyOutArgumentBuffer(4, &two_properties_size, sizeof(two_properties_size));
        STRICT_EXPECTED_CALL(amqpvalue_create_map())
            .SetReturn(TEST_UAMQP_MAP);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_key"))
            .SetReturn(TEST_PROPERTY_1_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("test_property_value"))
            .SetReturn(TEST_PROPERTY_1_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_1_KEY_AMQP_VALUE, TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_1_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_key_2"))
            .SetReturn(TEST_PROPERTY_2_KEY_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_create_string("prop_value_2"))
            .SetReturn(TEST_PROPERTY_2_VALUE_AMQP_VALUE);
        STRICT_EXPECTED_CALL(amqpvalue_set_map_value(TEST_UAMQP_MAP, TEST_PROPERTY_2_KEY_AMQP_VALUE, TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_KEY_AMQP_VALUE));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_PROPERTY_2_VALUE_AMQP_VALUE));
        amqp_binary amqy_binary;
        amqy_binary.bytes = test_data;
        amqy_binary.length = (uint32_t)length;
        STRICT_EXPECTED_CALL(amqpvalue_create_data(amqy_binary))
            .SetReturn(TEST_DATA_1);
        STRICT_EXPECTED_CALL(amqpvalue_create_application_properties(TEST_UAMQP_MAP))
            .SetReturn(TEST_APPLICATION_PROPERTIES_1);
        unsigned char properties_encoded_data[] = { 0x42, 0x43, 0x44 };
        size_t properties_encoded_size = sizeof(properties_encoded_data);
        STRICT_EXPECTED_CALL(amqpvalue_get_encoded_size(TEST_APPLICATION_PROPERTIES_1, IGNORED_PTR_ARG))
            .CopyOutArgumentBuffer(2, &properties_encoded_size, sizeof(properties_encoded_size))
            .SetReturn(1);

        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_APPLICATION_PROPERTIES_1));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_UAMQP_MAP));
        STRICT_EXPECTED_CALL(amqpvalue_destroy(TEST_DATA_1));
        STRICT_EXPECTED_CALL(message_destroy(TEST_MESSAGE_HANDLE));
        STRICT_EXPECTED_CALL(sendAsyncConfirmationCallback(EVENTHUBCLIENT_CONFIRMATION_ERROR, (void*)0x4242));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_2));
        STRICT_EXPECTED_CALL(DList_RemoveEntryList(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        STRICT_EXPECTED_CALL(connection_dowork(TEST_CONNECTION_HANDLE));

        // act
        EventHubClient_LL_DoWork(eventHubHandle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* EventHubClient_LL_SendBatchAsync */

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_012: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventhubClientLLHandle or eventDataList are NULL or if sendAsnycConfirmationCallback equals NULL and userContextCallback does not equal NULL.] */
    TEST_FUNCTION(EventHubClient_LL_SendBatchAsync_with_NULL_sendAsyncConfirmationCallbackandNonNullUSerContext_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };

        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), NULL, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_012: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventhubClientLLHandle or eventDataList are NULL or if sendAsnycConfirmationCallback equals NULL and userContextCallback does not equal NULL.]  */
    TEST_FUNCTION(EventHubClient_LL_SendBatchAsync_with_NULL_eventHubLLHandle_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(NULL, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_012: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventhubClientLLHandle or eventDataList are NULL or if sendAsnycConfirmationCallback equals NULL and userContextCallback does not equal NULL.] */
    TEST_FUNCTION(EventHubClient_LL_SendBatchAsync_with_NULL_event_data_list_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, NULL, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_095: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if the count argument is zero.] */
    TEST_FUNCTION(EventHubClient_LL_SendBatchAsync_with_zero_count_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, 0, sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_014: [EventHubClient_LL_SendBatchAsync shall clone each item in the eventDataList by calling EventData_Clone.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_07_015: [On success EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_OK.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_097: [The partition key for each event shall be obtained by calling EventData_getPartitionKey.] */
    TEST_FUNCTION(EventHubClient_LL_SendBatchAsync_clones_the_event_data)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_2);
        STRICT_EXPECTED_CALL(DList_InsertTailList(IGNORED_PTR_ARG, IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_01_097: [The partition key for each event shall be obtained by calling EventData_getPartitionKey.] */
    /* Tests_SRS_EVENTHUBCLIENT_LL_01_096: [If the partitionKey properties on the events in the batch are not the same then EventHubClient_LL_SendBatchAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
    TEST_FUNCTION(when_the_partitions_on_the_events_do_not_match_then_EventHubClient_LL_SendBatchAsync_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey1");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey2");

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.] */
    TEST_FUNCTION(when_allocating_memory_for_the_event_batch_fails_then_EventHubClient_LL_SendBatchAsync_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.]  */
    TEST_FUNCTION(when_allocating_memory_for_the_list_of_events_fails_then_EventHubClient_LL_SendBatchAsync_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.]  */
    TEST_FUNCTION(when_cloning_the_first_item_fails_EventHubClient_LL_SendBatchAsync_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.]  */
    TEST_FUNCTION(when_cloning_the_second_item_fails_EventHubClient_LL_SendBatchAsync_fails)
    {
        // arrange
        EVENTDATA_HANDLE batch[] = { TEST_EVENTDATA_HANDLE_1, TEST_EVENTDATA_HANDLE_2 };
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_1))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(EventData_GetPartitionKey(TEST_EVENTDATA_HANDLE_2))
            .SetReturn("partitionKey");
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(tickcounter_get_current_ms(TICK_COUNT_HANDLE_TEST, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_1))
            .SetReturn(TEST_CLONED_EVENTDATA_HANDLE_1);
        STRICT_EXPECTED_CALL(EventData_Clone(TEST_EVENTDATA_HANDLE_2))
            .SetReturn(NULL);
        STRICT_EXPECTED_CALL(EventData_Destroy(TEST_CLONED_EVENTDATA_HANDLE_1));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(gballoc_free(IGNORED_PTR_ARG));

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SendBatchAsync(eventHubHandle, batch, sizeof(batch) / sizeof(batch[0]), sendAsyncConfirmationCallback, &g_confirmationResult);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_ERROR, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_016: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetStateChangeCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
    TEST_FUNCTION(EventHubClient_LL_SetStateChangeCallback_EventHubClient_NULL_fails)
    {
        // arrange

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SetStateChangeCallback(NULL, eventhub_state_change_callback, NULL);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_016: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetStateChangeCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_017: [** If state_change_cb is non-NULL then EventHubClient_LL_SetStateChangeCallback shall call state_change_cb when a state changes is encountered. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_018: [** If state_change_cb is NULL EventHubClient_LL_SetStateChangeCallback shall no longer call state_change_cb on state changes. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_019: [** If EventHubClient_LL_SetStateChangeCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**
    TEST_FUNCTION(EventHubClient_LL_SetStateChangeCallback_succeed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SetStateChangeCallback(eventHubHandle, eventhub_state_change_callback, NULL);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_020: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetErrorCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
    TEST_FUNCTION(EventHubClient_LL_SetErrorCallback_EventHubClient_NULL_fails)
    {
        // arrange

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SetErrorCallback(NULL, eventhub_error_callback, NULL);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_020: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetErrorCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_021: [** If failure_cb is non-NULL EventHubClient_LL_SetErrorCallback shall execute the on_error_cb on failures with a EVENTHUBCLIENT_FAILURE_RESULT. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_022: [** If failure_cb is NULL EventHubClient_LL_SetErrorCallback shall no longer call on_error_cb on failure. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_023: [** If EventHubClient_LL_SetErrorCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**
    TEST_FUNCTION(EventHubClient_LL_SetErrorCallback_succeed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EVENTHUBCLIENT_RESULT result = EventHubClient_LL_SetErrorCallback(eventHubHandle, eventhub_error_callback, NULL);

        //assert
        ASSERT_ARE_EQUAL(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_OK, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_025: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetLogTrace shall do nothing. **]**
    TEST_FUNCTION(EventHubClient_LL_SetLogTrace_EventHubClient_NULL_fails)
    {
        // arrange

        // act
        EventHubClient_LL_SetLogTrace(NULL, false);

        //assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
    }

    //**Tests_SRS_EVENTHUBCLIENT_LL_07_024: [** If eventHubClientLLHandle is non-NULL EventHubClient_LL_SetLogTrace shall call the uAmqp trace function with the log_trace_on. **]**
    //**Tests_SRS_EVENTHUBCLIENT_LL_07_025: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetLogTrace shall do nothing. **]**
    TEST_FUNCTION(EventHubClient_LL_SetLogTrace_succeed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EventHubClient_LL_SetLogTrace(eventHubHandle, true);

        //assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_026: [ If eventHubClientLLHandle is NULL EventHubClient_LL_SetMessageTimeout shall do nothing. ] */
    TEST_FUNCTION(EventHubClient_LL_SetMessageTimeout_EventHubClient_NULL_fails)
    {
        // arrange

        // act
        EventHubClient_LL_SetMessageTimeout(NULL, 1000);

        //assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
    }

    /* Tests_SRS_EVENTHUBCLIENT_LL_07_027: [ EventHubClient_LL_SetMessageTimeout shall save the timeout_value. ] */
    TEST_FUNCTION(EventHubClient_LL_SetMessageTimeout_succeed)
    {
        // arrange
        setup_createfromconnectionstring_success();
        EVENTHUBCLIENT_LL_HANDLE eventHubHandle = EventHubClient_LL_CreateFromConnectionString(CONNECTION_STRING, TEST_EVENTHUB_PATH);
        umock_c_reset_all_calls();

        // act
        EventHubClient_LL_SetMessageTimeout(eventHubHandle, 1000);

        //assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        //cleanup
        EventHubClient_LL_Destroy(eventHubHandle);
    }

END_TEST_SUITE(eventhubclient_ll_unittests)
