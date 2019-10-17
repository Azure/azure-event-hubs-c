// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "azure_c_shared_utility/azure_base64.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/connection_string_parser.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/doublylinkedlist.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_macro_utils/macro_utils.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/sastoken.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/tlsio.h"
#include "azure_c_shared_utility/urlencode.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/connection.h"
#include "azure_uamqp_c/link.h"
#include "azure_uamqp_c/message.h"
#include "azure_uamqp_c/messaging.h"
#include "azure_uamqp_c/message_sender.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"
#include "azure_uamqp_c/session.h"
#include "azure_uamqp_c/amqp_definitions_data.h"
#include "azure_uamqp_c/amqp_definitions_application_properties.h"

#include "eventhubauth.h"
#include "eventhubclient_ll.h"
#include "version.h"

MU_DEFINE_ENUM_STRINGS(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_RESULT_VALUES)

#define AMQP_MAX_MESSAGE_SIZE           (256*1024)

#define AUTH_TIMEOUT_SECS               30
#define AUTH_EXPIRATION_SECS            (60 * 60)
#define AUTH_REFRESH_SECS               (48 * 60)  //80% of expiration period

/* Event Hub Client (sender) AMQP States */
#define EVENTHUBCLIENT_AMQP_STATE_VALUES        \
        SENDER_AMQP_UNINITIALIZED,              \
        SENDER_AMQP_PENDING_AUTHORIZATION,      \
        SENDER_AMQP_PENDING_SENDER_CREATE,      \
        SENDER_AMQP_INITIALIZED

MU_DEFINE_ENUM(EVENTHUBCLIENT_AMQP_STATE, EVENTHUBCLIENT_AMQP_STATE_VALUES);

#define LOG_ERROR(x) LogError("result = %s", MU_ENUM_TO_STRING(EVENTHUBCLIENT_RESULT, x));

static const char SB_STRING[] = "sb://";
#define SB_STRING_LENGTH      ((sizeof(SB_STRING) / sizeof(SB_STRING[0])) - 1)   /* length for sb:// */

static const size_t OUTGOING_WINDOW_SIZE = 10;
static const size_t OUTGOING_WINDOW_BUFFER = 5;

typedef struct EVENT_DATA_BINARY_TAG
{
    unsigned char* bytes;
    size_t length;
} EVENT_DATA_BINARY;

typedef enum EVENTHUB_EVENT_STATUS_TAG
{
    WAITING_TO_BE_SENT = 0,
    WAITING_FOR_ACK
} EVENTHUB_EVENT_STATUS;

typedef struct EVENTHUB_EVENT_LIST_TAG
{
    EVENTDATA_HANDLE* eventDataList;
    size_t eventCount;
    EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK callback;
    void* context;
    EVENTHUB_EVENT_STATUS currentStatus;
    DLIST_ENTRY entry;
    tickcounter_ms_t idle_timer;
} EVENTHUB_EVENT_LIST, *PEVENTHUB_EVENT_LIST;

typedef struct EVENTHUBCLIENT_LL_TAG
{
    STRING_HANDLE keyName;
    STRING_HANDLE keyValue;
    STRING_HANDLE event_hub_path;
    STRING_HANDLE host_name;
    STRING_HANDLE target_address;
    STRING_HANDLE sender_publisher_id;
    STRING_HANDLE ext_refresh_sas_token;
    EVENTHUBAUTH_CBS_CONFIG* ext_sas_token_parse_config;
    DLIST_ENTRY outgoingEvents;
    CONNECTION_HANDLE connection;
    SESSION_HANDLE session;
    LINK_HANDLE link;
    SASL_MECHANISM_HANDLE sasl_mechanism_handle;
    XIO_HANDLE sasl_io;
    XIO_HANDLE tls_io;
    MESSAGE_SENDER_STATE message_sender_state;
    MESSAGE_SENDER_HANDLE message_sender;
    EVENTHUB_CLIENT_STATECHANGE_CALLBACK state_change_cb;
    void* statuschange_callback_context;
    EVENTHUB_CLIENT_ERROR_CALLBACK on_error_cb;
    void* error_callback_context;
    int trace_on;
    uint64_t msg_timeout;
    TICK_COUNTER_HANDLE counter;
    EVENTHUBAUTH_CBS_HANDLE cbs_handle;
    EVENTHUBCLIENT_AMQP_STATE amqp_state;
    EVENTHUBAUTH_CREDENTIAL_TYPE credential;
} EVENTHUBCLIENT_LL;

static const char ENDPOINT_SUBSTRING[] = "Endpoint=sb://";
static const size_t ENDPOINT_SUBSTRING_LENGTH = sizeof(ENDPOINT_SUBSTRING) / sizeof(ENDPOINT_SUBSTRING[0]) - 1;

static const char* PARTITION_KEY_NAME = "x-opt-partition-key";

static int add_partition_key_to_message(MESSAGE_HANDLE message, EVENTDATA_HANDLE event_data)
{
    int result;
    const char* currPartKey = EventData_GetPartitionKey(event_data);
    if (currPartKey != NULL)
    {
        AMQP_VALUE partition_map;
        AMQP_VALUE partition_name;
        AMQP_VALUE partition_value;

        if ((partition_map = amqpvalue_create_map()) == NULL)
        {
            LogError("Failure creating amqp map");
            result = __LINE__;
        }
        else if ( (partition_name = amqpvalue_create_symbol(PARTITION_KEY_NAME) ) == NULL)
        {
            LogError("Failure creating amqp symbol");
            amqpvalue_destroy(partition_map);
            result = __LINE__;
        }
        else if ((partition_value = amqpvalue_create_string(currPartKey)) == NULL)
        {
            LogError("Failure creating amqp string");
            amqpvalue_destroy(partition_name);
            amqpvalue_destroy(partition_map);
            result = __LINE__;
        }
        else if (amqpvalue_set_map_value(partition_map, partition_name, partition_value) != 0)
        {
            LogError("amqpvalue_set_map_value failed");
            amqpvalue_destroy(partition_value);
            amqpvalue_destroy(partition_name);
            amqpvalue_destroy(partition_map);
            result = __LINE__;
        }
        else
        {
            AMQP_VALUE partition_annotation = amqpvalue_create_message_annotations(partition_map);
            if (partition_annotation != NULL)
            {
                if (message_set_message_annotations(message, partition_annotation) == 0)
                {
                    result = 0;
                }
                else
                {
                    LogError("message_set_message_annotations failed");
                    result = __LINE__;
                }
            }
            else
            {
                LogError("amqpvalue_create_message_annotations failed");
                result = __LINE__;
            }

            amqpvalue_destroy(partition_value);
            amqpvalue_destroy(partition_name);
            amqpvalue_destroy(partition_map);
            amqpvalue_destroy(partition_annotation);
        }
    }
    else
    {
        result = 0;
    }
    return result;
}

static int ValidateEventDataList(EVENTDATA_HANDLE *eventDataList, size_t count)
{
    int result = 0;
    const char* partitionKey = NULL;
    size_t index;

    for (index = 0; index < count; index++)
    {
        if (eventDataList[index] == NULL)
        {
            result = __LINE__;
            LogError("handle index %d NULL result = %s", (int)index, MU_ENUM_TO_STRING(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_INVALID_ARG));
            break;
        }
        else
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_097: [The partition key for each event shall be obtained by calling EventData_getPartitionKey.] */
            const char* currPartKey = EventData_GetPartitionKey(eventDataList[index]);
            if (index == 0)
            {
                partitionKey = currPartKey;
            }
            else
            {
                if ((currPartKey == NULL && partitionKey != NULL) || (currPartKey != NULL && partitionKey == NULL))
                {
                    result = __LINE__;
                    LogError("All event data in a SendBatch operation must have the same partition key result = %s", MU_ENUM_TO_STRING(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_PARTITION_KEY_MISMATCH));
                    break;
                }
                else
                {
                    if (currPartKey != NULL && partitionKey != NULL)
                    {
                        if (strcmp(partitionKey, currPartKey) != 0)
                        {
                            /*Codes_SRS_EVENTHUBCLIENT_07_045: [If all of the eventDataHandle objects contain differing partitionKey values then EventHubClient_SendBatch shall fail and return EVENTHUBCLIENT_PARTITION_KEY_MISMATCH.]*/
                            result = __LINE__;
                            LogError("All event data in a SendBatch operation must have the same partition key result = %s", MU_ENUM_TO_STRING(EVENTHUBCLIENT_RESULT, EVENTHUBCLIENT_PARTITION_KEY_MISMATCH));
                            break;
                        }
                    }
                }
            }
        }
    }
    return result;
}

static void destroy_uamqp_stack(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_072: \[**The message sender shall be destroyed by calling messagesender_destroy.**\]**
    if (eventhub_client_ll->message_sender != NULL)
    {
        messagesender_destroy(eventhub_client_ll->message_sender);
        eventhub_client_ll->message_sender = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_073: \[**The link shall be destroyed by calling link_destroy.**\]**
    if (eventhub_client_ll->link != NULL)
    {
        link_destroy(eventhub_client_ll->link);
        eventhub_client_ll->link = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_151: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    if (eventhub_client_ll->cbs_handle != NULL)
    {
        EventHubAuthCBS_Destroy(eventhub_client_ll->cbs_handle);
        eventhub_client_ll->cbs_handle = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_074: \[**The session shall be destroyed by calling session_destroy.**\]**
    if (eventhub_client_ll->session != NULL)
    {
        session_destroy(eventhub_client_ll->session);
        eventhub_client_ll->session = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_075: \[**The connection shall be destroyed by calling connection_destroy.**\]**
    if (eventhub_client_ll->connection != NULL)
    {
        connection_destroy(eventhub_client_ll->connection);
        eventhub_client_ll->connection = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_076: \[**The SASL IO shall be destroyed by calling xio_destroy.**\]**
    if (eventhub_client_ll->sasl_io != NULL)
    {
        xio_destroy(eventhub_client_ll->sasl_io);
        eventhub_client_ll->sasl_io = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_077: \[**The TLS IO shall be destroyed by calling xio_destroy.**\]**
    if (eventhub_client_ll->tls_io != NULL)
    {
        xio_destroy(eventhub_client_ll->tls_io);
        eventhub_client_ll->tls_io = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_01_078: \[**The SASL mechanism shall be destroyed by calling saslmechanism_destroy.**\]**
    if (eventhub_client_ll->sasl_mechanism_handle != NULL)
    {
        saslmechanism_destroy(eventhub_client_ll->sasl_mechanism_handle);
        eventhub_client_ll->sasl_mechanism_handle = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_152: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    if (eventhub_client_ll->ext_refresh_sas_token != NULL)
    {
        STRING_delete(eventhub_client_ll->ext_refresh_sas_token);
        eventhub_client_ll->ext_refresh_sas_token = NULL;
    }
    eventhub_client_ll->message_sender_state = MESSAGE_SENDER_STATE_IDLE;
    eventhub_client_ll->amqp_state = SENDER_AMQP_UNINITIALIZED;
}

static void on_message_sender_state_changed(void* context, MESSAGE_SENDER_STATE new_state, MESSAGE_SENDER_STATE previous_state)
{
    EVENTHUBCLIENT_LL* eventhub_client_ll = (EVENTHUBCLIENT_LL*)context;
    (void)previous_state;
    eventhub_client_ll->message_sender_state = new_state;

    if (new_state != previous_state)
    {
        if (eventhub_client_ll->state_change_cb)
        {
            if (new_state == MESSAGE_SENDER_STATE_OPEN)
            {
                eventhub_client_ll->state_change_cb(EVENTHUBCLIENT_CONN_AUTHENTICATED, eventhub_client_ll->statuschange_callback_context);
            }
            else if (new_state == MESSAGE_SENDER_STATE_CLOSING || (new_state == MESSAGE_SENDER_STATE_ERROR) )
            {
                eventhub_client_ll->state_change_cb(EVENTHUBCLIENT_CONN_UNAUTHENTICATED, eventhub_client_ll->statuschange_callback_context);
            }
        }

        /* Codes_SRS_EVENTHUBCLIENT_LL_01_060: [When on_messagesender_state_changed is called with MESSAGE_SENDER_STATE_ERROR, the uAMQP stack shall be brough down so that it can be created again if needed in dowork:] */
        if (new_state == MESSAGE_SENDER_STATE_ERROR)
        {
            if (eventhub_client_ll->on_error_cb != NULL)
            {
                eventhub_client_ll->on_error_cb(EVENTHUBCLIENT_SOCKET_SEND_FAILURE, eventhub_client_ll->error_callback_context);
            }
        }
    }
}

static int create_sas_token(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    int result;
    unsigned int timeout = (eventhub_client_ll->msg_timeout < INT_MAX) ? (unsigned int)eventhub_client_ll->msg_timeout : INT_MAX;

    if (eventhub_client_ll->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        EVENTHUBAUTH_CBS_CONFIG cfg;

        //**Codes_SRS_EVENTHUBCLIENT_LL_29_110: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a EVENTHUBAUTH_CBS_CONFIG structure params hostName, eventHubPath, sharedAccessKeyName, sharedAccessKey using the values set previously. Set senderPublisherId to "sender". Set receiverConsumerGroup, receiverPartitionId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_SENDER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
        cfg.hostName = eventhub_client_ll->host_name;
        cfg.eventHubPath = eventhub_client_ll->event_hub_path;
        cfg.sharedAccessKeyName = eventhub_client_ll->keyName;
        cfg.sharedAccessKey = eventhub_client_ll->keyValue;
        cfg.credential = eventhub_client_ll->credential;
        cfg.senderPublisherId = eventhub_client_ll->sender_publisher_id;
        cfg.mode = EVENTHUBAUTH_MODE_SENDER;
        cfg.sasTokenAuthFailureTimeoutInSecs = timeout;
        cfg.sasTokenExpirationTimeInSec = AUTH_EXPIRATION_SECS;
        cfg.sasTokenRefreshPeriodInSecs = AUTH_REFRESH_SECS;
        cfg.receiverConsumerGroup = NULL;
        cfg.receiverPartitionId = NULL;
	    cfg.extSASToken = NULL;
        cfg.extSASTokenURI = NULL;
        cfg.extSASTokenExpTSInEpochSec = 0;
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
        eventhub_client_ll->cbs_handle = EventHubAuthCBS_Create(&cfg, eventhub_client_ll->session);
    }
    else
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_111: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the EVENTHUBAUTH_CBS_CONFIG obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
        eventhub_client_ll->ext_sas_token_parse_config->sasTokenAuthFailureTimeoutInSecs = timeout;
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_113: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
        eventhub_client_ll->cbs_handle = EventHubAuthCBS_Create(eventhub_client_ll->ext_sas_token_parse_config, eventhub_client_ll->session);
    }

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_114: \[**If EventHubAuthCBS_Create returns NULL, a log message will be logged and the function returns immediately.**\]**
    if (eventhub_client_ll->cbs_handle == NULL)
    {
        LogError("Couldn't create CBS based Auth Handle\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

static int handle_sas_token_auth(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll, bool *is_timeout, bool *is_auth_in_progress)
{
    int result;
    EVENTHUBAUTH_STATUS auth_status;
    EVENTHUBAUTH_RESULT auth_result;

    *is_timeout = false;
    *is_auth_in_progress = false;

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_120: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    if ((auth_result = EventHubAuthCBS_GetStatus(eventhub_client_ll->cbs_handle, &auth_status)) != EVENTHUBAUTH_RESULT_OK)
    {
        LogError("EventHubAuthCBS_GetStatus Failed. Code:%u\r\n", auth_result);
        result = __LINE__;
    }
    else
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
        if (auth_status == EVENTHUBAUTH_STATUS_FAILURE)
        {
            LogError("EventHubAuthCBS Status Failed.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
        else if (auth_status == EVENTHUBAUTH_STATUS_EXPIRED)
        {
            LogError("EventHubAuthCBS Status Expired.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_122: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT and EventHubClient_LL_DoWork shall bring down AMQP stack so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
        else if (auth_status == EVENTHUBAUTH_STATUS_TIMEOUT)
        {
            *is_timeout = true;
            LogError("EventHubAuthCBS Status Timeout.\r\n");
            result = 0;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_123: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, connection_dowork shall be invoked to perform work to establish/refresh the SAS token.**\]**
        else if (auth_status == EVENTHUBAUTH_STATUS_IN_PROGRESS)
        {
            *is_auth_in_progress = true;
            result = 0;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_124: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
        else if (auth_status == EVENTHUBAUTH_STATUS_REFRESH_REQUIRED)
        {
            auth_result = EventHubAuthCBS_Refresh(eventhub_client_ll->cbs_handle, NULL);
            if (auth_result != EVENTHUBAUTH_RESULT_OK)
            {
                LogError("EventHubAuthCBS_Refresh Failed. Code:%u\r\n", auth_result);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_125: \[**If status is EVENTHUBAUTH_STATUS_IDLE, EventHubAuthCBS_Authenticate shall be invoked to create and install the SAS token.**\]**
        else if (auth_status == EVENTHUBAUTH_STATUS_IDLE)
        {
            *is_auth_in_progress = true;
            auth_result = EventHubAuthCBS_Authenticate(eventhub_client_ll->cbs_handle);

            if (auth_result != EVENTHUBAUTH_RESULT_OK)
            {
                LogError("EventHubAuthCBS_Refresh For Ext SAS Token Failed. Code:%u\r\n", auth_result);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
        else if (auth_status == EVENTHUBAUTH_STATUS_OK)
        {
            // check if we a ext token the client has requested to refresh
            if ((eventhub_client_ll->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT) && (eventhub_client_ll->ext_refresh_sas_token != NULL))
            {
                //**Codes_SRS_EVENTHUBCLIENT_LL_29_126: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
                auth_result = EventHubAuthCBS_Refresh(eventhub_client_ll->cbs_handle, eventhub_client_ll->ext_refresh_sas_token);
                if (auth_result != EVENTHUBAUTH_RESULT_OK)
                {
                    LogError("EventHubAuthCBS_Refresh For Ext SAS Token Failed. Code:%u\r\n", auth_result);
                    result = __LINE__;
                }
                else
                {
                    result = 0;
                }
                STRING_delete(eventhub_client_ll->ext_refresh_sas_token);
                eventhub_client_ll->ext_refresh_sas_token = NULL;
            }
            else
            {
                if (eventhub_client_ll->amqp_state == SENDER_AMQP_PENDING_AUTHORIZATION)
                {
                    eventhub_client_ll->amqp_state = SENDER_AMQP_PENDING_SENDER_CREATE;
                }
                result = 0;
            }
        }
        else
        {
            //**Codes_SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubClient_LL_DoWork.**\]**
            LogError("EventHubAuthCBS_Refresh Returned Invalid State. Status:%u\r\n", auth_status);
            result = __LINE__;
        }
    }

    return result;
}

static int initialize_uamqp_stack_common(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    int result;
    const char* host_name_temp;

    if ((host_name_temp = STRING_c_str(eventhub_client_ll->host_name)) == NULL)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_080: [If any other error happens while bringing up the uAMQP stack, EventHubClient_LL_DoWork shall not attempt to open the message_sender and return without sending any messages.] */
        result = __LINE__;
        LogError("Couldn't assemble target address");
    }
    else
    {
        const SASL_MECHANISM_INTERFACE_DESCRIPTION* sasl_mechanism_interface;
        const IO_INTERFACE_DESCRIPTION* tlsio_interface;
        const IO_INTERFACE_DESCRIPTION* saslclientio_interface;
        TLSIO_CONFIG tls_io_config;

        memset(&tls_io_config, 0, sizeof(tls_io_config));
        tls_io_config.hostname = host_name_temp;
        tls_io_config.port = 5671;

        //**Codes_SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
        if ((sasl_mechanism_interface = saslmssbcbs_get_interface()) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_006: [If saslmssbcbs_get_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("Cannot obtain SASL CBS interface.");
        }
        /* create SASL handler */
        //**Codes_SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
        else if ((eventhub_client_ll->sasl_mechanism_handle = saslmechanism_create(sasl_mechanism_interface, NULL)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_011: [If sasl_mechanism_create fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("saslmechanism_create failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_002: [The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.] */
        else if ((tlsio_interface = platform_get_default_tlsio()) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_001: [If platform_get_default_tlsio_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages. ] */
            result = __LINE__;
            LogError("Could not obtain default TLS IO.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_03_030: [A TLS IO shall be created by calling xio_create.] */
        else if ((eventhub_client_ll->tls_io = xio_create(tlsio_interface, &tls_io_config)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_003: [If xio_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("TLS IO creation failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_013: [The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.] */
        else if ((saslclientio_interface = saslclientio_get_interface_description()) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_014: [If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("TLS IO creation failed.");
        }
        else
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_015: [The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.] */
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_016: [The underlying_io members shall be set to the previously created TLS IO.] */
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_017: [The sasl_mechanism shall be set to the previously created SASL mechanism.] */
            SASLCLIENTIO_CONFIG sasl_io_config;
            sasl_io_config.underlying_io = eventhub_client_ll->tls_io;
            sasl_io_config.sasl_mechanism = eventhub_client_ll->sasl_mechanism_handle;

            /* Codes_SRS_EVENTHUBCLIENT_LL_01_012: [A SASL client IO shall be created by calling xio_create.] */
            if ((eventhub_client_ll->sasl_io = xio_create(saslclientio_interface, &sasl_io_config)) == NULL)
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_018: [If xio_create fails creating the SASL client IO then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
                result = __LINE__;
                LogError("SASL client IO creation failed.");
            }
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_019: [An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.] */
            else if ((eventhub_client_ll->connection = connection_create(eventhub_client_ll->sasl_io, host_name_temp, "eh_client_connection", NULL, NULL)) == NULL)
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_020: [If connection_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
                result = __LINE__;
                LogError("connection_create failed.");
            }
            else
            {
                //**Codes_SRS_EVENTHUBCLIENT_LL_29_115: \[**EventHubClient_LL_DoWork shall invoke connection_set_trace using the current value of the trace on boolean.**\]**
                connection_set_trace(eventhub_client_ll->connection, eventhub_client_ll->trace_on == 1 ? true : false);
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_028: [An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.] */
                if ((eventhub_client_ll->session = session_create(eventhub_client_ll->connection, NULL, NULL)) == NULL)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_029: [If session_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
                    result = __LINE__;
                    LogError("session_create failed.");
                }
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_030: [The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.] */
                else if (session_set_outgoing_window(eventhub_client_ll->session, 10) != 0)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_031: [If setting the outgoing window fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
                    result = __LINE__;
                    LogError("session_set_outgoing_window failed.");
                }
                else if (create_sas_token(eventhub_client_ll) != 0)
                {
                    result = __LINE__;
                    LogError("create_sas_token failed.");
                }
                else
                {
                    result = 0;
                }
            }
        }
    }

    if (result != 0)
    {
        destroy_uamqp_stack(eventhub_client_ll);
    }

    return result;
}

static int initialize_uamqp_sender_stack(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    int result;
    const char* target_address_str;

    if ((target_address_str = STRING_c_str(eventhub_client_ll->target_address)) == NULL)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_080: [If any other error happens while bringing up the uAMQP stack, EventHubClient_LL_DoWork shall not attempt to open the message_sender and return without sending any messages.] */
        result = __LINE__;
        LogError("cannot get the previously constructed target address.");
    }
    else
    {
        AMQP_VALUE source = NULL;
        AMQP_VALUE target = NULL;

        /* Codes_SRS_EVENTHUBCLIENT_LL_01_021: [A source AMQP value shall be created by calling messaging_create_source.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_022: [The source address shall be "ingress".] */
        if ((source = messaging_create_source("ingress")) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_025: [If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("messaging_create_source failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_023: [A target AMQP value shall be created by calling messaging_create_target.] */
        else if ((target = messaging_create_target(target_address_str)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_025: [If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("messaging_create_target failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_026: [An AMQP link shall be created by calling link_create and passing as arguments the session handle, "sender-link" as link name, role_sender and the previously created source and target values.] */
        else if ((eventhub_client_ll->link = link_create(eventhub_client_ll->session, "sender-link", role_sender, source, target)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_027: [If creating the link fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("link_create failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_032: [The link sender settle mode shall be set to unsettled by calling link_set_snd_settle_mode.] */
        else if (link_set_snd_settle_mode(eventhub_client_ll->link, sender_settle_mode_unsettled) != 0)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_033: [If link_set_snd_settle_mode fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("link_set_snd_settle_mode failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_034: [The message size shall be set to 256K by calling link_set_max_message_size.] */
        else if (link_set_max_message_size(eventhub_client_ll->link, AMQP_MAX_MESSAGE_SIZE) != 0)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_035: [If link_set_max_message_size fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("link_set_max_message_size failed.");
        }
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_036: [A message sender shall be created by calling messagesender_create and passing as arguments the link handle, a state changed callback, a context and NULL for the logging function.] */
        else if ((eventhub_client_ll->message_sender = messagesender_create(eventhub_client_ll->link, on_message_sender_state_changed, eventhub_client_ll)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_037: [If creating the message sender fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.] */
            result = __LINE__;
            LogError("messagesender_create failed.");
        }
        else
        {
            result = 0;
        }

        if (source != NULL)
        {
            amqpvalue_destroy(source);
        }

        if (target != NULL)
        {
            amqpvalue_destroy(target);
        }
    }

    if (result != 0)
    {
        destroy_uamqp_stack(eventhub_client_ll);
    }

    return result;
}

static int initialize_uamqp_stack(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    int result;

    if (eventhub_client_ll->amqp_state == SENDER_AMQP_UNINITIALIZED)
    {
        eventhub_client_ll->message_sender_state = MESSAGE_SENDER_STATE_IDLE;
        if (initialize_uamqp_stack_common(eventhub_client_ll) != 0)
        {
            result = __LINE__;
            LogError("Could Not Initialize Common AMQP Sender Stack.\r\n");
        }
        else
        {
            eventhub_client_ll->amqp_state = SENDER_AMQP_PENDING_AUTHORIZATION;
            result = 0;
        }
    }
    else if (eventhub_client_ll->amqp_state == SENDER_AMQP_PENDING_SENDER_CREATE)
    {
        if (initialize_uamqp_sender_stack(eventhub_client_ll) != 0)
        {
            result = __LINE__;
            LogError("Could Not Initialize Sender AMQP Connection Data.\r\n");
        }
        else
        {
            eventhub_client_ll->amqp_state = SENDER_AMQP_INITIALIZED;
            result = 0;
        }
    }
    else
    {
        LogError("Unexpected State:%u\r\n", eventhub_client_ll->amqp_state);
        result = __LINE__;
    }

    return result;
}

static void on_message_send_complete(void* context, MESSAGE_SEND_RESULT send_result, AMQP_VALUE delivery_state)
{
    PDLIST_ENTRY currentListEntry = (PDLIST_ENTRY)context;
    EVENTHUBCLIENT_CONFIRMATION_RESULT callback_confirmation_result;
    PEVENTHUB_EVENT_LIST currentEvent = containingRecord(currentListEntry, EVENTHUB_EVENT_LIST, entry);
    size_t index;

    (void)delivery_state;

    if (send_result == MESSAGE_SEND_OK)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_061: [When on_message_send_complete is called with MESSAGE_SEND_OK the pending message shall be indicated as sent correctly by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_OK.] */
        callback_confirmation_result = EVENTHUBCLIENT_CONFIRMATION_OK;
    }
    else
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_063: [When on_message_send_complete is called with a result code different than MESSAGE_SEND_OK the pending message shall be indicated as having an error by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_ERROR.]  */
        callback_confirmation_result = EVENTHUBCLIENT_CONFIRMATION_ERROR;
    }

    if (currentEvent->callback)
    {
        currentEvent->callback(callback_confirmation_result, currentEvent->context);
    }

    for (index = 0; index < currentEvent->eventCount; index++)
    {
        EventData_Destroy(currentEvent->eventDataList[index]);
    }

    /* Codes_SRS_EVENTHUBCLIENT_LL_01_062: [The pending message shall be removed from the pending list.] */
    DList_RemoveEntryList(currentListEntry);
    free(currentEvent->eventDataList);
    free(currentEvent);
}

static void eventhub_client_init(EVENTHUBCLIENT_LL* eventhub_client_ll)
{
    eventhub_client_ll->keyName = NULL;
    eventhub_client_ll->keyValue = NULL;
    eventhub_client_ll->event_hub_path = NULL;
    eventhub_client_ll->host_name = NULL;
    eventhub_client_ll->target_address = NULL;
    eventhub_client_ll->sender_publisher_id = NULL;
    eventhub_client_ll->ext_refresh_sas_token = NULL;
    eventhub_client_ll->ext_sas_token_parse_config = NULL;
    /* Codes_SRS_EVENTHUBCLIENT_LL_04_016: [EventHubClient_LL_CreateFromConnectionString shall initialize the pending list that will be used to send Events.] */
    DList_InitializeListHead(&(eventhub_client_ll->outgoingEvents));
    eventhub_client_ll->connection = NULL;
    eventhub_client_ll->session = NULL;
    eventhub_client_ll->link = NULL;
    eventhub_client_ll->sasl_mechanism_handle = NULL;
    eventhub_client_ll->sasl_io = NULL;
    eventhub_client_ll->tls_io = NULL;
    eventhub_client_ll->message_sender_state = MESSAGE_SENDER_STATE_IDLE;
    eventhub_client_ll->message_sender = NULL;
    eventhub_client_ll->state_change_cb = NULL;
    eventhub_client_ll->statuschange_callback_context = NULL;
    eventhub_client_ll->on_error_cb = NULL;
    eventhub_client_ll->error_callback_context = NULL;
    eventhub_client_ll->trace_on = 0;
    eventhub_client_ll->msg_timeout = 0;
    eventhub_client_ll->counter = NULL;
    eventhub_client_ll->cbs_handle = NULL;
    eventhub_client_ll->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_UNKNOWN;
    eventhub_client_ll->amqp_state = SENDER_AMQP_UNINITIALIZED;
}

static int validate_refresh_token_config(const EVENTHUBAUTH_CBS_CONFIG* cfg1, const EVENTHUBAUTH_CBS_CONFIG* cfg2)
{
    int result;
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_407: \[**EventHubClient_LL_RefreshSASTokenAsync shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubClient_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
    if (STRING_compare(cfg1->extSASTokenURI, cfg2->extSASTokenURI) != 0)
    {
        LogError("Token URIs mismatch.");
        result = -1;
    }
    else
    {
        result = 0;
    }

    return result;
}

EVENTHUBCLIENT_RESULT EventHubClient_LL_RefreshSASTokenAsync(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, const char* eventHubRefreshSasToken)
{
    EVENTHUBCLIENT_RESULT result;
    EVENTHUBAUTH_CBS_CONFIG* refresh_sas_token_cfg = NULL;
    EVENTHUBCLIENT_LL* eventhub_client_ll = (EVENTHUBCLIENT_LL*)eventHubClientLLHandle;

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_401: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubClientLLHandle or eventHubSasToken is NULL.**\]**
    if ((eventhub_client_ll == NULL) || (eventHubRefreshSasToken == NULL))
    {
        LogError("Invalid Arguments EventHubClient_LL_RefreshSASToken.");
        result = EVENTHUBCLIENT_INVALID_ARG;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_402: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if eventHubClientLLHandle credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
    else if (eventhub_client_ll->credential != EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        LogError("EventHubClient_LL_RefreshSASToken Not Permitted For Non EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.");
        result = EVENTHUBCLIENT_ERROR;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_403: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if AMQP stack is not fully initialized.**\]**
    else if (eventhub_client_ll->amqp_state != SENDER_AMQP_INITIALIZED)
    {
        LogError("Refresh Operation Not Valid.");
        result = EVENTHUBCLIENT_ERROR;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_404: \[**EventHubClient_LL_RefreshSASTokenAsync shall check if any prior refresh ext SAS token was applied, if so EVENTHUBCLIENT_ERROR shall be returned.**\]**
    else if (eventhub_client_ll->ext_refresh_sas_token != NULL)
    {
        LogError("Refresh Operation Not Permitted Because There is a Refresh Token in progress.");
        result = EVENTHUBCLIENT_ERROR;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_405: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
    else if ((refresh_sas_token_cfg = EventHubAuthCBS_SASTokenParse(eventHubRefreshSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_406: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
        LogError("Could Not Obtain Connection Parameters from EventHubAuthCBS_SASTokenParse.");
        result = EVENTHUBCLIENT_ERROR;
    }
    else if (validate_refresh_token_config(refresh_sas_token_cfg, eventhub_client_ll->ext_sas_token_parse_config) != 0)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_408: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR if eventHubRefreshSasToken is not compatible.**\]**
        LogError("Refresh SAS Token Incompatible with token used with EventHubClient_LL_CreateFromSASToken.");
        result = EVENTHUBCLIENT_ERROR;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_409: \[**EventHubClient_LL_RefreshSASTokenAsync shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in EventHubClient_LL_DoWork.**\]**
    else if ((eventhub_client_ll->ext_refresh_sas_token = STRING_construct(eventHubRefreshSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_411: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_ERROR on failure.**\]**
        LogError("Could Not Construct Refresh SAS Token.");
        result = EVENTHUBCLIENT_ERROR;
    }
    else
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_410: \[**EventHubClient_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_OK on success.**\]**
        result = EVENTHUBCLIENT_OK;
    }

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_412: \[**EventHubClient_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
    if (refresh_sas_token_cfg != NULL)
    {
        EventHubAuthCBS_Config_Destroy(refresh_sas_token_cfg);
    }

    return result;
}

static int eventhub_client_assemble_target_address(EVENTHUBCLIENT_LL* eventhub_client_ll, const char* event_hub_path, const char* publisher_id)
{
    int result;

    /* Codes_SRS_EVENTHUBCLIENT_LL_01_024: [The target address shall be "amqps://" {eventhub hostname} / {eventhub name}.] */
    if (((eventhub_client_ll->target_address = STRING_construct("amqps://")) == NULL) ||
        (STRING_concat_with_STRING(eventhub_client_ll->target_address, eventhub_client_ll->host_name) != 0) ||
        (STRING_concat(eventhub_client_ll->target_address, "/") != 0) ||
        (STRING_concat(eventhub_client_ll->target_address, event_hub_path) != 0) ||
        ((publisher_id != NULL) && (STRING_concat(eventhub_client_ll->target_address, "/publishers/") != 0)) ||
        ((publisher_id != NULL) && (STRING_concat(eventhub_client_ll->target_address, publisher_id) != 0)))
    {
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

EVENTHUBCLIENT_LL_HANDLE EventHubClient_LL_CreateFromConnectionString(const char* connectionString, const char* eventHubPath)
{
    EVENTHUBCLIENT_LL* eventhub_client_ll;
    STRING_HANDLE connection_string;

    /* Codes_SRS_EVENTHUBCLIENT_LL_05_001: [EventHubClient_LL_CreateFromConnectionString shall obtain the version string by a call to EventHubClient_GetVersionString.] */
    /* Codes_SRS_EVENTHUBCLIENT_LL_05_002: [EventHubClient_LL_CreateFromConnectionString shall print the version string to standard output.] */
    LogInfo("Event Hubs Client SDK for C, version %s", EventHubClient_GetVersionString());

    /* Codes_SRS_EVENTHUBCLIENT_LL_03_003: [EventHubClient_LL_CreateFromConnectionString shall return a NULL value if connectionString or eventHubPath is NULL.] */
    if (connectionString == NULL || eventHubPath == NULL)
    {
        LogError("Invalid arguments. connectionString=%p, eventHubPath=%p", connectionString, eventHubPath);
        eventhub_client_ll = NULL;
    }
    else if ((connection_string = STRING_construct(connectionString)) == NULL)
    {
        LogError("Error creating connection string handle.");
        eventhub_client_ll = NULL;
    }
    else
    {
        MAP_HANDLE connection_string_values_map;

        /* Codes_SRS_EVENTHUBCLIENT_LL_01_065: [The connection string shall be parsed to a map of strings by using connection_string_parser_parse.] */
        if ((connection_string_values_map = connectionstringparser_parse(connection_string)) == NULL)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_066: [If connection_string_parser_parse fails then EventHubClient_LL_CreateFromConnectionString shall fail and return NULL.] */
            LogError("Error parsing connection string.");
            eventhub_client_ll = NULL;
        }
        else
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_03_002: [EventHubClient_LL_CreateFromConnectionString shall allocate a new event hub client LL instance.] */
            /* Codes_SRS_EVENTHUBCLIENT_LL_03_016: [EventHubClient_LL_CreateFromConnectionString shall return a non-NULL handle value upon success.] */
            if ((eventhub_client_ll = (EVENTHUBCLIENT_LL*)malloc(sizeof(EVENTHUBCLIENT_LL))) == NULL)
            {
                LogError("Memory Allocation Failed for eventhub_client_ll.");
            }
            else
            {
                bool error;
                const char* value;
                const char* endpoint;

                eventhub_client_init(eventhub_client_ll);

                if ((eventhub_client_ll->counter = tickcounter_create()) == NULL)
                {
                    error = true;
                    LogError("failure creating tick counter handle.");
                }
                /* Codes_SRS_EVENTHUBCLIENT_LL_03_017: [EventHubClient_ll expects a service bus connection string in one of the following formats: Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[key name];SharedAccessKey=[key value] Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[key name];SharedAccessKey=[key value] ]*/
                else if ((endpoint = Map_GetValueFromKey(connection_string_values_map, "Endpoint")) == NULL)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
                    error = true;
                    LogError("Couldn't find endpoint in connection string");
                }
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_067: [The endpoint shall be looked up in the resulting map and used to construct the host name to be used for connecting by removing the sb://.] */
                else
                {
                    size_t hostname_length = strlen(endpoint);

                    if ((hostname_length > SB_STRING_LENGTH) && (endpoint[hostname_length - 1] == '/'))
                    {
                        hostname_length--;
                    }

                    if ((hostname_length <= SB_STRING_LENGTH) ||
                        (strncmp(endpoint, SB_STRING, SB_STRING_LENGTH) != 0))
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
                        error = true;
                        LogError("Couldn't create host name string");
                    }
                    else if ((eventhub_client_ll->host_name = STRING_construct_n(endpoint + SB_STRING_LENGTH, hostname_length - SB_STRING_LENGTH)) == NULL)
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
                        error = true;
                        LogError("Couldn't create host name string");
                    }
                    else if (((value = Map_GetValueFromKey(connection_string_values_map, "SharedAccessKeyName")) == NULL) ||
                        (strlen(value) == 0))
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
                        error = true;
                        LogError("Couldn't find key name in connection string");
                    }
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_068: [The key name and key shall be looked up in the resulting map and they should be stored as is for later use in connecting.] */
                    else if ((eventhub_client_ll->keyName = STRING_construct(value)) == NULL)
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
                        error = true;
                        LogError("Couldn't create key name string");
                    }
                    else if (((value = Map_GetValueFromKey(connection_string_values_map, "SharedAccessKey")) == NULL) ||
                        (strlen(value) == 0))
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_018: [EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.] */
                        error = true;
                        LogError("Couldn't find key in connection string");
                    }
                    else if ((eventhub_client_ll->keyValue = STRING_construct(value)) == NULL)
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_03_004: [For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.] */
                        error = true;
                        LogError("Couldn't create key string");
                    }
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_024: [The target address shall be amqps://{eventhub hostname}/{eventhub name}.] */
                    else if (eventhub_client_assemble_target_address(eventhub_client_ll, eventHubPath, NULL) != 0)
                    {
                        error = true;
                        LogError("Couldn't assemble target address");
                    }
                    else if ((eventhub_client_ll->event_hub_path = STRING_construct(eventHubPath)) == NULL)
                    {
                        error = true;
                        LogError("Couldn't create event hub path");
                    }
                    else
                    {
                        error = false;
                        eventhub_client_ll->sender_publisher_id = NULL;
                        eventhub_client_ll->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO;
                    }
                }

                if (error == true)
                {
                    if (eventhub_client_ll->counter)
                    {
                        tickcounter_destroy(eventhub_client_ll->counter);
                    }
                    if (eventhub_client_ll->host_name != NULL)
                    {
                        STRING_delete(eventhub_client_ll->host_name);
                    }
                    if (eventhub_client_ll->keyName != NULL)
                    {
                        STRING_delete(eventhub_client_ll->keyName);
                    }
                    if (eventhub_client_ll->keyValue != NULL)
                    {
                        STRING_delete(eventhub_client_ll->keyValue);
                    }
                    if (eventhub_client_ll->target_address != NULL)
                    {
                        STRING_delete(eventhub_client_ll->target_address);
                    }
                    if (eventhub_client_ll->event_hub_path)
                    {
                        STRING_delete(eventhub_client_ll->event_hub_path);
                    }
                    if (eventhub_client_ll->sender_publisher_id != NULL)
                    {
                        STRING_delete(eventhub_client_ll->sender_publisher_id);
                    }
                    free(eventhub_client_ll);
                    eventhub_client_ll = NULL;
                }
            }

            Map_Destroy(connection_string_values_map);
        }

        STRING_delete(connection_string);
    }

    return ((EVENTHUBCLIENT_LL_HANDLE)eventhub_client_ll);
}

EVENTHUBCLIENT_LL_HANDLE EventHubClient_LL_CreateFromSASToken(const char* eventHubSasToken)
{
    EVENTHUBCLIENT_LL* eventhub_client_ll;
    EVENTHUBAUTH_CBS_CONFIG* sas_token_parse_config = NULL;

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_300: \[**EventHubClient_LL_CreateFromSASToken shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_301: \[**EventHubClient_LL_CreateFromSASToken shall print the version string to standard output.**\]**
    LogInfo("Event Hubs Client SDK for C, version %s", EventHubClient_GetVersionString());

    //**Codes_SRS_EVENTHUBCLIENT_LL_29_302: \[**EventHubClient_LL_CreateFromSASToken shall return NULL if eventHubSasToken is NULL.**\]**
    if (eventHubSasToken == NULL)
    {
        LogError("Invalid argument. eventHubSasToken");
        eventhub_client_ll = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_303: \[**EventHubClient_LL_CreateFromSASToken parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
    else if ((sas_token_parse_config = EventHubAuthCBS_SASTokenParse(eventHubSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_304: \[**EventHubClient_LL_CreateFromSASToken shall fail if EventHubAuthCBS_SASTokenParse return NULL.**\]**
        LogError("Could Not Obtain Connection Parameters from EventHubAuthCBS_SASTokenParse.");
        eventhub_client_ll = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_305: \[**EventHubClient_LL_CreateFromSASToken shall check if sasTokenData mode is EVENTHUBAUTH_MODE_SENDER, if not, NULL is returned.**\]**
    else if (sas_token_parse_config->mode != EVENTHUBAUTH_MODE_SENDER)
    {
        LogError("Invalid Mode Obtained From SASToken. Mode:%u", sas_token_parse_config->mode);
        EventHubAuthCBS_Config_Destroy(sas_token_parse_config);
        eventhub_client_ll = NULL;
    }
    //**Codes_SRS_EVENTHUBCLIENT_LL_29_306: \[**EventHubClient_LL_CreateFromSASToken shall allocate a new event hub client LL instance.**\]**
    else if ((eventhub_client_ll = (EVENTHUBCLIENT_LL*)malloc(sizeof(EVENTHUBCLIENT_LL))) == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_308: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
        LogError("Could not allocate memory for EVENTHUBCLIENT_LL\r\n");
        EventHubAuthCBS_Config_Destroy(sas_token_parse_config);
    }
    else
    {
        bool is_error;
        const char *publisher_id, *event_hub_path;

        //**Codes_SRS_EVENTHUBCLIENT_LL_29_312: \[**EventHubClient_LL_CreateFromSASToken shall initialize connection tracing to false by default.**\]**
        eventhub_client_init(eventhub_client_ll);

        //**Codes_SRS_EVENTHUBCLIENT_LL_29_308: \[**EventHubClient_LL_CreateFromSASToken shall create a tick counter handle using API tickcounter_create.**\]**
        if ((eventhub_client_ll->counter = tickcounter_create()) == NULL)
        {
            LogError("Could not create tick counter\r\n");
            is_error = true;
        }
        else if ((publisher_id = STRING_c_str(sas_token_parse_config->senderPublisherId)) == NULL)
        {
            LogError("Could not obtain underlying string buffer of publisher id\r\n");
            is_error = true;
        }
        else if ((event_hub_path = STRING_c_str(sas_token_parse_config->eventHubPath)) == NULL)
        {
            LogError("Could not obtain underlying string buffer of event hub path\r\n");
            is_error = true;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_310: \[**EventHubClient_LL_CreateFromSASToken shall clone the senderPublisherId and eventHubPath strings using API STRING_Clone.**\]**
        else if ((eventhub_client_ll->sender_publisher_id = STRING_clone(sas_token_parse_config->senderPublisherId)) == NULL)
        {
            LogError("Could Not Clone Event Hub Path.\r\n");
            is_error = true;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_310: \[**EventHubClient_LL_CreateFromSASToken shall clone the senderPublisherId and eventHubPath strings using API STRING_Clone.**\]**
        else if ((eventhub_client_ll->event_hub_path = STRING_clone(sas_token_parse_config->eventHubPath)) == NULL)
        {
            LogError("Could Not Clone Event Hub Path.\r\n");
            is_error = true;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_309: \[**EventHubClient_LL_CreateFromSASToken shall clone the hostName string using API STRING_Clone.**\]**
        else if ((eventhub_client_ll->host_name = STRING_clone(sas_token_parse_config->hostName)) == NULL)
        {
            LogError("Could Not Clone Host Name.\r\n");
            is_error = true;
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_311: \[**EventHubClient_LL_CreateFromSASToken shall initialize sender target address using the eventHub and senderPublisherId with format amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]**
        else if (eventhub_client_assemble_target_address(eventhub_client_ll, event_hub_path, publisher_id) != 0)
        {
            is_error = true;
            LogError("Couldn't assemble target address");
        }
        else
        {
            //**Codes_SRS_EVENTHUBCLIENT_LL_29_313: \[**EventHubClient_LL_CreateFromSASToken shall return the allocated event hub client LL instance on success.**\]**
            is_error = false;
            eventhub_client_ll->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;
            eventhub_client_ll->ext_sas_token_parse_config = sas_token_parse_config;
        }
        if (is_error == true)
        {
            //**Codes_SRS_EVENTHUBCLIENT_LL_29_307: \[**EventHubClient_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations.**\]**
            if (eventhub_client_ll->counter)
            {
                tickcounter_destroy(eventhub_client_ll->counter);
            }
            if (eventhub_client_ll->sender_publisher_id)
            {
                STRING_delete(eventhub_client_ll->sender_publisher_id);
            }
            if (eventhub_client_ll->event_hub_path)
            {
                STRING_delete(eventhub_client_ll->event_hub_path);
            }
            if (eventhub_client_ll->host_name != NULL)
            {
                STRING_delete(eventhub_client_ll->host_name);
            }
            if (eventhub_client_ll->target_address)
            {
                STRING_delete(eventhub_client_ll->target_address);
            }
            free(eventhub_client_ll);
            eventhub_client_ll = NULL;
            EventHubAuthCBS_Config_Destroy(sas_token_parse_config);
        }
    }

    return ((EVENTHUBCLIENT_LL_HANDLE)eventhub_client_ll);
}

void EventHubClient_LL_Destroy(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    /* Codes_SRS_EVENTHUBCLIENT_LL_03_010: [If the eventhub_client_ll is NULL, EventHubClient_LL_Destroy shall not do anything.] */
    if (eventhub_client_ll != NULL)
    {
        PDLIST_ENTRY unsend;

        /* Codes_SRS_EVENTHUBCLIENT_LL_03_009: [EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_042: [The message sender shall be freed by calling messagesender_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_043: [The link shall be freed by calling link_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_044: [The session shall be freed by calling session_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_045: [The connection shall be freed by calling connection_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_046: [The SASL client IO shall be freed by calling xio_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_047: [The TLS IO shall be freed by calling xio_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_048: [The SASL plain mechanism shall be freed by calling saslmechanism_destroy.] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_29_153: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\] */
        destroy_uamqp_stack(eventhub_client_ll);

        /* Codes_SRS_EVENTHUBCLIENT_LL_01_081: [The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.] */
        STRING_delete(eventhub_client_ll->target_address);
        STRING_delete(eventhub_client_ll->host_name);
        STRING_delete(eventhub_client_ll->event_hub_path);
        if (eventhub_client_ll->sender_publisher_id != NULL)
        {
            STRING_delete(eventhub_client_ll->sender_publisher_id);
        }

        if (eventhub_client_ll->keyName)
        {
            STRING_delete(eventhub_client_ll->keyName);
        }
        if (eventhub_client_ll->keyValue)
        {
            STRING_delete(eventhub_client_ll->keyValue);
        }
        tickcounter_destroy(eventhub_client_ll->counter);

        /* Codes_SRS_EVENTHUBCLIENT_LL_03_009: \[**EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.**\] */
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
        while ((unsend = DList_RemoveHeadList(&(eventhub_client_ll->outgoingEvents))) != &(eventhub_client_ll->outgoingEvents))
        {
            EVENTHUB_EVENT_LIST* temp = containingRecord(unsend, EVENTHUB_EVENT_LIST, entry);
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_040: [All the pending messages shall be indicated as error by calling the associated callback with EVENTHUBCLIENT_CONFIRMATION_DESTROY.] */
            if (temp->callback != NULL)
            {
                temp->callback(EVENTHUBCLIENT_CONFIRMATION_DESTROY, temp->context);
            }

            /* Codes_SRS_EVENTHUBCLIENT_LL_01_041: [All pending message data shall be freed.] */
            for (size_t index = 0; index < temp->eventCount; index++)
            {
                EventData_Destroy(temp->eventDataList[index]);
            }

            free(temp->eventDataList);
            free(temp);
        }

        free(eventhub_client_ll);
    }
}

void EventHubClient_LL_SetMessageTimeout(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, size_t timeout_value)
{
    /* Codes_SRS_EVENTHUBCLIENT_LL_07_026: [ If eventHubClientLLHandle is NULL EventHubClient_LL_SetMessageTimeout shall do nothing. ] */
    if (eventHubClientLLHandle == NULL)
    {
        LogError("Invalid Argument eventHubClientLLHandle was specified");
    }
    else
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_07_027: [ EventHubClient_LL_SetMessageTimeout shall save the timeout_value. ] */
        EVENTHUBCLIENT_LL* ehClientLLData = (EVENTHUBCLIENT_LL*)eventHubClientLLHandle;
        ehClientLLData->msg_timeout = (uint64_t)timeout_value;
    }
}

EVENTHUBCLIENT_RESULT EventHubClient_LL_SetStateChangeCallback(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTHUB_CLIENT_STATECHANGE_CALLBACK state_change_cb, void* userContextCallback)
{
    EVENTHUBCLIENT_RESULT result;
    if (eventHubClientLLHandle == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_016: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetStateChangeCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
        LogError("Invalid Argument eventHubClientLLHandle was specified");
        result = EVENTHUBCLIENT_INVALID_ARG;
    }
    else
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_017: [** If state_change_cb is non-NULL then EventHubClient_LL_SetStateChangeCallback shall call state_change_cb when a state changes is encountered. **]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_018: [** If state_change_cb is NULL EventHubClient_LL_SetStateChangeCallback shall no longer call state_change_cb on state changes. **]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_019: [** If EventHubClient_LL_SetStateChangeCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**
        EVENTHUBCLIENT_LL* ehClientLLData = (EVENTHUBCLIENT_LL*)eventHubClientLLHandle;
        ehClientLLData->state_change_cb = state_change_cb;
        ehClientLLData->statuschange_callback_context = userContextCallback;
        result = EVENTHUBCLIENT_OK;
    }
    return result;
}

EVENTHUBCLIENT_RESULT EventHubClient_LL_SetErrorCallback(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTHUB_CLIENT_ERROR_CALLBACK on_error_cb, void* userContextCallback)
{
    EVENTHUBCLIENT_RESULT result;
    if (eventHubClientLLHandle == NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_020: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetErrorCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
        LogError("Invalid Argument eventHubClientLLHandle was specified");
        result = EVENTHUBCLIENT_INVALID_ARG;
    }
    else
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_021: [** If failure_cb is non-NULL EventHubClient_LL_SetErrorCallback shall execute the on_error_cb on failures with a EVENTHUBCLIENT_FAILURE_RESULT. **]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_022: [** If failure_cb is NULL EventHubClient_LL_SetErrorCallback shall no longer call on_error_cb on failure. **]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_023: [** If EventHubClient_LL_SetErrorCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**
        EVENTHUBCLIENT_LL* ehClientLLData = (EVENTHUBCLIENT_LL*)eventHubClientLLHandle;
        ehClientLLData->on_error_cb = on_error_cb;
        ehClientLLData->error_callback_context = userContextCallback;
        result = EVENTHUBCLIENT_OK;
    }
    return result;
}

void EventHubClient_LL_SetLogTrace(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, bool log_trace_on)
{
    if (eventHubClientLLHandle != NULL)
    {
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_024: [** If eventHubClientLLHandle is non-NULL EventHubClient_LL_SetLogTrace shall call the uAmqp trace function with the log_trace_on. **]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_07_025: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetLogTrace shall do nothing. **]**
        EVENTHUBCLIENT_LL* ehClientLLData = (EVENTHUBCLIENT_LL*)eventHubClientLLHandle;
        ehClientLLData->trace_on = log_trace_on ? 1 : 0;
    }
}

EVENTHUBCLIENT_RESULT EventHubClient_LL_SendAsync(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll, EVENTDATA_HANDLE eventDataHandle, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK sendAsyncConfirmationCallback, void* userContextCallback)
{
   EVENTHUBCLIENT_RESULT result;

    /* Codes_SRS_EVENTHUBCLIENT_LL_04_011: [EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_INVALID_ARG if parameter eventhub_client_ll or eventDataHandle is NULL.] */
    /* Codes_SRS_EVENTHUBCLIENT_LL_04_012: [EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_INVALID_ARG if parameter telemetryConfirmationCallBack is NULL and userContextCallBack is not NULL.] */
    if (eventhub_client_ll == NULL || eventDataHandle == NULL || (sendAsyncConfirmationCallback == NULL && userContextCallback != NULL))
    {
        result = EVENTHUBCLIENT_INVALID_ARG;
        LOG_ERROR(result);
    }
    else
    {
        EVENTHUB_EVENT_LIST* newEntry = (EVENTHUB_EVENT_LIST*)malloc(sizeof(EVENTHUB_EVENT_LIST));
        if (newEntry == NULL)
        {
            result = EVENTHUBCLIENT_ERROR;
            LOG_ERROR(result);
        }
        else
        {
            newEntry->currentStatus = WAITING_TO_BE_SENT;
            newEntry->eventCount = 1;
            newEntry->eventDataList = (EVENTDATA_HANDLE*)malloc(sizeof(EVENTDATA_HANDLE));

            if (newEntry->eventDataList == NULL)
            {
                result = EVENTHUBCLIENT_ERROR;
                free(newEntry);
                LOG_ERROR(result);
            }
            else
            {
                (void)tickcounter_get_current_ms(eventhub_client_ll->counter, &newEntry->idle_timer);

                /* Codes_SRS_EVENTHUBCLIENT_LL_04_013: [EventHubClient_LL_SendAsync shall add the DLIST outgoingEvents a new record cloning the information from eventDataHandle, telemetryConfirmationCallback and userContextCallBack.] */
                if ((newEntry->eventDataList[0] = EventData_Clone(eventDataHandle)) == NULL)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_04_014: [If cloning and/or adding the information fails for any reason, EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
                    result = EVENTHUBCLIENT_ERROR;
                    free(newEntry->eventDataList);
                    free(newEntry);
                    LOG_ERROR(result);
                }
                else
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_04_013: [EventHubClient_LL_SendAsync shall add the DLIST outgoingEvents a new record cloning the information from eventDataHandle, telemetryConfirmationCallback and userContextCallBack.] */
                    newEntry->callback = sendAsyncConfirmationCallback;
                    newEntry->context = userContextCallback;
                    DList_InsertTailList(&(eventhub_client_ll->outgoingEvents), &(newEntry->entry));
                    /* Codes_SRS_EVENTHUBCLIENT_LL_04_015: [Otherwise EventHubClient_LL_SendAsync shall succeed and return EVENTHUBCLIENT_OK.] */
                    result = EVENTHUBCLIENT_OK;
                }
            }
        }
    }

    return result;
}

EVENTHUBCLIENT_RESULT EventHubClient_LL_SendBatchAsync(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll, EVENTDATA_HANDLE* eventDataList, size_t count, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK sendAsyncConfirmationCallback, void* userContextCallback)
{
    EVENTHUBCLIENT_RESULT result;
    /* Codes_SRS_EVENTHUBCLIENT_LL_07_012: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALLID_ARG if eventhubClientLLHandle or eventDataList are NULL or if sendAsnycConfirmationCallback equals NULL and userContextCallback does not equal NULL.] */
    /* Codes_SRS_EVENTHUBCLIENT_LL_01_095: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if the count argument is zero.] */
    if (eventhub_client_ll == NULL || eventDataList == NULL || count == 0 || (sendAsyncConfirmationCallback == NULL && userContextCallback != NULL))
    {
        result = EVENTHUBCLIENT_INVALID_ARG;
        LOG_ERROR(result);
    }
    else
    {
        size_t index;
        if (ValidateEventDataList(eventDataList, count) != 0)
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_096: [If the partitionKey properties on the events in the batch are not the same then EventHubClient_LL_SendBatchAsync shall fail and return EVENTHUBCLIENT_ERROR.] */
            result = EVENTHUBCLIENT_ERROR;
            LOG_ERROR(result);
        }
        else
        {
            EVENTHUB_EVENT_LIST *newEntry = (EVENTHUB_EVENT_LIST*)malloc(sizeof(EVENTHUB_EVENT_LIST));
            if (newEntry == NULL)
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.] */
                result = EVENTHUBCLIENT_ERROR;
                LOG_ERROR(result);
            }
            else
            {
                newEntry->currentStatus = WAITING_TO_BE_SENT;
                newEntry->eventCount = count;
                (void)tickcounter_get_current_ms(eventhub_client_ll->counter, &newEntry->idle_timer);

                newEntry->eventDataList = (EVENTDATA_HANDLE*)malloc(sizeof(EVENTDATA_HANDLE)*count);
                if (newEntry->eventDataList == NULL)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.] */
                    free(newEntry);
                    result = EVENTHUBCLIENT_ERROR;
                    LOG_ERROR(result);
                }
                else
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_07_014: [EventHubClient_LL_SendBatchAsync shall clone each item in the eventDataList by calling EventData_Clone.] */
                    for (index = 0; index < newEntry->eventCount; index++)
                    {
                        if ( (newEntry->eventDataList[index] = EventData_Clone(eventDataList[index])) == NULL)
                        {
                            break;
                        }
                    }

                    if (index < newEntry->eventCount)
                    {
                        for (size_t i = 0; i < index; i++)
                        {
                            EventData_Destroy(newEntry->eventDataList[i]);
                        }
                        /* Codes_SRS_EVENTHUBCLIENT_LL_07_013: [EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.] */
                        result = EVENTHUBCLIENT_ERROR;
                        free(newEntry->eventDataList);
                        free(newEntry);
                        LOG_ERROR(result);
                    }
                    else
                    {
                        EVENTHUBCLIENT_LL* ehClientLLData = (EVENTHUBCLIENT_LL*)eventhub_client_ll;
                        newEntry->callback = sendAsyncConfirmationCallback;
                        newEntry->context = userContextCallback;
                        DList_InsertTailList(&(ehClientLLData->outgoingEvents), &(newEntry->entry));
                        /* Codes_SRS_EVENTHUBCLIENT_LL_07_015: [On success EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_OK.] */
                        result = EVENTHUBCLIENT_OK;
                    }
                }
            }
        }
    }
    return result;
}

static int encode_callback(void* context, const unsigned char* bytes, size_t length)
{
    EVENT_DATA_BINARY* message_body_binary = (EVENT_DATA_BINARY*)context;
    (void)memcpy(message_body_binary->bytes + message_body_binary->length, bytes, length);
    message_body_binary->length += length;
    return 0;
}

static int create_properties_map(EVENTDATA_HANDLE event_data_handle, AMQP_VALUE* uamqp_properties_map)
{
    int result;
    MAP_HANDLE properties_map;
    const char* const* property_keys;
    const char* const* property_values;
    size_t property_count;

    if ((properties_map = EventData_Properties(event_data_handle)) == NULL)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
        LogError("Cannot get the properties map.");
        result = __LINE__;
    }
    else if (Map_GetInternals(properties_map, &property_keys, &property_values, &property_count) != MAP_OK)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
        LogError("Cannot get the properties map.");
        result = __LINE__;
    }
    else
    {
        if (property_count == 0)
        {
            *uamqp_properties_map = NULL;
            result = 0;
        }
        else
        {
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_054: [If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.] */
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_055: [A map shall be created to hold the application properties by calling amqpvalue_create_map.] */
            *uamqp_properties_map = amqpvalue_create_map();
            if (*uamqp_properties_map == NULL)
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                LogError("Cannot build uAMQP properties map.");
                result = __LINE__;
            }
            else
            {
                size_t i;

                for (i = 0; i < property_count; i++)
                {
                    AMQP_VALUE property_key;
                    AMQP_VALUE property_value;

                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_056: [For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.] */
                    if ((property_key = amqpvalue_create_string(property_keys[i])) == NULL)
                    {
                        break;
                    }

                    if ((property_value = amqpvalue_create_string(property_values[i])) == NULL)
                    {
                        amqpvalue_destroy(property_key);
                        break;
                    }

                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_057: [Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.] */
                    if (amqpvalue_set_map_value(*uamqp_properties_map, property_key, property_value) != 0)
                    {
                        amqpvalue_destroy(property_key);
                        amqpvalue_destroy(property_value);
                        break;
                    }

                    amqpvalue_destroy(property_key);
                    amqpvalue_destroy(property_value);
                }

                if (i < property_count)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                    LogError("Could not fill all properties in the uAMQP properties map.");
                    amqpvalue_destroy(*uamqp_properties_map);
                    result = __LINE__;
                }
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_058: [The resulting map shall be set as the message application properties by calling message_set_application_properties.] */
                else
                {
                    result = 0;
                }
            }
        }
    }

    return result;
}

int create_batch_message(MESSAGE_HANDLE message, EVENTDATA_HANDLE* event_data_list, size_t event_count)
{
    int result = 0;
    size_t index;

    /* Codes_SRS_EVENTHUBCLIENT_LL_01_082: [If the number of event data entries for the message is greater than 1 (batched) then the message format shall be set to 0x80013700 by calling message_set_message_format.] */
    if (message_set_message_format(message, 0x80013700) != 0)
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_083: [If message_set_message_format fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
        LogError("Failed setting the message format to MS message format.");
        result = __LINE__;
    }
    else
    {
        /* Codes_SRS_EVENTHUBCLIENT_LL_01_084: [For each event in the batch:] */
        for (index = 0; index < event_count; index++)
        {
            BINARY_DATA payload;

            if (EventData_GetData(event_data_list[index], &payload.bytes, &payload.length) != EVENTDATA_OK)
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                break;
            }
            else
            {
                AMQP_VALUE uamqp_properties_map;
                bool is_error = false;

                /* create the properties map */
                if (create_properties_map(event_data_list[index], &uamqp_properties_map) != 0)
                {
                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                    is_error = true;
                }
                else
                {
                    data bin_data;
                    bin_data.bytes = payload.bytes;
                    bin_data.length = (uint32_t)payload.length;

                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_088: [The event payload shall be serialized as an AMQP message data section.] */
                    AMQP_VALUE data_value = amqpvalue_create_data(bin_data);
                    if (data_value == NULL)
                    {
                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                        is_error = true;
                    }
                    else
                    {
                        size_t payload_length = 0;
                        size_t temp_length = 0;
                        AMQP_VALUE application_properties = NULL;

                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_093: [If the property count is 0 for an event part of the batch, then no property map shall be serialized for that event.] */
                        if (uamqp_properties_map != NULL)
                        {
                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_087: [The properties shall be serialized as AMQP application_properties.] */
                            application_properties = amqpvalue_create_application_properties(uamqp_properties_map);
                            if (application_properties == NULL)
                            {
                                is_error = true;
                            }
                            else
                            {
                                /* Codes_SRS_EVENTHUBCLIENT_LL_01_091: [The size needed for the properties and data section shall be obtained by calling amqpvalue_get_encoded_size.] */
                                if (amqpvalue_get_encoded_size(application_properties, &temp_length) != 0)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    is_error = true;
                                }
                                else
                                {
                                    payload_length += temp_length;
                                }
                            }
                        }

                        if (!is_error)
                        {
                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_091: [The size needed for the properties and data section shall be obtained by calling amqpvalue_get_encoded_size.] */
                            if (amqpvalue_get_encoded_size(data_value, &temp_length) != 0)
                            {
                                /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                is_error = true;
                            }
                            else
                            {
                                EVENT_DATA_BINARY event_data_binary;

                                payload_length += temp_length;

                                event_data_binary.length = 0;
                                /* Codes_SRS_EVENTHUBCLIENT_LL_01_090: [Enough memory shall be allocated to hold the properties and binary payload for each event part of the batch.] */
                                event_data_binary.bytes = (unsigned char*)malloc(payload_length);
                                if (event_data_binary.bytes == NULL)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    is_error = true;
                                }
                                else
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_092: [The properties and binary data shall be encoded by calling amqpvalue_encode and passing an encoding function that places the encoded data into the memory allocated for the event.] */
                                    if (((uamqp_properties_map != NULL) && (amqpvalue_encode(application_properties, &encode_callback, &event_data_binary) != 0)) ||
                                        (amqpvalue_encode(data_value, &encode_callback, &event_data_binary) != 0))
                                    {
                                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_094: [If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                        is_error = true;
                                    }
                                    else
                                    {
                                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_085: [The event shall be added to the message by into a separate data section by calling message_add_body_amqp_data.] */
                                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_086: [The buffer passed to message_add_body_amqp_data shall contain the properties and the binary event payload serialized as AMQP values.] */
                                        BINARY_DATA body_binary_data;
                                        body_binary_data.bytes = event_data_binary.bytes;
                                        body_binary_data.length = event_data_binary.length;
                                        if (message_add_body_amqp_data(message, body_binary_data) != 0)
                                        {
                                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_089: [If message_add_body_amqp_data fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                            is_error = true;
                                        }
                                    }

                                    free(event_data_binary.bytes);
                                }
                            }
                        }

                        if (application_properties != NULL)
                        {
                            amqpvalue_destroy(application_properties);
                        }
                        if (uamqp_properties_map != NULL)
                        {
                            amqpvalue_destroy(uamqp_properties_map);
                        }

                        amqpvalue_destroy(data_value);
                    }
                }

                if (is_error)
                {
                    break;
                }
            }
        }

        if (index < event_count)
        {
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

void EventHubClient_LL_DoWork(EVENTHUBCLIENT_LL_HANDLE eventhub_client_ll)
{
    /* Codes_SRS_EVENTHUBCLIENT_LL_04_018: [if parameter eventhub_client_ll is NULL EventHubClient_LL_DoWork shall immediately return.]   */
    if (eventhub_client_ll != NULL)
    {
        int error_code;
        bool is_timeout = false, is_authentication_in_progress = false;

        /* Codes_SRS_EVENTHUBCLIENT_LL_01_079: [EventHubClient_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already been brought up:] */
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_202: \[**EventHubClient_LL_DoWork shall initialize the uAMQP Message Sender stack if it has not already been brought up. **\]**
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_202: \[**EventHubClient_LL_DoWork shall initialize the uAMQP Message Sender stack if it has not already brought up. **\]**
        if (((eventhub_client_ll->amqp_state == SENDER_AMQP_UNINITIALIZED) ||
            (eventhub_client_ll->amqp_state == SENDER_AMQP_PENDING_SENDER_CREATE)) &&
            ((error_code = initialize_uamqp_stack(eventhub_client_ll)) != 0))
        {
            LogError("Error initializing uAMPQ sender stack. Code:%d Status:%u\r\n", error_code, eventhub_client_ll->amqp_state);
            if (eventhub_client_ll->on_error_cb != NULL)
            {
                eventhub_client_ll->on_error_cb(EVENTHUBCLIENT_AMQP_INIT_FAILURE, eventhub_client_ll->error_callback_context);
            }
        }
        else if (eventhub_client_ll->message_sender_state == MESSAGE_SENDER_STATE_ERROR)
        {
            destroy_uamqp_stack(eventhub_client_ll);
        }
        //**Codes_SRS_EVENTHUBCLIENT_LL_29_201: \[**EventHubClient_LL_DoWork shall perform SAS token handling. **\]**
        else if ((error_code = handle_sas_token_auth(eventhub_client_ll, &is_timeout, &is_authentication_in_progress)) != 0)
        {
            LogError("Error Seen handle_sas_token_auth Code:%d\r\n", error_code);
            if (eventhub_client_ll->on_error_cb != NULL)
            {
                eventhub_client_ll->on_error_cb(EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE, eventhub_client_ll->error_callback_context);
            }
            destroy_uamqp_stack(eventhub_client_ll);
        }
        else if (is_timeout == true)
        {
            LogError("Authorization Timeout Observed\r\n");
            if (eventhub_client_ll->on_error_cb != NULL)
            {
                eventhub_client_ll->on_error_cb(EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT, eventhub_client_ll->error_callback_context);
            }
            destroy_uamqp_stack(eventhub_client_ll);
        }
        else if (is_authentication_in_progress == true)
        {
            connection_dowork(eventhub_client_ll->connection);
        }
        else if (eventhub_client_ll->amqp_state == SENDER_AMQP_INITIALIZED)
        {
            //**Codes_SRS_EVENTHUBCLIENT_LL_29_203: \[**EventHubClient_LL_DoWork shall perform message send handling **\]**
            /* Codes_SRS_EVENTHUBCLIENT_LL_01_038: [EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.] */
            if ((eventhub_client_ll->message_sender_state == MESSAGE_SENDER_STATE_IDLE) && (messagesender_open(eventhub_client_ll->message_sender) != 0))
            {
                /* Codes_SRS_EVENTHUBCLIENT_LL_01_039: [If messagesender_open fails, no further actions shall be carried out.] */
                LogError("Error opening message sender.");
            }
            else
            {
                PDLIST_ENTRY currentListEntry;

                currentListEntry = eventhub_client_ll->outgoingEvents.Flink;
                while (currentListEntry != &(eventhub_client_ll->outgoingEvents))
                {
                    PEVENTHUB_EVENT_LIST currentEvent = containingRecord(currentListEntry, EVENTHUB_EVENT_LIST, entry);
                    PDLIST_ENTRY next_list_entry = currentListEntry->Flink;

                    if (currentEvent->currentStatus == WAITING_TO_BE_SENT)
                    {
                        bool has_timeout = false;
                        bool is_error = false;

                        /* Codes_SRS_EVENTHUBCLIENT_LL_01_049: [If the message has not yet been given to uAMQP then a new message shall be created by calling message_create.] */
                        MESSAGE_HANDLE message = message_create();
                        if (message == NULL)
                        {
                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_070: [If creating the message fails, then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                            LogError("Error creating the uAMQP message.");
                            is_error = true;
                        }
                        else
                        {
                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_050: [If the number of event data entries for the message is 1 (not batched) then the message body shall be set to the event data payload by calling message_add_body_amqp_data.] */
                            if (currentEvent->eventCount == 1)
                            {
                                BINARY_DATA body;
                                AMQP_VALUE properties_map;

                                /* Codes_SRS_EVENTHUBCLIENT_LL_01_051: [The pointer to the payload and its length shall be obtained by calling EventData_GetData.] */
                                if (EventData_GetData(currentEvent->eventDataList[0], &body.bytes, &body.length) != EVENTDATA_OK)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_052: [If EventData_GetData fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    LogError("Error getting event data.");
                                    is_error = true;
                                }
                                else if (message_add_body_amqp_data(message, body) != 0)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_071: [If message_add_body_amqp_data fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    LogError("Cannot get the message_add_body_amqp_data.");
                                    is_error = true;
                                }
                                else if (add_partition_key_to_message(message, currentEvent->eventDataList[0]) != 0)
                                {
                                    LogError("Cannot add partition key.");
                                    is_error = true;
                                }
                                else if (create_properties_map(currentEvent->eventDataList[0], &properties_map) != 0)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    is_error = true;
                                }
                                else
                                {
                                    if (properties_map != NULL)
                                    {
                                        if (message_set_application_properties(message, properties_map) != 0)
                                        {
                                            /* Codes_SRS_EVENTHUBCLIENT_LL_01_059: [If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                            LogError("Could not set message application properties on the message.");
                                            is_error = true;
                                        }

                                        amqpvalue_destroy(properties_map);
                                    }
                                }
                            }
                            else
                            {
                                if (create_batch_message(message, currentEvent->eventDataList, currentEvent->eventCount) != 0)
                                {
                                    is_error = true;
                                    LogError("Failed creating batch message.");
                                }
                            }

                            if (!is_error)
                            {
                                currentEvent->currentStatus = WAITING_FOR_ACK;

                                tickcounter_ms_t current_time;
                                (void)tickcounter_get_current_ms(eventhub_client_ll->counter, &current_time);
                                if (eventhub_client_ll->msg_timeout > 0 && ((current_time-currentEvent->idle_timer)/1000) > eventhub_client_ll->msg_timeout)
                                {
                                    has_timeout = true;
                                }
                                /* Codes_SRS_EVENTHUBCLIENT_LL_01_069: [The AMQP message shall be given to uAMQP by calling messagesender_send_async, while passing as arguments the message sender handle, the message handle, a callback function and its context.] */
                                else if (messagesender_send_async(eventhub_client_ll->message_sender, message, on_message_send_complete, currentListEntry, 0) == NULL)
                                {
                                    /* Codes_SRS_EVENTHUBCLIENT_LL_01_053: [If messagesender_send_async failed then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.] */
                                    is_error = true;
                                    LogError("messagesender_send_async failed.");
                                }
                            }

                            message_destroy(message);
                        }

                        if (has_timeout)
                        {
                            size_t index;

                            /* Codes_SRS_EVENTHUBCLIENT_LL_07_028: [If the message idle time is greater than the msg_timeout, EventHubClient_LL_DoWork shall call callback with EVENTHUBCLIENT_CONFIRMATION_TIMEOUT.] */
                            currentEvent->callback(EVENTHUBCLIENT_CONFIRMATION_TIMEOUT, currentEvent->context);
                            for (index = 0; index < currentEvent->eventCount; index++)
                            {
                                EventData_Destroy(currentEvent->eventDataList[index]);
                            }

                            DList_RemoveEntryList(currentListEntry);
                            free(currentEvent->eventDataList);
                            free(currentEvent);
                        }
                        else if (is_error)
                        {
                            size_t index;

                            currentEvent->callback(EVENTHUBCLIENT_CONFIRMATION_ERROR, currentEvent->context);

                            for (index = 0; index < currentEvent->eventCount; index++)
                            {
                                EventData_Destroy(currentEvent->eventDataList[index]);
                            }

                            DList_RemoveEntryList(currentListEntry);
                            free(currentEvent->eventDataList);
                            free(currentEvent);
                        }
                    }

                    currentListEntry = next_list_entry;
                }

                /* Codes_SRS_EVENTHUBCLIENT_LL_01_064: [EventHubClient_LL_DoWork shall call connection_dowork while passing as argument the connection handle obtained in EventHubClient_LL_Create.] */
                connection_dowork(eventhub_client_ll->connection);
            }
        }
    }
}
