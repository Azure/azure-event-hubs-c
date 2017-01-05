#EventData Requirements
 
##Overview

EventHubAuth is a module that can be used for SAS token creation, establishment, token expiration and refresh required for EventHub IO is taken care by this module. 
This utility module is not meant to be used directly by clients, rather this is suited for internal consumption.

##References

Event Hubs Authentication [https://docs.microsoft.com/en-us/azure/event-hubs/event-hubs-authentication-and-security-model-overview]

##Exposed API

```c

//** EVENTHUBAUTH_RESULT_OK              Indicates success */
//** EVENTHUBAUTH_RESULT_INVALID_ARG     Indicates invalid function arguments were passed in */
//** EVENTHUBAUTH_RESULT_NOT_PERMITED    Indicates that the operation is not permitted */
//** EVENTHUBAUTH_RESULT_ERROR           Indicates an error has occurred in operation */
#define EVENTHUBAUTH_RESULT_VALUES              \
        EVENTHUBAUTH_RESULT_OK,                 \
        EVENTHUBAUTH_RESULT_INVALID_ARG,        \
        EVENTHUBAUTH_RESULT_NOT_PERMITED,       \
        EVENTHUBAUTH_RESULT_ERROR

DEFINE_ENUM(EVENTHUBAUTH_RESULT, EVENTHUBAUTH_RESULT_VALUES);


//** EVENTHUBAUTH_STATUS_OK               Status indicates that SAS token has been authorized and is valid */
//** EVENTHUBAUTH_STATUS_IDLE             Status indicates that SAS token has been not been created and no authorization has been achieved */
//** EVENTHUBAUTH_STATUS_IN_PROGRESS      Token authentication is in progress */
//** EVENTHUBAUTH_STATUS_TIMEOUT          Token authentication operation exceeded timeout period */
//** EVENTHUBAUTH_STATUS_REFRESH_REQUIRED A new token will need to be to maintain authorization */
//** EVENTHUBAUTH_STATUS_FAILURE          Runtime error taken place during token authentication operation */
#define EVENTHUBAUTH_STATUS_VALUES              \
        EVENTHUBAUTH_STATUS_OK,                 \
        EVENTHUBAUTH_STATUS_IDLE,               \
        EVENTHUBAUTH_STATUS_IN_PROGRESS,        \
        EVENTHUBAUTH_STATUS_TIMEOUT,            \
        EVENTHUBAUTH_STATUS_REFRESH_REQUIRED,   \
        EVENTHUBAUTH_STATUS_FAILURE

DEFINE_ENUM(EVENTHUBAUTH_STATUS, EVENTHUBAUTH_STATUS_VALUES);

/** EVENTHUBAUTH_MODE_UNKNOWN    Unknown Mode */
/** EVENTHUBAUTH_MODE_SENDER     Authorization requested for EventHub Sender */
/** EVENTHUBAUTH_MODE_RECEIVER   Authorization requested for EventHub Receiver */
#define EVENTHUBAUTH_MODE_VALUES                \
        EVENTHUBAUTH_MODE_UNKNOWN,              \
        EVENTHUBAUTH_MODE_SENDER,               \
        EVENTHUBAUTH_MODE_RECEIVER       

DEFINE_ENUM(EVENTHUBAUTH_MODE, EVENTHUBAUTH_MODE_VALUES);

//** EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO - SAS token creation, refresh and deletion to be taken care of automatically */
#define EVENTHUBAUTH_CREDENTIAL_TYPE_VALUES          \
        EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO

DEFINE_ENUM(EVENTHUBAUTH_CREDENTIAL_TYPE, EVENTHUBAUTH_CREDENTIAL_TYPE_VALUES);

typedef struct EVENTHUBAUTH_CBS_CONFIG_TAG
{
    const char* hostName;                           //** EventHub Host name extracted from the connection string. */
    const char* eventHubPath;                       //** EventHub Path. */
    const char* receiverConsumerGroup;              //** Consumer Group value required for an EventHub Receiver. Should be set to NULL otherwise. */
    const char* receiverPartitionId;                //** Partition Id value required for an EventHub Receiver. Should be set to NULL otherwise. */
    const char* senderPublisherId;                  //** Sender Publisher ID value required for an EventHub Sender. Should be set to NULL otherwise. */
    const char* sharedAccessKeyName;                //** Share Access Key Name from the connection string. */
    const char* sharedAccessKey;                    //** Share Access Key from the connection string. */
    unsigned int sasTokenExpirationTimeInSec;       //** Time in seconds for the lifetime of a SAS Token since its creation. */
    unsigned int sasTokenRefreshPeriodInSecs;       //** Time in seconds for refreshing the SAS token before it expires. This has to be lesser than sasTokenExpirationTimeInSec. */
    unsigned int sasTokenAuthFailureTimeoutInSecs;  //** Timeout value in seconds for establishing token authentication. If the timeout period is exceeded status is changed to EVENTHUBAUTH_STATUS_TIMEOUT. */
    EVENTHUBAUTH_MODE mode;                         //** Mode value to distinguish type of EventHub IO. */
    EVENTHUBAUTH_CREDENTIAL_TYPE credential;        //** Type of credential. */
} EVENTHUBAUTH_CBS_CONFIG;

typedef struct EVENTHUBAUTH_CBS_STRUCT_TAG* EVENTHUBAUTH_CBS_HANDLE;

MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_HANDLE, EventHubAuthCBS_Create, EVENTHUBAUTH_CBS_CONFIG*, eventHubAuthConfig, SESSION_HANDLE, cbsSessionHandle);
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Destroy, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Authenticate, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Reset, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Refresh, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_GetStatus, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, EVENTHUBAUTH_STATUS*, returnStatus);

```

###EventHubAuthCBS_Create

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_HANDLE, EventHubAuthCBS_Create, EVENTHUBAUTH_CBS_CONFIG*, eventHubAuthConfig, SESSION_HANDLE, cbsSessionHandle);
```
**SRS_EVENTHUB_AUTH_29_001: \[**`EventHubAuthCBS_Create` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthConfig or cbsSessionHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_002: \[**`EventHubAuthCBS_Create` shall validate the configuration parameters passed into eventHubAuthConfig. If either fields hostName, eventHubPath, sharedAccessKeyName, sharedAccessKey are NULL, EVENTHUBAUTH_RESULT_INVALID_ARG shall be returned. If mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER, EVENTHUBAUTH_RESULT_INVALID_ARG shall be returned. If credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, if sasTokenExpirationTimeInSec is zero or sasTokenRefreshPeriodInSecs is greater than sasTokenExpirationTimeInSec, EVENTHUBAUTH_RESULT_INVALID_ARG shall be returned. If mode is EVENTHUBAUTH_MODE_SENDER and senderPublisherId is NULL then EVENTHUBAUTH_RESULT_INVALID_ARG shall be returned. If mode is EVENTHUBAUTH_MODE_RECEIVER and either receiverConsumerGroup or receiverPartitionId is NULL, EVENTHUBAUTH_RESULT_INVALID_ARG shall be returned.**\]**

**SRS_EVENTHUB_AUTH_29_003: \[**`EventHubAuthCBS_Create` shall allocate new memory to store the specified configuration data by using API malloc.**\]**

**SRS_EVENTHUB_AUTH_29_004: \[**`EventHubAuthCBS_Create` shall construct a STRING_HANDLE to store eventHubAuthConfig->sharedAccessKeyName using API STRING_construct.**\]**

**SRS_EVENTHUB_AUTH_29_005: \[**`EventHubAuthCBS_Create` shall construct a URI of the format "sb://" using API STRING_construct.**\]**

**SRS_EVENTHUB_AUTH_29_006: \[**`EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->hostName, "/", eventHubAuthConfig->eventHubPath using API STRING_concat respectively.**\]**

**SRS_EVENTHUB_AUTH_29_007: \[**If mode is EVENTHUBAUTH_MODE_RECEIVER, `EventHubAuthCBS_Create` shall further concatenate the URI with "/ConsumerGroups/", eventHubAuthConfig->receiverConsumerGroup, "/Partitions/" and eventHubAuthConfig->receiverPartitionId using API STRING_concat respectively.**\]**

**SRS_EVENTHUB_AUTH_29_008: \[**If mode is EVENTHUBAUTH_MODE_SENDER, `EventHubAuthCBS_Create` shall further concatenate the URI with "/publishers/" and eventHubAuthConfig->senderPublisherId using API STRING_concat respectively.**\]**

**SRS_EVENTHUB_AUTH_29_009: \[**`EventHubAuthCBS_Create` shall create a new STRING_HANDLE by encoding the URI using API URL_Encode.**\]**

**SRS_EVENTHUB_AUTH_29_010: \[**`EventHubAuthCBS_Create` shall create a BUFFER_HANDLE using API BUFFER_create and pass in eventHubAuthConfig->sharedAccessKey and its length.**\]**

**SRS_EVENTHUB_AUTH_29_011: \[**`EventHubAuthCBS_Create` shall create a new STRING_HANDLE by Base64 encoding the buffer handle created above by using API Base64_Encode.**\]**

**SRS_EVENTHUB_AUTH_29_012: \[**`EventHubAuthCBS_Create` shall initialize a CBS handle by calling API cbs_create.**\]**

**SRS_EVENTHUB_AUTH_29_013: \[**`EventHubAuthCBS_Create` shall open the CBS handle by calling API cbs_open.**\]**

**SRS_EVENTHUB_AUTH_29_014: \[**`EventHubAuthCBS_Create` shall initialize its internal data structures using the configuration data passed in.**\]**

**SRS_EVENTHUB_AUTH_29_015: \[**`EventHubAuthCBS_Create` shall return a non-NULL handle encapsulating the storage of the data provided.**\]**

**SRS_EVENTHUB_AUTH_29_016: \[**For all other errors, `EventHubAuthCBS_Create` shall return NULL.**\]** 


###EventHubAuthCBS_Destroy
```c
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Destroy, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```

**SRS_EVENTHUB_AUTH_29_100: \[**`EventHubAuthCBS_Destroy` shall return immediately eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_101: \[**`EventHubAuthCBS_Destroy` shall destroy the CBS handle if not null by using API cbs_destroy.**\]**

**SRS_EVENTHUB_AUTH_29_102: \[**`EventHubAuthCBS_Destroy` shall destroy the SAS token if not NULL by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_103: \[**`EventHubAuthCBS_Destroy` shall destroy the Base64 encoded shared access key by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_104: \[**`EventHubAuthCBS_Destroy` shall destroy the encoded URI by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_105: \[**`EventHubAuthCBS_Destroy` shall destroy the URI by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_106: \[**`EventHubAuthCBS_Destroy` shall destroy the shared access key name by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_107: \[**`EventHubAuthCBS_Destroy` shall free the internal data structure allocated earlier using API free.**\]**

###EventHubAuthCBS_Authenticate

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Authenticate, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```

**SRS_EVENTHUB_AUTH_29_201: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_202: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_STATUS_OK immediately if the status is not EVENTHUBAUTH_STATUS_IDLE or EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_203: \[**`EventHubAuthCBS_Authenticate` shall delete the existing SAS token STRING_HANDLE if not NULL by calling STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_204: \[**`EventHubAuthCBS_Authenticate` shall create a new SAS token STRING_HANDLE using API SASToken_Create and passing in Base64 encoded shared access key STRING_HANDLE, encoded URI STRING_HANDLE, shared access key name STRING_HANDLE and expiration time in seconds from epoch.**\]**

**SRS_EVENTHUB_AUTH_29_205: \[**`EventHubAuthCBS_Authenticate` shall obtain the underlying C string buffer of STRING_HANDLE URI by calling STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_206: \[**`EventHubAuthCBS_Authenticate` shall obtain the underlying C string buffer of STRING_HANDLE SAS token by calling STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_207: \[**`EventHubAuthCBS_Authenticate` shall establish (put) the new token by calling API cbs_put_token and passing in the CBS handle, "servicebus.windows.net:sastoken", URI string buffer, SAS token string buffer, OnCBSPutTokenOperationComplete and eventHubAuthHandle.**\]**

**SRS_EVENTHUB_AUTH_29_209: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_OK on success.**\]**

**SRS_EVENTHUB_AUTH_29_210: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**

####OnCBSPutTokenOperationComplete

```c
static void OnCBSPutTokenOperationComplete(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description)
```
**SRS_EVENTHUB_AUTH_29_211: \[**`OnCBSPutTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_OK if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**

**SRS_EVENTHUB_AUTH_29_212: \[**`OnCBSPutTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_FAILURE if cbs_operation_result is not CBS_OPERATION_RESULT_OK and a message shall be logged.**\]**

###EventHubAuthCBS_Reset

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Reset, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```
**SRS_EVENTHUB_AUTH_29_301: \[**`EventHubAuthCBS_Reset` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_302: \[**`EventHubAuthCBS_Reset` shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if the current state EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**

**SRS_EVENTHUB_AUTH_29_303: \[**`EventHubAuthCBS_Reset` shall update the status to EVENTHUBAUTH_STATUS_IDLE and return EVENTHUBAUTH_RESULT_OK immediately if the current state is in EVENTHUBAUTH_STATUS_TIMEOUT, EVENTHUBAUTH_STATUS_FAILURE.**\]**

**SRS_EVENTHUB_AUTH_29_304: \[**`EventHubAuthCBS_Reset` shall obtain the underlying C string buffer of STRING_HANDLE URI by calling STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_305: \[**`EventHubAuthCBS_Reset` shall attempt to delete the existing SAS token by calling cbs_delete_token by passing in the CBS handle, URI string buffer, "servicebus.windows.net:sastoken", OnCBSDeleteTokenOperationComplete callback and eventHubAuthHandle. **\]**

**SRS_EVENTHUB_AUTH_29_306: \[**`EventHubAuthCBS_Reset` shall return EVENTHUBAUTH_RESULT_OK on success.**\]**

**SRS_EVENTHUB_AUTH_29_307: \[**`EventHubAuthCBS_Reset` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**


####OnCBSDeleteTokenOperationComplete

```c
static void OnCBSDeleteTokenOperationComplete(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description)
```
**SRS_EVENTHUB_AUTH_29_308: \[**`OnCBSDeleteTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_IDLE if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**

**SRS_EVENTHUB_AUTH_29_309: \[**`OnCBSDeleteTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_FAILURE if cbs_operation_result is not CBS_OPERATION_RESULT_OK and a message shall be logged.**\]**

###EventHubAuthCBS_Refresh

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Refresh, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```
**SRS_EVENTHUB_AUTH_29_400: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_401: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_ERROR if the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_402: \[**`EventHubAuthCBS_Refresh` shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**

**SRS_EVENTHUB_AUTH_29_403: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_OK on success.**\]**

**SRS_EVENTHUB_AUTH_29_404: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**


###EventHubAuthCBS_GetStatus

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_GetStatus, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, EVENTHUBAUTH_STATUS*, returnStatus);
```
**SRS_EVENTHUB_AUTH_29_501: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle or returnStatus is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_502: \[**`EventHubAuthCBS_GetStatus` shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occured the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_503: \[**`EventHubAuthCBS_GetStatus` shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occured the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**

**SRS_EVENTHUB_AUTH_29_504: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**

**SRS_EVENTHUB_AUTH_29_505: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
