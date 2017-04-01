// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "azure_c_shared_utility/base64.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/connection_string_parser.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/gballoc.h"
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
#include "azure_uamqp_c/message_receiver.h"
#include "azure_uamqp_c/saslclientio.h"
#include "azure_uamqp_c/sasl_plain.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"
#include "azure_uamqp_c/session.h"

#include "eventhubauth.h"
#include "eventhubreceiver_ll.h"
#include "version.h"

/* Event Hub Receiver States */
#define EVENTHUBRECEIVER_STATE_VALUES       \
        RECEIVER_STATE_INACTIVE,            \
        RECEIVER_STATE_INACTIVE_PENDING,    \
        RECEIVER_STATE_ACTIVE

DEFINE_ENUM(EVENTHUBRECEIVER_STATE, EVENTHUBRECEIVER_STATE_VALUES);

/* Event Hub Receiver AMQP States */
#define EVENTHUBRECEIVER_AMQP_STATE_VALUES      \
        RECEIVER_AMQP_UNINITIALIZED,            \
        RECEIVER_AMQP_PENDING_AUTHORIZATION,    \
        RECEIVER_AMQP_PENDING_RECEIVER_CREATE,  \
        RECEIVER_AMQP_INITIALIZED

DEFINE_ENUM(EVENTHUBRECEIVER_AMQP_STATE, EVENTHUBRECEIVER_AMQP_STATE_VALUES);

/* Default TLS port */
#define TLS_PORT                    5671

#define AUTH_EXPIRATION_SECS        (60 * 60)
#define AUTH_REFRESH_SECS           (48 * 60)

/* length for sb:// */
static const char SB_STRING[] = "sb://";
#define SB_STRING_LENGTH        ((sizeof(SB_STRING) / sizeof(SB_STRING[0])) - 1)

/* AMPQ payload max size */
#define AMQP_MAX_MESSAGE_SIZE       (256*1024)

typedef struct EVENTHUBCONNECTIONPARAMS_TAG
{
    STRING_HANDLE hostName;
    STRING_HANDLE eventHubPath;
    STRING_HANDLE eventHubSharedAccessKeyName;
    STRING_HANDLE eventHubSharedAccessKey;
    STRING_HANDLE targetAddress;
    STRING_HANDLE consumerGroup;
    STRING_HANDLE partitionId;
    EVENTHUBAUTH_CREDENTIAL_TYPE credential;
    EVENTHUBAUTH_CBS_CONFIG* extSASTokenConfig;
} EVENTHUBCONNECTIONPARAMS_STRUCT;

typedef struct EVENTHUBAMQPSTACK_STRUCT_TAG
{
    CONNECTION_HANDLE       connection;
    SESSION_HANDLE          session;
    LINK_HANDLE             link;
    XIO_HANDLE              saslIO;
    XIO_HANDLE              tlsIO;
    SASL_MECHANISM_HANDLE   saslMechanismHandle;
    MESSAGE_RECEIVER_HANDLE messageReceiver;
    EVENTHUBAUTH_CBS_HANDLE cbsHandle;
} EVENTHUBAMQPSTACK_STRUCT;

typedef struct EVENTHUBRECEIVER_LL_CALLBACK_TAG
{
    EVENTHUBRECEIVER_ASYNC_CALLBACK onEventReceiveCallback;
    void* onEventReceiveUserContext;
    volatile int wasMessageReceived;
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK onEventReceiveErrorCallback;
    void* onEventReceiveErrorUserContext;
    EVENTHUBRECEIVER_ASYNC_END_CALLBACK onEventReceiveEndCallback;
    void* onEventReceiveEndUserContext;
} EVENTHUBRECEIVER_LL_CALLBACK_STRUCT;

typedef struct EVENTHUBRECEIVER_LL_STRUCT_TAG
{
    EVENTHUBCONNECTIONPARAMS_STRUCT     connectionParams;
    EVENTHUBAMQPSTACK_STRUCT            receiverAMQPConnection;
    EVENTHUBRECEIVER_AMQP_STATE         receiverAMQPConnectionState;
    EVENTHUBRECEIVER_STATE              state;
    EVENTHUBRECEIVER_LL_CALLBACK_STRUCT callback;
    STRING_HANDLE                       receiverQueryFilter;
    STRING_HANDLE                       extRefreshSASToken;
    uint64_t            startTimestamp;
    tickcounter_ms_t    lastActivityTimestamp;
    unsigned int        waitTimeoutInMs;
    TICK_COUNTER_HANDLE tickCounter;
    bool                connectionTracing;
} EVENTHUBRECEIVER_LL_STRUCT;

// Forward Declarations
static void EventHubConnectionAMQP_ConnectionDoWork(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL);
static void EventHubConnectionParams_DeInitialize(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams);
static int EventHubConnectionParams_Initialize
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    const char* connectionString,
    const char* eventHubPath,
    const char* consumerGroup,
    const char* partitionId
);

static const char* EventHubConnectionParams_GetHostName(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams)
{
    return STRING_c_str(eventHubConnectionParams->hostName);
}

static const char* EventHubConnectionParams_GetSharedAccessKeyName(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams)
{
    return STRING_c_str(eventHubConnectionParams->eventHubSharedAccessKeyName);
}

static const char* EventHubConnectionParams_GetSharedAccessKey(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams)
{
    return STRING_c_str(eventHubConnectionParams->eventHubSharedAccessKey);
}

static const char* EventHubConnectionParams_GetTargetAddress(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams)
{
    return STRING_c_str(eventHubConnectionParams->targetAddress);
}

static int EventHubConnectionParams_CreateHostName
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    MAP_HANDLE connectionStringValuesMap
)
{
    int result;
    const char* endpoint;
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_111: \[**EventHubReceiver_LL_Create shall lookup the endpoint in the resulting map using API Map_GetValueFromKey and argument "Endpoint".**\]**
    if ((endpoint = Map_GetValueFromKey(connectionStringValuesMap, "Endpoint")) == NULL)
    {
        LogError("Couldn't find endpoint in connection string.\r\n");
        result = __LINE__;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_112: \[**EventHubReceiver_LL_Create shall obtain the host name after parsing characters after substring "sb://".**\]**
        size_t hostnameLength = strlen(endpoint);
        if ((hostnameLength > SB_STRING_LENGTH) && (endpoint[hostnameLength - 1] == '/'))
        {
            hostnameLength--;
        }
        if ((hostnameLength <= SB_STRING_LENGTH) ||
            (strncmp(endpoint, SB_STRING, SB_STRING_LENGTH) != 0))
        {
            LogError("Invalid Host Name String\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_113: \[**EventHubReceiver_LL_Create shall create a host name STRING_HANDLE using API STRING_construct_n and the host name substring and its length obtained above as parameters.**\]**
        else if ((eventHubConnectionParams->hostName = STRING_construct_n(endpoint + SB_STRING_LENGTH, hostnameLength - SB_STRING_LENGTH)) == NULL)
        {
            LogError("Couldn't create host name string\r\n");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

static int CreateSasToken(EVENTHUBAMQPSTACK_STRUCT* eventHubCommStack, EVENTHUBCONNECTIONPARAMS_STRUCT* connectionParams, unsigned int waitTimeInSecs)
{
    int result;

    if (connectionParams->extSASTokenConfig == NULL)
    {
        EVENTHUBAUTH_CBS_CONFIG cfg;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_521: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a EVENTHUBAUTH_CBS_CONFIG structure params hostName, eventHubPath, receiverConsumerGroup, receiverPartitionId, sharedAccessKeyName, sharedAccessKey using previously set values. Set senderPublisherId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_RECEIVER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
        cfg.hostName = connectionParams->hostName;
        cfg.eventHubPath = connectionParams->eventHubPath;
        cfg.receiverConsumerGroup = connectionParams->consumerGroup;
        cfg.receiverPartitionId = connectionParams->partitionId;
        cfg.senderPublisherId = NULL;
        cfg.sharedAccessKeyName = connectionParams->eventHubSharedAccessKeyName;
        cfg.sharedAccessKey = connectionParams->eventHubSharedAccessKey;
        cfg.sasTokenAuthFailureTimeoutInSecs = waitTimeInSecs;
        cfg.sasTokenExpirationTimeInSec = AUTH_EXPIRATION_SECS;
        cfg.sasTokenRefreshPeriodInSecs = AUTH_REFRESH_SECS;
        cfg.mode = EVENTHUBAUTH_MODE_RECEIVER;
        cfg.credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO;
        cfg.extSASToken = NULL;
        cfg.extSASTokenURI = NULL;
        cfg.extSASTokenExpTSInEpochSec = 0;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_524: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
        eventHubCommStack->cbsHandle = EventHubAuthCBS_Create(&cfg, eventHubCommStack->session);
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_522: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the EVENTHUBAUTH_CBS_CONFIG obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
        connectionParams->extSASTokenConfig->sasTokenAuthFailureTimeoutInSecs = waitTimeInSecs;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_524: \[**EventHubAuthCBS_Create shall be invoked using the config structure reference and the session handle created earlier.**\]**
        eventHubCommStack->cbsHandle = EventHubAuthCBS_Create(connectionParams->extSASTokenConfig, eventHubCommStack->session);
    }

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_525: \[**If EventHubAuthCBS_Create returns NULL, a log message will be logged and the function returns immediately.**\]**
    if (eventHubCommStack->cbsHandle == NULL)
    {
        LogError("Couldn't create CBS based Authorization Handle\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

static int HandleSASTokenAuth(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL, bool* isTimeout, bool *isAuthInProgress)
{
    int result;
    EVENTHUBAUTH_STATUS authStatus;
    EVENTHUBAUTH_RESULT authResult;

    *isTimeout = false;
    *isAuthInProgress = false;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_541: \[**EventHubAuthCBS_GetStatus shall be invoked to obtain the authorization status.**\]**
    if ((authResult = EventHubAuthCBS_GetStatus(eventHubReceiverLL->receiverAMQPConnection.cbsHandle, &authStatus)) != EVENTHUBAUTH_RESULT_OK)
    {
        LogError("EventHubAuthCBS_GetStatus Failed. Code:%u\r\n", authResult);
        result = __LINE__;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
        if (authStatus == EVENTHUBAUTH_STATUS_FAILURE)
        {
            LogError("EventHubAuthCBS Status Failed.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_EXPIRED)
        {
            LogError("EventHubAuthCBS Status Expired.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_543: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT and EventHubReceiver_LL_DoWork shall bring down AMQP stack so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_TIMEOUT)
        {
            *isTimeout = true;
            LogError("EventHubAuthCBS Authentication Status Timed Out.\r\n");
            result = 0;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_544: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, connection_dowork shall be invoked to perform work to establish/refresh the SAS token.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_IN_PROGRESS)
        {
            *isAuthInProgress = true;
            result = 0;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_545: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_REFRESH_REQUIRED)
        {
            authResult = EventHubAuthCBS_Refresh(eventHubReceiverLL->receiverAMQPConnection.cbsHandle, NULL);
            if (authResult != EVENTHUBAUTH_RESULT_OK)
            {
                LogError("EventHubAuthCBS_Refresh Failed. Code:%u\r\n", authResult);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_546: \[**If status is EVENTHUBAUTH_STATUS_IDLE, EventHubAuthCBS_Authenticate shall be invoked to create and install the SAS token.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_IDLE)
        {
            *isAuthInProgress = true;
            authResult = EventHubAuthCBS_Authenticate(eventHubReceiverLL->receiverAMQPConnection.cbsHandle);

            if (authResult != EVENTHUBAUTH_RESULT_OK)
            {
                LogError("EventHubAuthCBS_Refresh For Ext SAS Token Failed. Code:%u\r\n", authResult);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_547: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  EventHubAuthCBS_Refresh shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
        else if (authStatus == EVENTHUBAUTH_STATUS_OK)
        {
            if ((eventHubReceiverLL->connectionParams.credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT) && (eventHubReceiverLL->extRefreshSASToken != NULL))
            {
                authResult = EventHubAuthCBS_Refresh(eventHubReceiverLL->receiverAMQPConnection.cbsHandle, eventHubReceiverLL->extRefreshSASToken);
                if (authResult != EVENTHUBAUTH_RESULT_OK)
                {
                    LogError("EventHubAuthCBS_Refresh For Ext SAS Token Failed. Code:%u\r\n", authResult);
                    result = __LINE__;
                }
                else
                {
                    result = 0;
                }
                STRING_delete(eventHubReceiverLL->extRefreshSASToken);
                eventHubReceiverLL->extRefreshSASToken = NULL;
            }
            else
            {
                if (eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_PENDING_AUTHORIZATION)
                {
                    eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_PENDING_RECEIVER_CREATE;
                }
                result = 0;
            }
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
        else
        {
            LogError("EventHubAuthCBS_Refresh Returned Invalid State. Status:%u\r\n", authStatus);
            result = __LINE__;
        }
    }

    return result;
}

static int EventHubConnectionParams_CommonInit
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    const char* connectionString,
    const char* eventHubPath
)
{
    int result;
    MAP_HANDLE connectionStringValuesMap = NULL;
    STRING_HANDLE connectionStringHandle = NULL;

    /* **Codes_SRS_EVENTHUBRECEIVER_LL_29_107: \[**EventHubReceiver_LL_Create shall expect a service bus connection string in one of the following formats:
        Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
        Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] **\]**
    */
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_108: \[**EventHubReceiver_LL_Create shall create a temp connection STRING_HANDLE using connectionString as the parameter.**\]**
    if ((connectionStringHandle = STRING_construct(connectionString)) == NULL)
    {
        LogError("Error creating connection string handle.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_109: \[**EventHubReceiver_LL_Create shall parse the connection string handle to a map of strings by using API connection_string_parser_parse.**\]**
    else if ((connectionStringValuesMap = connectionstringparser_parse(connectionStringHandle)) == NULL)
    {
        LogError("Error parsing connection string.\r\n");
        result = __LINE__;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_110: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument eventHubPath.**\]**
        if ((eventHubConnectionParams->eventHubPath = STRING_construct(eventHubPath)) == NULL)
        {
            LogError("Failure allocating eventHubPath string.");
            result = __LINE__;
        }
        else
        {
            const char* value;
            if (EventHubConnectionParams_CreateHostName(eventHubConnectionParams, connectionStringValuesMap) != 0)
            {
                LogError("Couldn't create host name from connection string.\r\n");
                result = __LINE__;
            }
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_114: \[**EventHubReceiver_LL_Create shall lookup the SharedAccessKeyName in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKeyName".**\]**
            else if (((value = Map_GetValueFromKey(connectionStringValuesMap, "SharedAccessKeyName")) == NULL) ||
                 (strlen(value) == 0))
            {
                LogError("Couldn't find SharedAccessKeyName in connection string\r\n");
                result = __LINE__;
            }
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_115: \[**EventHubReceiver_LL_Create shall create SharedAccessKeyName STRING_HANDLE using API STRING_construct and using the "SharedAccessKeyName" key's value obtained above as parameter.**\]**
            else if ((eventHubConnectionParams->eventHubSharedAccessKeyName = STRING_construct(value)) == NULL)
            {
                LogError("Couldn't create SharedAccessKeyName string\r\n");
                result = __LINE__;
            }
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_116: \[**EventHubReceiver_LL_Create shall lookup the SharedAccessKey in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKey".**\]**
            else if (((value = Map_GetValueFromKey(connectionStringValuesMap, "SharedAccessKey")) == NULL) ||
                (strlen(value) == 0))
            {
                LogError("Couldn't find SharedAccessKey in connection string\r\n");
                result = __LINE__;
            }
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_117: \[**EventHubReceiver_LL_Create shall create SharedAccessKey STRING_HANDLE using API STRING_construct and using the "SharedAccessKey" key's value obtained above as parameter.**\]**
            else if ((eventHubConnectionParams->eventHubSharedAccessKey = STRING_construct(value)) == NULL)
            {
                LogError("Couldn't create SharedAccessKey string\r\n");
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
    }

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_118: \[**EventHubReceiver_LL_Create shall destroy the map handle using API Map_Destroy.**\]**
    if (connectionStringValuesMap != NULL)
    {
        Map_Destroy(connectionStringValuesMap);
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_119: \[**EventHubReceiver_LL_Create shall destroy the temp connection string handle using API STRING_delete.**\]**
    if (connectionStringHandle != NULL)
    {
        STRING_delete(connectionStringHandle);
    }

    return result;
}

static int EventHubConnectionParams_InitReceiverPartitionURI
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams
)
{
    int result;

    // Format - {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}
    if (((eventHubConnectionParams->targetAddress = STRING_clone(eventHubConnectionParams->eventHubPath)) == NULL) ||
        (STRING_concat(eventHubConnectionParams->targetAddress, "/ConsumerGroups/") != 0) ||
        (STRING_concat_with_STRING(eventHubConnectionParams->targetAddress, eventHubConnectionParams->consumerGroup) != 0) ||
        (STRING_concat(eventHubConnectionParams->targetAddress, "/Partitions/") != 0) ||
        (STRING_concat_with_STRING(eventHubConnectionParams->targetAddress, eventHubConnectionParams->partitionId) != 0))
    {
        LogError("Couldn't assemble target URI\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

static int EventHubConnectionParams_ReceiverInit
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    const char* consumerGroup,
    const char* partitionId
)
{
    int result;
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_120: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument consumerGroup.**\]**
    if ((eventHubConnectionParams->consumerGroup = STRING_construct(consumerGroup)) == NULL)
    {
        LogError("Failure allocating consumerGroup string.");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_121: \[**EventHubReceiver_LL_Create shall create a STRING_HANDLE using API STRING_construct for holding the argument partitionId.**\]**
    else if ((eventHubConnectionParams->partitionId = STRING_construct(partitionId)) == NULL)
    {
        LogError("Failure allocating partitionId string.");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_122: \[**EventHubReceiver_LL_Create shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
    else if ((result = EventHubConnectionParams_InitReceiverPartitionURI(eventHubConnectionParams)) != 0)
    {
        LogError("Couldn't create receiver target partition URI.\r\n");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

static int EventHubConnectionParams_Initialize
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    const char* connectionString,
    const char* eventHubPath,
    const char* consumerGroup,
    const char* partitionId
)
{
    int result;

    {
        eventHubConnectionParams->hostName = NULL;
        eventHubConnectionParams->eventHubPath = NULL;
        eventHubConnectionParams->eventHubSharedAccessKeyName = NULL;
        eventHubConnectionParams->eventHubSharedAccessKey = NULL;
        eventHubConnectionParams->targetAddress = NULL;
        eventHubConnectionParams->consumerGroup = NULL;
        eventHubConnectionParams->partitionId = NULL;
        eventHubConnectionParams->extSASTokenConfig = NULL;
        eventHubConnectionParams->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO;

        if (EventHubConnectionParams_CommonInit(eventHubConnectionParams, connectionString, eventHubPath) != 0)
        {
            LogError("Could Not Initialize Common Connection Parameters.\r\n");
            EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
            result = __LINE__;
        }
        else if (EventHubConnectionParams_ReceiverInit(eventHubConnectionParams, consumerGroup, partitionId) != 0)
        {
            LogError("Could Not Initialize Receiver Connection Parameters.\r\n");
            EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }

    return result;
}

static int EventHubConnectionParams_InitializeSASToken
(
    EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams,
    EVENTHUBAUTH_CBS_CONFIG* sasTokenConfig
)
{
    int result, errorCode;

    eventHubConnectionParams->hostName = NULL;
    eventHubConnectionParams->eventHubPath = NULL;
    eventHubConnectionParams->eventHubSharedAccessKeyName = NULL;
    eventHubConnectionParams->eventHubSharedAccessKey = NULL;
    eventHubConnectionParams->targetAddress = NULL;
    eventHubConnectionParams->consumerGroup = NULL;
    eventHubConnectionParams->partitionId = NULL;
    eventHubConnectionParams->extSASTokenConfig = NULL;
    eventHubConnectionParams->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_160: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the hostName string using API STRING_clone.**\]**
    if ((eventHubConnectionParams->hostName = STRING_clone(sasTokenConfig->hostName)) == NULL)
    {
        LogError("Could Not Clone Host Name.\r\n");
        EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_161: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the eventHubPath string using API STRING_clone.**\]**
    else if ((eventHubConnectionParams->eventHubPath = STRING_clone(sasTokenConfig->eventHubPath)) == NULL)
    {
        LogError("Could Not Clone Event Hub Path.\r\n");
        EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_162: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the consumerGroup string using API STRING_clone.**\]**
    else if ((eventHubConnectionParams->consumerGroup = STRING_clone(sasTokenConfig->receiverConsumerGroup)) == NULL)
    {
        LogError("Could Not Clone Consumer Group.\r\n");
        EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_163: \[**EventHubReceiver_LL_CreateFromSASToken shall clone the receiverPartitionId string using API STRING_clone.**\]**
    else if ((eventHubConnectionParams->partitionId = STRING_clone(sasTokenConfig->receiverPartitionId)) == NULL)
    {
        LogError("Could Not Clone Partition ID.\r\n");
        EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_164: \[**EventHubReceiver_LL_CreateFromSASToken shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
    else if ((errorCode = EventHubConnectionParams_InitReceiverPartitionURI(eventHubConnectionParams)) != 0)
    {
        LogError("Couldn't create receiver target partition URI. Code:%d\r\n", errorCode);
        EventHubConnectionParams_DeInitialize(eventHubConnectionParams);
        result = __LINE__;
    }
    else
    {
        eventHubConnectionParams->extSASTokenConfig = sasTokenConfig;
        result = 0;
    }

    return result;
}

static void EventHubConnectionParams_DeInitialize(EVENTHUBCONNECTIONPARAMS_STRUCT* eventHubConnectionParams)
{
    if (eventHubConnectionParams->hostName != NULL)
    {
        STRING_delete(eventHubConnectionParams->hostName);
        eventHubConnectionParams->hostName = NULL;
    }
    if (eventHubConnectionParams->eventHubPath != NULL)
    {
        STRING_delete(eventHubConnectionParams->eventHubPath);
        eventHubConnectionParams->eventHubPath = NULL;
    }
    if (eventHubConnectionParams->eventHubSharedAccessKeyName != NULL)
    {
        STRING_delete(eventHubConnectionParams->eventHubSharedAccessKeyName);
        eventHubConnectionParams->eventHubSharedAccessKeyName = NULL;
    }
    if (eventHubConnectionParams->eventHubSharedAccessKey != NULL)
    {
        STRING_delete(eventHubConnectionParams->eventHubSharedAccessKey);
        eventHubConnectionParams->eventHubSharedAccessKey = NULL;
    }
    if (eventHubConnectionParams->targetAddress != NULL)
    {
        STRING_delete(eventHubConnectionParams->targetAddress);
        eventHubConnectionParams->targetAddress = NULL;
    }
    if (eventHubConnectionParams->consumerGroup != NULL)
    {
        STRING_delete(eventHubConnectionParams->consumerGroup);
        eventHubConnectionParams->consumerGroup = NULL;
    }
    if (eventHubConnectionParams->partitionId != NULL)
    {
        STRING_delete(eventHubConnectionParams->partitionId);
        eventHubConnectionParams->partitionId = NULL;
    }
    if (eventHubConnectionParams->extSASTokenConfig != NULL)
    {
        EventHubAuthCBS_Config_Destroy(eventHubConnectionParams->extSASTokenConfig);
        eventHubConnectionParams->extSASTokenConfig = NULL;
    }
}

void EventHubAMQP_DeInitializeStack(EVENTHUBAMQPSTACK_STRUCT* eventHubCommStack)
{
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_602: \[**All pending message data not reported to the calling client shall be freed by calling messagereceiver_close and messagereceiver_destroy.**\]**
    if (eventHubCommStack->messageReceiver != NULL)
    {
        if (messagereceiver_close(eventHubCommStack->messageReceiver) != 0)
        {
            LogError("Failed closing the AMQP message receiver.");
        }
        messagereceiver_destroy(eventHubCommStack->messageReceiver);
        eventHubCommStack->messageReceiver = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_603: \[**The link shall be freed by calling link_destroy.**\]**
    if (eventHubCommStack->link != NULL)
    {
        link_destroy(eventHubCommStack->link);
        eventHubCommStack->link = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_604: \[**EventHubAuthCBS_Destroy shall be called to destroy the event hub auth handle.**\]**
    if (eventHubCommStack->cbsHandle != NULL)
    {
        EventHubAuthCBS_Destroy(eventHubCommStack->cbsHandle);
        eventHubCommStack->cbsHandle = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_605: \[**The session shall be freed by calling session_destroy.**\]**
    if (eventHubCommStack->session != NULL)
    {
        session_destroy(eventHubCommStack->session);
        eventHubCommStack->session = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_606: \[**The connection shall be freed by calling connection_destroy.**\]**
    if (eventHubCommStack->connection != NULL)
    {
        connection_destroy(eventHubCommStack->connection);
        eventHubCommStack->connection = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_607: \[**The SASL client IO shall be freed by calling xio_destroy.**\]**
    if (eventHubCommStack->saslIO != NULL)
    {
        xio_destroy(eventHubCommStack->saslIO);
        eventHubCommStack->saslIO = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_608: \[**The TLS IO shall be freed by calling xio_destroy.**\]**
    if (eventHubCommStack->tlsIO != NULL)
    {
        xio_destroy(eventHubCommStack->tlsIO);
        eventHubCommStack->tlsIO = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_609: \[**The SASL mechanism shall be freed by calling saslmechanism_destroy.**\]**
    if (eventHubCommStack->saslMechanismHandle != NULL)
    {
        saslmechanism_destroy(eventHubCommStack->saslMechanismHandle);
        eventHubCommStack->saslMechanismHandle = NULL;
    }
}

static int EventHubAMQP_InitializeStackCommon
(
    EVENTHUBAMQPSTACK_STRUCT* eventHubCommStack,
    EVENTHUBCONNECTIONPARAMS_STRUCT* connectionParams,
    bool connectionTracing,
    unsigned int waitTimeInMs,
    const char* connectionName
)
{
    int result;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_501: \[**The SASL interface to be passed into saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
    const SASL_MECHANISM_INTERFACE_DESCRIPTION* saslMechanismInterface = saslmssbcbs_get_interface();
    if (saslMechanismInterface == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_503: \[**If saslmssbcbs_get_interface fails then a log message will be logged and the function returns immediately.**\]**
        LogError("Cannot obtain SASL CBS interface.\r\n");
        result = __LINE__;
    }
    else
    {
        const char* hostName;
        TLSIO_CONFIG tlsIOConfig;
        const IO_INTERFACE_DESCRIPTION* saslClientIOInterface;
        const IO_INTERFACE_DESCRIPTION* tlsioInterface;

        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_502: \[**A SASL mechanism shall be created by calling saslmechanism_create with the interface obtained above.**\]**
        eventHubCommStack->saslMechanismHandle = saslmechanism_create(saslMechanismInterface, NULL);
        if (eventHubCommStack->saslMechanismHandle == NULL)
        {
            LogError("saslmechanism_create failed.\r\n");
            result = __LINE__;
        }
        else
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_504: \[**A TLS IO shall be created by calling xio_create using TLS port 5671 and host name obtained from the connection string**\]**
            hostName = EventHubConnectionParams_GetHostName(connectionParams);
            tlsIOConfig.hostname = hostName;
            tlsIOConfig.port = TLS_PORT;
            tlsIOConfig.underlying_io_interface = NULL;
            tlsIOConfig.underlying_io_parameters = NULL;

            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_505: \[**The interface passed to xio_create shall be obtained by calling platform_get_default_tlsio.**\]**
            if ((tlsioInterface = platform_get_default_tlsio()) == NULL)
            {
                LogError("Could not obtain default TLS IO.\r\n");
                result = __LINE__;
            }
            else if ((eventHubCommStack->tlsIO = xio_create(tlsioInterface, &tlsIOConfig)) == NULL)
            {
                //**Codes_SRS_EVENTHUBRECEIVER_LL_29_506: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**
                LogError("TLS IO creation failed.\r\n");
                result = __LINE__;
            }
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_507: \[**The SASL client IO interface shall be obtained using saslclientio_get_interface_description**\]**
            else if ((saslClientIOInterface = saslclientio_get_interface_description()) == NULL)
            {
                //**Codes_SRS_EVENTHUBRECEIVER_LL_29_516: \[**If saslclientio_get_interface_description returns NULL, a log message will be logged and the function returns immediately.**\]**
                LogError("SASL Client Info IO creation failed.\r\n");
                result = __LINE__;
            }
            else
            {
                //**Codes_SRS_EVENTHUBRECEIVER_LL_29_508: \[**A SASL client IO shall be created by calling xio_create using TLS IO interface created previously and the SASL mechanism created earlier. The SASL client IO interface to be used will be the one obtained above.**\]**
                SASLCLIENTIO_CONFIG saslIOConfig = { eventHubCommStack->tlsIO, eventHubCommStack->saslMechanismHandle };
                if ((eventHubCommStack->saslIO = xio_create(saslClientIOInterface, &saslIOConfig)) == NULL)
                {
                    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_509: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**
                    LogError("SASL client IO creation failed.\r\n");
                    result = __LINE__;
                }
                //**Codes_SRS_EVENTHUBRECEIVER_LL_29_510: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle created previously, hostname, connection name and NULL for the new session handler end point and context.**\]**
                else if ((eventHubCommStack->connection = connection_create(eventHubCommStack->saslIO, hostName, connectionName, NULL, NULL)) == NULL)
                {
                    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_511: \[**If connection_create fails then a log message will be logged and the function returns immediately.**\]**
                    LogError("connection_create failed.\r\n");
                    result = __LINE__;
                }
                else
                {
                    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_512: \[**Connection tracing shall be called with the current value of the tracing flag**\]**
                    connection_set_trace(eventHubCommStack->connection, connectionTracing);
                    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_513: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
                    if ((eventHubCommStack->session = session_create(eventHubCommStack->connection, NULL, NULL)) == NULL)
                    {
                        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_514: \[**If session_create fails then a log message will be logged and the function returns immediately.**\]**
                        LogError("session_create failed.\r\n");
                        result = __LINE__;
                    }
                    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_515: \[**Configure the session incoming window by calling session_set_incoming_window and set value to INTMAX.**\]**
                    else if (session_set_incoming_window(eventHubCommStack->session, INT_MAX) != 0)
                    {
                        LogError("session_set_outgoing_window failed.\r\n");
                        result = __LINE__;
                    }
                    else if (CreateSasToken(eventHubCommStack, connectionParams, waitTimeInMs/1000) != 0)
                    {
                        result = __LINE__;
                        LogError("CreateSasToken failed.");
                    }
                    else
                    {
                        result = 0;
                    }
                }
            }
        }
    }
    if (result != 0)
    {
        EventHubAMQP_DeInitializeStack(eventHubCommStack);
    }
    return result;
}

static int EventHubAMQP_InitializeStackReceiver
(
    EVENTHUBAMQPSTACK_STRUCT* eventHubCommStack,
    EVENTHUBCONNECTIONPARAMS_STRUCT* connectionParams,
    ON_MESSAGE_RECEIVER_STATE_CHANGED onStateChangeCB,
    void* onStateChangeCBContext,
    ON_MESSAGE_RECEIVED onMsgReceivedCB,
    void* onMsgReceivedCBContext,
    STRING_HANDLE filter
)
{
    static const char filterName[] = "apache.org:selector-filter:string";
    int result;
    AMQP_VALUE source = NULL;
    AMQP_VALUE target = NULL;
    filter_set filterSet = NULL;
    AMQP_VALUE filterKey = NULL;
    AMQP_VALUE descriptor = NULL;
    AMQP_VALUE filterValue = NULL;
    AMQP_VALUE describedFilterValue = NULL;
    SOURCE_HANDLE sourceHandle = NULL;
    AMQP_VALUE addressValue = NULL;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_561: \[**A filter_set shall be created and initialized using key "apache.org:selector-filter:string" and value as the query filter created previously.**\]**
    if ((filterSet = amqpvalue_create_map()) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed creating filter map with filter\r\n");
        result = __LINE__;
    }
    else if ((filterKey = amqpvalue_create_symbol(filterName)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed creating filter key for filter %s", filterName);
        result = __LINE__;
    }
    else if ((descriptor = amqpvalue_create_symbol(filterName)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed creating filter descriptor for filter %s", filterName);
        result = __LINE__;
    }
    else if ((filterValue = amqpvalue_create_string(STRING_c_str(filter))) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed creating filterValue for query filter %s", STRING_c_str(filter));
        result = __LINE__;
    }
    else if ((describedFilterValue = amqpvalue_create_described(descriptor, filterValue)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed creating describedFilterValue with %s filter.", filterName);
        result = __LINE__;
    }
    else if (amqpvalue_set_map_value(filterSet, filterKey, describedFilterValue) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
        LogError("Failed amqpvalue_set_map_value.");
        result = __LINE__;
    }
    else
    {
        const char* receiveAddress = EventHubConnectionParams_GetTargetAddress(connectionParams);

        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_565: \[**The message receiver 'source' shall be created using source_create.**\]**
        if ((sourceHandle = source_create()) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed source_create.");
            result = __LINE__;
        }
        else if ((addressValue = amqpvalue_create_string(receiveAddress)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed to create AMQP string for receive address.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_569: \[**The message receiver link 'source' address shall be initialized using the partition target address created earlier.**\]**
        else if (source_set_address(sourceHandle, addressValue) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed source_set_address.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_568: \[**The message receiver link 'source' filter shall be initialized by calling source_set_filter and using the filter_set created earlier.**\]**
        else if (source_set_filter(sourceHandle, filterSet) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed source_set_filter.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_567: \[**The message receiver link 'source' shall be created using API amqpvalue_create_source**\]**
        else if ((source = amqpvalue_create_source(sourceHandle)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed to create source.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_570: \[**The message receiver link target shall be created using messaging_create_target with address obtained from the partition target address created earlier.**\]**
        else if ((target = messaging_create_target(receiveAddress)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_564: \[**If messaging_create_target fails, then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed creating target for link.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_566: \[**An AMQP link for the to be created message receiver shall be created by calling link_create with role as role_receiver and name as "receiver-link"**\]**
        else if ((eventHubCommStack->link = link_create(eventHubCommStack->session, "receiver-link", role_receiver, source, target)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_571: \[**If link_create fails then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed creating link.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_572: \[**Configure the link settle mode by calling link_set_rcv_settle_mode and set value to receiver_settle_mode_first.**\]**
        else if (link_set_rcv_settle_mode(eventHubCommStack->link, receiver_settle_mode_first) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_573: \[**If link_set_rcv_settle_mode fails then a log message will be logged and the function returns immediately.**\]**
            LogError("Failed setting link receive settle mode.");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_574: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]**
        else if (link_set_max_message_size(eventHubCommStack->link, AMQP_MAX_MESSAGE_SIZE) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_575: \[**If link_set_max_message_size fails then a log message will be logged and the function returns immediately.**\]**
            LogError("link_set_max_message_size failed.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_576: \[**A message receiver shall be created by calling messagereceiver_create and passing as arguments the link handle, a state changed callback and context.**\]**
        else if ((eventHubCommStack->messageReceiver = messagereceiver_create(eventHubCommStack->link, onStateChangeCB, onStateChangeCBContext)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_577: \[**If messagereceiver_create fails then a log message will be logged and the function returns immediately.**\]**
            LogError("messagesender_create failed.\r\n");
            result = __LINE__;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_578: \[**The created message receiver shall be transitioned to OPEN by calling messagereceiver_open.**\]**
        else if (messagereceiver_open(eventHubCommStack->messageReceiver, onMsgReceivedCB, onMsgReceivedCBContext) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_579: \[**If messagereceiver_open fails then a log message will be logged and the function returns immediately.**\]**
            LogError("Error opening message receiver.\r\n");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    if (sourceHandle != NULL) source_destroy(sourceHandle);
    if (addressValue != NULL) amqpvalue_destroy(addressValue);
    if (filterKey != NULL) amqpvalue_destroy(filterKey);
    if (target != NULL) amqpvalue_destroy(target);
    if (source != NULL) amqpvalue_destroy(source);
    if (describedFilterValue != NULL) amqpvalue_destroy(describedFilterValue);
    if (filterSet != NULL) amqpvalue_destroy(filterSet);

    if (result != 0)
    {
        EventHubAMQP_DeInitializeStack(eventHubCommStack);
    }

    return result;
}

void static EventHubConnectionAMQP_Reset(EVENTHUBAMQPSTACK_STRUCT* receiverAMQPConnection)
{
    receiverAMQPConnection->connection = NULL;
    receiverAMQPConnection->link = NULL;
    receiverAMQPConnection->messageReceiver = NULL;
    receiverAMQPConnection->saslIO = NULL;
    receiverAMQPConnection->saslMechanismHandle = NULL;
    receiverAMQPConnection->session = NULL;
    receiverAMQPConnection->tlsIO = NULL;
    receiverAMQPConnection->cbsHandle = NULL;
}

static int EventHubConnectionAMQP_ReceiverInitialize
(
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL,
    ON_MESSAGE_RECEIVER_STATE_CHANGED onStateChangeCB,
    void* onStateChangeCBContext,
    ON_MESSAGE_RECEIVED onMsgReceivedCB,
    void* onMsgReceivedCBContext
)
{
    static const char receiverConnectionName[] = "eh_receiver_connection";
    int result, errorCode;

    if (eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_UNINITIALIZED)
    {
        EventHubConnectionAMQP_Reset(&eventHubReceiverLL->receiverAMQPConnection);
        if ((errorCode = EventHubAMQP_InitializeStackCommon(&eventHubReceiverLL->receiverAMQPConnection,
                                                            &eventHubReceiverLL->connectionParams,
                                                            eventHubReceiverLL->connectionTracing,
                                                            eventHubReceiverLL->waitTimeoutInMs,
                                                            receiverConnectionName)) != 0)
        {
            LogError("Could Not Initialize Common AMQP Connection Data.\r\n");
            result = __LINE__;
        }
        else
        {
            eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_PENDING_AUTHORIZATION;
            result = 0;
        }
    }
    else if (eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_PENDING_RECEIVER_CREATE)
    {
        if ((errorCode = EventHubAMQP_InitializeStackReceiver(&eventHubReceiverLL->receiverAMQPConnection,
                                                                &eventHubReceiverLL->connectionParams,
                                                                onStateChangeCB, onStateChangeCBContext,
                                                                onMsgReceivedCB, onMsgReceivedCBContext,
                                                                eventHubReceiverLL->receiverQueryFilter)) != 0)
        {
            LogError("Could Not Initialize Receiver AMQP Connection Data.\r\n");
            result = __LINE__;
        }
        else
        {
            result = 0;
            if (eventHubReceiverLL->waitTimeoutInMs > 0)
            {
                if ((errorCode = tickcounter_get_current_ms(eventHubReceiverLL->tickCounter, &eventHubReceiverLL->lastActivityTimestamp)) != 0)
                {
                    LogError("Could not get Tick Counter. Code:%d\r\n", errorCode);
                    result = __LINE__;
                }
            }

            if (result == 0)
            {
                eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_INITIALIZED;
            }
        }
    }
    else
    {
        LogError("Unexpected State:%u\r\n", eventHubReceiverLL->receiverAMQPConnectionState);
        result = __LINE__;
    }

    return result;
}

static void EventHubConnectionAMQP_ReceiverDeInitialize(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL)
{
    EventHubAMQP_DeInitializeStack(&eventHubReceiverLL->receiverAMQPConnection);
    eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_UNINITIALIZED;
}

static void EventHubConnectionAMQP_ConnectionDoWork(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL)
{
    if (eventHubReceiverLL->receiverAMQPConnectionState != RECEIVER_AMQP_UNINITIALIZED)
    {
        connection_dowork(eventHubReceiverLL->receiverAMQPConnection.connection);
    }
}

static void EventHubConnectionAMQP_SetConnectionTracing(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL, bool onOff)
{
    if (eventHubReceiverLL->receiverAMQPConnectionState != RECEIVER_AMQP_UNINITIALIZED)
    {
        connection_set_trace(eventHubReceiverLL->receiverAMQPConnection.connection, onOff);
    }
}

static STRING_HANDLE EventHubConnectionAMQP_CreateStartTimeFilter(uint64_t startTimestampInSec)
{
    STRING_HANDLE handle = STRING_construct_sprintf("amqp.annotation.x-opt-enqueuedtimeutc > %" PRIu64, startTimestampInSec);
    if (handle == NULL)
    {
        LogError("Failure allocating filter string.");
    }
    return handle;
}

static void EventHubConnectionAMQP_DestroyFilter(STRING_HANDLE filter)
{
    STRING_delete(filter);
}

EVENTHUBRECEIVER_LL_HANDLE EventHubReceiver_LL_Create
(
    const char* connectionString,
    const char* eventHubPath,
    const char* consumerGroup,
    const char* partitionId
)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_101: \[**EventHubReceiver_LL_Create shall obtain the version string by a call to EVENTHUBRECEIVER_GetVersionString.**\]**
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_102: \[**EventHubReceiver_LL_Create shall print the version string to standard output.**\]**
    LogInfo("Event Hubs Client SDK for C, version %s", EventHubClient_GetVersionString());

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_103: \[**EventHubReceiver_LL_Create shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
    if ((connectionString == NULL) || (eventHubPath == NULL) || (consumerGroup == NULL) || (partitionId == NULL))
    {
        LogError("Invalid arguments. connectionString=%p, eventHubPath=%p consumerGroup=%p partitionId=%p\r\n",
            connectionString, eventHubPath, consumerGroup, partitionId);
        eventHubReceiverLL = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_105: \[**EventHubReceiver_LL_Create shall allocate a new event hub receiver LL instance.**\]**
    else if ((eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)malloc(sizeof(EVENTHUBRECEIVER_LL_STRUCT))) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, EventHubReceiver_LL_Create shall return NULL and cleanup any allocated resources as needed.**\]**
        LogError("Could not allocate memory for EVENTHUBRECEIVER_LL_STRUCT\r\n");
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_106: \[**EventHubReceiver_LL_Create shall create a tickcounter using API tickcounter_create.**\]**
    else if ((eventHubReceiverLL->tickCounter = tickcounter_create()) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, EventHubReceiver_LL_Create shall return NULL and cleanup any allocated resources as needed.**\]**
        LogError("Could not create tick counter\r\n");
        free(eventHubReceiverLL);
        eventHubReceiverLL = NULL;
    }
    else
    {
        int errorCode;
        if ((errorCode = EventHubConnectionParams_Initialize(&eventHubReceiverLL->connectionParams, connectionString, eventHubPath, consumerGroup, partitionId)) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, EventHubReceiver_LL_Create shall return NULL and cleanup any allocated resources as needed.**\]**
            LogError("Could Not Initialize Connection Parameters. Code:%d\r\n", errorCode);
            tickcounter_destroy(eventHubReceiverLL->tickCounter);
            free(eventHubReceiverLL);
            eventHubReceiverLL = NULL;
        }
        else
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_123: \[**EventHubReceiver_LL_Create shall return a non-NULL handle value upon success.**\]**
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_124: \[**EventHubReceiver_LL_Create shall initialize connection tracing to false by default.**\]**
            eventHubReceiverLL->connectionTracing = false;
            eventHubReceiverLL->receiverQueryFilter = NULL;
            eventHubReceiverLL->extRefreshSASToken = NULL;
            eventHubReceiverLL->callback.onEventReceiveCallback = NULL;
            eventHubReceiverLL->callback.onEventReceiveUserContext = NULL;
            eventHubReceiverLL->callback.wasMessageReceived = 0;
            eventHubReceiverLL->callback.onEventReceiveErrorCallback = NULL;
            eventHubReceiverLL->callback.onEventReceiveErrorUserContext = NULL;
            eventHubReceiverLL->startTimestamp = 0;
            eventHubReceiverLL->waitTimeoutInMs = 0;
            eventHubReceiverLL->lastActivityTimestamp = 0;
            EventHubConnectionAMQP_Reset(&eventHubReceiverLL->receiverAMQPConnection);
            eventHubReceiverLL->state = RECEIVER_STATE_INACTIVE;
            eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_UNINITIALIZED;
        }
    }

    return (EVENTHUBRECEIVER_LL_HANDLE)eventHubReceiverLL;
}

EVENTHUBRECEIVER_LL_HANDLE EventHubReceiver_LL_CreateFromSASToken(const char* eventHubSasToken)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL;
    EVENTHUBAUTH_CBS_CONFIG* sasTokenConfig = NULL;
    int errorCode;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_151: \[**EventHubReceiver_LL_CreateFromSASToken shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_152: \[**EventHubReceiver_LL_CreateFromSASToken shall print the version string to standard output.**\]**
    LogInfo("Event Hubs Client SDK for C, version %s", EventHubClient_GetVersionString());

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_153: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL if parameter eventHubSasToken is NULL.**\]**
    if (eventHubSasToken == NULL)
    {
        LogError("Invalid argument. eventHubSasToken");
        eventHubReceiverLL = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_154: \[**EventHubReceiver_LL_CreateFromSASToken parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
    else if ((sasTokenConfig = EventHubAuthCBS_SASTokenParse(eventHubSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_155: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL if EventHubAuthCBS_SASTokenParse fails and returns NULL.**\]**
        LogError("Could Not Obtain Connection Parameters from EventHubAuthCBS_SASTokenParse.");
        eventHubReceiverLL = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_156: \[**EventHubReceiver_LL_CreateFromSASToken shall check if sasTokenData mode is EVENTHUBAUTH_MODE_RECEIVER and if not, any de allocations shall be done and NULL is returned.**\]**
    else if (sasTokenConfig->mode != EVENTHUBAUTH_MODE_RECEIVER)
    {
        LogError("Invalid Mode Obtained From SASToken. Mode:%u", sasTokenConfig->mode);
        EventHubAuthCBS_Config_Destroy(sasTokenConfig);
        eventHubReceiverLL = NULL;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_157: \[**EventHubReceiver_LL_CreateFromSASToken shall allocate memory to hold structure EVENTHUBRECEIVER_LL_STRUCT.**\]**
    else if ((eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)malloc(sizeof(EVENTHUBRECEIVER_LL_STRUCT))) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_158: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations on failures.**\]**
        LogError("Could not allocate memory for EVENTHUBRECEIVER_LL_STRUCT\r\n");
        EventHubAuthCBS_Config_Destroy(sasTokenConfig);
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_159: \[**EventHubReceiver_LL_CreateFromSASToken shall create a tick counter handle using API tickcounter_create.**\]**
    else if ((eventHubReceiverLL->tickCounter = tickcounter_create()) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_158: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations on failures.**\]**
        LogError("Could not create tick counter\r\n");
        free(eventHubReceiverLL);
        eventHubReceiverLL = NULL;
        EventHubAuthCBS_Config_Destroy(sasTokenConfig);
    }
    else if ((errorCode = EventHubConnectionParams_InitializeSASToken(&eventHubReceiverLL->connectionParams, sasTokenConfig)) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_158: \[**EventHubReceiver_LL_CreateFromSASToken shall return NULL on a failure and free up any allocations on failures.**\]**
        LogError("Could not EventHub Connection Params By SAS Token. Code:%d\r\n", errorCode);
        tickcounter_destroy(eventHubReceiverLL->tickCounter);
        free(eventHubReceiverLL);
        eventHubReceiverLL = NULL;
        EventHubAuthCBS_Config_Destroy(sasTokenConfig);
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_165: \[**EventHubReceiver_LL_CreateFromSASToken shall initialize connection tracing to false by default.**\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_166: \[**EventHubReceiver_LL_CreateFromSASToken shall return the allocated EVENTHUBRECEIVER_LL_STRUCT on success.**\]**
        eventHubReceiverLL->connectionTracing = false;
        eventHubReceiverLL->receiverQueryFilter = NULL;
        eventHubReceiverLL->extRefreshSASToken = NULL;
        eventHubReceiverLL->callback.onEventReceiveCallback = NULL;
        eventHubReceiverLL->callback.onEventReceiveUserContext = NULL;
        eventHubReceiverLL->callback.wasMessageReceived = 0;
        eventHubReceiverLL->callback.onEventReceiveErrorCallback = NULL;
        eventHubReceiverLL->callback.onEventReceiveErrorUserContext = NULL;
        eventHubReceiverLL->startTimestamp = 0;
        eventHubReceiverLL->waitTimeoutInMs = 0;
        eventHubReceiverLL->lastActivityTimestamp = 0;
        EventHubConnectionAMQP_Reset(&eventHubReceiverLL->receiverAMQPConnection);
        eventHubReceiverLL->state = RECEIVER_STATE_INACTIVE;
        eventHubReceiverLL->receiverAMQPConnectionState = RECEIVER_AMQP_UNINITIALIZED;
    }

    return (EVENTHUBRECEIVER_LL_HANDLE)eventHubReceiverLL;
}

static int EHRValidateRefreshTokenConfiguration(const EVENTHUBAUTH_CBS_CONFIG* cfg1, const EVENTHUBAUTH_CBS_CONFIG* cfg2)
{
    int result;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_406: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubReceiver_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
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

EVENTHUBRECEIVER_RESULT EventHubReceiver_LL_RefreshSASTokenAsync
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    const char* eventHubRefreshSasToken
)
{
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBAUTH_CBS_CONFIG* refreshSASTokenConfig = NULL;
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_400: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventHubReceiverLLHandle or eventHubRefreshSasToken is NULL.**\]**
    if ((eventHubReceiverLL == NULL) || (eventHubRefreshSasToken == NULL))
    {
        LogError("Invalid Arguments EventHubClient_LL_RefreshSASToken.\r\n");
        result = EVENTHUBRECEIVER_INVALID_ARG;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_402: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_NOT_ALLOWED if the token type is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
    else if (eventHubReceiverLL->connectionParams.extSASTokenConfig == NULL)
    {
        LogError("Refresh Operation Not Valid for Auto SAS Token Mode.\r\n");
        result = EVENTHUBRECEIVER_NOT_ALLOWED;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_401: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
    else if (eventHubReceiverLL->state != RECEIVER_STATE_ACTIVE)
    {
        LogError("No active receiver. Refresh Operation Not Valid.\r\n");
        result = EVENTHUBRECEIVER_NOT_ALLOWED;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_403: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall check if any prior refresh ext SAS token was applied, if so EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
    else if (eventHubReceiverLL->extRefreshSASToken != NULL)
    {
        LogError("Refresh Token Operation in progress. Operation not valid.\r\n");
        result = EVENTHUBRECEIVER_NOT_ALLOWED;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_404: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
    else if ((refreshSASTokenConfig = EventHubAuthCBS_SASTokenParse(eventHubRefreshSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_405: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
        LogError("Could Not Obtain Connection Parameters from EventHubAuthCBS_SASTokenParse.");
        result = EVENTHUBRECEIVER_ERROR;
    }
    else if (EHRValidateRefreshTokenConfiguration(refreshSASTokenConfig, eventHubReceiverLL->connectionParams.extSASTokenConfig) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_407: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR if eventHubRefreshSasToken is not compatible.**\]**
        LogError("Refresh SAS Token Incompatible with token used with EventHubReceiver_LL_CreateFromSASToken.");
        result = EVENTHUBRECEIVER_ERROR;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_408: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in EventHubReceiver_LL_DoWork.**\]**
    else if ((eventHubReceiverLL->extRefreshSASToken = STRING_construct(eventHubRefreshSasToken)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_411: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_ERROR on failure.**\]**
        LogError("Could Not Construct Refresh SAS Token.\r\n");
        result = EVENTHUBRECEIVER_ERROR;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_410: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall return EVENTHUBRECEIVER_OK on success.**\]**
        result = EVENTHUBRECEIVER_OK;
    }

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_412: \[**EventHubReceiver_LL_RefreshSASTokenAsync shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**
    if (refreshSASTokenConfig != NULL)
    {
        EventHubAuthCBS_Config_Destroy(refreshSASTokenConfig);
    }

    return result;
}

static void TeardownReceiverStack(EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL)
{
    EventHubConnectionAMQP_ReceiverDeInitialize(eventHubReceiverLL);
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_610: \[**The filter string shall be freed by STRING_delete.**\]**
    if (eventHubReceiverLL->receiverQueryFilter != NULL)
    {
        EventHubConnectionAMQP_DestroyFilter(eventHubReceiverLL->receiverQueryFilter);
        eventHubReceiverLL->receiverQueryFilter = NULL;
    }
    eventHubReceiverLL->callback.onEventReceiveCallback = NULL;
    eventHubReceiverLL->callback.onEventReceiveUserContext = NULL;
    eventHubReceiverLL->callback.wasMessageReceived = 0;
    eventHubReceiverLL->callback.onEventReceiveErrorCallback = NULL;
    eventHubReceiverLL->callback.onEventReceiveErrorUserContext = NULL;
    eventHubReceiverLL->startTimestamp = 0;
    eventHubReceiverLL->waitTimeoutInMs = 0;
    eventHubReceiverLL->lastActivityTimestamp = 0;
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_613: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**
    if (eventHubReceiverLL->extRefreshSASToken != NULL)
    {
        STRING_delete(eventHubReceiverLL->extRefreshSASToken);
        eventHubReceiverLL->extRefreshSASToken = NULL;
    }
    eventHubReceiverLL->state = RECEIVER_STATE_INACTIVE;
}

void EventHubReceiver_LL_Destroy(EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_200: \[**EventHubReceiver_LL_Destroy return immediately if eventHubReceiverHandle is NULL.**\]**
    if (eventHubReceiverLL != NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_201: \[**EventHubReceiver_LL_Destroy shall tear down connection with the event hub.**\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_202: \[**EventHubReceiver_LL_Destroy shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**
        TeardownReceiverStack(eventHubReceiverLL);
        EventHubConnectionParams_DeInitialize(&eventHubReceiverLL->connectionParams);
        tickcounter_destroy(eventHubReceiverLL->tickCounter);
        free(eventHubReceiverLL);
    }
}

EVENTHUBRECEIVER_RESULT EventHubReceiver_LL_ReceiveEndAsync
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_END_CALLBACK onEventReceiveEndCallback,
    void* onEventReceiveEndUserContext
)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;
    EVENTHUBRECEIVER_RESULT result;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_900: \[**EventHubReceiver_LL_ReceiveEndAsync shall validate arguments, in case they are invalid, error code EVENTHUBRECEIVER_INVALID_ARG will be returned.**\]**
    if (eventHubReceiverLL == NULL)
    {
        LogError("Invalid arguments\r\n");
        result = EVENTHUBRECEIVER_INVALID_ARG;
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_901: \[**EventHubReceiver_LL_ReceiveEndAsync shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned and a message will be logged.**\]**
    else if (eventHubReceiverLL->state != RECEIVER_STATE_ACTIVE)
    {
        LogError("Operation not permitted as there is no active Receiver instance.\r\n");
        result = EVENTHUBRECEIVER_NOT_ALLOWED;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_902: \[**EventHubReceiver_LL_ReceiveEndAsync save off the user callback and context and defer the UAMQP stack tear down to EventHubReceiver_LL_DoWork.**\]**
        eventHubReceiverLL->state = RECEIVER_STATE_INACTIVE_PENDING;
        eventHubReceiverLL->callback.onEventReceiveEndCallback = onEventReceiveEndCallback;
        eventHubReceiverLL->callback.onEventReceiveEndUserContext = onEventReceiveEndUserContext;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_903: \[**Upon Success, EventHubReceiver_LL_ReceiveEndAsync shall return EVENTHUBRECEIVER_OK.**\]**
        result = EVENTHUBRECEIVER_OK;
    }

    return result;
}

static EVENTHUBRECEIVER_RESULT EHR_LL_ReceiveAsyncCommon
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK onEventReceiveCallback,
    void *onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK onEventReceiveErrorCallback,
    void *onEventReceiveErrorUserContext,
    uint64_t startTimestampInSec,
    unsigned int waitTimeoutInMs
)
{
    EVENTHUBRECEIVER_RESULT result;
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;
    int errorCode;
    
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_301: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverHandle, onEventReceiveErrorCallback, onEventReceiveErrorCallback are NULL.**\]**
    if ((eventHubReceiverLL == NULL) || (onEventReceiveCallback == NULL) || (onEventReceiveErrorCallback == NULL))
    {
        LogError("Invalid arguments\r\n");
        result = EVENTHUBRECEIVER_INVALID_ARG;
    }
    else if (eventHubReceiverLL->state != RECEIVER_STATE_INACTIVE)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_307: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall return an error code of EVENTHUBRECEIVER_NOT_ALLOWED if a user called EventHubReceiver_LL_Receive* more than once on the same handle.**\]**
        LogError("Operation not permitted as there is an exiting Receiver instance.\r\n");
        result = EVENTHUBRECEIVER_NOT_ALLOWED;
    }
    else
    {
        startTimestampInSec *= 1000;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_306: \[**Initialize timeout value (zero if no timeout) and a current timestamp of now.**\]**
        if (waitTimeoutInMs && ((errorCode = tickcounter_get_current_ms(eventHubReceiverLL->tickCounter, &eventHubReceiverLL->lastActivityTimestamp)) != 0))
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_303: \[**If tickcounter_get_current_ms fails, EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_ERROR.**\]**
            LogError("Could not get Tick Counter. Code:%d\r\n", errorCode);
            result = EVENTHUBRECEIVER_ERROR;
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_304: \[**Create a filter string using format "apache.org:selector-filter:string" and "amqp.annotation.x-opt-enqueuedtimeutc > startTimestampInSec" using STRING_sprintf**\]**
        else if ((eventHubReceiverLL->receiverQueryFilter = EventHubConnectionAMQP_CreateStartTimeFilter(startTimestampInSec)) == NULL)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_305: \[**If filter string create fails then a log message will be logged and an error code of EVENTHUBRECEIVER_ERROR shall be returned.**\]**
            LogError("Operation not permitted as there is an exiting Receiver instance.\r\n");
            result = EVENTHUBRECEIVER_ERROR;
        }
        else
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_302: \[**EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall record the callbacks and contexts in the EVENTHUBRECEIVER_LL_STRUCT.**\]**
            eventHubReceiverLL->state = RECEIVER_STATE_ACTIVE;
            eventHubReceiverLL->callback.onEventReceiveCallback = onEventReceiveCallback;
            eventHubReceiverLL->callback.onEventReceiveUserContext = onEventReceiveUserContext;
            eventHubReceiverLL->callback.wasMessageReceived = 0;
            eventHubReceiverLL->callback.onEventReceiveErrorCallback = onEventReceiveErrorCallback;
            eventHubReceiverLL->callback.onEventReceiveErrorUserContext = onEventReceiveErrorUserContext;
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_306: \[**Initialize timeout value (zero if no timeout) and a current timestamp of now.**\]**
            eventHubReceiverLL->startTimestamp = startTimestampInSec;
            eventHubReceiverLL->waitTimeoutInMs = waitTimeoutInMs;
            eventHubReceiverLL->callback.onEventReceiveEndCallback = NULL;
            eventHubReceiverLL->callback.onEventReceiveEndUserContext = NULL;
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_308: \[**Otherwise EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall succeed and return EVENTHUBRECEIVER_OK.**\]**
            result = EVENTHUBRECEIVER_OK;
        }
    }

    return result;
}

EVENTHUBRECEIVER_RESULT EventHubReceiver_LL_ReceiveFromStartTimestampAsync
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK onEventReceiveCallback,
    void *onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK onEventReceiveErrorCallback,
    void *onEventReceiveErrorUserContext,
    uint64_t startTimestampInSec
)
{
    return EHR_LL_ReceiveAsyncCommon(eventHubReceiverLLHandle,
                                        onEventReceiveCallback, onEventReceiveUserContext,
                                        onEventReceiveErrorCallback, onEventReceiveErrorUserContext,
                                        startTimestampInSec, 0);
}

EVENTHUBRECEIVER_RESULT EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK onEventReceiveCallback,
    void *onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK onEventReceiveErrorCallback,
    void *onEventReceiveErrorUserContext,
    uint64_t startTimestampInSec,
    unsigned int waitTimeoutInMs
)
{
    return EHR_LL_ReceiveAsyncCommon(eventHubReceiverLLHandle,
                                        onEventReceiveCallback, onEventReceiveUserContext,
                                        onEventReceiveErrorCallback, onEventReceiveErrorUserContext,
                                        startTimestampInSec, waitTimeoutInMs);
}

EVENTHUBRECEIVER_RESULT EventHubReceiver_LL_SetConnectionTracing
(
    EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle,
    bool traceEnabled
)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;
    EVENTHUBRECEIVER_RESULT result;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_800: \[**EventHubReceiver_LL_SetConnectionTracing shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverLLHandle.**\]**
    if (eventHubReceiverLL == NULL)
    {
        LogError("Invalid arguments\r\n");
        result = EVENTHUBRECEIVER_INVALID_ARG;
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_801: \[**EventHubReceiver_LL_SetConnectionTracing shall save the value of tracingOnOff in eventHubReceiverLLHandle**\]**
        eventHubReceiverLL->connectionTracing = traceEnabled;
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_802: \[**If an active connection has been setup, EventHubReceiver_LL_SetConnectionTracing shall be called with the value of connection_set_trace tracingOnOff**\]**
        EventHubConnectionAMQP_SetConnectionTracing(eventHubReceiverLL, traceEnabled);
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_803: \[**Upon success, EventHubReceiver_LL_SetConnectionTracing shall return EVENTHUBRECEIVER_OK**\]**
        result = EVENTHUBRECEIVER_OK;
    }

    return result;
}

static void EHR_LL_OnStateChanged(const void* context, MESSAGE_RECEIVER_STATE newState, MESSAGE_RECEIVER_STATE previousState)
{
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_630: \[**When EHR_LL_OnStateChanged is invoked, obtain the EventHubReceiverLL handle from the context and update the message receiver state with the new state received in the callback.**\]**
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)context;
    if ((newState != previousState) && (newState == MESSAGE_RECEIVER_STATE_ERROR))
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_631: \[**If the new state is MESSAGE_RECEIVER_STATE_ERROR, and previous state is not MESSAGE_RECEIVER_STATE_ERROR, EHR_LL_OnStateChanged shall invoke the user supplied error callback along with error callback context**\]**
        LogError("Message Receiver Error Observed.\r\n");
        if (eventHubReceiverLL->callback.onEventReceiveErrorCallback != NULL)
        {
            eventHubReceiverLL->callback.onEventReceiveErrorCallback(EVENTHUBRECEIVER_CONNECTION_RUNTIME_ERROR, eventHubReceiverLL->callback.onEventReceiveErrorUserContext);
        }
    }
}

static int ProcessApplicationProperties(MESSAGE_HANDLE message, EVENTDATA_HANDLE eventDataHandle)
{
    int result, errorCode;
    AMQP_VALUE handle = NULL;
    MAP_HANDLE eventDataMap;

    if ((eventDataMap = EventData_Properties(eventDataHandle)) == NULL)
    {
        LogError("EventData_Properties Failed %d.\r\n");
        result = __LINE__;
    }
    else if ((errorCode = message_get_application_properties(message, &handle)) != 0)
    {
        LogError("message_get_properties Failed. Code:%d.\r\n", errorCode);
        result = __LINE__;
    }
    else if (handle != NULL)
    {
        AMQP_VALUE descriptor, descriptorValue, uamqpMap;
        uint32_t idx;
        uint32_t propertyCount = 0;

        if ((descriptor = amqpvalue_get_inplace_descriptor(handle)) == NULL)
        {
            LogError("Unexpected Inplace Properties Received.\r\n");
            result = __LINE__;
        }
        else if ((descriptorValue = amqpvalue_get_inplace_described_value(handle)) == NULL)
        {
            LogError("Unexpected Inplace Described Properties Type.\r\n");
            result = __LINE__;
        } 
        else if ((errorCode = amqpvalue_get_map(descriptorValue, &uamqpMap)) != 0)
        {
            LogError("Could not get Map type. Code:%d.\r\n", errorCode);
            result = __LINE__;
        }
        else if ((errorCode = amqpvalue_get_map_pair_count(uamqpMap, &propertyCount)) != 0)
        {
            LogError("Could not Map KVP count. Code:%d.\r\n", errorCode);
            result = __LINE__;
        }
        else
        {
            result = 0;
            for (idx = 0; ((idx < propertyCount) && (!result)); idx++)
            {
                AMQP_VALUE amqpKey = NULL;
                AMQP_VALUE amqpValue = NULL;

                if ((errorCode = amqpvalue_get_map_key_value_pair(uamqpMap, idx, &amqpKey, &amqpValue)) != 0)
                {
                    LogError("Error Getting KVP. Code:%d.\r\n", errorCode);
                    result = __LINE__;
                }
                else
                {
                    const char *key = NULL, *value = NULL;
                    if ((amqpvalue_get_string(amqpKey, &key) != 0) || (amqpvalue_get_string(amqpValue, &value) != 0))
                    {
                        LogError("Error Getting AMQP KVP Strings.\r\n");
                        result = __LINE__;
                    }
                    else if ((errorCode = Map_Add(eventDataMap, key, value)) != MAP_OK)
                    {
                        LogError("Error Adding AMQP KVP to Event Data Map. Code:%d\r\n", errorCode);
                        result = __LINE__;
                    }
                }
                if (amqpKey != NULL)
                {
                    amqpvalue_destroy(amqpKey);
                }
                if (amqpValue != NULL)
                {
                    amqpvalue_destroy(amqpValue);
                }
            }
        }
        application_properties_destroy(handle);
    }
    else
    {
        result = 0;
    }

    return result;
}

static int ProcessMessageAnnotations(MESSAGE_HANDLE message, EVENTDATA_HANDLE eventDataHandle)
{
    static const char* KEY_NAME_ENQUEUED_TIME = "x-opt-enqueued-time";
    annotations handle = NULL;
    int errorCode, result;
    
    if ((errorCode = message_get_message_annotations(message, &handle)) != 0)
    {
        LogError("message_get_message_annotations Failed. Code:%d.\r\n", errorCode);
        result = __LINE__;
    }
    else if (handle != NULL)
    {
        AMQP_VALUE uamqpMap;
        uint32_t idx;
        uint32_t propertyCount = 0;
        if ((errorCode = amqpvalue_get_map(handle, &uamqpMap)) != 0)
        {
            LogError("Could not retrieve map from message annotations. Code%d\r\n", errorCode);
            result = __LINE__;
        }
        else if ((errorCode = amqpvalue_get_map_pair_count(uamqpMap, &propertyCount)) != 0)
        {
            LogError("Could not Map KVP count. Code:%d.\r\n", errorCode);
            result = __LINE__;
        }
        else
        {
            result = 0;
            for (idx = 0; ((idx < propertyCount) && (!result)); idx++)
            {
                AMQP_VALUE amqpKey = NULL;
                AMQP_VALUE amqpValue = NULL;

                if ((errorCode = amqpvalue_get_map_key_value_pair(uamqpMap, idx, &amqpKey, &amqpValue)) != 0)
                {
                    LogError("Error Getting KVP. Code:%d.\r\n", errorCode);
                    result = __LINE__;
                }
                else
                {
                    const char *key = NULL;
                    uint64_t timestampUTC = 0;
                    if (amqpvalue_get_symbol(amqpKey, &key) != 0)
                    {
                        LogError("Error Getting AMQP key Strings.\r\n");
                        result = __LINE__;
                    }
                    else
                    {
                        if (strcmp(key, KEY_NAME_ENQUEUED_TIME) == 0)
                        {
                            if ((errorCode = amqpvalue_get_timestamp(amqpValue, (int64_t*)(&timestampUTC))) != 0)
                            {
                                LogError("Could not get UTC timestamp. Code:%u\r\n", errorCode);
                                result = __LINE__;
                            }
                            else if ((errorCode = EventData_SetEnqueuedTimestampUTCInMs(eventDataHandle, timestampUTC)) != EVENTDATA_OK)
                            {
                                LogError("Could not set UTC timestamp in event data. Code:%u\r\n", errorCode);
                                result = __LINE__;
                            }
                        }
                    }
                }
                if (amqpKey != NULL)
                {
                    amqpvalue_destroy(amqpKey);
                }
                if (amqpValue != NULL)
                {
                    amqpvalue_destroy(amqpValue);
                }
            }
        }
        annotations_destroy(handle);
    }
    else
    {
        result = 0;
    }

    return result;
}

static AMQP_VALUE EHR_LL_OnMessageReceived(const void* context, MESSAGE_HANDLE message)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)context;
    BINARY_DATA binaryData;
    EVENTDATA_HANDLE  eventDataHandle;
    AMQP_VALUE result;
    int errorCode;

    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_641: \[**When EHR_LL_OnMessageReceived is invoked, message_get_body_amqp_data shall be called to obtain the data into a BINARY_DATA buffer.**\]**
    if ((errorCode = message_get_body_amqp_data(message, 0, &binaryData)) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen EHR_LL_OnMessageReceived shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
        LogError("message_get_body_amqp_data Failed. Code:%d.\r\n", errorCode);
        result = messaging_delivery_rejected("Rejected due to failure reading AMQP message", "Failed reading message body");
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_642: \[**EHR_LL_OnMessageReceived shall create a EVENT_DATA handle using EventData_CreateWithNewMemory and pass in the buffer data pointer and size as arguments.**\]**
    else if ((eventDataHandle = EventData_CreateWithNewMemory(binaryData.bytes, binaryData.length)) == NULL)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen EHR_LL_OnMessageReceived shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
        LogError("EventData_CreateWithNewMemory Failed.\r\n");
        result = messaging_delivery_rejected("Rejected due to failure reading AMQP message", "Failed to allocate memory for received event data");
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_644: \[**EHR_LL_OnMessageReceived shall obtain event data specific properties using message_get_message_annotations() and populate the EVENT_DATA handle with these properties.**\]**
    else if ((errorCode = ProcessMessageAnnotations(message, eventDataHandle)) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen EHR_LL_OnMessageReceived shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
        LogError("ProcessMessageAnnotations Failed. Code:%d.\r\n", errorCode);
        EventData_Destroy(eventDataHandle);
        result = messaging_delivery_rejected("Rejected due to failure reading AMQP message", "Failed to process message properties");
    }
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_643: \[**EHR_LL_OnMessageReceived shall obtain the application properties using message_get_application_properties() and populate the EVENT_DATA handle map with these key value pairs.**\]**
    else if ((errorCode = ProcessApplicationProperties(message, eventDataHandle)) != 0)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen EHR_LL_OnMessageReceived shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
        LogError("ProcessApplicationProperties Failed. Code:%d.\r\n", errorCode);
        EventData_Destroy(eventDataHandle);
        result = messaging_delivery_rejected("Rejected due to failure reading AMQP message", "Failed to process application properties");
    }
    else
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_646: \[**`EHR_LL_OnMessageReceived` shall invoke the user registered onMessageReceive callback with status code EVENTHUBRECEIVER_OK, the EVENT_DATA handle and the context passed in by the user.**\]**
        eventHubReceiverLL->callback.wasMessageReceived = 1;
        eventHubReceiverLL->callback.onEventReceiveCallback(EVENTHUBRECEIVER_OK, eventDataHandle, eventHubReceiverLL->callback.onEventReceiveUserContext);
        EventData_Destroy(eventDataHandle);
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_647: \[**After EHR_LL_OnMessageReceived invokes the user callback, messaging_delivery_accepted shall be called.**\]**
        result = messaging_delivery_accepted();
    }

    return result;
}

void EventHubReceiver_LL_DoWork(EVENTHUBRECEIVER_LL_HANDLE eventHubReceiverLLHandle)
{
    EVENTHUBRECEIVER_LL_STRUCT* eventHubReceiverLL = (EVENTHUBRECEIVER_LL_STRUCT*)eventHubReceiverLLHandle;
    
    //**Codes_SRS_EVENTHUBRECEIVER_LL_29_450: \[**EventHubReceiver_LL_DoWork shall return immediately if the supplied handle is NULL**\]**
    if (eventHubReceiverLL == NULL)
    {
        LogError("Invalid arguments\r\n");
    }
    else if (eventHubReceiverLL->state == RECEIVER_STATE_INACTIVE_PENDING)
    {
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_601: \[**EventHubReceiver_LL_DoWork shall do the work to tear down the AMQP stack when a user had called EventHubReceiver_LL_ReceiveEndAsync.**\]**
        TeardownReceiverStack(eventHubReceiverLL);

        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_611: \[**Upon Success, EventHubReceiver_LL_DoWork shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_OK.**\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_612: \[**Upon failure, EventHubReceiver_LL_DoWork shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_ERROR.**\]**
        if (eventHubReceiverLL->callback.onEventReceiveEndCallback)
        {
            eventHubReceiverLL->callback.onEventReceiveEndCallback(EVENTHUBRECEIVER_OK, eventHubReceiverLL->callback.onEventReceiveEndUserContext);
            eventHubReceiverLL->callback.onEventReceiveEndCallback = NULL;
            eventHubReceiverLL->callback.onEventReceiveEndUserContext = NULL;
        }
    }
    else if (eventHubReceiverLL->state == RECEIVER_STATE_ACTIVE)
    {
        int errorCode;
        bool isTimeout = false, isAuthorizationInProgress = false;

        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_451: \[**EventHubReceiver_LL_DoWork shall initialize and bring up the uAMQP stack if it has not already brought up**\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_453: \[**EventHubReceiver_LL_DoWork shall initialize the uAMQP Message Receiver stack if it has not already brought up. **\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_454: \[**EventHubReceiver_LL_DoWork shall create a message receiver if not already created. **\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_620: \[**EventHubReceiver_LL_DoWork shall setup the message receiver_create by passing in EHR_LL_OnStateChanged as the ON_MESSAGE_RECEIVER_STATE_CHANGED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_create is called.**\]**
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_621: \[**EventHubReceiver_LL_DoWork shall open the message receiver_create by passing in EHR_LL_OnMessageReceived as the ON_MESSAGE_RECEIVED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_open is called.**\]**
        if (((eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_UNINITIALIZED) ||
            (eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_PENDING_RECEIVER_CREATE)) &&
            ((errorCode = EventHubConnectionAMQP_ReceiverInitialize(eventHubReceiverLL,
                                                                    EHR_LL_OnStateChanged, (void*)eventHubReceiverLL,
                                                                    EHR_LL_OnMessageReceived, (void*)eventHubReceiverLL)) != 0))
        {
            LogError("Error initializing uAMPQ stack. Code:%d Status:%u\r\n", errorCode, eventHubReceiverLL->receiverAMQPConnectionState);
        }
        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_452: \[**EventHubReceiver_LL_DoWork shall perform SAS token handling. **\]**
        else if ((errorCode = HandleSASTokenAuth(eventHubReceiverLL, &isTimeout, &isAuthorizationInProgress)) != 0)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
            LogError("Error Seen HandleSASTokenAuth Code:%d\r\n", errorCode);
            eventHubReceiverLL->callback.onEventReceiveErrorCallback(EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE, eventHubReceiverLL->callback.onEventReceiveErrorUserContext);
            TeardownReceiverStack(eventHubReceiverLL);
        }
        else if (isTimeout == true)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_543: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT and EventHubReceiver_LL_DoWork shall bring down AMQP stack so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
            LogError("Authorization Timeout Observed\r\n");
            eventHubReceiverLL->callback.onEventReceiveErrorCallback(EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT, eventHubReceiverLL->callback.onEventReceiveErrorUserContext);
            TeardownReceiverStack(eventHubReceiverLL);
        }
        else if (isAuthorizationInProgress == true)
        {
            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_544: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, connection_dowork shall be invoked to perform work to establish/refresh the SAS token.**\]**
            EventHubConnectionAMQP_ConnectionDoWork(eventHubReceiverLL);
        }
        else if (eventHubReceiverLL->receiverAMQPConnectionState == RECEIVER_AMQP_INITIALIZED)
        {
            tickcounter_ms_t nowTimestamp, timespan;

            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_455: \[**EventHubReceiver_LL_DoWork shall invoke connection_dowork **\]**
            EventHubConnectionAMQP_ConnectionDoWork(eventHubReceiverLL);

            //**Codes_SRS_EVENTHUBRECEIVER_LL_29_661: \[**EventHubReceiver_LL_DoWork shall manage timeouts as long as the user specified timeout value is non zero **\]**
            if (eventHubReceiverLL->waitTimeoutInMs != 0)
            {
                if ((errorCode = tickcounter_get_current_ms(eventHubReceiverLL->tickCounter, &nowTimestamp)) != 0)
                {
                    LogError("Could not get Tick Counter. Code:%d\r\n", errorCode);
                }
                else
                {
                    if (eventHubReceiverLL->callback.wasMessageReceived == 0)
                    {
                        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_663: \[**If a message was not received, check if the time now minus the last activity time is greater than or equal to the user specified timeout. If greater, the user registered callback is invoked along with the user supplied context with status code EVENTHUBRECEIVER_TIMEOUT. Last activity time shall be updated to the current time i.e. now.**\]**
                        timespan = nowTimestamp - eventHubReceiverLL->lastActivityTimestamp;
                        if (timespan >= (uint64_t)(eventHubReceiverLL->waitTimeoutInMs))
                        {
                            if (eventHubReceiverLL->callback.onEventReceiveCallback != NULL)
                            {
                                eventHubReceiverLL->callback.onEventReceiveCallback(EVENTHUBRECEIVER_TIMEOUT, NULL, eventHubReceiverLL->callback.onEventReceiveUserContext);
                            }
                            eventHubReceiverLL->lastActivityTimestamp = nowTimestamp;
                        }
                    }
                    else
                    {
                        //**Codes_SRS_EVENTHUBRECEIVER_LL_29_662: \[**EventHubReceiver_LL_DoWork shall check if a message was received, if so, reset the last activity time to the current time i.e. now**\]**
                        eventHubReceiverLL->lastActivityTimestamp = nowTimestamp;
                    }
                }
            }
            eventHubReceiverLL->callback.wasMessageReceived = 0;
        }
    }
}
