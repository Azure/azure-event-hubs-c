// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/buffer_.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/map.h"

#include "eventdata.h"

MU_DEFINE_ENUM_STRINGS(EVENTDATA_RESULT, EVENTDATA_ENUM_VALUES)

typedef struct EVENT_DATA_TAG
{
    BUFFER_HANDLE buffer;
    STRING_HANDLE partitionKey;
    MAP_HANDLE properties;
    uint64_t enqueuedTimestampUTC;
} EVENT_DATA;

typedef struct EVENT_PROPERTY_TAG
{
    STRING_HANDLE key;
    STRING_HANDLE value;
} EVENT_PROPERTY;

static bool ContainsOnlyUsAscii(const char* asciiValue)
{
    bool result = true;;
    const char* iterator = asciiValue;
    while (iterator != NULL && *iterator != '\0')
    {
        // Allow only printable ascii char 
        if (*iterator < ' ' || *iterator > '~')
        {
            result = false;
            break;
        }
        iterator++;
    }
    return result;
}

static int ValidateAsciiCharactersFilter(const char* mapKey, const char* mapValue)
{
    int result;
    if (!ContainsOnlyUsAscii(mapKey) || !ContainsOnlyUsAscii(mapValue) )
    {
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

EVENTDATA_HANDLE EventData_CreateWithNewMemory(const unsigned char* data, size_t length)
{
    EVENTDATA_HANDLE result;
    EVENT_DATA* eventData;
    /* Codes_SRS_EVENTDATA_03_003: [EventData_Create shall return a NULL value if length is not zero and data is NULL.] */
    if (length != 0 && data == NULL)
    {
        result = NULL;
        LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG));
    }
    else if ((eventData = (EVENT_DATA*)malloc(sizeof(EVENT_DATA))) == NULL)
    {
        /* Codes_SRS_EVENTDATA_03_004: [For all other errors, EventData_Create shall return NULL.] */
        result = NULL;
        LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
    }
    /* Codes_SRS_EVENTDATA_03_008: [EventData_CreateWithNewMemory shall allocate new memory to store the specified data.] */
    else if ((eventData->buffer = BUFFER_new()) == NULL)
    {
        free(eventData);
        /* Codes_SRS_EVENTDATA_03_004: [For all other errors, EventData_Create shall return NULL.] */
        result = NULL;
        LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
    }
    else if (length != 0)
    {
        /* Codes_SRS_EVENTDATA_03_002: [EventData_CreateWithNewMemory shall provide a none-NULL handle encapsulating the storage of the data provided.] */
        if (BUFFER_build(eventData->buffer, data, length) != 0)
        {
            BUFFER_delete(eventData->buffer);
            free(eventData);
            /* Codes_SRS_EVENTDATA_03_004: [For all other errors, EventData_Create shall return NULL.] */
            result = NULL;
            LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
        }
        else
        {
            eventData->partitionKey = NULL;
            if ( (eventData->properties = Map_Create(ValidateAsciiCharactersFilter) ) == NULL)
            {
                BUFFER_delete(eventData->buffer);
                free(eventData);
                /* Codes_SRS_EVENTDATA_03_004: [For all other errors, EventData_Create shall return NULL.] */
                result = NULL;
                LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
            }
            else
            {
                eventData->enqueuedTimestampUTC = 0;
                result = (EVENTDATA_HANDLE)eventData;
            }
        }
    }
    else
    {
        eventData->partitionKey = NULL;
        if ( (eventData->properties = Map_Create(ValidateAsciiCharactersFilter) ) == NULL)
        {
            BUFFER_delete(eventData->buffer);
            free(eventData);
            /* Codes_SRS_EVENTDATA_03_004: [For all other errors, EventData_Create shall return NULL.] */
            result = NULL;
            LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
        }
        else
        {
            eventData->enqueuedTimestampUTC = 0;
            result = (EVENTDATA_HANDLE)eventData;
        }
    }
    return result;
}

void EventData_Destroy(EVENTDATA_HANDLE eventDataHandle)
{
    EVENT_DATA* eventData;
    /* Codes_SRS_EVENTDATA_03_006: [EventData_Destroy shall not do anything if eventDataHandle is NULL.] */
    if (eventDataHandle != NULL)
    {
        /* Codes_SRS_EVENTDATA_03_005: [EventData_Destroy shall deallocate all resources related to the eventDataHandle specified.] */
        eventData = (EVENT_DATA*)eventDataHandle;
        BUFFER_delete(eventData->buffer);
        STRING_delete(eventData->partitionKey);
        eventData->partitionKey = NULL;
        Map_Destroy(eventData->properties);

        free(eventData);
    }
}

/* Codes_SRS_EVENTDATA_03_019: [EventData_GetData shall provide a pointer and size for the data associated with the eventDataHandle.] */
EVENTDATA_RESULT EventData_GetData(EVENTDATA_HANDLE eventDataHandle, const unsigned char** buffer, size_t* size)
{
    EVENTDATA_RESULT result;

    /* Codes_SRS_EVENTDATA_03_022: [If any of the arguments passed to EventData_GetData is NULL, EventData_GetData shall return EVENTDATA_INVALID_ARG.] */
    if (eventDataHandle == NULL || buffer == NULL || size == NULL)
    {
        result = EVENTDATA_INVALID_ARG;
        LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, result));
    }
    else
    {
        /* Codes_SRS_EVENTDATA_03_020: [The pointer shall be obtained by using BUFFER_content and it shall be copied in the buffer argument. The size of the associated data shall be obtained by using BUFFER_size and it shall be copied to the size argument.] */
        if (BUFFER_content(((EVENT_DATA*)eventDataHandle)->buffer, buffer) != 0)
        {
            /* Codes_SRS_EVENTDATA_03_023: [If EventData_GetData fails because of any other error it shall return EVENTDATA_ERROR.]*/
            result = EVENTDATA_ERROR;
            LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, result));
        }
        else if (BUFFER_size(((EVENT_DATA*)eventDataHandle)->buffer, size) != 0)
        {
            /* Codes_SRS_EVENTDATA_03_023: [If EventData_GetData fails because of any other error it shall return EVENTDATA_ERROR.]*/
            result = EVENTDATA_ERROR;
            LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, result));
        }
        else
        {
            /* Codes_SRS_EVENTDATA_03_021: [On success, EventData_GetData shall return EVENTDATA_OK.] */
            result = EVENTDATA_OK;
        }
    }
    return result;
}

const char* EventData_GetPartitionKey(EVENTDATA_HANDLE eventDataHandle)
{
    const char* result;
    /* Codes_SRS_EVENTDATA_07_024: [EventData_GetPartitionKey shall return NULL if the eventDataHandle parameter is NULL.] */
    if (eventDataHandle == NULL)
    {
        result = NULL;
        LogError("EventData_GetPartitionKey result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG));
    }
    else
    {
        /* Codes_SRS_EVENTDATA_07_025: [EventData_GetPartitionKey shall return NULL if the partitionKey in the EVENTDATA_HANDLE is NULL.] */
        EVENT_DATA* eventData = (EVENT_DATA*)eventDataHandle;
        if (eventData->partitionKey == NULL)
        {
            result = NULL;
        }
        else
        {
            /* Codes_SRS_EVENTDATA_07_026: [On success EventData_GetPartitionKey shall return a const char* variable that is pointing to the Partition Key value that is stored in the EVENTDATA_HANDLE.] */
            result = STRING_c_str(eventData->partitionKey);
        }
    }
    return result;
}

EVENTDATA_RESULT EventData_SetPartitionKey(EVENTDATA_HANDLE eventDataHandle, const char* partitionKey)
{
    EVENTDATA_RESULT result;
    /* Codes_SRS_EVENTDATA_07_031: [EventData_SetPartitionKey shall return a nonzero value if eventDataHandle or partitionKey is NULL.] */
    if (eventDataHandle == NULL)
    {
        result = EVENTDATA_INVALID_ARG;
        LogError("EventData_SetPartitionKey result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, result));
    }
    else
    {
        /* Codes_SRS_EVENTDATA_07_027: [If the partitionKey variable contained in the eventDataHandle parameter is not NULL then EventData_SetPartitionKey shall delete the partitionKey STRING_HANDLE.] */
        EVENT_DATA* eventData = (EVENT_DATA*)eventDataHandle;
        if (eventData->partitionKey != NULL)
        {
            // Delete the memory if there is any
            STRING_delete(eventData->partitionKey);
        }
        if (partitionKey != NULL)
        {
            /* Codes_SRS_EVENTDATA_07_028: [On success EventData_SetPartitionKey shall store the const char* partitionKey parameter in the EVENTDATA_HANDLE data structure partitionKey variable.] */
            eventData->partitionKey = STRING_construct(partitionKey);
        }
        /* Codes_SRS_EVENTDATA_07_030: [On Success EventData_SetPartitionKey shall return EVENTDATA_OK.] */
        result = EVENTDATA_OK;
    }
    return result;
}

EVENTDATA_HANDLE EventData_Clone(EVENTDATA_HANDLE eventDataHandle)
{
    EVENT_DATA* result;
    if (eventDataHandle == NULL)
    {
        /* Codes_SRS_EVENTDATA_07_050: [EventData_Clone shall return NULL when the eventDataHandle is NULL.] */
        result = NULL;
        LogError("EventData_Clone result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_INVALID_ARG));
    }
    else
    {
        EVENT_DATA* srcData = (EVENT_DATA*)eventDataHandle;

        if ( (result = (EVENT_DATA*)malloc(sizeof(EVENT_DATA) ) ) == NULL)
        {
            /* Codes_SRS_EVENTDATA_07_053: [EventData_Clone shall return NULL if it fails for any reason.] */
            result = NULL;
            LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
        }
        else
        {
            // Need to initialize this here in case it doesn't get set later
            result->partitionKey = NULL;
            result->enqueuedTimestampUTC = srcData->enqueuedTimestampUTC;
            /* Codes_SRS_EVENTDATA_07_051: [EventData_Clone shall make use of BUFFER_Clone to clone the EVENT_DATA buffer.] */
            if ( (result->buffer = BUFFER_clone(srcData->buffer) ) == NULL)
            {
                /* Codes_SRS_EVENTDATA_07_053: [EventData_Clone shall return NULL if it fails for any reason.] */
                free(result);
                result = NULL;
                LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
            }
            /* Codes_SRS_EVENTDATA_07_052: [EventData_Clone shall make use of STRING_Clone to clone the partitionKey if it is not set.] */
            else if ( (srcData->partitionKey != NULL) && (result->partitionKey = STRING_clone(srcData->partitionKey) ) == NULL)
            {
                /* Codes_SRS_EVENTDATA_07_053: [EventData_Clone shall return NULL if it fails for any reason.] */
                BUFFER_delete(result->buffer);
                free(result);
                result = NULL;
                LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
            }
            /* Codes_SRS_EVENTDATA_07_054: [EventData_Clone shall make use of Map_Clone to clone the properties if it is not set.] */
            else if ( (result->properties = Map_Clone(srcData->properties)) == NULL)
            {
                /* Codes_SRS_EVENTDATA_07_053: [EventData_Clone shall return NULL if it fails for any reason.] */
                STRING_delete(result->partitionKey);
                BUFFER_delete(result->buffer);
                free(result);
                result = NULL;
                LogError("result = %s", MU_ENUM_TO_STRING(EVENTDATA_RESULT, EVENTDATA_ERROR));
            }
        }
    }
    return (EVENTDATA_HANDLE)result; 
}

MAP_HANDLE EventData_Properties(EVENTDATA_HANDLE eventDataHandle)
{
    MAP_HANDLE result;
    /* Codes_SRS_EVENTDATA_07_034: [if eventDataHandle is NULL then EventData_Properties shall return NULL.] */
    if (eventDataHandle == NULL)
    {
        LogError("invalid arg (NULL) passed to EventData_Properties");
        result = NULL;
    }
    else
    {
        /* Codes_SRS_EVENTDATA_07_035: [Otherwise, for any non-NULL eventDataHandle it shall return a non-NULL MAP_HANDLE.] */
        EVENT_DATA* handleData = (EVENT_DATA*)eventDataHandle;
        result = handleData->properties;
    }
    return result;
}

EVENTDATA_RESULT EventData_SetEnqueuedTimestampUTCInMs(EVENTDATA_HANDLE eventDataHandle, uint64_t timestampInMs)
{
    EVENTDATA_RESULT result;

    if (eventDataHandle == NULL)
    {
        //**Codes_SRS_EVENTDATA_29_060: \[**`EventData_SetEnqueuedTimestampUTCInMs` shall return EVENTDATA_INVALID_ARG if eventDataHandle parameter is NULL.**\]**
        LogError("invalid arg (NULL) passed to EventData_SetEnqueuedTimestampUTCInMS\r\n");
        result = EVENTDATA_INVALID_ARG;
    }
    else
    {
        //**Codes_SRS_EVENTDATA_29_061: \[**On success `EventData_SetEnqueuedTimestampUTCInMs` shall store the timestamp parameter in the EVENTDATA_HANDLE data structure.**\]**
        //**Codes_SRS_EVENTDATA_29_062: \[**On Success `EventData_SetEnqueuedTimestampUTCInMs` shall return EVENTDATA_OK.**\]**
        EVENT_DATA* handleData = (EVENT_DATA*)eventDataHandle;
        handleData->enqueuedTimestampUTC = timestampInMs;
        result = EVENTDATA_OK;
    }
    return result;
}

uint64_t EventData_GetEnqueuedTimestampUTCInMs(EVENTDATA_HANDLE eventDataHandle)
{
    uint64_t result;

    if (eventDataHandle == NULL)
    {
        //**Codes_SRS_EVENTDATA_07_070: \[**`EventData_GetEnqueuedTimestampUTCInMs` shall return 0 if the eventDataHandle parameter is NULL.**\]**
        LogError("invalid arg (NULL) passed to EventData_GetEnqueuedTimestampUTCInMS\r\n");
        result = 0;
    }
    else
    {
        //**Codes_SRS_EVENTDATA_07_071: \[**If eventDataHandle is not null, `EventData_GetEnqueuedTimestampUTCInMs` shall return the timestamp value stored in the EVENTDATA_HANDLE.**\]**
        EVENT_DATA* handleData = (EVENT_DATA*)eventDataHandle;
        result = handleData->enqueuedTimestampUTC;
    }

    return result;
}
