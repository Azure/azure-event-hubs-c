// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "azure_c_shared_utility/agenttime.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/base64.h"
#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/map.h"
#include "azure_c_shared_utility/sastoken.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/string_tokenizer.h"
#include "azure_c_shared_utility/urlencode.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_uamqp_c/cbs.h"
#include "azure_uamqp_c/sasl_mssbcbs.h"

#include "kvp_parser.h"
#include "eventhubauth.h"

#define EVENTHUBAUTH_OP_VALUES      \
        EVENTHUBAUTH_OP_NONE,       \
        EVENTHUBAUTH_OP_PUT,        \
        EVENTHUBAUTH_OP_DELETE

DEFINE_ENUM(EVENTHUBAUTH_OP, EVENTHUBAUTH_OP_VALUES);

#define SASTOKEN_URI_ELEMENT_VALUES         \
        SASTOKEN_URI_ELEMENT_HOSTNAME,      \
        SASTOKEN_URI_ELEMENT_EVENTHUB,      \
        SASTOKEN_URI_ELEMENT_PUBLISHERORCONSUMER,      \
        SASTOKEN_URI_ELEMENT_PUBLSIHERIDORCONSUMERID,  \
        SASTOKEN_URI_ELEMENT_PARTITION,     \
        SASTOKEN_URI_ELEMENT_PARTITIONID,   \
        SASTOKEN_URI_ELEMENT_UNKNOWN

DEFINE_ENUM(SASTOKEN_URI_ELEMENT, SASTOKEN_URI_ELEMENT_VALUES);

#define SAS_TOKEN_TYPE          "servicebus.windows.net:sastoken"

static const char SB_STRING[] = "sb://";

typedef struct EVENTHUBAUTH_CBS_STRUCT_TAG
{
    STRING_HANDLE       uri;
    STRING_HANDLE       encodedURI;
    STRING_HANDLE       sasTokenHandle;
    STRING_HANDLE       sharedAccessKeyName;
    STRING_HANDLE       encodedSASKeyValue;
    STRING_HANDLE       extSASToken;
    STRING_HANDLE       extSASTokenURI;
    uint64_t            extSASTokenExpTSInEpochSec;
    unsigned int        sasTokenExpirationTimeInSec;
    unsigned int        sasTokenRefreshPeriodInSecs;
    unsigned int        sasTokenAuthFailureTimeoutInSecs;
    uint64_t            cbsAuthCompleteTimestamp;
    uint64_t            sasTokenCreateTimestamp;
    uint64_t            sasTokenPutTime;
    CBS_HANDLE          cbsHandle;
    EVENTHUBAUTH_STATUS cbsStatus;
    EVENTHUBAUTH_OP     cbOperation;
    EVENTHUBAUTH_CREDENTIAL_TYPE credential;
} EVENTHUBAUTH_CBS_STRUCT;

static const char sasTokenStart[] = "SharedAccessSignature ";
static size_t sasTokenStartSize = sizeof(sasTokenStart);
static const char sasTokenURI[] = "sb%3a%2f%2f";
static size_t sasTokenURISize = sizeof(sasTokenURI);
static const char sasTokenDelim[] = "%2f";

static int UpdateEventHubAuthACBSStatus(EVENTHUBAUTH_CBS_STRUCT* eventHubAuth);

static void on_cbs_open_complete(void* context, CBS_OPEN_COMPLETE_RESULT open_complete_result)
{
    (void)context;
    (void)open_complete_result;
}

static void on_cbs_error(void* context)
{
    (void)context;
}

static int GetSecondsSinceEpoch(uint64_t* seconds)
{
    int result;
    time_t current_time;

    if ((current_time = get_time(NULL)) == (time_t)(-1))
    {
        result = __LINE__;
        LogError("Failed getting the current local time (get_time() failed)\r\n");
    }
    else
    {
        *seconds = (uint64_t)get_difftime(current_time, (time_t)0);
        result = 0;
    }

    return result;
}

static int GetURIAndExpirationFromSASToken(const char* sasToken, STRING_HANDLE uriFromSASToken, uint64_t* expirationTimestamp)
{
    int result;
    MAP_HANDLE tokenMap = NULL;
    const char *uri, *expirationTS, *sig, *skn;
    const char* sasTokenScopeURI = sasToken + sasTokenStartSize - 1;

    //**Codes_SRS_EVENTHUB_AUTH_29_350: \[**GetURIAndExpirationFromSASToken shall return a non zero value immediately if sasToken does not begins with substring "SharedAccessSignature ".**\]**
    if (strncmp(sasTokenStart, sasToken, sasTokenStartSize - 1) != 0)
    {
        LogError("Invalid SASToken, could not find \"%s\"\r\n", sasTokenStart);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_353: \[**GetURIAndExpirationFromSASToken shall call kvp_parser_parse and pass in the SAS token STRING handle and "=" and "&" as key and value delimiters.**\]**
    else if ((tokenMap = kvp_parser_parse(sasTokenScopeURI, "=", "&")) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_354: \[**GetURIAndExpirationFromSASToken shall return a non zero value if kvp_parser_parse returns a NULL MAP handle.**\]**
        LogError("Could not parse SAS token\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_355: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token URI using key "sr" in API Map_GetValueFromKey.**\]**
    else if ((uri = Map_GetValueFromKey(tokenMap, "sr")) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_356: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the URI value is NULL or the length of the URI is 0.**\]**
        LogError("SAS token invalid key \"sr\" not found.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_357: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if the URI does not begin with substring "sb%3a%2f%2f".**\]**
    else if (strlen(uri) == 0)
    {
        LogError("SAS token URI zero length.\r\n");
        result = __LINE__;
    }
    else if (strncmp(sasTokenURI, uri, sasTokenURISize - 1) != 0)
    {
        LogError("SAS token URI invalid format %s.\r\n", uri);
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_358: \[**GetURIAndExpirationFromSASToken shall populate the URI STRING using the substring of the URI after "sb%3a%2f%2f" using API STRING_copy.**\]**
    else if ((STRING_copy(uriFromSASToken, uri + sasTokenURISize - 1)) != 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_359: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if STRING_copy fails.**\]**
        LogError("Could Not Copy SAS token URI.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_360: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token signed hash key using key "sig" in API Map_GetValueFromKey.**\]**
    else if ((sig = Map_GetValueFromKey(tokenMap, "sig")) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_361: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the sig string is NULL or the length of the string is 0.**\]**
        LogError("SAS token invalid key \"sig\" not found.\r\n");
        result = __LINE__;
    }
    else if (strlen(sig) == 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_361: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the sig string is NULL or the length of the string is 0.**\]**
        LogError("SAS token sig zero length.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_362: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token node using key "skn" in API Map_GetValueFromKey.**\]**
    else if ((skn = Map_GetValueFromKey(tokenMap, "skn")) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_363: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the skn string is NULL or the length of the string is 0.**\]**
        LogError("SAS token invalid key \"skn\" not found.\r\n");
        result = __LINE__;
    }
    else if (strlen(skn) == 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_363: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the skn string is NULL or the length of the string is 0.**\]**
        LogError("SAS token skn zero length.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_364: \[**GetURIAndExpirationFromSASToken shall obtain the SAS token expiration using key "se" in API Map_GetValueFromKey.**\]**
    else if ((expirationTS = Map_GetValueFromKey(tokenMap, "se")) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_365: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**
        LogError("SAS token invalid key \"se\" not found.\r\n");
        result = __LINE__;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_365: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**
    else if (strlen(expirationTS) == 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_365: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if either the expiration string is NULL or the length of the string is 0.**\]**
        LogError("SAS token expiration timestamp invalid length %s.\r\n", uri);
        result = __LINE__;
    }
    else
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_366: \[**GetURIAndExpirationFromSASToken shall convert the expiration timestamp to a decimal number by calling API strtoull_s.**\]**
        *expirationTimestamp = strtoull_s(expirationTS, NULL, 10);
        if ((ULLONG_MAX == *expirationTimestamp) && (errno == ERANGE))
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_367: \[**GetURIAndExpirationFromSASToken shall return a non zero value and free up any allocated memory if calling strtoull_s fails when converting the expiration string to the expirationTimestamp.**\]**
            LogError("Could Not Covert Expiration TS to ULL %s.\r\n", expirationTS);
            result = __LINE__;
        }
        else
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_369: \[**GetURIAndExpirationFromSASToken shall return 0 on success.**\]**
            result = 0;
        }
    }

    //**Codes_SRS_EVENTHUB_AUTH_29_368: \[**GetURIAndExpirationFromSASToken shall free up the MAP handle created as a result of calling kvp_parser_parse after parsing is complete.**\]**
    if (tokenMap != NULL)
    {
        Map_Destroy(tokenMap);
    }

    return result;
}

EVENTHUBAUTH_CBS_CONFIG* EventHubAuthCBS_SASTokenParse(const char* extSASToken)
{
    EVENTHUBAUTH_CBS_CONFIG* result;
    STRING_HANDLE sasTokenHandle = NULL;
    STRING_HANDLE uriData = NULL;
    uint64_t expirationTimestamp;
    int errorCode;

    //**Codes_SRS_EVENTHUB_AUTH_29_301: \[**EventHubAuthCBS_SASTokenParse shall return NULL if sasTokenData is NULL.**\]**
    if (extSASToken == NULL)
    {
        LogError("Invalid Argument eventHubAuthHandle\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_302: \[**EventHubAuthCBS_SASTokenParse shall construct a STRING using sasToken as data using API STRING_construct.**\]**
    else if ((sasTokenHandle = STRING_construct(extSASToken)) == NULL)
    {
        LogError("Could Not Construct Ext SAS Token String.\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_303: \[**EventHubAuthCBS_SASTokenParse shall construct a new STRING uriFromSASToken to hold the sasToken URI substring using API STRING_new.**\]**
    else if ((uriData = STRING_new()) == NULL)
    {
        LogError("Could Not Create URI Data String.\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_304: \[**EventHubAuthCBS_SASTokenParse shall call GetURIAndExpirationFromSASToken' and pass in sasToken, uriFromSASToken and a pointer to a uint64_t to hold the token expiration time.**\]**
    else if ((errorCode = GetURIAndExpirationFromSASToken(extSASToken, uriData, &expirationTimestamp)) != 0)
    {
        LogError("GetURIAndExpirationFromSASToken Returned Error. Code:%d\r\n", errorCode);
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_305: \[**EventHubAuthCBS_SASTokenParse shall allocate the EVENTHUBAUTH_CBS_CONFIG structure using malloc.**\]**
    else if ((result = (EVENTHUBAUTH_CBS_CONFIG*)malloc(sizeof(EVENTHUBAUTH_CBS_CONFIG))) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_306: \[**EventHubAuthCBS_SASTokenParse shall return NULL if memory allocation fails.**\]**
        LogError("Could Not Allocate EVENTHUBAUTH_SASTOKEN_PARSE_DATA.\r\n");
    }
    else
    {
        STRING_TOKENIZER_HANDLE tokenizer = NULL;

        //**Codes_SRS_EVENTHUB_AUTH_29_309: \[**EventHubAuthCBS_SASTokenParse shall create a STRING tokenizer using API STRING_TOKENIZER_create and pass the URI scope as argument.**\]**
        if ((tokenizer = STRING_TOKENIZER_create(uriData)) == NULL)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_310: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_TOKENIZER_create fails.**\]**
            LogError("Could Not Create STRING tokenizer.\r\n");
            free(result);
            result = NULL;
        }
        else
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_307: \[**EventHubAuthCBS_SASTokenParse shall create a token STRING handle using API STRING_new for handling the to be parsed tokens.**\]**
            STRING_HANDLE tokenHandle = STRING_new();
            size_t len;

            if (tokenHandle == NULL)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_308: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated memory if STRING_new fails.**\]**
                LogError("Could Not Create token STRING handle.\r\n");
                free(result);
                result = NULL;
            }
            else
            {
                SASTOKEN_URI_ELEMENT elem = SASTOKEN_URI_ELEMENT_HOSTNAME;
                bool isError = false, isDone = false;
                
                memset(result, 0, sizeof(EVENTHUBAUTH_CBS_CONFIG));
                
                //**Codes_SRS_EVENTHUB_AUTH_29_311: \[**EventHubAuthCBS_SASTokenParse shall perform the following actions until parsing is complete.**\]**
                //**Codes_SRS_EVENTHUB_AUTH_29_312: \[**EventHubAuthCBS_SASTokenParse shall find a token delimited by the delimiter string "%2f" by calling STRING_TOKENIZER_get_next_token.**\]**
                //**Codes_SRS_EVENTHUB_AUTH_29_313: \[**EventHubAuthCBS_SASTokenParse shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**
                while (!isError && !isDone && (STRING_TOKENIZER_get_next_token(tokenizer, tokenHandle, sasTokenDelim) == 0))
                {
                    //**Codes_SRS_EVENTHUB_AUTH_29_315: \[**EventHubAuthCBS_SASTokenParse shall obtain the C strings for the key and value from the previously parsed STRINGs by using STRING_c_str.**\]**
                    const char* token = STRING_c_str(tokenHandle);
                    if ((token == NULL) || (((len = strlen(token)) == 0)))
                    {
                        //**Codes_SRS_EVENTHUB_AUTH_29_316: \[**EventHubAuthCBS_SASTokenParse shall fail if STRING_c_str returns NULL.**\]**
                        //**Codes_SRS_EVENTHUB_AUTH_29_317: \[**EventHubAuthCBS_SASTokenParse shall fail if the token length is zero.**\]**
                        LogError("The key token is NULL or empty.\r\n");
                        isError = true;
                    }
                    else
                    {
                        switch (elem)
                        {
                        case SASTOKEN_URI_ELEMENT_HOSTNAME:
                            //**Codes_SRS_EVENTHUB_AUTH_29_318: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the hostName after parsing the first token using API STRING_construct_n.**\]**
                            if ((result->hostName = STRING_construct_n(token, len)) == NULL)
                            {
                                LogError("Could Not Construct HostName String\r\n");
                                isError = true;
                            }
                            else
                            {
                                elem = SASTOKEN_URI_ELEMENT_EVENTHUB;
                            }
                            break;
                        case SASTOKEN_URI_ELEMENT_EVENTHUB:
                            //**Codes_SRS_EVENTHUB_AUTH_29_319: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the eventHubPath after parsing the second token using API STRING_construct_n.**\]**
                            if ((result->eventHubPath = STRING_construct_n(token, len)) == NULL)
                            {
                                LogError("Could Not Construct EventHub Path String\r\n");
                                isError = true;
                            }
                            else
                            {
                                elem = SASTOKEN_URI_ELEMENT_PUBLISHERORCONSUMER;
                            }
                            break;
                        case SASTOKEN_URI_ELEMENT_PUBLISHERORCONSUMER:
                            //**Codes_SRS_EVENTHUB_AUTH_29_320: \[**EventHubAuthCBS_SASTokenParse shall parse the third token and determine if the SAS token is meant for a EventHub sender or receiver. For senders the token value should be "publishers" and "ConsumerGroups" for receivers. In all other cases these checks fail.**\]**
                            if (strncmp(token, "publishers", len) == 0)
                            {
                                result->mode = EVENTHUBAUTH_MODE_SENDER;
                                elem = SASTOKEN_URI_ELEMENT_PUBLSIHERIDORCONSUMERID;
                            }
                            else if (strncmp(token, "ConsumerGroups", len) == 0)
                            {
                                result->mode = EVENTHUBAUTH_MODE_RECEIVER;
                                elem = SASTOKEN_URI_ELEMENT_PUBLSIHERIDORCONSUMERID;
                            }
                            else
                            {
                                LogError("Invalid Format, Expected Substrings \"publishers\" or \"ConsumerGroups\"\r\n");
                                isError = true;
                            }
                            break;
                        case SASTOKEN_URI_ELEMENT_PUBLSIHERIDORCONSUMERID:
                            if (result->mode == EVENTHUBAUTH_MODE_SENDER)
                            {
                                //**Codes_SRS_EVENTHUB_AUTH_29_321: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the senderPublisherId if the SAS token is a EventHub sender using API STRING_construct_n.**\]**
                                if ((result->senderPublisherId = STRING_construct_n(token, len)) == NULL)
                                {
                                    LogError("Could Not Construct Publisher ID String\r\n");
                                    isError = true;
                                }
                                else
                                {
                                    isDone = true;
                                }
                            }
                            else
                            {
                                //**Codes_SRS_EVENTHUB_AUTH_29_322: \[**EventHubAuthCBS_SASTokenParse shall parse the fourth token and create a STRING handle to hold either the receiverConsumerGroup if the SAS token is a EventHub receiver using API STRING_construct_n.**\]**
                                if ((result->receiverConsumerGroup = STRING_construct_n(token, len)) == NULL)
                                {
                                    LogError("Could Not Construct Consumer Group String\r\n");
                                    isError = true;
                                }
                                else
                                {
                                    elem = SASTOKEN_URI_ELEMENT_PARTITION;
                                }
                            }
                            break;
                        case SASTOKEN_URI_ELEMENT_PARTITION:
                            //**Codes_SRS_EVENTHUB_AUTH_29_323: \[**EventHubAuthCBS_SASTokenParse shall parse the fifth token and check if the token value should be "Partitions". In all other cases this check fails. **\]**
                            if (strncmp(token, "Partitions", len) != 0)
                            {
                                LogError("Invalid Format, Expected Substring \"Partitions\"\r\n");
                                isError = true;
                            }
                            else
                            {
                                elem = SASTOKEN_URI_ELEMENT_PARTITIONID;
                            }
                            break;
                        case SASTOKEN_URI_ELEMENT_PARTITIONID:
                            //**Codes_SRS_EVENTHUB_AUTH_29_324: \[**EventHubAuthCBS_SASTokenParse shall create a STRING handle to hold the receiverPartitionId after parsing the sixth token using API STRING_construct_n.**\]**
                            if ((result->receiverPartitionId = STRING_construct_n(token, len)) == NULL)
                            {
                                LogError("Could Not Construct Partition ID String\r\n");
                                isError = true;
                            }
                            else
                            {
                                isDone = true;
                            }
                            break;
                        default:
                            LogError("Invalid URI Element %u\r\n", elem);
                            isError = true;
                            break;
                        };
                    }
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_325: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated token STRING handle using API STRING_delete after parsing is complete.**\]**
                STRING_delete(tokenHandle);
                if ((isError) || (!isDone))
                {
                    //**Codes_SRS_EVENTHUB_AUTH_29_314: \[**EventHubAuthCBS_SASTokenParse shall return NULL and free up any allocated resources if any failure is encountered.**\]**
                    STRING_delete(uriData);
                    STRING_delete(sasTokenHandle);
                    EventHubAuthCBS_Config_Destroy(result);
                    result = NULL;
                }
                else
                {
                    result->extSASTokenExpTSInEpochSec = expirationTimestamp;
                    result->extSASTokenURI = uriData;
                    result->extSASToken = sasTokenHandle;
                    result->credential = EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT;
                }
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_326: \[**EventHubAuthCBS_SASTokenParse shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**
            STRING_TOKENIZER_destroy(tokenizer);
        }
    }

    //**Codes_SRS_EVENTHUB_AUTH_29_327: \[**EventHubAuthCBS_SASTokenParse shall return the allocated EVENTHUBAUTH_CBS_CONFIG structure on success.**\]**
    return result;
}

void EventHubAuthCBS_Config_Destroy(EVENTHUBAUTH_CBS_CONFIG* cfg)
{
    //**Codes_SRS_EVENTHUB_AUTH_29_401: \[**EventHubAuthCBS_Config_Destroy shall return immediately if cfg is NULL.**\]**
    if (cfg != NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_402: \[**EventHubAuthCBS_Config_Destroy shall destroy the hostName STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->hostName != NULL)
        {
            STRING_delete(cfg->hostName);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_403: \[**EventHubAuthCBS_Config_Destroy shall destroy the eventHubPath STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->eventHubPath != NULL)
        {
            STRING_delete(cfg->eventHubPath);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_404: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverConsumerGroup STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->receiverConsumerGroup != NULL)
        {
            STRING_delete(cfg->receiverConsumerGroup);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_405: \[**EventHubAuthCBS_Config_Destroy shall destroy the receiverPartitionId STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->receiverPartitionId != NULL)
        {
            STRING_delete(cfg->receiverPartitionId);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_406: \[**EventHubAuthCBS_Config_Destroy shall destroy the senderPublisherId STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->senderPublisherId != NULL)
        {
            STRING_delete(cfg->senderPublisherId);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_407: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKeyName STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->sharedAccessKeyName != NULL)
        {
            STRING_delete(cfg->sharedAccessKeyName);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_408: \[**EventHubAuthCBS_Config_Destroy shall destroy the sharedAccessKey STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->sharedAccessKey != NULL)
        {
            STRING_delete(cfg->sharedAccessKey);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_409: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASToken STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->extSASToken != NULL)
        {
            STRING_delete(cfg->extSASToken);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_410: \[**EventHubAuthCBS_Config_Destroy shall destroy the extSASTokenURI STRING handle if not null by calling API STRING_delete.**\]**
        if (cfg->extSASTokenURI != NULL)
        {
            STRING_delete(cfg->extSASTokenURI);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_411: \[**EventHubAuthCBS_Config_Destroy shall destroy the EVENTHUBAUTH_CBS_CONFIG data structure using free.**\]**
        free(cfg);
    }
}

static void OnCBSPutTokenOperationComplete(void* context, CBS_OPERATION_RESULT cbs_operation_result, unsigned int status_code, const char* status_description)
{
    EVENTHUBAUTH_CBS_STRUCT* eventHubAuth = (EVENTHUBAUTH_CBS_STRUCT*)context;
    uint64_t secondsSinceEpoch;

    //**Codes_SRS_EVENTHUB_AUTH_29_150: \[**OnCBSPutTokenOperationComplete shall obtain seconds from epoch by calling APIs get_time and get_difftime if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**
    if (cbs_operation_result == CBS_OPERATION_RESULT_OK)
    {
        int errorCode;
        //**Codes_SRS_EVENTHUB_AUTH_29_151: \[**OnCBSPutTokenOperationComplete shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_OK if cbs_operation_result is CBS_OPERATION_RESULT_OK.**\]**
        eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_OK;
        if ((errorCode = GetSecondsSinceEpoch(&secondsSinceEpoch)) != 0)
        {
            eventHubAuth->cbsAuthCompleteTimestamp = secondsSinceEpoch;
            LogError("Could Not Get Seconds Since Epoch. Code:%d\r\n", errorCode);
        }
    }
    else
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_152: \[**OnCBSPutTokenOperationComplete shall update the current EventHubAuth to EVENTHUBAUTH_STATUS_FAILURE if cbs_operation_result is not CBS_OPERATION_RESULT_OK and a message shall be logged.**\]**
        eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_FAILURE;
        LogError("CBS reported status code %u, error: %s for put token operation\r\n", status_code, status_description);
    }

    eventHubAuth->cbOperation = EVENTHUBAUTH_OP_NONE;
}

static int CheckPutTimeoutStatus(EVENTHUBAUTH_CBS_STRUCT* eventHubAuth, bool* hasTimeoutOccured)
{
    int result;
    uint64_t secondsSinceEpoch;
    int errorCode = GetSecondsSinceEpoch(&secondsSinceEpoch);

    if (0 != errorCode)
    {
        LogError("Could Not Get Seconds Since Epoch. Code:%d\r\n", errorCode);
        result = __LINE__;
    }
    else
    {
        if (eventHubAuth->sasTokenAuthFailureTimeoutInSecs > 0)
        {
            *hasTimeoutOccured = ((secondsSinceEpoch - eventHubAuth->sasTokenPutTime) >= eventHubAuth->sasTokenAuthFailureTimeoutInSecs) ? true : false;
        }
        else
        {
            *hasTimeoutOccured = false;
        }
        result = 0;
    }

    return result;
}

static int CheckExpirationAndRefreshStatus(EVENTHUBAUTH_CBS_STRUCT* eventHubAuth, bool* isExpired, bool* isRefreshRequired)
{
    int result;
    uint64_t secondsSinceEpoch;
    int errorCode = GetSecondsSinceEpoch(&secondsSinceEpoch);

    if (0 != errorCode)
    {
        LogError("Could Not Get Seconds Since Epoch. Code:%d\r\n", errorCode);
        result = __LINE__;
    }
    else
    {
        if (eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
        {
            *isExpired = ((secondsSinceEpoch - eventHubAuth->sasTokenCreateTimestamp) >= eventHubAuth->sasTokenExpirationTimeInSec) ? true : false;
            *isRefreshRequired = ((secondsSinceEpoch - eventHubAuth->sasTokenCreateTimestamp) >= eventHubAuth->sasTokenRefreshPeriodInSecs) ? true : false;
        }
        else
        {
            *isExpired = (secondsSinceEpoch >= eventHubAuth->extSASTokenExpTSInEpochSec) ? true : false;
            *isRefreshRequired = false;
        }
        
        result = 0;
    }

    return result;
}

EVENTHUBAUTH_CBS_HANDLE EventHubAuthCBS_Create(const EVENTHUBAUTH_CBS_CONFIG* eventHubAuthConfig, SESSION_HANDLE cbsSessionHandle)
{
    EVENTHUBAUTH_CBS_STRUCT* result;
    int errorCode;

    //**Codes_SRS_EVENTHUB_AUTH_29_001: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig or cbsSessionHandle is NULL.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_002: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->mode is not EVENTHUBAUTH_MODE_SENDER or EVENTHUBAUTH_MODE_RECEIVER.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_003: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is not EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO or EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT.**\]**
    if ((eventHubAuthConfig == NULL) || (cbsSessionHandle == NULL) ||
        ((eventHubAuthConfig->mode != EVENTHUBAUTH_MODE_SENDER) && (eventHubAuthConfig->mode != EVENTHUBAUTH_MODE_RECEIVER)) ||
        ((eventHubAuthConfig->credential != EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO) && (eventHubAuthConfig->credential != EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)))
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**
        LogError("Invalid Arguments\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_004: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->hostName or eventHubAuthConfig->eventHubPath are NULL.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_005: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sasTokenExpirationTimeInSec or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is zero or eventHubAuthConfig->sasTokenRefreshPeriodInSecs is greater than eventHubAuthConfig->sasTokenExpirationTimeInSec.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_006: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->sharedAccessKeyName or eventHubAuthConfig->sharedAccessKey are NULL.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_007: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if eventHubAuthConfig->mode is EVENTHUBAUTH_MODE_RECEIVER and eventHubAuthConfig->receiverConsumerGroup or eventHubAuthConfig->receiverPartitionId is NULL.**\]**
    else if ((eventHubAuthConfig->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO) &&
              ((eventHubAuthConfig->sharedAccessKeyName == NULL) || (eventHubAuthConfig->sharedAccessKey == NULL) ||
               (eventHubAuthConfig->hostName == NULL) || (eventHubAuthConfig->eventHubPath == NULL) ||
               ((eventHubAuthConfig->mode == EVENTHUBAUTH_MODE_RECEIVER) && ((eventHubAuthConfig->receiverConsumerGroup == NULL) || (eventHubAuthConfig->receiverPartitionId == NULL))) ||
                (eventHubAuthConfig->sasTokenExpirationTimeInSec == 0) || (eventHubAuthConfig->sasTokenRefreshPeriodInSecs == 0) ||
                (eventHubAuthConfig->sasTokenExpirationTimeInSec <= eventHubAuthConfig->sasTokenRefreshPeriodInSecs)))
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**
        LogError("Invalid SASTOKEN_AUTO Arguments\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_009: \[**EventHubAuthCBS_Create shall return NULL if eventHubAuthConfig->credential is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if eventHubAuthConfig->extSASToken is NULL or eventHubAuthConfig->extSASTokenURI is NULL or eventHubAuthConfig->extSASTokenExpTSInEpochSec equals 0.**\]**
    else if ((eventHubAuthConfig->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT) &&
             ((eventHubAuthConfig->extSASToken == NULL) || (eventHubAuthConfig->extSASTokenURI == NULL) ||
              (eventHubAuthConfig->extSASTokenExpTSInEpochSec == 0)))
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**
        LogError("Invalid SASTOKEN_EXT Arguments\r\n");
        result = NULL;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_010: \[**EventHubAuthCBS_Create shall allocate new memory to store the specified configuration data by using API malloc.**\]**
    else if ((result = (EVENTHUBAUTH_CBS_STRUCT*)malloc(sizeof(EVENTHUBAUTH_CBS_STRUCT))) == NULL)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**
        LogError("Could Not Allocate Memory For EVENTHUBAUTH_CBS_STRUCT\r\n");
    }
    else
    {
        bool isError;

        memset(result, 0, sizeof(EVENTHUBAUTH_CBS_STRUCT));
        if (eventHubAuthConfig->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
        {
            result->extSASTokenExpTSInEpochSec = eventHubAuthConfig->extSASTokenExpTSInEpochSec;
            //**Codes_SRS_EVENTHUB_AUTH_29_012: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Create shall clone the external SAS token using API STRING_clone.**\]**
            if ((result->extSASToken = STRING_clone(eventHubAuthConfig->extSASToken)) == NULL)
            {
                LogError("Could Not Clone Ext SAS Token String \r\n");
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_013: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Create shall clone the external SAS token URI using API STRING_clone.**\]**
            else if ((result->extSASTokenURI = STRING_clone(eventHubAuthConfig->extSASTokenURI)) == NULL)
            {
                LogError("Could Not Clone Ext SAS Token URI String \r\n");
                isError = true;
            }
            else
            {
                isError = false;
            }
        }
        else
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_014: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall construct a URI of the format "sb://" using API STRING_construct.**\]**
            if ((result->uri = STRING_construct(SB_STRING)) == NULL)
            {
                LogError("Could Not Construct String handle For the URI string\r\n");
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_015: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->hostName using API STRING_concat_with_STRING.**\]**
            else if ((errorCode = STRING_concat_with_STRING(result->uri, eventHubAuthConfig->hostName)) != 0)
            {
                LogError("Could Not Concatenate Host Name to the URI handle\r\n");
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_016: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with "/" using API STRING_concat.**\]**
            else if ((errorCode = STRING_concat(result->uri, "/")) != 0)
            {
                LogError("Could Not Concatenate / after Host Name to the URI handle\r\n");
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_017: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->eventHubPath using API STRING_concat_with_STRING.**\]**
            else if ((errorCode = STRING_concat_with_STRING(result->uri, eventHubAuthConfig->eventHubPath)) != 0)
            {
                LogError("Could Not Concatenate Event Hub Path to the URI handle\r\n");
                isError = true;
            }
            else
            {
                if (eventHubAuthConfig->mode == EVENTHUBAUTH_MODE_RECEIVER)
                {
                    //**Codes_SRS_EVENTHUB_AUTH_29_019: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with "/ConsumerGroups/" using API STRING_concat.**\]**
                    if ((errorCode = STRING_concat(result->uri, "/ConsumerGroups/")) != 0)
                    {
                        LogError("Could Not Concatenate \"/ConsumerGroups/\" to the URI handle\r\n");
                        isError = true;
                    }
                    //**Codes_SRS_EVENTHUB_AUTH_29_020: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->receiverConsumerGroup using API STRING_concat_with_STRING.**\]**
                    else if ((errorCode = STRING_concat_with_STRING(result->uri, eventHubAuthConfig->receiverConsumerGroup)) != 0)
                    {
                        LogError("Could Not Concatenate Consumer Group to the URI handle\r\n");
                        isError = true;
                    }
                    //**Codes_SRS_EVENTHUB_AUTH_29_021: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with "/Partitions/" using API STRING_concat.**\]**
                    else if ((errorCode = STRING_concat(result->uri, "/Partitions/")) != 0)
                    {
                        LogError("Could Not Concatenate \"/Partitions/\" to the URI handle\r\n");
                        isError = true;
                    }
                    //**Codes_SRS_EVENTHUB_AUTH_29_022: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_RECEIVER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->receiverPartitionId using API STRING_concat_with_STRING.**\]**
                    else if ((errorCode = STRING_concat_with_STRING(result->uri, eventHubAuthConfig->receiverPartitionId)) != 0)
                    {
                        LogError("Could Not Concatenate Consumer Group to the URI handle\r\n");
                        isError = true;
                    }
                    else
                    {
                        isError = false;
                    }
                }
                else
                {
                    //**Codes_SRS_EVENTHUB_AUTH_29_023: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, EventHubAuthCBS_Create shall further concatenate the URI with "/publishers/" using API STRING_concat.**\]**
                    if ((eventHubAuthConfig->senderPublisherId != NULL) &&
                        ((errorCode = STRING_concat(result->uri, "/publishers/")) != 0))
                    {
                        LogError("Could Not Concatenate \"/publishers/\" to the URI handle\r\n");
                        isError = true;
                    }
                    //**Codes_SRS_EVENTHUB_AUTH_29_024: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and if mode is EVENTHUBAUTH_MODE_SENDER, EventHubAuthCBS_Create shall further concatenate the URI with eventHubAuthConfig->senderPublisherId using API STRING_concat_with_STRING.**\]**
                    else if ((eventHubAuthConfig->senderPublisherId != NULL) &&
                        ((errorCode = STRING_concat_with_STRING(result->uri, eventHubAuthConfig->senderPublisherId)) != 0))
                    {
                        LogError("Could Not Concatenate Publisher Id to the URI handle\r\n");
                        isError = true;
                    }
                    else
                    {
                        isError = false;
                    }
                }
            }
            if (isError == false)
            {
                BUFFER_HANDLE bufferHandle = NULL;
                const char* sharedAccessKey;
                //**Codes_SRS_EVENTHUB_AUTH_29_025: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall construct a STRING_HANDLE to store eventHubAuthConfig->sharedAccessKeyName using API STRING_clone.**\]**
                if ((result->sharedAccessKeyName = STRING_clone(eventHubAuthConfig->sharedAccessKeyName)) == NULL)
                {
                    LogError("Could Not Construct String handle for Shared Access Key Name\r\n");
                    isError = true;
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_026: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a new STRING_HANDLE by encoding the URI using API URL_Encode.**\]**
                else if ((result->encodedURI = URL_Encode(result->uri)) == NULL)
                {
                    LogError("Could Not URL Encode the URI handle\r\n");
                    isError = true;
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_027: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall obtain the underlying C string buffer of eventHubAuthConfig->sharedAccessKey using API STRING_c_str.**\]**
                else if ((sharedAccessKey = STRING_c_str(eventHubAuthConfig->sharedAccessKey)) == NULL)
                {
                    LogError("Could Not Obtain String buffer from shared access key\r\n");
                    isError = true;
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_028: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a BUFFER_HANDLE using API BUFFER_create and pass in the eventHubAuthConfig->sharedAccessKey buffer and its length.**\]**
                else if ((bufferHandle = BUFFER_create((const unsigned char*)sharedAccessKey, strlen(sharedAccessKey))) == NULL)
                {
                    LogError("Could Not Create Buffer Handle\r\n");
                    isError = true;
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_029: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Create shall create a new STRING_HANDLE by Base64 encoding the buffer handle created above by using API Base64_Encoder.**\]**
                else if ((result->encodedSASKeyValue = Base64_Encoder(bufferHandle)) == NULL)
                {
                    LogError("Could Not Base64 Encode the Shared Key Buffer\r\n");
                    isError = true;
                }
                if (bufferHandle != NULL)
                {
                    BUFFER_delete(bufferHandle);
                }
            }
        }

        if (isError == false)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_030: \[**EventHubAuthCBS_Create shall initialize a CBS handle by calling API cbs_create.**\]**
            if ((result->cbsHandle = cbs_create(cbsSessionHandle)) == NULL)
            {
                LogError("Could Not Create CBS Handle\r\n");
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_031: \[**EventHubAuthCBS_Create shall open the CBS handle by calling API `cbs_open_async`.**\]**
            else if ((errorCode = cbs_open_async(result->cbsHandle, on_cbs_open_complete, result->cbsHandle, on_cbs_error, result->cbsHandle)) != 0)
            {
                LogError("Could Not Open CBS Handle %d\r\n", errorCode);
                isError = true;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_032: \[**EventHubAuthCBS_Create shall initialize its internal data structures using the configuration data passed in.**\]**
            else
            {
                
                result->credential = eventHubAuthConfig->credential;
                result->sasTokenExpirationTimeInSec = eventHubAuthConfig->sasTokenExpirationTimeInSec;
                result->sasTokenRefreshPeriodInSecs = eventHubAuthConfig->sasTokenRefreshPeriodInSecs;
                result->sasTokenAuthFailureTimeoutInSecs = eventHubAuthConfig->sasTokenAuthFailureTimeoutInSecs;
                result->sasTokenPutTime = 0;
                result->cbsAuthCompleteTimestamp = 0;
                result->sasTokenCreateTimestamp = 0;
                result->cbsStatus = EVENTHUBAUTH_STATUS_IDLE;
                result->cbOperation = EVENTHUBAUTH_OP_NONE;
                result->sasTokenHandle = NULL;
            }
        }

        if (isError == true)
        {
            if (result->cbsHandle != NULL) cbs_destroy(result->cbsHandle);
            if (result->encodedSASKeyValue != NULL) STRING_delete(result->encodedSASKeyValue);
            if (result->encodedURI != NULL) STRING_delete(result->encodedURI);
            if (result->sharedAccessKeyName != NULL) STRING_delete(result->sharedAccessKeyName);
            if (result->uri != NULL) STRING_delete(result->uri);
            if (result->extSASToken != NULL) STRING_delete(result->extSASToken);
            if (result->extSASTokenURI != NULL) STRING_delete(result->extSASTokenURI);
            free(result);
            //**Codes_SRS_EVENTHUB_AUTH_29_011: \[**For all errors, EventHubAuthCBS_Create shall return NULL.**\]**
            result = NULL;
        }
    }

    //**Codes_SRS_EVENTHUB_AUTH_29_033: \[**EventHubAuthCBS_Create shall return a non-NULL handle encapsulating the storage of the data provided.**\]**
    return (EVENTHUBAUTH_CBS_HANDLE)result;
}

void EventHubAuthCBS_Destroy(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle)
{
    EVENTHUBAUTH_CBS_STRUCT* eventHubAuth = (EVENTHUBAUTH_CBS_STRUCT*)eventHubAuthHandle;

    //**Codes_SRS_EVENTHUB_AUTH_29_070: \[**EventHubAuthCBS_Destroy shall return immediately eventHubAuthHandle is NULL.**\]**
    if (eventHubAuth)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_071: \[**EventHubAuthCBS_Destroy shall destroy the CBS handle if not NULL by using API cbs_destroy.**\]**
        if (eventHubAuth->cbsHandle != NULL)
        {
            cbs_destroy(eventHubAuth->cbsHandle);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_072: \[**EventHubAuthCBS_Destroy shall destroy the SAS token if not NULL by calling API STRING_delete.**\]**
        if (eventHubAuth->sasTokenHandle != NULL)
        {
            STRING_delete(eventHubAuth->sasTokenHandle);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_073: \[**EventHubAuthCBS_Destroy shall destroy the Base64 encoded shared access key by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->encodedSASKeyValue != NULL)
        {
            STRING_delete(eventHubAuth->encodedSASKeyValue);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_074: \[**EventHubAuthCBS_Destroy shall destroy the encoded URI by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->encodedURI != NULL)
        {
            STRING_delete(eventHubAuth->encodedURI);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_075: \[**EventHubAuthCBS_Destroy shall destroy the shared access key name by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->sharedAccessKeyName != NULL)
        {
            STRING_delete(eventHubAuth->sharedAccessKeyName);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_076: \[**EventHubAuthCBS_Destroy shall destroy the ext SAS token by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->extSASToken != NULL)
        {
            STRING_delete(eventHubAuth->extSASToken);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_077: \[**EventHubAuthCBS_Destroy shall destroy the ext SAS token URI by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->extSASTokenURI != NULL)
        {
            STRING_delete(eventHubAuth->extSASTokenURI);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_078: \[**EventHubAuthCBS_Destroy shall destroy the URI by calling API STRING_delete if not NULL.**\]**
        if (eventHubAuth->uri != NULL)
        {
            STRING_delete(eventHubAuth->uri);
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_079: \[**EventHubAuthCBS_Destroy shall free the internal data structure allocated earlier using API free.**\]**
        free(eventHubAuth);
    }
}

EVENTHUBAUTH_RESULT EventHubAuthCBS_Authenticate(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle)
{
    EVENTHUBAUTH_RESULT result;
    bool putToken;
    int errorCode;
    uint64_t secondsSinceEpoch;
    STRING_HANDLE sasTokenHandle, uriHandle;
    EVENTHUBAUTH_CBS_STRUCT* eventHubAuth = (EVENTHUBAUTH_CBS_STRUCT*)eventHubAuthHandle;

    //**Codes_SRS_EVENTHUB_AUTH_29_101: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**
    if (eventHubAuth == NULL)
    {
        LogError("Invalid Argument eventHubAuthHandle\r\n");
        result = EVENTHUBAUTH_RESULT_INVALID_ARG;
        putToken = false;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_102: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_NOT_PERMITED immediately if the status is in EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
    else if (eventHubAuth->cbsStatus == EVENTHUBAUTH_STATUS_IN_PROGRESS)
    {
        LogError("Operation Not Permitted. Token Put Already In Progress.\r\n");
        result = EVENTHUBAUTH_RESULT_NOT_PERMITED;
        putToken = false;
    }
    //**Codes_SRS_EVENTHUB_AUTH_29_103: \[**EventHubAuthCBS_Authenticate shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
    else if ((errorCode = GetSecondsSinceEpoch(&secondsSinceEpoch)) != 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
        LogError("Could Not Get Seconds Since Epoch. Code:%d\r\n", errorCode);
        result = EVENTHUBAUTH_RESULT_ERROR;
        putToken = false;
    }
    else if (eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_104: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Authenticate shall check if the ext token has expired using the seconds from epoch.**\]**
        if (secondsSinceEpoch < eventHubAuth->extSASTokenExpTSInEpochSec)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
            LogError("External SAS Token Expired.\r\n");
            result = EVENTHUBAUTH_RESULT_ERROR;
            putToken = false;
        }
        else
        {
            sasTokenHandle = eventHubAuth->extSASToken;
            uriHandle = eventHubAuth->extSASTokenURI;
            putToken = true;
        }
    }
    else if (eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_105: \[**If the credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Authenticate shall create a new SAS token STRING_HANDLE using API SASToken_Create and passing in Base64 encoded shared access key STRING_HANDLE, encoded URI STRING_HANDLE, shared access key name STRING_HANDLE and expiration time in seconds from epoch.**\]**
        size_t expirationTime;
        eventHubAuth->sasTokenCreateTimestamp = secondsSinceEpoch;
        expirationTime = (size_t)(secondsSinceEpoch + eventHubAuth->sasTokenExpirationTimeInSec);
        uriHandle = eventHubAuth->uri;
        
        if ((sasTokenHandle = SASToken_Create(eventHubAuth->encodedSASKeyValue, eventHubAuth->encodedURI, eventHubAuth->sharedAccessKeyName, expirationTime)) == NULL)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
            LogError("Could not create new SASToken\r\n");
            result = EVENTHUBAUTH_RESULT_ERROR;
            putToken = false;
        }
        else
        {
            putToken = true;
        }
    }
    else
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
        LogError("Operation Not Permitted.\r\n");
        result = EVENTHUBAUTH_RESULT_ERROR;
        putToken = false;
    }

    if (putToken)
    {
        const char *uri, *sasToken;
        //**Codes_SRS_EVENTHUB_AUTH_29_106: \[**EventHubAuthCBS_Authenticate shall delete the existing SAS token STRING_HANDLE if not NULL by calling STRING_delete.**\]**
        if (eventHubAuth->sasTokenHandle != NULL)
        {
            STRING_delete(eventHubAuth->sasTokenHandle);
        }
        eventHubAuth->sasTokenHandle = sasTokenHandle;

        //**Codes_SRS_EVENTHUB_AUTH_29_107: \[**EventHubAuthCBS_Authenticate shall obtain the underlying C string buffer of the uri STRING_HANDLE calling STRING_c_str.**\]**
        if ((uri = STRING_c_str(uriHandle)) == NULL)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
            LogError("Could Not Retrieve URI Buffer\r\n");
            result = EVENTHUBAUTH_RESULT_ERROR;
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_108: \[**EventHubAuthCBS_Authenticate shall obtain the underlying C string buffer of STRING_HANDLE SAS token by calling STRING_c_str.**\]**
        else if ((sasToken = STRING_c_str(eventHubAuth->sasTokenHandle)) == NULL)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
            LogError("Could Not Retrieve SASToken Buffer\r\n");
            result = EVENTHUBAUTH_RESULT_ERROR;
        }
        else
        {
            eventHubAuth->sasTokenPutTime = secondsSinceEpoch;
            eventHubAuth->cbOperation = EVENTHUBAUTH_OP_PUT;
            //**Codes_SRS_EVENTHUB_AUTH_29_109: \[**EventHubAuthCBS_Authenticate shall establish (put) the new token by calling API cbs_put_token and passing in the CBS handle, "servicebus.windows.net:sastoken", URI string buffer, SAS token string buffer, OnCBSPutTokenOperationComplete and eventHubAuthHandle.**\]**
            if ((errorCode = cbs_put_token_async(eventHubAuth->cbsHandle, SAS_TOKEN_TYPE, uri, sasToken, OnCBSPutTokenOperationComplete, eventHubAuth)) != 0)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_111: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
                LogError("cbs_put_token Returned Error %d\r\n", errorCode);
                result = EVENTHUBAUTH_RESULT_ERROR;
            }
            else
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_110: \[**EventHubAuthCBS_Authenticate shall return EVENTHUBAUTH_RESULT_OK on success and transition status to EVENTHUBAUTH_STATUS_IN_PROGRESS.**\]**
                eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_IN_PROGRESS;
                result = EVENTHUBAUTH_RESULT_OK;
            }
        }
    }

    return result;
}

EVENTHUBAUTH_RESULT EventHubAuthCBS_Refresh(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle, STRING_HANDLE extSASToken)
{
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_STRUCT* eventHubAuth = (EVENTHUBAUTH_CBS_STRUCT*)eventHubAuthHandle;
    int errorCode;
    bool performAuthentication;

    //**Codes_SRS_EVENTHUB_AUTH_29_200: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle is NULL.**\]**
    if (eventHubAuth == NULL)
    {
        LogError("Invalid Argument eventHubAuthHandle\r\n");
        result = EVENTHUBAUTH_RESULT_INVALID_ARG;
        performAuthentication = false;
    }
    else if (eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_201: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT and if extSASToken is NULL, EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_INVALID_ARG.**\]**
        if (extSASToken == NULL)
        {
            LogError("Invalid Argument extSASToken\r\n");
            result = EVENTHUBAUTH_RESULT_INVALID_ARG;
            performAuthentication = false;
        }
        else
        {
            const char* sasToken;
            STRING_HANDLE extSASTokenTemp, extSASTokenURITemp;
            uint64_t expirationTimestamp;

            //**Codes_SRS_EVENTHUB_AUTH_29_202: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall clone the extSASToken using API STRING_clone.**\]**
            if ((extSASTokenTemp = STRING_clone(extSASToken)) == NULL)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
                LogError("Could not clone extSASToken\r\n");
                result = EVENTHUBAUTH_RESULT_ERROR;
                performAuthentication = false;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_203: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall create a new temp STRING using API STRING_new to hold the refresh ext SAS token URI.**\]**
            else if ((extSASTokenURITemp = STRING_new()) == NULL)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
                LogError("Could not create new string handle\r\n");
                STRING_delete(extSASTokenTemp);
                result = EVENTHUBAUTH_RESULT_ERROR;
                performAuthentication = false;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_204: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall obtain the underlying C string buffer of cloned extSASToken using API STRING_c_str.**\]**
            else if ((sasToken = STRING_c_str(extSASTokenTemp)) == NULL)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
                LogError("Could not obtain string from SAS token\r\n");
                STRING_delete(extSASTokenTemp);
                STRING_delete(extSASTokenURITemp);
                result = EVENTHUBAUTH_RESULT_ERROR;
                performAuthentication = false;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_205: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall obtain the expiration time and the URI from the cloned extSASToken.**\]**
            else if (GetURIAndExpirationFromSASToken(sasToken, extSASTokenURITemp, &expirationTimestamp) != 0)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
                LogError("GetURIAndExpirationFromSASToken Returned Error.\r\n");
                STRING_delete(extSASTokenTemp);
                STRING_delete(extSASTokenURITemp);
                result = EVENTHUBAUTH_RESULT_ERROR;
                performAuthentication = false;
            }
            //**Codes_SRS_EVENTHUB_AUTH_29_206: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall compare the refresh token URI and the existing ext token URI using API STRING_compare. If a mismatch is observed, EVENTHUBAUTH_RESULT_ERROR shall be returned and any resources shall be deallocated.**\]**
            else if (STRING_compare(eventHubAuth->extSASTokenURI, extSASTokenURITemp) != 0)
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
                LogError("URI mismatch observed, invalid Refresh SAS Token. Original Token URI:%s, Refresh Token URI:%s\r\n", STRING_c_str(eventHubAuth->extSASTokenURI), STRING_c_str(extSASTokenURITemp));
                STRING_delete(extSASTokenTemp);
                STRING_delete(extSASTokenURITemp);
                result = EVENTHUBAUTH_RESULT_ERROR;
                performAuthentication = false;
            }
            else
            {
                //**Codes_SRS_EVENTHUB_AUTH_29_207: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall delete the prior ext SAS Token using API STRING_delete.**\]**
                if (eventHubAuth->extSASToken)
                {
                    STRING_delete(eventHubAuth->extSASToken);
                }
                //**Codes_SRS_EVENTHUB_AUTH_29_208: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_EXT, EventHubAuthCBS_Refresh shall delete the temp refresh ext SAS token URI using API STRING_delete.**\]**
                STRING_delete(extSASTokenURITemp);
                eventHubAuth->extSASToken = extSASTokenTemp;
                eventHubAuth->extSASTokenExpTSInEpochSec = expirationTimestamp;
                performAuthentication = true;
            }
        }
    }
    else if (eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_209: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO EventHubAuthCBS_Refresh shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
        //**Codes_SRS_EVENTHUB_AUTH_29_210: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_Refresh shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
        if ((errorCode = UpdateEventHubAuthACBSStatus(eventHubAuth)) != 0)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
            LogError("Error Getting Updated Event Hub Status. Code:%d\r\n", errorCode);
            result = EVENTHUBAUTH_RESULT_ERROR;
            performAuthentication = false;
        }
        //**Codes_SRS_EVENTHUB_AUTH_29_211: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_NOT_PERMITED if credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO and the status is not EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
        else if (eventHubAuth->cbsStatus != EVENTHUBAUTH_STATUS_REFRESH_REQUIRED)
        {
            LogError("Operation Not Permitted. Refresh Not Required. Status:%u\r\n", eventHubAuth->cbsStatus);
            result = EVENTHUBAUTH_RESULT_NOT_PERMITED;
            performAuthentication = false;
        }
        else
        {
            performAuthentication = true;
        }
    }

    if (performAuthentication == true)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_212: \[**EventHubAuthCBS_Refresh shall call EventHubAuthCBS_Authenticate and pass in the eventHubAuthHandle.**\]**
        //**Codes_SRS_EVENTHUB_AUTH_29_213: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_OK on success.**\]**
        //**Codes_SRS_EVENTHUB_AUTH_29_214: \[**EventHubAuthCBS_Refresh shall return EVENTHUBAUTH_RESULT_ERROR on errors.**\]**
        result = EventHubAuthCBS_Authenticate(eventHubAuthHandle);
    }

    return result;
}

EVENTHUBAUTH_RESULT EventHubAuthCBS_GetStatus(EVENTHUBAUTH_CBS_HANDLE eventHubAuthHandle, EVENTHUBAUTH_STATUS* returnStatus)
{
    int errorCode;
    EVENTHUBAUTH_RESULT result;
    EVENTHUBAUTH_CBS_STRUCT* eventHubAuth = (EVENTHUBAUTH_CBS_STRUCT*)eventHubAuthHandle;

    //**Codes_SRS_EVENTHUB_AUTH_29_250: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_INVALID_ARG if eventHubAuthHandle or returnStatus is NULL.**\]**
    if ((eventHubAuth == NULL) || (returnStatus == NULL))
    {
        result = EVENTHUBAUTH_RESULT_INVALID_ARG;
    }
    else if ((errorCode = UpdateEventHubAuthACBSStatus(eventHubAuth)) != 0)
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_255: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_ERROR on error.**\]**
        LogError("Error Getting Updated Event Hub Status. Code:%d\r\n", errorCode);
        result = EVENTHUBAUTH_RESULT_ERROR;
    }
    else
    {
        //**Codes_SRS_EVENTHUB_AUTH_29_254: \[**EventHubAuthCBS_GetStatus shall return EVENTHUBAUTH_RESULT_OK on success and copy the current EventHubAuth status into the returnStatus parameter.**\]**
        *returnStatus = eventHubAuth->cbsStatus;
        result = EVENTHUBAUTH_RESULT_OK;
    }

    return result;
}

static int UpdateEventHubAuthACBSStatus(EVENTHUBAUTH_CBS_STRUCT* eventHubAuth)
{
    int errorCode, result = 0;

    //**Codes_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
    //**Codes_SRS_EVENTHUB_AUTH_29_252: \[**If credential type is EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO, EventHubAuthCBS_GetStatus shall check if SAS token refresh is required by checking if the difference between time "now" and the token creation time is greater than the sasTokenRefreshPeriodInSecs configuration parameter. If a refresh timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_REFRESH_REQUIRED.**\]**
    if ((eventHubAuth->credential == EVENTHUBAUTH_CREDENTIAL_TYPE_SASTOKEN_AUTO) && 
        ((eventHubAuth->cbsStatus == EVENTHUBAUTH_STATUS_OK) || (eventHubAuth->cbsStatus == EVENTHUBAUTH_STATUS_REFRESH_REQUIRED)))
    {
        bool isExpired = false, isRefreshRequired = false;
        if ((errorCode = CheckExpirationAndRefreshStatus(eventHubAuth, &isExpired, &isRefreshRequired)) != 0)
        {
            LogError("Error Getting Updated Expiration/Refresh Status. Code:%d\r\n", errorCode);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }

        if (isExpired)
        {
            eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_EXPIRED;
        }
        else if (isRefreshRequired)
        {
            eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_REFRESH_REQUIRED;
        }
    }
    else if (eventHubAuth->cbsStatus == EVENTHUBAUTH_STATUS_IN_PROGRESS)
    {
        bool hasPutTimeoutOccured = false;
        if (eventHubAuth->cbOperation == EVENTHUBAUTH_OP_PUT)
        {
            //**Codes_SRS_EVENTHUB_AUTH_29_251: \[**EventHubAuthCBS_GetStatus shall obtain seconds from epoch by calling APIs get_time and get_difftime.**\]**
            //**Codes_SRS_EVENTHUB_AUTH_29_253: \[**EventHubAuthCBS_GetStatus shall check if SAS token put operation is in progress and checking if the difference between time "now" and the token put time is greater than the sasTokenAuthFailureTimeoutInSecs configuration parameter. If a put timeout has occurred the current EventHubAuth status shall be updated to EVENTHUBAUTH_STATUS_TIMEOUT.**\]**
            if ((errorCode = CheckPutTimeoutStatus(eventHubAuth, &hasPutTimeoutOccured)) != 0)
            {
                LogError("Error Getting Updated Expiration/Refresh Status. Code:%d\r\n", errorCode);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }

            if (hasPutTimeoutOccured)
            {
                eventHubAuth->cbsStatus = EVENTHUBAUTH_STATUS_TIMEOUT;
            }
        }
    }
    else
    {
        result = 0;
    }

    return result;
}
