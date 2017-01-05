// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/**
*   @file eventhubauth.h
*	@brief The EventHubAuth utility module implements operations for
*          SAS token based authentication for the EventHub Sender and
*          EventHub Receiver.
*
*	@details EventHubAuth is a module that can be used for SAS token
*            creation, establishment, token expiration and refresh
*            required for EventHub IO is taken care by this module.
*            This utility module is not meant to be used directly 
*            by clients, rather this is suited for internal consumption.
*/

#ifndef EVENTHUBAUTH_H
#define EVENTHUBAUTH_H

#include "azure_c_shared_utility/macro_utils.h"
#include "azure_c_shared_utility/umock_c_prod.h"

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

/**
* @brief	A callback definition for asynchronous callback used by
*           clients of EventHubReceiver_LL for purposes of communication
*           with an existing Event Hub. This callback will be invoked
*           when an event is received (read from) an event hub partition.
*
* @param	eventHubAuthConfig  Pointer to a initialized EVENTHUBAUTH_CBS_CONFIG struct
* @param	cbsSessionHandle    Handle to a valid session for which authentication needs to be established
*
* @return   On success a non null handle will be returned, null otherwise.
*/
MOCKABLE_FUNCTION(, EVENTHUBAUTH_CBS_HANDLE, EventHubAuthCBS_Create, EVENTHUBAUTH_CBS_CONFIG*, eventHubAuthConfig, SESSION_HANDLE, cbsSessionHandle);

/**
* @brief	Disposes of resources allocated by the EventHubAuthCBS_Create.
*
* @param	eventHubAuthHandle	The handle created by a call to the create function.
*
* @note     This is a blocking call.
*
* @return	None
*/
MOCKABLE_FUNCTION(, void, EventHubAuthCBS_Destroy, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);

/**
* @brief	Perform the required operation needed to establish token based authentication
*           with the Event hub for any IO.
*
* @param	eventHubAuthHandle	Valid handle returned by EventHubAuthCBS_Create.
*
* @note     This is a blocking call.
*
* @return	EVENTHUBAUTH_RESULT_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Authenticate, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);

/**
* @brief	Perform the required operation delete any tokens and tear down communication with the
*           EventHub authentication service.
*
* @param	eventHubAuthHandle	Valid handle returned by EventHubAuthCBS_Create.
*
* @note     This is a blocking call.
*
* @return	EVENTHUBAUTH_RESULT_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Reset, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);

/**
* @brief	Perform the required operation needed to establish a new token based authentication
*           with the Event hub for any IO, while one is still valid and active.
*
* @param	eventHubAuthHandle	Valid handle returned by EventHubAuthCBS_Create.
*
* @note     This is a blocking call.
*
* @return	EVENTHUBAUTH_RESULT_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_Refresh, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle);

/**
* @brief	Return the status of the EventHub authentication service.
*           This is used by clients to perform any follow up operations
*           based on the returned state.
*
* @param	eventHubAuthHandle	Valid handle returned by EventHubAuthCBS_Create.
* @param	status	            EventHubAuth status will be returned here.
*
* @return	EVENTHUBAUTH_RESULT_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBAUTH_RESULT, EventHubAuthCBS_GetStatus, EVENTHUBAUTH_CBS_HANDLE, eventHubAuthHandle, EVENTHUBAUTH_STATUS*, returnStatus);

#endif  //EVENTHUBAUTH_H
