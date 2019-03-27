// Copyright (c) Microsoft. All rights reserved.

#define GBALLOC_H

#define DList_InitializeListHead real_DList_InitializeListHead
#define DList_IsListEmpty real_DList_IsListEmpty
#define DList_InsertTailList real_DList_InsertTailList
#define DList_InsertHeadList real_DList_InsertHeadList
#define DList_AppendTailList real_DList_AppendTailList
#define DList_RemoveEntryList real_DList_RemoveEntryList
#define DList_RemoveHeadList real_DList_RemoveHeadList

#include "doublylinkedlist.c"
