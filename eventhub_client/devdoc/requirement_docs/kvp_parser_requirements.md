#KVP Parser Requirements

##Overview

KVP Parser is a module that can be used for parsing strings that contain key value pairs and providing the parsed data to the user.


##Exposed API

```c
/**
* @brief	A utility API to parse a string containing key value pairs.
*           As an example, if the format of the string KEY1=VAL1;KEY2=VAL2
*           the "=" and ";" should be used as the key_delim and
*           value_delim respectively.
*
* @param	input_string    	STRING_HANDLE containing the string to parse.
* @param	key_delim          	NULL terminated C string of key token delimiters
* @param	value_delim         NULL terminated C string of value token delimiters
*
* @return   A non null MAP_HANDLE on success, and NULL otherwise.
*/
MOCKABLE_FUNCTION(, MAP_HANDLE, kvp_parser_parse, STRING_HANDLE, input_string, const char*, key_delim, const char*, value_delim);
```

###kvp_parser_parse

```c
MOCKABLE_FUNCTION(, MAP_HANDLE, kvp_parser_parse, STRING_HANDLE, input_string, const char*, key_delim, const char*, value_delim);
```
**SRS_KVP_PARSER_29_001: \[**`kvp_parser_parse` shall return NULL if either input_string, key_delim or value_delim is NULL.**\]**

**SRS_KVP_PARSER_29_002: \[**`kvp_parser_parse` shall create a STRING handle using API STRING_construct with parameter as input_string.**\]**

**SRS_KVP_PARSER_29_003: \[**`kvp_parser_parse` shall create a STRING tokenizer to be used for parsing the input_string handle, by calling STRING_TOKENIZER_create.**\]**

**SRS_KVP_PARSER_29_004: \[**`kvp_parser_parse` shall start scanning at the beginning of the input_string handle.**\]**

**SRS_KVP_PARSER_29_005: \[**`kvp_parser_parse` shall return NULL if STRING_TOKENIZER_create fails.**\]**

**SRS_KVP_PARSER_29_006: \[**`kvp_parser_parse` shall allocate 2 STRING handles to hold the to be parsed key and value tokens using API STRING_new.**\]**

**SRS_KVP_PARSER_29_007: \[**`kvp_parser_parse` shall return NULL if allocating the STRING handles fail.**\]**

**SRS_KVP_PARSER_29_008: \[**`kvp_parser_parse` shall create a Map to hold the to be parsed keys and values using API Map_Create.**\]**

**SRS_KVP_PARSER_29_009: \[**`kvp_parser_parse` shall return NULL if Map_Create fails.**\]**

**SRS_KVP_PARSER_29_010: \[**`kvp_parser_parse` shall perform the following actions until parsing is complete.**\]**

**SRS_KVP_PARSER_29_011: \[**`kvp_parser_parse` shall find a token (the key of the key/value pair) delimited by the key_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**

**SRS_KVP_PARSER_29_012: \[**`kvp_parser_parse` shall stop parsing further if STRING_TOKENIZER_get_next_token returns non zero.**\]**

**SRS_KVP_PARSER_29_013: \[**`kvp_parser_parse` shall find a token (the value of the key/value pair) delimited by the value_delim string, by calling STRING_TOKENIZER_get_next_token.**\]**

**SRS_KVP_PARSER_29_014: \[**`kvp_parser_parse` shall fail and return NULL if STRING_TOKENIZER_get_next_token fails (freeing the allocated result map).**\]**

**SRS_KVP_PARSER_29_015: \[**`kvp_parser_parse` shall obtain the C string for the key from the previously parsed key STRING by using STRING_c_str.**\]**

**SRS_KVP_PARSER_29_016: \[**`kvp_parser_parse` shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**

**SRS_KVP_PARSER_29_017: \[**`kvp_parser_parse` shall fail and return NULL if the key length is zero (freeing the allocated result map).**\]**

**SRS_KVP_PARSER_29_018: \[**`kvp_parser_parse` shall obtain the C string for the key from the previously parsed value STRING by using STRING_c_str.**\]**

**SRS_KVP_PARSER_29_019: \[**`kvp_parser_parse` shall fail and return NULL if STRING_c_str fails (freeing the allocated result map).**\]**

**SRS_KVP_PARSER_29_020: \[**`kvp_parser_parse` shall add the key and value to the result map by using Map_Add.**\]**

**SRS_KVP_PARSER_29_021: \[**`kvp_parser_parse` shall fail and return NULL if Map_Add fails (freeing the allocated result map).**\]**

**SRS_KVP_PARSER_29_022: \[**`kvp_parser_parse` shall free up the allocated STRING handle for holding the parsed value string using API STRING_delete after parsing is complete.**\]**

**SRS_KVP_PARSER_29_023: \[**`kvp_parser_parse` shall free up the allocated STRING handle for holding the parsed key string using API STRING_delete after parsing is complete.**\]**

**SRS_KVP_PARSER_29_024: \[**`kvp_parser_parse` shall free up the allocated STRING tokenizer using API STRING_TOKENIZER_destroy after parsing is complete.**\]**

**SRS_KVP_PARSER_29_025: \[**`kvp_parser_parse` shall free up the allocated input_string handle using API STRING_delete.**\]**

**SRS_KVP_PARSER_29_026: \[**`kvp_parser_parse` shall return the created MAP_HANDLE.**\]**
