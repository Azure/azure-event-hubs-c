// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef KVPPARSER_H
#define KVPPARSER_H

#include "azure_c_shared_utility/map.h"
#include "umock_c/umock_c_prod.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
* @brief    A utility API to parse a string containing key value pairs.
*           As an example, if the format of the string KEY1=VAL1;KEY2=VAL2
*           the "=" and ";" should be used as the key_delim and
*           value_delim respectively.
*
* @param    input_string        NULL terminated string to parse.
* @param    key_delim           NULL terminated C string of key token delimiters
* @param    value_delim         NULL terminated C string of value token delimiters
*
* @return   A non null MAP_HANDLE on success, and NULL otherwise.
*/
MOCKABLE_FUNCTION(, MAP_HANDLE, kvp_parser_parse, const char*, input_string, const char*, key_delim, const char*, value_delim);

#ifdef __cplusplus
}
#endif

#endif //KVPPARSER_H
