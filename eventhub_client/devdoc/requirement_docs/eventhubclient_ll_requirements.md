#EventHubClient_LL Requirements
 
##Overview

The EventHubClient_LL (LL stands for Lower Layer) is module used for communication with an existing Event Hub. The EventHubClient Lower Layer module makes use of uAMQP for AMQP communication with the Event Hub. This library is developed to provide similar usage to that provided via the EventHubClient Class in .Net. This library also doesn't use Thread or Lock, allowing it to be used in embedded applications that are limited as resources. 

##References

Event Hubs [http://msdn.microsoft.com/en-us/library/azure/dn789973.aspx]
EventHubClient Class for .net [http://msdn.microsoft.com/en-us/library/microsoft.servicebus.messaging.eventhubclient.aspx]

##Exposed API

```c
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
    EVENTHUBCLIENT_SENDER_AMQP_UNINITIALIZED        \

DEFINE_ENUM(EVENTHUBCLIENT_ERROR_RESULT, EVENTHUBCLIENT_ERROR_RESULT_VALUES);

typedef struct EVENTHUBCLIENT_LL_TAG* EVENTHUBCLIENT_LL_HANDLE;
typedef void(*EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK)(EVENTHUBCLIENT_CONFIRMATION_RESULT result, void* userContextCallback);

typedef void(*EVENTHUB_CLIENT_STATECHANGE_CALLBACK)(EVENTHUBCLIENT_STATE eventhub_state, void* userContextCallback);
typedef void(*EVENTHUB_CLIENT_ERROR_CALLBACK)(EVENTHUBCLIENT_ERROR_RESULT eventhub_failure, void* userContextCallback);
typedef void(*EVENTHUB_CLIENT_TIMEOUT_CALLBACK)(EVENTDATA_HANDLE eventDataHandle, void* userContextCallback);

MOCKABLE_FUNCTION(, EVENTHUBCLIENT_LL_HANDLE, EventHubClient_LL_CreateFromConnectionString, const char*, connectionString, const char*, eventHubPath);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_LL_HANDLE, EventHubClient_LL_CreateFromSASToken, const char*, eventHubSasToken);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_RefreshSASTokenAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, const char*, eventHubRefreshSasToken);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SendAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTDATA_HANDLE, eventDataHandle, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK, sendAsyncConfirmationCallback, void*, userContextCallback);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SendBatchAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTDATA_HANDLE*, eventDataList, size_t, count, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK, sendAsyncConfirmationCallback, void*, userContextCallback);

MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SetStateChangeCallback, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTHUB_CLIENT_STATECHANGE_CALLBACK, state_change_cb, void*, userContextCallback);
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_SetErrorCallback, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, EVENTHUB_CLIENT_ERROR_CALLBACK, failure_cb, void*, userContextCallback);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_SetMessageTimeout, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, size_t, timeout_value);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_SetLogTrace, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, bool, log_trace_on);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_DoWork, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle);

MOCKABLE_FUNCTION(, void, EventHubClient_LL_Destroy, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle);

```

###EventHubClient_LL_CreateFromConnectionString

```c
extern EVENTHUBCLIENT_LL_HANDLE EventHubClient_LL_CreateFromConnectionString(const char* connectionString, const char* eventHubPath);
```
**SRS_EVENTHUBCLIENT_LL_03_002: \[**EventHubClient_LL_CreateFromConnectionString shall allocate a new event hub client LL instance.**\]**
**SRS_EVENTHUBCLIENT_LL_05_001: \[**EventHubClient_LL_CreateFromConnectionString shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
**SRS_EVENTHUBCLIENT_LL_05_002: \[**EventHubClient_LL_CreateFromConnectionString shall print the version string to standard output.**\]**
**SRS_EVENTHUBCLIENT_LL_03_017: \[**EventHubClient_LL expects a service bus connection string in one of the following formats:
Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
**\]**
**SRS_EVENTHUBCLIENT_LL_03_018: \[**EventHubClient_LL_CreateFromConnectionString shall return NULL if the connectionString format is invalid.**\]**
**SRS_EVENTHUBCLIENT_LL_01_065: \[**The connection string shall be parsed to a map of strings by using connection_string_parser_parse.**\]**
**SRS_EVENTHUBCLIENT_LL_01_066: \[**If connection_string_parser_parse fails then EventHubClient_LL_CreateFromConnectionString shall fail and return NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_01_067: \[**The endpoint shall be looked up in the resulting map and used to construct the host name to be used for connecting by removing the sb://.**\]**
**SRS_EVENTHUBCLIENT_LL_01_068: \[**The key name and key shall be looked up in the resulting map and they should be stored as is for later use in connecting.**\]** 
**SRS_EVENTHUBCLIENT_LL_03_016: \[**EventHubClient_LL_CreateFromConnectionString shall return a non-NULL handle value upon success.**\]**
**SRS_EVENTHUBCLIENT_LL_03_003: \[**EventHubClient_LL_CreateFromConnectionString shall return a NULL value if connectionString or eventHubPath is NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_04_016: \[**EventHubClient_LL_CreateFromConnectionString shall initialize the pending list that will be used to send Events.**\]** 
**SRS_EVENTHUBCLIENT_LL_03_004: \[**For all other errors, EventHubClient_LL_CreateFromConnectionString shall return NULL.**\]**


###EventHubClient_LL_CreateFromSASToken
```c
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_LL_HANDLE, EventHubClient_LL_CreateFromSASToken, const char*, eventHubSasToken);
```
**SRS_EVENTHUBCLIENT_LL_29_300: \[**`EventHubClient_LL_CreateFromSASToken` shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
**SRS_EVENTHUBCLIENT_LL_29_301: \[**`EventHubClient_LL_CreateFromSASToken` shall print the version string to standard output.**\]**
**SRS_EVENTHUBCLIENT_LL_29_302: \[**`EventHubClient_LL_CreateFromSASToken` shall return NULL if eventHubSasToken is NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_29_303: \[**`EventHubClient_LL_CreateFromSASToken` parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
**SRS_EVENTHUBCLIENT_LL_29_304: \[**`EventHubClient_LL_CreateFromSASToken` shall fail if EventHubAuthCBS_SASTokenParse return NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_29_305: \[**`EventHubClient_LL_CreateFromSASToken` shall check if sasTokenData mode is EVENTHUBAUTH_MODE_SENDER, if not, NULL is returned.**\]**
**SRS_EVENTHUBCLIENT_LL_29_306: \[**`EventHubClient_LL_CreateFromSASToken` shall allocate a new event hub client LL instance.**\]**
**SRS_EVENTHUBCLIENT_LL_29_307: \[**`EventHubClient_LL_CreateFromSASToken` shall return NULL on a failure and free up any allocations.**\]**
**SRS_EVENTHUBCLIENT_LL_29_308: \[**`EventHubClient_LL_CreateFromSASToken` shall create a tick counter handle using API tickcounter_create.**\]**
**SRS_EVENTHUBCLIENT_LL_29_309: \[**`EventHubClient_LL_CreateFromSASToken` shall clone the hostName string using API STRING_Clone.**\]**
**SRS_EVENTHUBCLIENT_LL_29_310: \[**`EventHubClient_LL_CreateFromSASToken` shall clone the senderPublisherId and eventHubPath strings using API STRING_Clone.**\]**
**SRS_EVENTHUBCLIENT_LL_29_311: \[**`EventHubClient_LL_CreateFromSASToken` shall initialize sender target address using the eventHub and senderPublisherId with format amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]**
**SRS_EVENTHUBCLIENT_LL_29_312: \[**`EventHubClient_LL_CreateFromSASToken` shall initialize connection tracing to false by default.**\]**
**SRS_EVENTHUBCLIENT_LL_29_313: \[**`EventHubClient_LL_CreateFromSASToken` shall return the allocated event hub client LL instance on success.**\]**


###EventHubClient_LL_RefreshSASTokenAsync
```c
MOCKABLE_FUNCTION(, EVENTHUBCLIENT_RESULT, EventHubClient_LL_RefreshSASTokenAsync, EVENTHUBCLIENT_LL_HANDLE, eventHubClientLLHandle, const char*, eventHubSasToken);
```
**SRS_EVENTHUBCLIENT_LL_29_401: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_INVALID_ARG if eventHubClientLLHandle or eventHubSasToken is NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_29_402: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_ERROR if eventHubClientLLHandle credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
**SRS_EVENTHUBCLIENT_LL_29_403: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_ERROR if AMQP stack is not fully initialized.**\]**
**SRS_EVENTHUBCLIENT_LL_29_404: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall check if any prior refresh ext SAS token was applied, if so EVENTHUBCLIENT_ERROR shall be returned.**\]**
**SRS_EVENTHUBCLIENT_LL_29_405: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
**SRS_EVENTHUBCLIENT_LL_29_406: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_29_407: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when `EventHubClient_LL_CreateFromSASToken` was invoked by using API STRING_compare.**\]**
**SRS_EVENTHUBCLIENT_LL_29_408: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_ERROR if eventHubRefreshSasToken is not compatible.**\]**
**SRS_EVENTHUBCLIENT_LL_29_409: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in `EventHubClient_LL_DoWork`.**\]**
**SRS_EVENTHUBCLIENT_LL_29_410: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_OK on success.**\]**
**SRS_EVENTHUBCLIENT_LL_29_411: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_ERROR on failure.**\]**
**SRS_EVENTHUBCLIENT_LL_29_412: \[**`EventHubClient_LL_RefreshSASTokenAsync` shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**


###EventHubClient_LL_SendAsync

```c
extern EVENTHUBCLIENT_RESULT EventHubClient_LL_SendAsync(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle , EVENTDATA_HANDLE eventDataHandle, EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK sendAsyncConfirmationCallback, void* userContextCallback);
```

**SRS_EVENTHUBCLIENT_LL_04_011: \[**EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_LL_INVALID_ARG if parameter eventHubClientLLHandle or eventDataHandle is NULL.**\]** 
**SRS_EVENTHUBCLIENT_LL_04_012: \[**EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_LL_INVALID_ARG if parameter telemetryConfirmationCallBack is NULL and userContextCallBack is not NULL.**\]** 
**SRS_EVENTHUBCLIENT_LL_04_013: \[**EventHubClient_LL_SendAsync shall add to the pending events DLIST outgoingEvents a new record cloning the information from eventDataHandle, telemetryConfirmationCallback and userContextCallBack.**\]** 
**SRS_EVENTHUBCLIENT_LL_04_014: \[**If cloning and/or adding the information fails for any reason, EventHubClient_LL_SendAsync shall fail and return EVENTHUBCLIENT_LL_ERROR.**\]**
**SRS_EVENTHUBCLIENT_LL_04_015: \[**Otherwise EventHubClient_LL_SendAsync shall succeed and return EVENTHUBCLIENT_LL_OK.**\]** 

###EventHubClient_LL_SendBatchAsync

```c
extern EVENTHUBCLIENT_RESULT EventHubClient_LL_SendBatchAsync(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTDATA_HANDLE* eventDataList, size_t count,  EVENTHUB_CLIENT_SENDASYNC_CONFIRMATION_CALLBACK sendAsyncConfirmationCallback, void* userContextCallback);
```

**SRS_EVENTHUBCLIENT_LL_07_012: \[**EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if eventhubClientLLHandle or eventDataList are NULL or if sendAsnycConfirmationCallback equals NULL and userContextCallback does not equal NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_01_095: \[**EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_INVALID_ARG if the count argument is zero.**\]** 
**SRS_EVENTHUBCLIENT_LL_07_013: \[**EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_ERROR for any Error that is encountered.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_096: \[**If the partitionKey properties on the events in the batch are not the same then EventHubClient_LL_SendBatchAsync shall fail and return EVENTHUBCLIENT_ERROR.**\]**
**SRS_EVENTHUBCLIENT_LL_01_097: \[**The partition key for each event shall be obtained by calling EventData_getPartitionKey.**\]** 
**SRS_EVENTHUBCLIENT_LL_07_014: \[**EventHubClient_LL_SendBatchAsync shall clone each item in the eventDataList by calling EventData_Clone.**\]** 
**SRS_EVENTHUBCLIENT_LL_07_015: \[**On success EventHubClient_LL_SendBatchAsync shall return EVENTHUBCLIENT_OK.**\]** 

###EventHubClient_LL_DoWork

```c
extern void EventHubClient_LL_DoWork(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle);
```

#### General
**SRS_EVENTHUBCLIENT_LL_04_018: \[**if parameter eventHubClientLLHandle is NULL EventHubClient_LL_DoWork shall immediately return.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_079: \[**`EventHubClient_LL_DoWork` shall initialize and bring up the uAMQP stack if it has not already been brought up**\]**
**SRS_EVENTHUBCLIENT_LL_29_201: \[**`EventHubClient_LL_DoWork` shall perform SAS token handling. **\]**
**SRS_EVENTHUBCLIENT_LL_29_202: \[**`EventHubClient_LL_DoWork` shall initialize the uAMQP Message Sender stack if it has not already brought up. **\]**
**SRS_EVENTHUBCLIENT_LL_29_203: \[**`EventHubClient_LL_DoWork` shall perform message send handling **\]** 

#### uAMQP Stack Initialize 
**SRS_EVENTHUBCLIENT_LL_01_004: \[**A SASL mechanism shall be created by calling saslmechanism_create.**\]**
**SRS_EVENTHUBCLIENT_LL_01_005: \[**The interface passed to saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
**SRS_EVENTHUBCLIENT_LL_01_006: \[**If saslmssbcbs_get_interface fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]**
**SRS_EVENTHUBCLIENT_LL_01_011: \[**If sasl_mechanism_create fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_03_030: \[**A TLS IO shall be created by calling xio_create.**\]**
**SRS_EVENTHUBCLIENT_LL_01_002: \[**The TLS IO interface description passed to xio_create shall be obtained by calling platform_get_default_tlsio_interface.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_001: \[**If platform_get_default_tlsio_interface fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages. **\]**
**SRS_EVENTHUBCLIENT_LL_01_003: \[**If xio_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_012: \[**A SASL client IO shall be created by calling xio_create.**\]**
**SRS_EVENTHUBCLIENT_LL_01_013: \[**The IO interface description for the SASL client IO shall be obtained by calling saslclientio_get_interface_description.**\]**
**SRS_EVENTHUBCLIENT_LL_01_014: \[**If saslclientio_get_interface_description fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]**
**SRS_EVENTHUBCLIENT_LL_01_015: \[**The IO creation parameters passed to xio_create shall be in the form of a SASLCLIENTIO_CONFIG.**\]**
**SRS_EVENTHUBCLIENT_LL_01_016: \[**The underlying_io members shall be set to the previously created TLS IO.**\]**
**SRS_EVENTHUBCLIENT_LL_01_017: \[**The sasl_mechanism shall be set to the previously created SASL mechanism.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_018: \[**If xio_create fails creating the SASL client IO then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_019: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle, eventhub hostname, "eh_client_connection" as container name and NULL for the new session handler and context.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_020: \[**If connection_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_028: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_029: \[**If session_create fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_030: \[**The outgoing window for the session shall be set to 10 by calling session_set_outgoing_window.**\]**
**SRS_EVENTHUBCLIENT_LL_01_031: \[**If setting the outgoing window fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 

**SRS_EVENTHUBCLIENT_LL_29_110: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a `EVENTHUBAUTH_CBS_CONFIG` structure params hostName, eventHubPath, sharedAccessKeyName, sharedAccessKey using the values set previously. Set senderPublisherId to "sender". Set receiverConsumerGroup, receiverPartitionId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_SENDER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
**SRS_EVENTHUBCLIENT_LL_29_111: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the `EVENTHUBAUTH_CBS_CONFIG` obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
**SRS_EVENTHUBCLIENT_LL_29_113: \[**`EventHubAuthCBS_Create` shall be invoked using the config structure reference and the session handle created earlier.**\]**
**SRS_EVENTHUBCLIENT_LL_29_114: \[**If `EventHubAuthCBS_Create` returns NULL, a log message will be logged and the function returns immediately.**\]**

**SRS_EVENTHUBCLIENT_LL_29_115: \[**`EventHubClient_LL_DoWork` shall invoke connection_set_trace using the current value of the trace on boolean.**\]**

#### SAS Token handling
**SRS_EVENTHUBCLIENT_LL_29_120: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
**SRS_EVENTHUBCLIENT_LL_29_121: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in `EventHubClient_LL_DoWork`.**\]**
**SRS_EVENTHUBCLIENT_LL_29_122: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBCLIENT_SASTOKEN_AUTH_TIMEOUT and `EventHubClient_LL_DoWork` shall bring down AMQP stack so that it can be created again if needed in `EventHubClient_LL_DoWork`.**\]**
**SRS_EVENTHUBCLIENT_LL_29_123: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, `connection_dowork` shall be invoked to perform work to establish/refresh the SAS token.**\]**
**SRS_EVENTHUBCLIENT_LL_29_124: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
**SRS_EVENTHUBCLIENT_LL_29_125: \[**If status is EVENTHUBAUTH_STATUS_IDLE, `EventHubAuthCBS_Authenticate` shall be invoked to create and install the SAS token.**\]**
**SRS_EVENTHUBCLIENT_LL_29_126: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
**SRS_EVENTHUBCLIENT_LL_29_127: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubClient_LL_DoWork`.**\]**


#### uAMQP Message Sender Stack Initialize
**SRS_EVENTHUBCLIENT_LL_01_021: \[**A source AMQP value shall be created by calling messaging_create_source.**\]**
**SRS_EVENTHUBCLIENT_LL_01_022: \[**The source address shall be "ingress".**\]**
**SRS_EVENTHUBCLIENT_LL_01_023: \[**A target AMQP value shall be created by calling messaging_create_target.**\]**
**SRS_EVENTHUBCLIENT_LL_01_024: \[**The target address shall be amqps://{eventhub hostname}/{eventhub name}/publishers/<PUBLISHER_NAME>.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_025: \[**If creating the source or target values fails then EventHubClient_LL_DoWork shall shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_026: \[**An AMQP link shall be created by calling link_create and passing as arguments the session handle, "sender-link" as link name, role_sender and the previously created source and target values.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_027: \[**If creating the link fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_032: \[**The link sender settle mode shall be set to unsettled by calling link_set_snd_settle_mode.**\]**
**SRS_EVENTHUBCLIENT_LL_01_033: \[**If link_set_snd_settle_mode fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_034: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_035: \[**If link_set_max_message_size fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_036: \[**A message sender shall be created by calling messagesender_create and passing as arguments the link handle, a state changed callback, a context and NULL for the logging function.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_037: \[**If creating the message sender fails then EventHubClient_LL_DoWork shall not proceed with sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_080: \[**If any other error happens while bringing up the uAMQP stack, EventHubClient_LL_DoWork shall not attempt to open the message_sender and return without sending any messages.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_038: \[**EventHubClient_LL_DoWork shall perform a messagesender_open if the state of the message_sender is not OPEN.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_039: \[**If messagesender_open fails, no further actions shall be carried out.**\]** 

#### For Each Pending Message Handling:
**SRS_EVENTHUBCLIENT_LL_01_049: \[**If the message has not yet been given to uAMQP then a new message shall be created by calling message_create.**\]**
**SRS_EVENTHUBCLIENT_LL_01_070: \[**If creating the message fails, then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_050: \[**If the number of event data entries for the message is 1 (not batched) then the message body shall be set to the event data payload by calling message_add_body_amqp_data.**\]**
**SRS_EVENTHUBCLIENT_LL_01_071: \[**If message_add_body_amqp_data fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]**
**SRS_EVENTHUBCLIENT_LL_01_051: \[**The pointer to the payload and its length shall be obtained by calling EventData_GetData.**\]**
**SRS_EVENTHUBCLIENT_LL_01_052: \[**If EventData_GetData fails then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_054: \[**If the number of event data entries for the message is 1 (not batched) the event data properties shall be added as application properties to the message.**\]**
**SRS_EVENTHUBCLIENT_LL_01_055: \[**A map shall be created to hold the application properties by calling amqpvalue_create_map.**\]**
**SRS_EVENTHUBCLIENT_LL_01_056: \[**For each property a key and value AMQP value shall be created by calling amqpvalue_create_string.**\]**
**SRS_EVENTHUBCLIENT_LL_01_057: \[**Then each property shall be added to the application properties map by calling amqpvalue_set_map_value.**\]**
**SRS_EVENTHUBCLIENT_LL_01_058: \[**The resulting map shall be set as the message application properties by calling message_set_application_properties.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_059: \[**If any error is encountered while creating the application properties the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_082: \[**If the number of event data entries for the message is greater than 1 (batched) then the message format shall be set to 0x80013700 by calling message_set_message_format.**\]**
**SRS_EVENTHUBCLIENT_LL_01_083: \[**If message_set_message_format fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_084: \[**For each event in the batch:**\]** 
**SRS_EVENTHUBCLIENT_LL_01_085: \[**The event shall be added to the message by into a separate data section by calling message_add_body_amqp_data.**\]**
**SRS_EVENTHUBCLIENT_LL_01_086: \[**The buffer passed to message_add_body_amqp_data shall contain the properties and the binary event payload serialized as AMQP values.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_087: \[**The properties shall be serialized as AMQP application_properties.**\]**
**SRS_EVENTHUBCLIENT_LL_01_088: \[**The event payload shall be serialized as an AMQP message data section.**\]**
**SRS_EVENTHUBCLIENT_LL_01_089: \[**If message_add_body_amqp_data fails, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]**
**SRS_EVENTHUBCLIENT_LL_01_090: \[**Enough memory shall be allocated to hold the properties and binary payload for each event part of the batch.**\]**
**SRS_EVENTHUBCLIENT_LL_01_091: \[**The size needed for the properties and data section shall be obtained by calling amqpvalue_get_encoded_size.**\]**
**SRS_EVENTHUBCLIENT_LL_01_092: \[**The properties and binary data shall be encoded by calling amqpvalue_encode and passing an encoding function that places the encoded data into the memory allocated for the event.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_093: \[**If the property count is 0 for an event part of the batch, then no property map shall be serialized for that event.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_094: \[**If any error occurs during serializing each event properties and data that are part of the batch, the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_069: \[**The AMQP message shall be given to uAMQP by calling messagesender_send, while passing as arguments the message sender handle, the message handle, a callback function and its context.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_053: \[**If messagesender_send failed then the callback associated with the message shall be called with EVENTHUBCLIENT_CONFIRMATION_ERROR and the message shall be freed from the pending list.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_064: \[**EventHubClient_LL_DoWork shall call connection_dowork while passing as argument the connection handle obtained in EventHubClient_LL_Create.**\]** 
**SRS_EVENTHUBCLIENT_LL_07_028: [**If the message idle time is greater than the msg_timeout, EventHubClient_LL_DoWork shall call callback with EVENTHUBCLIENT_CONFIRMATION_TIMEOUT.**]**  

###on_messagesender_state_changed

**SRS_EVENTHUBCLIENT_LL_01_060: \[**When on_messagesender_state_changed is called with MESSAGE_SENDER_STATE_ERROR, the uAMQP stack shall be brough down so that it can be created again if needed in dowork:**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_072: \[**The message sender shall be destroyed by calling messagesender_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_073: \[**The link shall be destroyed by calling link_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_074: \[**The session shall be destroyed by calling session_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_075: \[**The connection shall be destroyed by calling connection_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_076: \[**The SASL IO shall be destroyed by calling xio_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_077: \[**The TLS IO shall be destroyed by calling xio_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_01_078: \[**The SASL mechanism shall be destroyed by calling saslmechanism_destroy.**\]** 
-   **SRS_EVENTHUBCLIENT_LL_29_151: \[**`EventHubAuthCBS_Destroy` shall be called to destroy the event hub auth handle.**\]**
-   **SRS_EVENTHUBCLIENT_LL_29_152: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**

###on_message_send_complete

**SRS_EVENTHUBCLIENT_LL_01_061: \[**When on_message_send_complete is called with MESSAGE_SEND_OK the pending message shall be indicated as sent correctly by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_OK.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_063: \[**When on_message_send_complete is called with a result code different than MESSAGE_SEND_OK the pending message shall be indicated as having an error by calling the callback associated with the pending message with EVENTHUBCLIENT_CONFIRMATION_ERROR.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_062: \[**The pending message shall be removed from the pending list.**\]** 

###EventHubClient_LL_Destroy

```c
extern void EventHubClient_LL_Destroy(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle);
```

**SRS_EVENTHUBCLIENT_LL_03_009: \[**EventHubClient_LL_Destroy shall terminate the usage of this EventHubClient_LL specified by the eventHubLLHandle and cleanup all associated resources.**\]**
**SRS_EVENTHUBCLIENT_LL_01_081: \[**The key host name, key name and key allocated in EventHubClient_LL_CreateFromConnectionString shall be freed.**\]** 
**SRS_EVENTHUBCLIENT_LL_03_010: \[**If the eventHubLLHandle is NULL, EventHubClient_Destroy shall not do anything.**\]**
**SRS_EVENTHUBCLIENT_LL_01_040: \[**All the pending messages shall be indicated as error by calling the associated callback with EVENTHUBCLIENT_CONFIRMATION_DESTROY.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_041: \[**All pending message data shall be freed.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_042: \[**The message sender shall be freed by calling messagesender_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_043: \[**The link shall be freed by calling link_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_044: \[**The session shall be freed by calling session_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_045: \[**The connection shall be freed by calling connection_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_046: \[**The SASL client IO shall be freed by calling xio_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_047: \[**The TLS IO shall be freed by calling xio_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_01_048: \[**The SASL mechanism shall be freed by calling saslmechanism_destroy.**\]** 
**SRS_EVENTHUBCLIENT_LL_29_153: \[**`EventHubAuthCBS_Destroy` shall be called to destroy the event hub auth handle.**\]**
**SRS_EVENTHUBCLIENT_LL_29_154: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**

###EventHubClient_LL_SetStateChangeCallback

```c
extern EVENTHUBCLIENT_RESULT EventHubClient_LL_SetStateChangeCallback(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTHUB_CLIENT_STATECHANGE_CALLBACK state_change_cb);
```

**SRS_EVENTHUBCLIENT_LL_07_016: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetStateChangeCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
**SRS_EVENTHUBCLIENT_LL_07_017: [** If state_change_cb is non-NULL then EventHubClient_LL_SetStateChangeCallback shall call state_change_cb when a state changes is encountered. **]**  
**SRS_EVENTHUBCLIENT_LL_07_018: [** If state_change_cb is NULL EventHubClient_LL_SetStateChangeCallback shall no longer call state_change_cb on state changes. **]**  
**SRS_EVENTHUBCLIENT_LL_07_019: [** If EventHubClient_LL_SetStateChangeCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**

###EventHubClient_LL_SetErrorCallback

```c
extern EVENTHUBCLIENT_RESULT EventHubClient_LL_SetErrorCallback(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, EVENTHUB_CLIENT_FAILURE_CALLBACK on_error_cb);
```

**SRS_EVENTHUBCLIENT_LL_07_020: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetErrorCallback shall return EVENTHUBCLIENT_INVALID_ARG. **]**
**SRS_EVENTHUBCLIENT_LL_07_021: [** If failure_cb is non-NULL EventHubClient_LL_SetErrorCallback shall execute the on_error_cb on failures with a EVENTHUBCLIENT_FAILURE_RESULT. **]**
**SRS_EVENTHUBCLIENT_LL_07_022: [** If failure_cb is NULL EventHubClient_LL_SetErrorCallback shall no longer call on_error_cb on failure. **]**
**SRS_EVENTHUBCLIENT_LL_07_023: [** If EventHubClient_LL_SetErrorCallback succeeds it shall return EVENTHUBCLIENT_OK. **]**

###EventHubClient_LL_SetLogTrace

```c
extern void EventHubClient_LL_SetLogTrace(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, bool log_trace_on);
```

**SRS_EVENTHUBCLIENT_LL_07_024: [** If eventHubClientLLHandle is non-NULL EventHubClient_LL_SetLogTrace shall call the uAmqp trace function with the log_trace_on. **]**
**SRS_EVENTHUBCLIENT_LL_07_025: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetLogTrace shall do nothing. **]**

###EventHubClient_LL_SetMessageTimeout

```c
extern void EventHubClient_LL_SetMessageTimeout(EVENTHUBCLIENT_LL_HANDLE eventHubClientLLHandle, size_t timeout_value);
```

**SRS_EVENTHUBCLIENT_LL_07_026: [** If eventHubClientLLHandle is NULL EventHubClient_LL_SetMessageTimeout shall do nothing. **]**  
**SRS_EVENTHUBCLIENT_LL_07_027: [** EventHubClient_LL_SetMessageTimeout shall save the timeout_value. **]**  
