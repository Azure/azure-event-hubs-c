#EventData Requirements
 
##Overview

EventHubAuth is a module that can be used for SAS token creation, establishment, token expiration and refresh required for EventHub IO is taken care by this module. 
This utility module is not meant to be used directly by clients, rather this is suited for internal consumption.

##References

Event Hubs Authentication [https://docs.microsoft.com/en-us/azure/event-hubs/event-hubs-authentication-and-security-model-overview]

##Exposed API

```c

#define EVENTHUBAUTH_RESULT_VALUES              \
        EVENTHUBAUTH_RESULT_OK,                 \
        EVENTHUBAUTH_RESULT_INVALID_ARG,        \
        EVENTHUBAUTH_RESULT_NOT_PERMITED,       \
        EVENTHUBAUTH_RESULT_ERROR

DEFINE_ENUM(EVENTHUBAUTH_RESULT, EVENTHUBAUTH_RESULT_VALUES);

#define EVENTHUBAUTH_STATUS_VALUES              \
        EVENTHUBAUTH_STATUS_OK,                 \
        EVENTHUBAUTH_STATUS_IDLE,               \
        EVENTHUBAUTH_STATUS_IN_PROGRESS,        \
        EVENTHUBAUTH_STATUS_TIMEOUT,            \
        EVENTHUBAUTH_STATUS_REFRESH_REQUIRED,   \
        EVENTHUBAUTH_STATUS_EXPIRED,            \
        EVENTHUBAUTH_STATUS_FAILURE

DEFINE_ENUM(EVENTHUBAUTH_STATUS, EVENTHUBAUTH_STATUS_VALUES);

#define EVENTHUBAUTH_MODE_VALUES                \
        EVENTHUBAUTH_MODE_UNKNOWN,              \
        EVENTHUBAUTH_MODE_SENDER,               \
        EVENTHUBAUTH_MODE_RECEIVER

DEFINE_ENUM(EVENTHUBAUTH_MODE, EVENTHUBAUTH_MODE_VALUES);

#define EVENTHUBAUTH_CREDENTIAL_TYPE_VALUES         \
        EVENTHUBAUTH_CREDENTIAL_TYPE_UNKNOWN,       \
        EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT,  \
        EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO

DEFINE_ENUM(EVENTHUBAUTH_CREDENTIAL_TYPE, EVENTHUBAUTH_CREDENTIAL_TYPE_VALUES);

typedef struct EVENTHUBAUTH_CBS_CONFIG_TAG
{
    STRING_HANDLE hostName;
    STRING_HANDLE eventHubPath;
    STRING_HANDLE receiverConsumerGroup;
    STRING_HANDLE receiverPartitionId;
    STRING_HANDLE senderPublisherId;
    STRING_HANDLE sharedAccessKeyName;
    STRING_HANDLE sharedAccessKey;
    STRING_HANDLE extSASToken;
    STRING_HANDLE extSASTokenURI;
    uint64_t extSASTokenExpTSInEpochSec;
    unsigned int sasTokenExpirationTimeInSec;
    unsigned int sasTokenRefreshPeriodInSecs;
    unsigned int sasTokenAuthFailureTimeoutInSecs
    EVENTHUBAUTH_MODE mode;
    EVENTHUBAUTH_CREDENTIAL_TYPE credential;      
} EVENTHUBAUTH_CBS_CONFIG;

typedef struct EVENTHUBAUTH_CBS_STRUCT_TAG* EVENTHUBAUTH_CBS_HANDLE;

MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_HANDLE, EventHubAuthCBS_Create, const EVENTHUBAUTH_CBS_CONFIG*, eventHubAuthConfig, SESSION_HANDLE, cbsSessionHandle);
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Destroy, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Authenticate, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Refresh, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, STRING_HANDLE, extSASToken);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_GetStatus, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, EVENTHUBAUTH_STATUS*, returnStatus);
MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_CONFIG*, EventHubAuthCBS_SASTokenParse, const char*, sasToken);
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Config_Destroy, EVENTHUBAUTH_CBS_CONFIG*, cfg);

```

###EventHubAuthCBS_Create

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_HANDLE, EventHubAuthCBS_Create, EVENTHUBAUTH_CBS_CONFIG*, eventHubAuthConfig, SESSION_HANDLE, cbsSessionHandle);
```
**SRS_EVENTHUB_AUTH_29_001: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig or cbsSessionHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_002: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER.**\]**

**SRS_EVENTHUB_AUTH_29_003: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO or EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**

**SRS_EVENTHUB_AUTH_29_004: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->hostName or eventHubAuthConfig->eventHubPath are NULL.**\]**

**SRS_EVENTHUB_AUTH_29_005: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sasTokenExpirationTimeInSec or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is zero or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is greater than eventHubAuthConfig->sasTokenExpirationTimeInSec.**\]**

**SRS_EVENTHUB_AUTH_29_006: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sharedAccessKeyName or eventHubAuthConfig->sharedAccessKey are NULL.**\]**

**SRS_EVENTHUB_AUTH_29_007: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->mode is EVENTHUBAUTH_MODE_RECEIVER and eventHubAuthConfig->receiverConsumerGroup or eventHubAuthConfig->receiverPartitionId is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_008: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->mode is EVENTHUBAUTH_MODE_SENDER and eventHubAuthConfig->senderPublisherId is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_009: \[**`EventHubAuthCBS_Create` shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if eventHubAuthConfig->extSASToken is NULL or eventHubAuthConfig->extSASTokenURI is NULL or eventHubAuthConfig->extSASTokenExpTSInEpochSec equals 0.**\]**

**SRS_EVENTHUB_AUTH_29_010: \[**`EventHubAuthCBS_Create` shall allocate new memory to store the specified configuration data by using API malloc.**\]**

**SRS_EVENTHUB_AUTH_29_011: \[**For all errors, `EventHubAuthCBS_Create` shall return NULL.**\]** 

**SRS_EVENTHUB_AUTH_29_012: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Create` shall clone the external SAS token using API STRING_clone.**\]**

**SRS_EVENTHUB_AUTH_29_013: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Create` shall clone the external SAS token URI using API STRING_clone.**\]**

**SRS_EVENTHUB_AUTH_29_014: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall construct a URI of the format "sb://" using API STRING_construct.**\]**

**SRS_EVENTHUB_AUTH_29_015: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->hostName using API STRING_concat_with_STRING.**\]**

**SRS_EVENTHUB_AUTH_29_016: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall further concatenate the URI with "/" using API STRING_concat.**\]**

**SRS_EVENTHUB_AUTH_29_017: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->eventHubPath using API STRING_concat_with_STRING.**\]**

**SRS_EVENTHUB_AUTH_29_019: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, `EventHubAuthCBS_Create` shall further concatenate the URI with "/ConsumerGroups/" using API STRING_concat.**\]**  

**SRS_EVENTHUB_AUTH_29_020: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, `EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->receiverConsumerGroup using API STRING_concat_with_STRING.**\]**

**SRS_EVENTHUB_AUTH_29_021: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, `EventHubAuthCBS_Create` shall further concatenate the URI with "/Partitions/" using API STRING_concat.**\]**  

**SRS_EVENTHUB_AUTH_29_022: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, `EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->receiverPartitionId using API STRING_concat_with_STRING.**\]**

**SRS_EVENTHUB_AUTH_29_023: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, `EventHubAuthCBS_Create` shall further concatenate the URI with "/publishers/" using API STRING_concat.**\]**

**SRS_EVENTHUB_AUTH_29_024: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, `EventHubAuthCBS_Create` shall further concatenate the URI with eventHubAuthConfig->senderPublisherId using API STRING_concat_with_STRING.**\]**

**SRS_EVENTHUB_AUTH_29_025: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall construct a STRING_HANDLE to store eventHubAuthConfig->sharedAccessKeyName using API STRING_clone.**\]**

**SRS_EVENTHUB_AUTH_29_026: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall create a new STRING_HANDLE by encoding the URI using API URL_Encode.**\]**

**SRS_EVENTHUB_AUTH_29_027: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall obtain the underlying C string buffer of eventHubAuthConfig->sharedAccessKey using API STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_028: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall create a BUFFER_HANDLE using API BUFFER_create and pass in the eventHubAuthConfig->sharedAccessKey buffer and its length.**\]**

**SRS_EVENTHUB_AUTH_29_029: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Create` shall create a new STRING_HANDLE by Base64 encoding the buffer handle created above by using API Base64_Encode.**\]**

**SRS_EVENTHUB_AUTH_29_030: \[**`EventHubAuthCBS_Create` shall initialize a CBS handle by calling API cbs_create.**\]**

**SRS_EVENTHUB_AUTH_29_031: \[**`EventHubAuthCBS_Create` shall open the CBS handle by calling API `cbs_open_async`.**\]**

**SRS_EVENTHUB_AUTH_29_032: \[**`EventHubAuthCBS_Create` shall initialize its internal data structures using the configuration data passed in.**\]**

**SRS_EVENTHUB_AUTH_29_033: \[**`EventHubAuthCBS_Create` shall return a non-NULL handle encapsulating the storage of the data provided.**\]**


###EventHubAuthCBS_Destroy
```c
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Destroy, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```

**SRS_EVENTHUB_AUTH_29_070: \[**`EventHubAuthCBS_Destroy` shall return immediately eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_071: \[**`EventHubAuthCBS_Destroy` shall destroy the CBS handle if not NULL by using API cbs_destroy.**\]**

**SRS_EVENTHUB_AUTH_29_072: \[**`EventHubAuthCBS_Destroy` shall destroy the SAS token if not NULL by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_073: \[**`EventHubAuthCBS_Destroy` shall destroy the Base64 encoded shared access key by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_074: \[**`EventHubAuthCBS_Destroy` shall destroy the encoded URI by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_075: \[**`EventHubAuthCBS_Destroy` shall destroy the shared access key name by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_076: \[**`EventHubAuthCBS_Destroy` shall destroy the ext SAS token by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_077: \[**`EventHubAuthCBS_Destroy` shall destroy the ext SAS token URI by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_078: \[**`EventHubAuthCBS_Destroy` shall destroy the URI by calling API STRING_delete if not NULL.**\]**

**SRS_EVENTHUB_AUTH_29_079: \[**`EventHubAuthCBS_Destroy` shall free the internal data structure allocated earlier using API free.**\]**


###EventHubAuthCBS_Authenticate

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Authenticate, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);
```

**SRS_EVENTHUB_AUTH_29_101: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_102: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_NOT_PERMITED immediately if the status is in EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**

**SRS_EVENTHUB_AUTH_29_103: \[**`EventHubAuthCBS_Authenticate` shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**

**SRS_EVENTHUB_AUTH_29_104: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Authenticate` shall check if the ext token has expired using the seconds from epoch.**\]**

**SRS_EVENTHUB_AUTH_29_105: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Authenticate` shall create a new SAS token STRING_HANDLE using API SASToken_Create and passing in Base64 encoded shared access key STRING_HANDLE, encoded URI STRING_HANDLE, shared access key name STRING_HANDLE and expiration time in seconds from epoch.**\]**

**SRS_EVENTHUB_AUTH_29_106: \[**`EventHubAuthCBS_Authenticate` shall delete the existing SAS token STRING_HANDLE if not NULL by calling STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_107: \[**`EventHubAuthCBS_Authenticate` shall obtain the underlying C string buffer of the uri STRING_HANDLE calling STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_108: \[**`EventHubAuthCBS_Authenticate` shall obtain the underlying C string buffer of STRING_HANDLE SAS token by calling STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_109: \[**`EventHubAuthCBS_Authenticate` shall establish (put) the new token by calling API cbs_put_token and passing in the CBS handle, "servicebus.windows.net:sastoken", URI string buffer, SAS token string buffer, OnCBSPutTokenOperationComplete and eventHubAuthHandle.**\]**

**SRS_EVENTHUB_AUTH_29_110: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**

**SRS_EVENTHUB_AUTH_29_111: \[**`EventHubAuthCBS_Authenticate` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**


####OnCBSPutTokenOperationComplete

```c
static void OnCBSPutTokenOperationComplete(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description)
```
**SRS_EVENTHUB_AUTH_29_150: \[**`OnCBSPutTokenOperationComplete` shall obtain seconds from epoch by calling APIs get_time and get_difftime if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**

**SRS_EVENTHUB_AUTH_29_151: \[**`OnCBSPutTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_OK if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**

**SRS_EVENTHUB_AUTH_29_152: \[**`OnCBSPutTokenOperationComplete` shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_FAILURE if cbs_operation_result is not CBS_OPERATION_RESULT_OK and a message shall be logged.**\]**


###EventHubAuthCBS_Refresh

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Refresh, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, STRING_HANDLE extSASToken);
```
**SRS_EVENTHUB_AUTH_29_200: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_201: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if extSASToken is NULL, `EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_INVALID_ARG.**\]**

**SRS_EVENTHUB_AUTH_29_202: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall clone the extSASToken using API STRING_clone.**\]**

**SRS_EVENTHUB_AUTH_29_203: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall create a new temp STRING using API STRING_new to hold the refresh ext SAS token URI.**\]**

**SRS_EVENTHUB_AUTH_29_204: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall obtain the underlying C string buffer of cloned extSASToken using API STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_205: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall obtain the expiration time and the URI from the cloned extSASToken.**\]**

**SRS_EVENTHUB_AUTH_29_206: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall compare the refresh token URI and the existing ext token URI using API STRING_compare. If a mismatch is observed, EVENTHUBAUTH_RESULT_ERROR shall be returned and any resources shall be deallocated.**\]**

**SRS_EVENTHUB_AUTH_29_207: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall delete the prior ext SAS Token using API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_208: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, `EventHubAuthCBS_Refresh` shall delete the temp refresh ext SAS token URI using API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO `EventHubAuthCBS_Refresh` shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**

**SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_Refresh` shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_211: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_212: \[**`EventHubAuthCBS_Refresh` shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**

**SRS_EVENTHUB_AUTH_29_213: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_OK on success.**\]**

**SRS_EVENTHUB_AUTH_29_214: \[**`EventHubAuthCBS_Refresh` shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**


###EventHubAuthCBS_GetStatus

```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_GetStatus, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, EVENTHUBAUTH_STATUS*, returnStatus);
```
**SRS_EVENTHUB_AUTH_29_250: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle or returnStatus is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_251: \[**`EventHubAuthCBS_GetStatus` shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**

**SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, `EventHubAuthCBS_GetStatus` shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**

**SRS_EVENTHUB_AUTH_29_253: \[**`EventHubAuthCBS_GetStatus` shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**

**SRS_EVENTHUB_AUTH_29_254: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**

**SRS_EVENTHUB_AUTH_29_255: \[**`EventHubAuthCBS_GetStatus` shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**


###EventHubAuthCBS_SASTokenParse
```c
MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_CONFIG*, EventHubAuthCBS_SASTokenParse, const char*, sasToken);
```
**SRS_EVENTHUB_AUTH_29_301: \[**`EventHubAuthCBS_SASTokenParse` shall return NULL if sasTokenData is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_302: \[**`EventHubAuthCBS_SASTokenParse` shall construct a STRING using sasToken as data using API STRING_construct.**\]**

**SRS_EVENTHUB_AUTH_29_303: \[**`EventHubAuthCBS_SASTokenParse` shall construct a new STRING uriFromSASToken to hold the sasToken URI substring using API STRING_new.**\]**

**SRS_EVENTHUB_AUTH_29_304: \[**`EventHubAuthCBS_SASTokenParse` shall call `GetURIAndExpirationFromSASToken' and pass in sasToken, uriFromSASToken and a pointer to a uint64_t to hold the token expiration time.**\]**

**SRS_EVENTHUB_AUTH_29_305: \[**`EventHubAuthCBS_SASTokenParse` shall allocate the EVENTHUBAUTH_CBS_CONFIG structure using malloc.**\]**

**SRS_EVENTHUB_AUTH_29_306: \[**`EventHubAuthCBS_SASTokenParse` shall return NULL if memory allocation fails.**\]**

**SRS_EVENTHUB_AUTH_29_307: \[**`EventHubAuthCBS_SASTokenParse` shall create a token STRING handle using API STRING_new for handling the to be parsed tokens.**\]**

**SRS_EVENTHUB_AUTH_29_308: \[**`EventHubAuthCBS_SASTokenParse` shall return NULL and free up any allocated memory if STRING_new fails.**\]**

**SRS_EVENTHUB_AUTH_29_309: \[**`EventHubAuthCBS_SASTokenParse` shall create a STRING tokenizer using API STRING_TOKENIZER_create and pass the URI scope as argument.**\]**

**SRS_EVENTHUB_AUTH_29_310: \[**`EventHubAuthCBS_SASTokenParse` shall return NULL and free up any allocated memory if STRING_TOKENIZER_create fails.**\]**

**SRS_EVENTHUB_AUTH_29_311: \[**`EventHubAuthCBS_SASTokenParse` shall perform the following actions until parsing is complete.**\]**

**SRS_EVENTHUB_AUTH_29_312: \[**`EventHubAuthCBS_SASTokenParse` shall find a token delimited by the delimiter string "%2f" by calling STRING_TOKENIZER_get_next_token.**\]**

**SRS_EVENTHUB_AUTH_29_313: \[**`EventHubAuthCBS_SASTokenParse` shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**

**SRS_EVENTHUB_AUTH_29_314: \[**`EventHubAuthCBS_SASTokenParse` shall return NULL and free up any allocated resources if any failure is encountered.**\]**

**SRS_EVENTHUB_AUTH_29_315: \[**`EventHubAuthCBS_SASTokenParse` shall obtain the C strings for the key and value from the previously parsed STRINGs by using STRING_c_str.**\]**

**SRS_EVENTHUB_AUTH_29_316: \[**`EventHubAuthCBS_SASTokenParse` shall fail if STRING_c_str returns NULL.**\]**

**SRS_EVENTHUB_AUTH_29_317: \[**`EventHubAuthCBS_SASTokenParse` shall fail if the token length is zero.**\]**

**SRS_EVENTHUB_AUTH_29_318: \[**`EventHubAuthCBS_SASTokenParse` shall create a STRING handle to hold the hostName after parsing the first token using API STRING_construct_n.**\]**

**SRS_EVENTHUB_AUTH_29_319: \[**`EventHubAuthCBS_SASTokenParse` shall create a STRING handle to hold the eventHubPath after parsing the second token using API STRING_construct_n.**\]**

**SRS_EVENTHUB_AUTH_29_320: \[**`EventHubAuthCBS_SASTokenParse` shall parse the third token and determine if the SAS token is meant for a EventHub sender or receiver. For senders the token value should be "publishers" and "ConsumerGroups" for receivers. In all other cases these checks fail.**\]**

**SRS_EVENTHUB_AUTH_29_321: \[**`EventHubAuthCBS_SASTokenParse` shall parse the fourth token and create a STRING handle to hold either the senderPublisherId if the SAS token is a EventHub sender using API STRING_construct_n.**\]**

**SRS_EVENTHUB_AUTH_29_322: \[**`EventHubAuthCBS_SASTokenParse` shall parse the fourth token and create a STRING handle to hold either the receiverConsumerGroup if the SAS token is a EventHub receiver using API STRING_construct_n.**\]**

**SRS_EVENTHUB_AUTH_29_323: \[**`EventHubAuthCBS_SASTokenParse` shall parse the fifth token and check if the token value should be "Partitions". In all other cases this check fails. **\]**

**SRS_EVENTHUB_AUTH_29_324: \[**`EventHubAuthCBS_SASTokenParse` shall create a STRING handle to hold the receiverPartitionId after parsing the sixth token using API STRING_construct_n.**\]**

**SRS_EVENTHUB_AUTH_29_325: \[**`EventHubAuthCBS_SASTokenParse` shall free up the allocated token STRING handle using API STRING_delete after parsing is complete.**\]**

**SRS_EVENTHUB_AUTH_29_326: \[**`EventHubAuthCBS_SASTokenParse` shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**

**SRS_EVENTHUB_AUTH_29_327: \[**`EventHubAuthCBS_SASTokenParse` shall return the allocated EVENTHUBAUTH_CBS_CONFIG structure on success.**\]**


####GetURIAndExpirationFromSASToken
```c
static int GetURIAndExpirationFromSASToken(const char* sasToken, STRING_HANDLE uriFromSASToken, uint64_t* expirationTimestamp);
```

**SRS_EVENTHUB_AUTH_29_350: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value immediately if sasToken does not begins with substring "SharedAccessSignature ".**\]**

**SRS_EVENTHUB_AUTH_29_353: \[**`GetURIAndExpirationFromSASToken` shall call kvp_parser_parse and pass in the SAS token STRING handle and "=" and "&" as key and value delimiters.**\]**

**SRS_EVENTHUB_AUTH_29_354: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value if kvp_parser_parse returns a NULL MAP handle.**\]**

**SRS_EVENTHUB_AUTH_29_355: \[**`GetURIAndExpirationFromSASToken` shall obtain the SAS token URI using key "sr" in API Map_GetValueFromKey.**\]**

**SRS_EVENTHUB_AUTH_29_356: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if either the URI value is NULL or the length of the URI is 0.**\]**

**SRS_EVENTHUB_AUTH_29_357: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if the URI does not begin with substring "sb%3a%2f%2f".**\]**

**SRS_EVENTHUB_AUTH_29_358: \[**`GetURIAndExpirationFromSASToken` shall populate the URI STRING using the substring of the URI after "sb%3a%2f%2f" using API STRING_copy.**\]**

**SRS_EVENTHUB_AUTH_29_359: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if STRING_copy fails.**\]**

**SRS_EVENTHUB_AUTH_29_360: \[**`GetURIAndExpirationFromSASToken` shall obtain the SAS token signed hash key using key "sig" in API Map_GetValueFromKey.**\]**

**SRS_EVENTHUB_AUTH_29_361: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if either the sig string is NULL or the length of the string is 0.**\]**

**SRS_EVENTHUB_AUTH_29_362: \[**`GetURIAndExpirationFromSASToken` shall obtain the SAS token node using key "skn" in API Map_GetValueFromKey.**\]**

**SRS_EVENTHUB_AUTH_29_363: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if either the skn string is NULL or the length of the string is 0.**\]**

**SRS_EVENTHUB_AUTH_29_364: \[**`GetURIAndExpirationFromSASToken` shall obtain the SAS token expiration using key "se" in API Map_GetValueFromKey.**\]**

**SRS_EVENTHUB_AUTH_29_365: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**

**SRS_EVENTHUB_AUTH_29_366: \[**`GetURIAndExpirationFromSASToken` shall convert the expiration timestamp to a decimal number by calling API strtoull_s.**\]**

**SRS_EVENTHUB_AUTH_29_367: \[**`GetURIAndExpirationFromSASToken` shall return a non zero value and free up any allocated memory if calling strtoull_s fails when converting the expiration string to the expirationTimestamp.**\]**

**SRS_EVENTHUB_AUTH_29_368: \[**`GetURIAndExpirationFromSASToken` shall free up the MAP handle created as a result of calling kvp_parser_parse after parsing is complete.**\]**

**SRS_EVENTHUB_AUTH_29_369: \[**`GetURIAndExpirationFromSASToken` shall return 0 on success.**\]**


###EventHubAuthCBS_Config_Destroy
```c
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Config_Destroy, EVENTHUBAUTH_CBS_CONFIG*, cfg);
```
**SRS_EVENTHUB_AUTH_29_401: \[**`EventHubAuthCBS_Config_Destroy` shall return immediately if cfg is NULL.**\]**

**SRS_EVENTHUB_AUTH_29_402: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the hostName STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_403: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the eventHubPath STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_404: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the receiverConsumerGroup STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_405: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the receiverPartitionId STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_406: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the senderPublisherId STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_407: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the sharedAccessKeyName STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_408: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the sharedAccessKey STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_409: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the extSASToken STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_410: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the extSASTokenURI STRING handle if not null by calling API STRING_delete.**\]**

**SRS_EVENTHUB_AUTH_29_411: \[**`EventHubAuthCBS_Config_Destroy` shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**