# EventHubReceiver_LL Requirements
 
## Overview

The EventHubReceiver_LL (LL stands for Lower Layer) is module used for communication with an existing Event Hub. 
The EVENTHUBRECEIVER Lower Layer module makes use of uAMQP for AMQP communication with the Event Hub for the 
purposes of reading event data from a partition. 
This library is developed to provide similar usage to that provided via the EVENTHUBRECEIVER Class in .Net.
This library also doesn't use Thread or Locks, allowing it to be used in embedded applications that are 
limited in resources.

## References

[Event Hubs](http://msdn.microsoft.com/en-us/library/azure/dn789973.aspx)

[EVENTHUBRECEIVER Class for .NET](http://msdn.microsoft.com/en-us/library/microsoft.servicebus.messaging.EVENTHUBRECEIVER.aspx)

## Exposed API
```c
#define EVENTHUBRECEIVER_RESULT_VALUES                \
        EVENTHUBRECEIVER_OK,                          \
        EVENTHUBRECEIVER_INVALID_ARG,                 \
        EVENTHUBRECEIVER_ERROR,                       \
        EVENTHUBRECEIVER_TIMEOUT,                     \
        EVENTHUBRECEIVER_CONNECTION_RUNTIME_ERROR,    \
        EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE,       \
        EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT,       \
        EVENTHUBRECEIVER_NOT_ALLOWED

DEFINE_ENUM(EVENTHUBRECEIVER_RESULT, EVENTHUBRECEIVER_RESULT_VALUES);

typedef struct EVENTHUBRECEIVER_LL_TAG* EVENTHUBRECEIVER_LL_HANDLE;

typedef void(*EVENTHUBRECEIVER_ASYNC_CALLBACK)(EVENTHUBRECEIVER_RESULT result,
                                                EVENTDATA_HANDLE eventDataHandle,
                                                void* userContext);

typedef void(*EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK)(EVENTHUBRECEIVER_RESULT errorCode,
                                                        void* userContextCallback);


typedef void(*EVENTHUBRECEIVER_ASYNC_END_CALLBACK)(EVENTHUBRECEIVER_RESULT result,
                                                                void* userContextCallback);


MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_LL_HANDLE, EventHubReceiver_LL_Create,
    const char*,  connectionString,
    const char*,  eventHubPath,
    const char*,  consumerGroup,
    const char*,  partitionId
);

MOCKABLE_FUNCTION(, void, EventHubReceiver_LL_Destroy, EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverHandle);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveFromStartTimestampAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle, 
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback, 
    void*, onEventReceiveUserContext, 
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
    uint64_t, startTimestampInSec
);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback,
    void*, onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
        uint64_t, startTimestampInSec,
        unsigned int, waitTimeoutInMs
);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveEndAsync,
                    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
                    EVENTHUBRECEIVER_ASYNC_END_CALLBACK, onEventReceiveEndCallback,
                    void*, onEventReceiveEndUserContext);

MOCKABLE_FUNCTION(, void, EventHubReceiver_LL_DoWork, EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_SetConnectionTracing,
                    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle, bool, traceEnabled);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_LL_HANDLE, EventHubReceiver_LL_CreateFromSASToken,
    const char*, eventHubSasToken
);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_RefreshSASTokenAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
    const char*, eventHubRefreshSasToken
);

```
### EventHubReceiver_LL_Create

```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_LL_HANDLE, EventHubReceiver_LL_Create,
    const char*,  connectionString,
    const char*,  eventHubPath,
    const char*,  consumerGroup,
    const char*,  partitionId
);
``` 
**SRS_EVENTHUBRECEIVER_LL_29_101: \[**`EventHubReceiver_LL_Create` shall obtain the version string by a call to EVENTHUBRECEIVER_GetVersionString.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_102: \[**`EventHubReceiver_LL_Create` shall print the version string to standard output.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_103: \[**`EventHubReceiver_LL_Create` shall return NULL if any parameter connectionString, eventHubPath, consumerGroup and partitionId is NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_104: \[**For all errors, `EventHubReceiver_LL_Create` shall return NULL and cleanup any allocated resources as needed.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_105: \[**`EventHubReceiver_LL_Create` shall allocate a new event hub receiver LL instance.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_106: \[**`EventHubReceiver_LL_Create` shall create a tickcounter using API tickcounter_create.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_107: \[**`EventHubReceiver_LL_Create` shall expect a service bus connection string in one of the following formats:
Endpoint=sb://[namespace].servicebus.windows.net/;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue] 
Endpoint=sb://[namespace].servicebus.windows.net;SharedAccessKeyName=[keyname];SharedAccessKey=[keyvalue]
**\]**
**SRS_EVENTHUBRECEIVER_LL_29_108: \[**`EventHubReceiver_LL_Create` shall create a temp connection STRING_HANDLE using connectionString as the parameter.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_109: \[**`EventHubReceiver_LL_Create` shall parse the connection string handle to a map of strings by using API connection_string_parser_parse.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_110: \[**`EventHubReceiver_LL_Create` shall create a STRING_HANDLE using API STRING_construct for holding the argument eventHubPath.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_111: \[**`EventHubReceiver_LL_Create` shall lookup the endpoint in the resulting map using API Map_GetValueFromKey and argument "Endpoint".**\]**
**SRS_EVENTHUBRECEIVER_LL_29_112: \[**`EventHubReceiver_LL_Create` shall obtain the host name after parsing characters after substring "sb://".**\]**
**SRS_EVENTHUBRECEIVER_LL_29_113: \[**`EventHubReceiver_LL_Create` shall create a host name STRING_HANDLE using API STRING_construct_n and the host name substring and its length obtained above as parameters.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_114: \[**`EventHubReceiver_LL_Create` shall lookup the SharedAccessKeyName in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKeyName".**\]**
**SRS_EVENTHUBRECEIVER_LL_29_115: \[**`EventHubReceiver_LL_Create` shall create SharedAccessKeyName STRING_HANDLE using API STRING_construct and using the "SharedAccessKeyName" key's value obtained above as parameter.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_116: \[**`EventHubReceiver_LL_Create` shall lookup the SharedAccessKey in the resulting map using API Map_GetValueFromKey and argument "SharedAccessKey".**\]**
**SRS_EVENTHUBRECEIVER_LL_29_117: \[**`EventHubReceiver_LL_Create` shall create SharedAccessKey STRING_HANDLE using API STRING_construct and using the "SharedAccessKey" key's value obtained above as parameter.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_118: \[**`EventHubReceiver_LL_Create` shall destroy the map handle using API Map_Destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_119: \[**`EventHubReceiver_LL_Create` shall destroy the temp connection string handle using API STRING_delete.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_120: \[**`EventHubReceiver_LL_Create` shall create a STRING_HANDLE using API STRING_construct for holding the argument consumerGroup.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_121: \[**`EventHubReceiver_LL_Create` shall create a STRING_HANDLE using API STRING_construct for holding the argument partitionId.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_122: \[**`EventHubReceiver_LL_Create` shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_123: \[**`EventHubReceiver_LL_Create` shall return a non-NULL handle value upon success.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_124: \[**`EventHubReceiver_LL_Create` shall initialize connection tracing to false by default.**\]**

```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_LL_HANDLE, EventHubReceiver_LL_CreateFromSASToken,
    const char*, eventHubSasToken
);
```
**SRS_EVENTHUBRECEIVER_LL_29_151: \[**`EventHubReceiver_LL_CreateFromSASToken` shall obtain the version string by a call to EventHubClient_GetVersionString.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_152: \[**`EventHubReceiver_LL_CreateFromSASToken` shall print the version string to standard output.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_153: \[**`EventHubReceiver_LL_CreateFromSASToken` shall return NULL if parameter eventHubSasToken is NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_154: \[**`EventHubReceiver_LL_CreateFromSASToken` parse the SAS token to obtain the sasTokenData by calling API EventHubAuthCBS_SASTokenParse and passing eventHubSasToken as argument.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_155: \[**`EventHubReceiver_LL_CreateFromSASToken` shall return NULL if EventHubAuthCBS_SASTokenParse fails and returns NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_156: \[**`EventHubReceiver_LL_CreateFromSASToken` shall check if sasTokenData mode is EVENTHUBAUTH_MODE_RECEIVER and if not, any de allocations shall be done and NULL is returned.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_157: \[**`EventHubReceiver_LL_CreateFromSASToken` shall allocate memory to hold structure EVENTHUBRECEIVER_LL_STRUCT.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_158: \[**`EventHubReceiver_LL_CreateFromSASToken` shall return NULL on a failure and free up any allocations on failures.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_159: \[**`EventHubReceiver_LL_CreateFromSASToken` shall create a tick counter handle using API tickcounter_create.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_160: \[**`EventHubReceiver_LL_CreateFromSASToken` shall clone the hostName string using API STRING_clone.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_161: \[**`EventHubReceiver_LL_CreateFromSASToken` shall clone the eventHubPath string using API STRING_clone.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_162: \[**`EventHubReceiver_LL_CreateFromSASToken` shall clone the consumerGroup string using API STRING_clone.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_163: \[**`EventHubReceiver_LL_CreateFromSASToken` shall clone the receiverPartitionId string using API STRING_clone.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_164: \[**`EventHubReceiver_LL_CreateFromSASToken` shall construct a STRING_HANDLE receiver URI using event hub name, consumer group, partition id data with format {eventHubName}/ConsumerGroups/{consumerGroup}/Partitions/{partitionID}.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_165: \[**`EventHubReceiver_LL_CreateFromSASToken` shall initialize connection tracing to false by default.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_166: \[**`EventHubReceiver_LL_CreateFromSASToken` shall return the allocated EVENTHUBRECEIVER_LL_STRUCT on success.**\]**


###EventHubReceiver_LL_RefreshSASTokenAsync
```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_RefreshSASTokenAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
    const char*, eventHubRefreshSasToken
);
```
**SRS_EVENTHUBRECEIVER_LL_29_400: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBCLIENT_INVALID_ARG if eventHubReceiverLLHandle or eventHubRefreshSasToken is NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_401: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_402: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBRECEIVER_NOT_ALLOWED if the token type is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_403: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall check if any prior refresh ext SAS token was applied, if so EVENTHUBRECEIVER_NOT_ALLOWED shall be returned.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_404: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall invoke EventHubAuthCBS_SASTokenParse to parse eventHubRefreshSasToken.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_405: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBRECEIVER_ERROR if EventHubAuthCBS_SASTokenParse returns NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_406: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall validate if the eventHubRefreshSasToken's URI is exactly the same as the one used when EventHubReceiver_LL_CreateFromSASToken was invoked by using API STRING_compare.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_407: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBRECEIVER_ERROR if eventHubRefreshSasToken is not compatible.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_408: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall construct a new STRING to hold the ext SAS token using API STRING_construct with parameter eventHubSasToken for the refresh operation to be done in `EventHubReceiver_LL_DoWork`.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_410: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBRECEIVER_OK on success.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_411: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall return EVENTHUBRECEIVER_ERROR on failure.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_412: \[**`EventHubReceiver_LL_RefreshSASTokenAsync` shall invoke EventHubAuthCBS_Config_Destroy to free up the parsed configuration of eventHubRefreshSasToken if required.**\]**

### EventHubReceiver_LL_Destroy
```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_Destroy, EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverHandle);
```
**SRS_EVENTHUBRECEIVER_LL_29_200: \[**`EventHubReceiver_LL_Destroy` return immediately if eventHubReceiverHandle is NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_201: \[**`EventHubReceiver_LL_Destroy` shall tear down connection with the event hub.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_202: \[**`EventHubReceiver_LL_Destroy` shall terminate the usage of the EVENTHUBRECEIVER_LL_STRUCT and cleanup all associated resources.**\]**

### EventHubReceiver_LL_ReceiveFromStartTimestampAsync and EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync
```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveFromStartTimestampAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle, 
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback, 
    void*, onEventReceiveUserContext, 
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
    uint64_t, startTimestampInSec
);

MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveFromStartTimestampWithTimeoutAsync,
    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback,
    void*, onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
        uint64_t, startTimestampInSec,
        unsigned int, waitTimeoutInMs
);
```
**SRS_EVENTHUBRECEIVER_LL_29_301: \[**`EventHubReceiver_LL_ReceiveFromStartTimestamp*Async` shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverHandle, onEventReceiveErrorCallback, onEventReceiveErrorCallback are NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_302: \[**`EventHubReceiver_LL_ReceiveFromStartTimestamp*Async` shall record the callbacks and contexts in the EVENTHUBRECEIVER_LL_STRUCT.**\]** 
**SRS_EVENTHUBRECEIVER_LL_29_303: \[**If tickcounter_get_current_ms fails, EventHubReceiver_LL_ReceiveFromStartTimestamp*Async shall fail and return EVENTHUBRECEIVER_ERROR.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_304: \[**Create a filter string using format "apache.org:selector-filter:string" and "amqp.annotation.x-opt-enqueuedtimeutc > startTimestampInSec" using STRING_sprintf**\]**
**SRS_EVENTHUBRECEIVER_LL_29_305: \[**If filter string create fails then a log message will be logged and an error code of EVENTHUBRECEIVER_ERROR shall be returned.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_306: \[**Initialize timeout value (zero if no timeout) and a current timestamp of now.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_307: \[**`EventHubReceiver_LL_ReceiveFromStartTimestamp*Async` shall return an error code of EVENTHUBRECEIVER_NOT_ALLOWED if a user called EventHubReceiver_LL_Receive* more than once on the same handle.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_308: \[**Otherwise `EventHubReceiver_LL_ReceiveFromStartTimestamp*Async` shall succeed and return EVENTHUBRECEIVER_OK.**\]**

### EventHubReceiver_LL_DoWork
```c
MOCKABLE_FUNCTION(, void, EventHubReceiver_LL_DoWork, EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle);
```
#### Parameter Verification
**SRS_EVENTHUBRECEIVER_LL_29_450: \[**`EventHubReceiver_LL_DoWork` shall return immediately if the supplied handle is NULL**\]**

#### General
**SRS_EVENTHUBRECEIVER_LL_29_451: \[**`EventHubReceiver_LL_DoWork` shall initialize and bring up the uAMQP stack if it has not already brought up**\]**
**SRS_EVENTHUBRECEIVER_LL_29_452: \[**`EventHubReceiver_LL_DoWork` shall perform SAS token handling. **\]**
**SRS_EVENTHUBRECEIVER_LL_29_453: \[**`EventHubReceiver_LL_DoWork` shall initialize the uAMQP Message Receiver stack if it has not already brought up. **\]** 
**SRS_EVENTHUBRECEIVER_LL_29_454: \[**`EventHubReceiver_LL_DoWork` shall create a message receiver if not already created. **\]** 
**SRS_EVENTHUBRECEIVER_LL_29_455: \[**`EventHubReceiver_LL_DoWork` shall invoke connection_dowork **\]** 

#### uAMQP Stack Initialize

**SRS_EVENTHUBRECEIVER_LL_29_501: \[**The SASL interface to be passed into saslmechanism_create shall be obtained by calling saslmssbcbs_get_interface.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_502: \[**A SASL mechanism shall be created by calling saslmechanism_create with the interface obtained above.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_503: \[**If saslmssbcbs_get_interface fails then a log message will be logged and the function returns immediately.**\]**

**SRS_EVENTHUBRECEIVER_LL_29_504: \[**A TLS IO shall be created by calling xio_create using TLS port 5671 and host name obtained from the connection string**\]**
**SRS_EVENTHUBRECEIVER_LL_29_505: \[**The interface passed to xio_create shall be obtained by calling platform_get_default_tlsio.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_506: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**

**SRS_EVENTHUBRECEIVER_LL_29_507: \[**The SASL client IO interface shall be obtained using `saslclientio_get_interface_description`**\]**
**SRS_EVENTHUBRECEIVER_LL_29_508: \[**A SASL client IO shall be created by calling xio_create using TLS IO interface created previously and the SASL  mechanism created earlier. The SASL client IO interface to be used will be the one obtained above.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_509: \[**If xio_create fails then a log message will be logged and the function returns immediately.**\]**

**SRS_EVENTHUBRECEIVER_LL_29_510: \[**An AMQP connection shall be created by calling connection_create and passing as arguments the SASL client IO handle created previously, hostname, connection name and NULL for the new session handler end point and context.**\]** 
**SRS_EVENTHUBRECEIVER_LL_29_511: \[**If connection_create fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_512: \[**Connection tracing shall be called with the current value of the tracing flag**\]**

**SRS_EVENTHUBRECEIVER_LL_29_513: \[**An AMQP session shall be created by calling session_create and passing as arguments the connection handle, and NULL for the new link handler and context.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_514: \[**If session_create fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_515: \[**Configure the session incoming window by calling `session_set_incoming_window` and set value to INTMAX.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_516: \[**If `saslclientio_get_interface_description` returns NULL, a log message will be logged and the function returns immediately.**\]**

**SRS_EVENTHUBRECEIVER_LL_29_521: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, initialize a `EVENTHUBAUTH_CBS_CONFIG` structure params hostName, eventHubPath, receiverConsumerGroup, receiverPartitionId, sharedAccessKeyName, sharedAccessKey using previously set values. Set senderPublisherId to NULL, sasTokenAuthFailureTimeoutInSecs to the client wait timeout value, sasTokenExpirationTimeInSec to 3600, sasTokenRefreshPeriodInSecs to 4800, mode as EVENTHUBAUTH_MODE_RECEIVER and credential as EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_522: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, use the `EVENTHUBAUTH_CBS_CONFIG` obtained earlier from parsing the SAS token in EventHubAuthCBS_Create.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_524: \[**`EventHubAuthCBS_Create` shall be invoked using the config structure reference and the session handle created earlier.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_525: \[**If `EventHubAuthCBS_Create` returns NULL, a log message will be logged and the function returns immediately.**\]**

#### SAS Token handling
**SRS_EVENTHUBRECEIVER_LL_29_541: \[**`EventHubAuthCBS_GetStatus` shall be invoked to obtain the authorization status.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_542: \[**If status is EVENTHUBAUTH_STATUS_FAILURE or EVENTHUBAUTH_STATUS_EXPIRED any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_FAILURE the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_543: \[**If status is EVENTHUBAUTH_STATUS_TIMEOUT, any registered client error callback shall be invoked with error code EVENTHUBRECEIVER_SASTOKEN_AUTH_TIMEOUT and `EventHubReceiver_LL_DoWork` shall bring down AMQP stack so that it can be created again if needed in EventHubReceiver_LL_DoWork.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_544: \[**If status is EVENTHUBAUTH_STATUS_IN_PROGRESS, `connection_dowork` shall be invoked to perform work to establish/refresh the SAS token.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_545: \[**If status is EVENTHUBAUTH_STATUS_REFRESH_REQUIRED, `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be NULL.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_546: \[**If status is EVENTHUBAUTH_STATUS_IDLE, `EventHubAuthCBS_Authenticate` shall be invoked to create and install the SAS token.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_547: \[**If status is EVENTHUBAUTH_STATUS_OK and an Ext refresh SAS Token was supplied by the user,  `EventHubAuthCBS_Refresh` shall be invoked to refresh the SAS token. Parameter extSASToken should be the refresh ext SAS token.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_548: \[**If an error is seen, the AMQP stack shall be brought down so that it can be created again if needed in `EventHubReceiver_LL_DoWork`.**\]**

#### uAMQP Message Receiver Stack Initialize
**SRS_EVENTHUBRECEIVER_LL_29_561: \[**A filter_set shall be created and initialized using key "apache.org:selector-filter:string" and value as the query filter created previously.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_562: \[**If creation of the filter_set fails, then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_563: \[**If a failure is observed during source creation and initialization, then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_564: \[**If messaging_create_target fails, then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_565: \[**The message receiver 'source' shall be created using source_create.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_566: \[**An AMQP link for the to be created message receiver shall be created by calling link_create with role as role_receiver and name as "receiver-link"**\]**
**SRS_EVENTHUBRECEIVER_LL_29_567: \[**The message receiver link 'source' shall be created using API amqpvalue_create_source**\]**
**SRS_EVENTHUBRECEIVER_LL_29_568: \[**The message receiver link 'source' filter shall be initialized by calling source_set_filter and using the filter_set created earlier.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_569: \[**The message receiver link 'source' address shall be initialized using the partition target address created earlier.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_570: \[**The message receiver link target shall be created using messaging_create_target with address obtained from the partition target address created earlier.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_571: \[**If link_create fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_572: \[**Configure the link settle mode by calling link_set_rcv_settle_mode and set value to receiver_settle_mode_first.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_573: \[**If link_set_rcv_settle_mode fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_574: \[**The message size shall be set to 256K by calling link_set_max_message_size.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_575: \[**If link_set_max_message_size fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_576: \[**A message receiver shall be created by calling messagereceiver_create and passing as arguments the link handle, a state changed callback and context.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_577: \[**If messagereceiver_create fails then a log message will be logged and the function returns immediately.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_578: \[**The created message receiver shall be transitioned to OPEN by calling messagereceiver_open.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_579: \[**If messagereceiver_open fails then a log message will be logged and the function returns immediately.**\]**

##### AMQP Stack DeInitialize
**SRS_EVENTHUBRECEIVER_LL_29_601: \[**`EventHubReceiver_LL_DoWork` shall do the work to tear down the AMQP stack when a user had called `EventHubReceiver_LL_ReceiveEndAsync`.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_602: \[**All pending message data not reported to the calling client shall be freed by calling messagereceiver_close and messagereceiver_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_603: \[**The link shall be freed by calling link_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_604: \[**`EventHubAuthCBS_Destroy` shall be called to destroy the event hub auth handle.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_605: \[**The session shall be freed by calling session_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_606: \[**The connection shall be freed by calling connection_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_607: \[**The SASL client IO shall be freed by calling xio_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_608: \[**The TLS IO shall be freed by calling xio_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_609: \[**The SASL mechanism shall be freed by calling saslmechanism_destroy.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_610: \[**The filter string shall be freed by STRING_delete.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_611: \[**Upon Success, `EventHubReceiver_LL_DoWork` shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_OK.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_612: \[**Upon failure, `EventHubReceiver_LL_DoWork` shall invoke the onEventReceiveEndCallback along with onEventReceiveEndUserContext with result code EVENTHUBRECEIVER_ERROR.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_613: \[**If any ext refresh SAS token is present, it shall be called to destroyed by calling STRING_delete.**\]**

##### Creation of the message receiver
**SRS_EVENTHUBRECEIVER_LL_29_620: \[**`EventHubReceiver_LL_DoWork` shall setup the message receiver_create by passing in `EHR_LL_OnStateChanged` as the ON_MESSAGE_RECEIVER_STATE_CHANGED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_create is called.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_621: \[**`EventHubReceiver_LL_DoWork` shall open the message receiver_create by passing in `EHR_LL_OnMessageReceived` as the ON_MESSAGE_RECEIVED parameter and the EVENTHUBRECEIVER_LL_HANDLE as the callback context for when messagereceiver_open is called.**\]**

##### EHR_LL_OnStateChanged
```c
static void EHR_LL_OnStateChanged(const void* context, MESSAGE_RECEIVER_STATE newState, MESSAGE_RECEIVER_STATE previousState)
```
**SRS_EVENTHUBRECEIVER_LL_29_630: \[**When `EHR_LL_OnStateChanged` is invoked, obtain the EventHubReceiverLL handle from the context and update the message receiver state with the new state received in the callback.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_631: \[**If the new state is MESSAGE_RECEIVER_STATE_ERROR, and previous state is not MESSAGE_RECEIVER_STATE_ERROR, `EHR_LL_OnStateChanged` shall invoke the user supplied error callback along with error callback context`**\]**


##### EHR_LL_OnMessageReceived
```c
static AMQP_VALUE EHR_LL_OnMessageReceived(const void* context, MESSAGE_HANDLE message)
```
**SRS_EVENTHUBRECEIVER_LL_29_641: \[**When `EHR_LL_OnMessageReceived` is invoked, message_get_body_amqp_data shall be called to obtain the data into a BINARY_DATA buffer.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_642: \[**`EHR_LL_OnMessageReceived` shall create a EVENT_DATA handle using EventData_CreateWithNewMemory and pass in the buffer data pointer and size as arguments.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_643: \[**`EHR_LL_OnMessageReceived` shall obtain the application properties using message_get_application_properties() and populate the EVENT_DATA handle map with these key value pairs.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_644: \[**`EHR_LL_OnMessageReceived` shall obtain event data specific properties using message_get_message_annotations() and populate the EVENT_DATA handle with these properties.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_645: \[**If any errors are seen `EHR_LL_OnMessageReceived` shall reject the incoming message by calling messaging_delivery_rejected() and return.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_646: \[**`EHR_LL_OnMessageReceived` shall invoke the user registered onMessageReceive callback with status code EVENTHUBRECEIVER_OK, the EVENT_DATA handle and the context passed in by the user.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_647: \[**After `EHR_LL_OnMessageReceived` invokes the user callback, messaging_delivery_accepted shall be called.**\]**

##### Timeout Management
**SRS_EVENTHUBRECEIVER_LL_29_661: \[**`EventHubReceiver_LL_DoWork` shall manage timeouts as long as the user specified timeout value is non zero **\]**
**SRS_EVENTHUBRECEIVER_LL_29_662: \[**`EventHubReceiver_LL_DoWork` shall check if a message was received, if so, reset the last activity time to the current time i.e. now**\]** 
**SRS_EVENTHUBRECEIVER_LL_29_663: \[**If a message was not received, check if the time now minus the last activity time is greater than or equal to the user specified timeout. If greater, the user registered callback is invoked along with the user supplied context with status code EVENTHUBRECEIVER_TIMEOUT. Last activity time shall be updated to the current time i.e. now.**\]**


### EventHubReceiver_LL_SetConnectionTracing
```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_SetConnectionTracing,
                    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle, bool, traceEnabled);
```
**SRS_EVENTHUBRECEIVER_LL_29_800: \[**`EventHubReceiver_LL_SetConnectionTracing` shall fail and return EVENTHUBRECEIVER_INVALID_ARG if parameter eventHubReceiverLLHandle.**\]** 
**SRS_EVENTHUBRECEIVER_LL_29_801: \[**`EventHubReceiver_LL_SetConnectionTracing` shall save the value of tracingOnOff in eventHubReceiverLLHandle**\]**
**SRS_EVENTHUBRECEIVER_LL_29_802: \[**If an active connection has been setup, `EventHubReceiver_LL_SetConnectionTracing` shall be called with the value of connection_set_trace tracingOnOff**\]**
**SRS_EVENTHUBRECEIVER_LL_29_803: \[**`Upon success, EventHubReceiver_LL_SetConnectionTracing` shall return EVENTHUBRECEIVER_OK**\]**

### 
```c
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_LL_ReceiveEndAsync,
                    EVENTHUBRECEIVER_LL_HANDLE, eventHubReceiverLLHandle,
                    EVENTHUBRECEIVER_ASYNC_END_CALLBACK, onEventReceiveEndCallback,
                    void*, onEventReceiveEndUserContext);
```
**SRS_EVENTHUBRECEIVER_LL_29_900: \[**`EventHubReceiver_LL_ReceiveEndAsync` shall validate arguments, in case they are invalid, error code EVENTHUBRECEIVER_INVALID_ARG will be returned.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_901: \[**`EventHubReceiver_LL_ReceiveEndAsync` shall check if a receiver connection is currently active. If no receiver is active, EVENTHUBRECEIVER_NOT_ALLOWED shall be returned and a message will be logged.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_902: \[**`EventHubReceiver_LL_ReceiveEndAsync` save off the user callback and context and defer the UAMQP stack tear down to `EventHubReceiver_LL_DoWork`.**\]**
**SRS_EVENTHUBRECEIVER_LL_29_903: \[**Upon Success, `EventHubReceiver_LL_ReceiveEndAsync` shall return EVENTHUBRECEIVER_OK.**\]**