/*
Microsoft Azure IoT Device Libraries
Copyright (c) Microsoft Corporation
All rights reserved.
MIT License
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the Software), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#include "doublylinkedlist.h"

void
DList_InitializeListHead(
    PDLIST_ENTRY ListHead
)
{
    /* Codes_SRS_DLIST_06_005: [DList_InitializeListHead will initialize the Flink & Blink to the address of the DLIST_ENTRY.] */
    ListHead->Flink = ListHead->Blink = ListHead;
    return;
}

int
DList_IsListEmpty(
    const PDLIST_ENTRY ListHead
)
{
    /* Codes_SRS_DLIST_06_003: [DList_IsListEmpty shall return a non-zero value if there are no DLIST_ENTRY's on this list other than the list head.] */
    /* Codes_SRS_DLIST_06_004: [DList_IsListEmpty shall return 0 if there is one or more items in the list.] */
    return (ListHead->Flink == ListHead);
}

int
DList_RemoveEntryList(
    PDLIST_ENTRY Entry
)
{
    /* Codes_SRS_DLIST_06_008: [DList_RemoveEntryList shall remove a listEntry from whatever list it is properly part of.] */
    /* Codes_SRS_DLIST_06_009: [The remaining list is properly formed.] */
    /* Codes_SRS_DLIST_06_010: [DList_RemoveEntryList shall return non-zero if the remaining list is empty.] */
    /* Codes_SRS_DLIST_06_011: [DList_RemoveEntryList shall return zero if the remaining list is NOT empty.] */
    PDLIST_ENTRY Blink;
    PDLIST_ENTRY Flink;

    Flink = Entry->Flink;
    Blink = Entry->Blink;
    Blink->Flink = Flink;
    Flink->Blink = Blink;
    return (Flink == Blink);
}

PDLIST_ENTRY
DList_RemoveHeadList(
    PDLIST_ENTRY ListHead
)
{
    /* Codes_SRS_DLIST_06_012: [DList_RemoveHeadList removes the oldest entry from the list defined by the listHead parameter and returns a pointer to that entry.] */
    /* Codes_SRS_DLIST_06_013: [DList_RemoveHeadList shall return listHead if that's the only item in the list.] */

    PDLIST_ENTRY Flink;
    PDLIST_ENTRY Entry;

    Entry = ListHead->Flink;
    Flink = Entry->Flink;
    ListHead->Flink = Flink;
    Flink->Blink = ListHead;
    return Entry;
}



void
DList_InsertTailList(
    PDLIST_ENTRY ListHead,
    PDLIST_ENTRY Entry
)
{
    PDLIST_ENTRY Blink;

    /* Codes_SRS_DLIST_06_006: [DListInsertTailList shall place the DLIST_ENTRY at the end of the list defined by the listHead parameter.] */
    Blink = ListHead->Blink;
    Entry->Flink = ListHead;
    Entry->Blink = Blink;
    Blink->Flink = Entry;
    ListHead->Blink = Entry;
    return;
}


void
DList_AppendTailList(
    PDLIST_ENTRY ListHead,
    PDLIST_ENTRY ListToAppend
)
{
    /* Codes_SRS_DLIST_06_007: [DList_AppendTailList shall place the list defined by listToAppend at the end of the list defined by the listHead parameter.] */
    PDLIST_ENTRY ListEnd = ListHead->Blink;

    ListHead->Blink->Flink = ListToAppend;
    ListHead->Blink = ListToAppend->Blink;
    ListToAppend->Blink->Flink = ListHead;
    ListToAppend->Blink = ListEnd;
    return;
}


/*Codes_SRS_DLIST_02_002: [DList_InsertHeadList inserts a singular entry in the list having as head listHead after "head".]*/
void DList_InsertHeadList(PDLIST_ENTRY listHead, PDLIST_ENTRY entry)
{
    entry->Blink = listHead;
    entry->Flink = listHead->Flink;
    listHead->Flink->Blink = entry;
    listHead->Flink = entry;
}
