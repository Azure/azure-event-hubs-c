#version Requirements
 
##Overview

`version` provides the API for retrieving the EH client version.

##Exposed API

```c
    MOCKABLE_FUNCTION(, const char*, EventHubClient_GetVersionString);
```

###EventHubClient_GetVersionString

```c
MOCKABLE_FUNCTION(, const char*, EventHubClient_GetVersionString);
```

**SRS_VERSION_01_001: [**EventHubClient_GetVersionString shall return a pointer to a constant string which indicates the version of EventHubClient API.**]** 
