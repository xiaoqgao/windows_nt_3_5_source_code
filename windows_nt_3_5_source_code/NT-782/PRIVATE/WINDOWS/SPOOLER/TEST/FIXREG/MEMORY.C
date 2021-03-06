/*++


Copyright (c) 1990  Microsoft Corporation

Module Name:

    memory.c

Abstract:

    This module provides all the memory management functions for all spooler
    components

Author:

    Krishna Ganugapati (KrishnaG) 03-Feb-1994

Revision History:

--*/
#define NOMINMAX
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


LPVOID
AllocSplMem(
    DWORD cb
)
/*++

Routine Description:

    This function will allocate local memory. It will possibly allocate extra
    memory and fill this with debugging information for the debugging version.

Arguments:

    cb - The amount of memory to allocate

Return Value:

    NON-NULL - A pointer to the allocated memory

    FALSE/NULL - The operation failed. Extended error status is available
    using GetLastError.

--*/
{
    LPDWORD  pMem;
    DWORD    cbNew;

    cbNew = cb+2*sizeof(DWORD);
    if (cbNew & 3)
        cbNew += sizeof(DWORD) - (cbNew & 3);

    pMem=(LPDWORD)LocalAlloc(LPTR, cbNew);

    if (!pMem) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    memset(pMem, 0x00, cbNew);

    *pMem=cb;
    *(LPDWORD)((LPBYTE)pMem+cbNew-sizeof(DWORD))=0xdeadbeef;

    return (LPVOID)(pMem+1);
}

BOOL
FreeSplMem(
   LPVOID pMem,
   DWORD  cb
)
{
    DWORD   cbNew;
    LPDWORD pNewMem;

    pNewMem = pMem;
    pNewMem--;

    cbNew = cb+2*sizeof(DWORD);
    if (cbNew & 3)
        cbNew += sizeof(DWORD) - (cbNew & 3);

    if ((*pNewMem != cb) ||
       (*(LPDWORD)((LPBYTE)pNewMem + cbNew - sizeof(DWORD)) != 0xdeadbeef)) {
//      DBGMSG(DBG_ERROR, ("Corrupt Memory in spooler : %0lx\n", pNewMem));
        return FALSE;
    }
    memset(pNewMem, 0x65, cbNew);
    LocalFree((LPVOID)pNewMem);

    return TRUE;
}

LPVOID
ReallocSplMem(
   LPVOID pOldMem,
   DWORD cbOld,
   DWORD cbNew
)
{
    LPVOID pNewMem;

    pNewMem=AllocSplMem(cbNew);

    if (pOldMem && pNewMem) {
        memcpy(pNewMem, pOldMem, min(cbNew, cbOld));
        FreeSplMem(pOldMem, cbOld);
    }

    return pNewMem;
}

LPWSTR
AllocSplStr(
    LPWSTR pStr
)
/*++

Routine Description:

    This function will allocate enough local memory to store the specified
    string, and copy that string to the allocated memory

Arguments:

    pStr - Pointer to the string that needs to be allocated and stored

Return Value:

    NON-NULL - A pointer to the allocated memory containing the string

    FALSE/NULL - The operation failed. Extended error status is available
    using GetLastError.

--*/
{
   LPWSTR pMem;

   if (!pStr)
      return 0;

   if (pMem = AllocSplMem( wcslen(pStr)*sizeof(WCHAR) + sizeof(WCHAR) ))
      wcscpy(pMem, pStr);

   return pMem;
}

BOOL
FreeSplStr(
   LPWSTR pStr
)
{
   return pStr ? FreeSplMem(pStr, wcslen(pStr)*sizeof(WCHAR)+sizeof(WCHAR))
               : FALSE;
}

BOOL
ReallocSplStr(
   LPWSTR *ppStr,
   LPWSTR pStr
)
{
   FreeSplStr(*ppStr);
   *ppStr=AllocSplStr(pStr);

   return TRUE;
}

                                                                                                                                                                                                                                                                                                                                                                                                                                   