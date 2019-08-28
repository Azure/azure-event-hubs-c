// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

/** @file eventhubreceiver.h
*    @brief Extends the EventHubReceiver_LL module with additional features.
*
*    @details EventHubReceiver is a module that extends the EventHubReceiver_LL
*             module with 2 features:
*                - scheduling the work for the EventHubReceiver using a
*                  thread, so that the user does not need to create their
*                  own thread
*                - thread-safe APIs
*/

#ifndef EVENTHUBRECEIVER_H
#define EVENTHUBRECEIVER_H

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

#include "eventhubreceiver_ll.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
* @brief    An opaque handle used by clients of EventHubReceiver for 
*           purposes of communication with an existing Event Hub.
*/
typedef void* EVENTHUBRECEIVER_HANDLE;

/**
* @brief    Creates a EventHubReceiver handle for communication with an existing
*           Event Hub using the specified parameters for the purposes of
*           reading event data posted on a specific partition within the
*           Event Hub.
*
* @param    connectionString    Pointer to a character string containing the connection string (see below)
* @param    eventHubPath        Pointer to a character string identifying the Event Hub path (see below)
* @param    consumerGroup       Pointer to a character string identifying a specific consumer group within a Event hub
* @param    partitionId         Pointer to a character string containing the specific ID for a partition within the event hub
*
* Sample connection string:
*  <blockquote>
*    <pre>Endpoint=sb://[Event Hub name goes here].[Event Hub suffix goes here, e.g., servicebus.windows.net];SharedAccessKeyName=[Policy key name here];SharedAccessKey=[Policy key];EntityPath=[Event Hub Path]</pre>
*  </blockquote>
*
* @return    A non-NULL @c EVENTHUBRECEIVER_HANDLE value that is used when invoking other functions for EventHubReceiver
*           @c NULL on failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_HANDLE, EventHubReceiver_Create,
    const char*, connectionString,
    const char*, eventHubPath,
    const char*, consumerGroup,
    const char*, partitionId
);

/**
* @brief    Creates a EventHubReceiver handle for communication with an existing
*           Event Hub using a SAS token for the purposes of
*           reading event data posted on a specific partition within the
*           Event Hub. This API should be used when clients want to generate and
*           manage their own SAS token for authentication purposes with the
*           EventHub service.
*
* @param    eventHubSasToken    Pointer to a character string containing the SAS token to be used for authentication
*
* Sample Token string:
*  <blockquote>
*    <pre>SharedAccessSignature sr={URI}&sig={HMAC_SHA256_SIGNATURE}&se={EXPIRATION_TIME}&skn={KEY_NAME}</pre>
*  </blockquote>
*
*  URI Format For a EventHubReceiver - sb%3a%2f%2f{EventHubHostName}%2f{EventHubPath}%2fConsumerGroup%2f{ConsumerGroup}%2f/Partitions/{PartitionID}
*  EventHubHostName - [Event Hub name].[Event Hub suffix e.g., servicebus.windows.net]
*  EventHubPath     - [Event Hub Path]
*  ConsumerGroup    - Ex. $Default
*  PartitionID      - Ex. "0", "12" etc.
*
* @return    A non-NULL @c EVENTHUBRECEIVER_HANDLE value that is used when invoking other functions for EventHubReceiver
*           @c NULL on failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_HANDLE, EventHubReceiver_CreateFromSASToken,
    const char*, eventHubSasToken
);

/**
* @brief    Disposes of resources allocated by the EventHubReceiver_Create.
*
* @param    eventHubReceiverHandle    The handle created by a call to the create function.
*
* @return   None
*/
MOCKABLE_FUNCTION(, void, EventHubReceiver_Destroy, EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle);

/**
* @brief    Asynchronous call to receive events message specified by @p eventHubReceiverHandle.
*
* @param    eventHubReceiverHandle      The handle created by a call to the create function.
* @param    onEventReceiveCallback      The callback specified by the user for receiving
*                                       event payload from the Event Hub. If there is event
*                                       data to be read, the callback result code will be 
*                                       EVENTHUBRECEIVER_OK.
* @param    onEventReceiveUserContext   User specified context that will be provided to the
*                                       callback. This can be @c NULL.
* @param    onEventReceiveErrorCallback The callback specified by the user for receiving
*                                       notifications of an unexpected error that might have
*                                       occurred during a receive operation from the Event Hub.
*                                       This will give the user an opportunity to manage this
*                                       situation such as cleaning up of resources etc.
* @param    onEventReceiveUserContext   User specified context that will be provided to the
*                                       callback. This can be @c NULL.
* @param    startTimestampInSec         Timestamp from UTC epoch in units of seconds from which
*                                       to read data from the partition in the Event Hub.
*
*            @b NOTE: The application behavior is undefined if the user calls
*            the EventHubReceiver_Destroy function from within any callback.
*
* @return    EVENTHUBRECEIVER_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION (, EVENTHUBRECEIVER_RESULT, EventHubReceiver_ReceiveFromStartTimestampAsync,
    EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback,
    void*, onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
    uint64_t, startTimestampInSec
);

/**
* @brief    Asynchronous call to receive events message specified by @p eventHubReceiverHandle.
*           Additionally users can specify a timeout value in milliseconds to wait in case there
*           are no more events to read from an Event Hub partition.
*
* @param    eventHubReceiverHandle      The handle created by a call to the create function.
* @param    onEventReceiveCallback      The callback specified by the user for receiving
*                                       event payload from the Event Hub. If there is event
*                                       data to be read, the callback result code will be
*                                       EVENTHUBRECEIVER_OK. If a timeout has occurred, 
*                                       the callback result code will be EVENTHUBRECEIVER_TIMEOUT.
* @param    onEventReceiveUserContext   User specified context that will be provided to the
*                                       callback. This can be @c NULL.
* @param    onEventReceiveErrorCallback The callback specified by the user for receiving
*                                       notifications of an unexpected error that might have
*                                       occurred during a receive operation from the Event Hub.
*                                       This will give the user an opportunity to manage this
*                                       situation such as cleaning up of resources etc.
* @param    onEventReceiveUserContext   User specified context that will be provided to the
*                                       callback. This can be @c NULL.
* @param    startTimestampInSec         Timestamp from UTC epoch in units of seconds from which
*                                       to read data from the partition in the Event Hub.
* @param    waitTimeoutInMs             Timeout wait period specified in milliseconds.
*                                       This is useful in cases when the user would like to get
*                                       notified due to inactivity for the specified wait period.
*                                       On expiration of the timeout period, the registered
*                                       onEventReceiveCallback will be invoked with result
*                                       parameter as EVENTHUBRECEIVER_TIMEOUT. Passing in a
*                                       value of zero will result in no timeouts and the behavior
*                                       will be similar to 
*                                       EventHubReceiver_ReceiveFromStartTimestampAsync.
*
*            @b NOTE: The application behavior is undefined if the user calls
*            the EventHubReceiver_Destroy function from within any callback.
*
* @return    EVENTHUBRECEIVER_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_ReceiveFromStartTimestampWithTimeoutAsync,
    EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle,
    EVENTHUBRECEIVER_ASYNC_CALLBACK, onEventReceiveCallback,
    void*, onEventReceiveUserContext,
    EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK, onEventReceiveErrorCallback,
    void*, onEventReceiveErrorUserContext,
    uint64_t, startTimestampInSec,
    unsigned int, waitTimeoutInMs
);

/**
* @brief    API to asynchronously cleanup and terminate communication with the event hub
*
* @param    eventHubReceiverHandle          The handle created by a call to the create function.
* @param    onEventReceiveEndCallback       The callback specified by the user notifying the
*                                           user that the communication to the event hub 
*                                           has terminated. On success, the callback result 
*                                           code will be EVENTHUBRECEIVER_OK and an error
*                                           code otherwise. This is an optional parameter and
*                                           therefore can be @c NULL.
* @param    onEventReceiveEndUserContext    User specified context that will be provided to the
*                                           callback. This can be @c NULL.
*
* @return   EVENTHUBRECEIVER_OK upon success or an error code upon failure.
*
*           @b NOTE: This API is safe to call within a EVENTHUBRECEIVER_ASYNC_CALLBACK or
*           EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK callback.
*
*           @b NOTE: The application behavior is undefined if the user calls
*           the EventHubReceiver_Destroy function from within any callback.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_ReceiveEndAsync,
    EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle,
    EVENTHUBRECEIVER_ASYNC_END_CALLBACK, onEventReceiveEndCallback,
    void*, onEventReceiveEndUserContext
);

/**
* @brief    API to enable diagnostic tracing related to connection establishing, data transfer
*           and tear down.
*
* @param    eventHubReceiverHandle      The handle created by a call to the create function.
* @param    traceEnabled                True to enable tracing and false to disable tracing
*
* @return   EVENTHUBRECEIVER_OK upon success or an error code upon failure.
*
* @note By default tracing is disabled.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_SetConnectionTracing,
    EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle,
    bool, traceEnabled
);

/**
* @brief    Asynchronous call to refresh the SAS token for the specified @p eventHubReceiverHandle.
*           This API is to be used only if EventHubReceiver_CreateWithSASToken was used to create
*           the EventHub handle.
*
* @param    eventHubReceiverHandle      The handle created by a call to the
*                                       EventHubReceiver_CreateWithSASToken function.
* @param    eventHubRefreshSasToken     Pointer to a non NULL character string containing the
*                                       SAS token to be used for authentication
*
*           @b NOTE: Since this is an async operation, any errors or timeouts
*           observed with this operation will be reported via any registered
*           EVENTHUBRECEIVER_ASYNC_ERROR_CALLBACK.
*
* @return   EVENTHUBRECEIVER_OK upon success or an error code upon failure.
*/
MOCKABLE_FUNCTION(, EVENTHUBRECEIVER_RESULT, EventHubReceiver_RefreshSASTokenAsync,
    EVENTHUBRECEIVER_HANDLE, eventHubReceiverHandle,
    const char*, eventHubRefreshSasToken
);

#ifdef __cplusplus
}
#endif

#endif /* EVENTHUBRECEIVER_H */
