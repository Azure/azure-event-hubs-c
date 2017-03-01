// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/string_tokenizer.h"
#include "azure_c_shared_utility/xlogging.h"

#include "kvp_parser.h"

static MAP_HANDLE kvp_parse_internal(STRING_HANDLE input_string_handle, const char* key_delim, const char* value_delim)
{
    MAP_HANDLE result;
    STRING_TOKENIZER_HANDLE tokenizer;

    //**Codes_SRS_KVP_PARSER_29_003: \[**kvp_parser_parse shall create a STRING tokenizer to be used for parsing the input_string handle, by calling STRING_TOKENIZER_create.**\]**
    //**Codes_SRS_KVP_PARSER_29_004: \[**kvp_parser_parse shall start scanning at the beginning of the input_string handle.**\]**
    if ((tokenizer = STRING_TOKENIZER_create(input_string_handle)) == NULL)
    {
        //**Codes_SRS_KVP_PARSER_29_005: \[**kvp_parser_parse shall return NULL if STRING_TOKENIZER_create fails.**\]**
        STRING_delete(input_string_handle);
        result = NULL;
        LogError("Error creating STRING tokenizer.\r\n");
    }
    else
    {
        //**Codes_SRS_KVP_PARSER_29_006: \[**kvp_parser_parse shall allocate 2 STRING handles to hold the to be parsed key and value tokens using API STRING_new.**\]**
        STRING_HANDLE token_key_string = STRING_new();
        if (token_key_string == NULL)
        {
            //**Codes_SRS_KVP_PARSER_29_007: \[**kvp_parser_parse shall return NULL if allocating the STRINGs fails.**\]**
            result = NULL;
            LogError("Error creating key token STRING.\r\n");
        }
        else
        {
            STRING_HANDLE token_value_string = STRING_new();
            if (token_value_string == NULL)
            {
                //**Codes_SRS_KVP_PARSER_29_007: \[**kvp_parser_parse shall return NULL if allocating the STRING handles fail.**\]**
                result = NULL;
                LogError("Error creating value token STRING.\r\n");
            }
            else
            {
                //**Codes_SRS_KVP_PARSER_29_008: \[**kvp_parser_parse shall create a Map to hold the to be parsed keys and values using API Map_Create.**\]**
                result = Map_Create(NULL);
                if (result == NULL)
                {
                    //**Codes_SRS_KVP_PARSER_29_009: \[**kvp_parser_parse shall return NULL if Map_Create fails.**\]**
                    LogError("Error creating Map\r\n");
                }
                else
                {
                    //**Codes_SRS_KVP_PARSER_29_010: \[**kvp_parser_parse shall perform the following actions until parsing is complete.**\]**
                    //**Codes_SRS_KVP_PARSER_29_011: \[**kvp_parser_parse shall find a token (the key of the key/value pair) delimited by the key_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**
                    //**Codes_SRS_KVP_PARSER_29_012: \[**kvp_parser_parse shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**
                    while (STRING_TOKENIZER_get_next_token(tokenizer, token_key_string, key_delim) == 0)
                    {
                        bool is_error = false;

                        //**Codes_SRS_KVP_PARSER_29_013: \[**kvp_parser_parse shall find a token (the value of the key/value pair) delimited by the value_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**
                        if (STRING_TOKENIZER_get_next_token(tokenizer, token_value_string, value_delim) != 0)
                        {
                            //**Codes_SRS_KVP_PARSER_29_014: \[**kvp_parser_parse shall fail and return NULL if STRING_TOKENIZER_get_next_token fails (freeing the allocated result map).**\]**
                            is_error = true;
                            LogError("Error reading value token from the string.\r\n");
                        }
                        else
                        {
                            //**Codes_SRS_KVP_PARSER_29_015: \[**kvp_parser_parse shall obtain the C string for the key from the previously parsed key STRING by using STRING_c_str.**\]**
                            const char* token = STRING_c_str(token_key_string);
                            //**Codes_SRS_KVP_PARSER_29_016: \[**kvp_parser_parse shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**
                            if ((token == NULL) ||
                                //**Codes_SRS_KVP_PARSER_29_017: \[**kvp_parser_parse shall fail and return NULL if the key length is zero (freeing the allocated result map).**\]**
                                (strlen(token) == 0))
                            {
                                is_error = true;
                                LogError("The key token is NULL or empty.\r\n");
                            }
                            else
                            {
                                //**Codes_SRS_KVP_PARSER_29_018: \[**kvp_parser_parse shall obtain the C string for the key from the previously parsed value STRING by using STRING_c_str.**\]**
                                const char* value = STRING_c_str(token_value_string);
                                if (value == NULL)
                                {
                                    //**Codes_SRS_KVP_PARSER_29_019: \[**kvp_parser_parse shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**
                                    is_error = true;
                                    LogError("Could not get C string for value token.\r\n");
                                }
                                else
                                {
                                    //**Codes_SRS_KVP_PARSER_29_020: \[**kvp_parser_parse shall add the key and value to the result map by using Map_Add.**\]**
                                    if (Map_Add(result, token, value) != 0)
                                    {
                                        //**Codes_SRS_KVP_PARSER_29_021: \[**kvp_parser_parse shall fail and return NULL if Map_Add fails (freeing the allocated result map).**\]**
                                        is_error = true;
                                        LogError("Could not add the key/value pair to the result map.\r\n");
                                    }
                                }
                            }
                        }

                        if (is_error)
                        {
                            LogError("Error parsing connection string.\r\n");
                            Map_Destroy(result);
                            result = NULL;
                            break;
                        }
                    }
                }
                //**Codes_SRS_KVP_PARSER_29_022: \[**kvp_parser_parse shall free up the allocated STRING handle for holding the parsed value string using API STRING_delete after parsing is complete.**\]**
                STRING_delete(token_value_string);
            }
            //**Codes_SRS_KVP_PARSER_29_023: \[**kvp_parser_parse shall free up the allocated STRING handle for holding the parsed key string using API STRING_delete after parsing is complete.**\]**
            STRING_delete(token_key_string);
        }

        //**Codes_SRS_KVP_PARSER_29_024: \[**kvp_parser_parse shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**
        STRING_TOKENIZER_destroy(tokenizer);
    }

    return result;
}

MAP_HANDLE kvp_parser_parse(const char* input_string, const char* key_delim, const char* value_delim)
{
    MAP_HANDLE result;

    if ((input_string == NULL) || (key_delim == NULL) || (value_delim == NULL))
    {
        //**Codes_SRS_KVP_PARSER_29_001: \[**kvp_parser_parse shall return NULL if either input_string, key_delim or value_delim is NULL.**\]**
        result = NULL;
        LogError("Invalid Arguments Passed.\r\n");
    }
    else
    {
        STRING_HANDLE input_string_handle;

        //**Codes_SRS_KVP_PARSER_29_002: \[**kvp_parser_parse shall create a STRING handle using API STRING_construct with parameter as input_string.**\]**
        if ((input_string_handle = STRING_construct(input_string)) == NULL)
        {
            result = NULL;
            LogError("Error constructing STRING handle for input_string.\r\n");
        }
        else
        {
            result = kvp_parse_internal(input_string_handle, key_delim, value_delim);
            //**Codes_SRS_KVP_PARSER_29_025: \[**kvp_parser_parse shall free up the allocated input_string handle using API STRING_delete.**\]**
            STRING_delete(input_string_handle);
        }
    }

    //**Codes_SRS_KVP_PARSER_29_026: \[**kvp_parser_parse shall return the created MAP_HANDLE.**\]**
    return result;
}
