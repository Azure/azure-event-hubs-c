// Copyright (c) Microsoft. All rights reserved.

#ifndef REAL_DOUBLYLINKEDLIST_H
#define REAL_DOUBLYLINKEDLIST_H

#include "azure_macro_utils/macro_utils.h"
#include "azure_c_shared_utility/doublylinkedlist.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_DOUBLYLINKEDLIST_GLOBAL_MOCK_HOOKS() \
    MU_FOR_EACH_1(R2, \
        DList_InitializeListHead, \
        DList_IsListEmpty, \
        DList_InsertTailList, \
        DList_InsertHeadList, \
        DList_AppendTailList, \
        DList_RemoveEntryList, \
        DList_RemoveHeadList \
    )

#ifdef __cplusplus
#include <cstddef>
extern "C"
{
#else
#include <stddef.h>
#endif

void real_DList_InitializeListHead(PDLIST_ENTRY listHead);
int real_DList_IsListEmpty(const PDLIST_ENTRY listHead);
void real_DList_InsertTailList(PDLIST_ENTRY listHead, PDLIST_ENTRY listEntry);
void real_DList_InsertHeadList(PDLIST_ENTRY listHead, PDLIST_ENTRY listEntry);
void real_DList_AppendTailList(PDLIST_ENTRY listHead, PDLIST_ENTRY ListToAppend);
int real_DList_RemoveEntryList(PDLIST_ENTRY listEntry);
PDLIST_ENTRY real_DList_RemoveHeadList(PDLIST_ENTRY listHead);

#ifdef __cplusplus
}
#endif

#endif // REAL_DOUBLYLINKEDLIST_H
