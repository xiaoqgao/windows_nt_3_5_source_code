/****************************** Module Header ******************************\
* Module Name: userinit.c
*
* Copyright (c) 1991, Microsoft Corporation
*
* Userinit main module
*
* Userinit is an app executed by winlogon at user logon.
* It executes in the security context of the user and on the user desktop.
* Its purpose is to complete any user initialization that may take an
* indeterminate time. e.g. code that interacts with the user.
* This process may be terminated at any time if a shutdown is initiated
* or if the user logs off by some other means.
*
* History:
* 20-Aug-92 Davidc       Created.
\***************************************************************************/

#include "userinit.h"
#include "winuserp.h"
#include <mpr.h>
#include <winspool.h>
#include "winsplrp.h"
#include "msgalias.h"
#include "stringid.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifdef DBCS_IME // by eichim, 10-Jul-92
#include <winnls32.h>
#include <ime.h>
#include <winnls3p.h>
#endif // DBCS_IME


//
// Define this to enable verbose output for this module
//

// #define DEBUG_USERINIT

#ifdef DEBUG_USERINIT
#define VerbosePrint(s) UIPrint(s)
#else
#define VerbosePrint(s)
#endif



//
// Define the environment variable names used to pass the logon
// server and script name from winlogon
//

#define LOGON_SERVER_VARIABLE       TEXT("UserInitLogonServer")
#define LOGON_SCRIPT_VARIABLE       TEXT("UserInitLogonScript")
#define MPR_LOGON_SCRIPT_VARIABLE   TEXT("UserInitMprLogonScript")

#ifdef LOGGING

#define FILE_HANDLE_VARIABLE TEXT("LogFileHandle")

HANDLE LogFileHandle;

#endif

//
// Define the path environment variable
//

#define PATH                   TEXT("PATH")

//
// Define path separator
//

#define PATH_SEPARATOR          TEXT("\\")

//
// Directory separator in environment strings
//

#define DIRECTORY_SEPARATOR     TEXT(";")

//
// Define filename extension separator
//

#define EXTENSION_SEPARATOR_CHAR TEXT('.')

//
// Define server name prefix
//

#define SERVER_PREFIX           TEXT("\\\\")

//
// Define Logon script paths.
//

#define SERVER_SCRIPT_PATH      TEXT("\\NETLOGON")
#define LOCAL_SCRIPT_PATH       TEXT("\\repl\\import\\scripts")


//
// Define extensions that should be added to scripts without extensions
// when we go search for them. Basically this list includes those extensions
// that CreateProcess handles when they are present in the executable file
// name but must be provided by the caller (us)
// We search for a script file with these extensions in this order and
// execute the first one we find.
//

static LPTSTR ScriptExtensions[] = { TEXT(".bat"), TEXT(".cmd") };

//
// Name of registry key and value to check for temp page file.
//
TCHAR szMemMan[] =
     TEXT("System\\CurrentControlSet\\Control\\Session Manager\\Memory Management");

TCHAR szNoPageFile[] = TEXT("TempPageFile");


//
// Timeout in miliseconds to wait for AddMessageAlias to complete
//

#define TIMEOUT_VALUE  (5L * 60L * 1000L)


LPTSTR
AllocAndGetEnvironmentVariable(
    LPTSTR lpName
    );



/***************************************************************************\
* ExecApplication
*
* Execs an application
*
* Returns TRUE on success, FALSE on failure.
*
* 21-Aug-92 Davidc   Created.
\***************************************************************************/

BOOL
ExecApplication(
    LPTSTR pch,
    BOOL bFileNameOnly
    )
{
    STARTUPINFO si;
    PROCESS_INFORMATION ProcessInformation;
    BOOL Result;

    //
    // Initialize process startup info
    //
    si.cb = sizeof(STARTUPINFO);
    si.lpReserved = pch; // This tells progman it's the shell!
    si.lpTitle = pch;
    si.lpDesktop = NULL; // Not used
    si.dwX = si.dwY = si.dwXSize = si.dwYSize = 0L;
    si.dwFlags = 0;
    si.wShowWindow = SW_SHOW;   // at least let the guy see it
    si.lpReserved2 = NULL;
    si.cbReserved2 = 0;


    //
    // Start the app
    //
    Result = CreateProcess(
                      bFileNameOnly ? pch : NULL,   // Image name
                      bFileNameOnly ? NULL : pch,   // Command line
                      NULL,  // Default process protection
                      NULL,  // Default thread protection
                      FALSE, // Don't inherit handles
                      HIGH_PRIORITY_CLASS,
                      NULL,  // Inherit environment
                      NULL,  // Inherit current directory
                      &si,
                      &ProcessInformation
                      );

    if (!Result) {
        VerbosePrint(("Failed to execute <%S>, error = %d", pch, GetLastError()));
    } else {

        //
        // Close our handles to the process and thread
        //

        CloseHandle(ProcessInformation.hProcess);
        CloseHandle(ProcessInformation.hThread);
    }

    return(Result);
}

/***************************************************************************\
* ExecProcesses
*
* Read win.ini for a list of system processes and start them up.
*
* Returns number of processes successfully started.
*
* 21-Aug-92 Davidc      Created
\***************************************************************************/

#if DEVL
BOOL bDebugProgMan;
BOOL bDebugWowExec;
#endif // DEVL

DWORD
ExecProcesses(
    LPTSTR pszKeyName,
    LPTSTR pszDefault
    )
{
    PTCHAR pchData, pchCmdLine, pchT;
    DWORD cb, cbCopied;
    DWORD dwExecuted = 0 ;
#if DEVL
    BOOL bDebug;
    TCHAR chDebugCmdLine[ MAX_PATH ];
#endif

    //
    // Read win.ini for the list of processes to startup
    //
    cb = 128;

    if ((pchData = (PTCHAR)Alloc( cb * sizeof (TCHAR) )) == NULL)
        return(0);

    while (TRUE) {
        /*
         * Grab a buffer and load up the keydata under the keyname currently
         * pointed to by pchKeyNames.
         */
        if ((cbCopied = GetProfileString(TEXT("winlogon"), pszKeyName, pszDefault, // LATER put in res file
                (LPTSTR)pchData, cb)) == 0) {
            LocalFree((HLOCAL)pchData);
            return(0);
        }

        /*
         * If the returned value is our passed size - 1 (weird way for error)
         * then our buffer is too small. Make it bigger and start over again.
         */
        if (cbCopied == cb - 1) {
            cb += 128;
            if ((pchData = (PTCHAR)ReAlloc(pchData, cb * sizeof (TCHAR) )) == NULL) {
                return(0);
            }
            continue;
        }

        break;
    }

    pchCmdLine = pchData;
    while (TRUE) {
        /*
         * Exec all applications separated by commas.
         */
        for (pchT = pchCmdLine; pchT < pchData + cbCopied; pchT++) {
            if (*pchT == 0)
                break;

            if (*pchT == ',') {
                *pchT = 0;
                break;
            }
        }

        if (*pchT != 0) {
            // We've reached the end of the buffer
            break;
        }

        /*
         * Skip any leading spaces.
         */
        while (*pchCmdLine == ' ') {
            pchCmdLine++;
        }

#if DEVL
        bDebug = FALSE;
        if (bDebugProgMan && !wcsicmp( pchCmdLine, TEXT("progman") )) {
            bDebug = TRUE;
        }
        else
        if (bDebugWowExec && !wcsicmp( pchCmdLine, TEXT("wowexec") )) {
            bDebug = TRUE;
        }

        if (bDebug) {
            wsprintf( chDebugCmdLine, TEXT("ntsd -d %S%S"),
                     bDebug == 2 ? TEXT("-g -G ") : TEXT(""),
                     pchCmdLine );
            pchCmdLine = chDebugCmdLine;
        }
#endif // DEVL

        /*
         * We have something... exec this application.
         */
        if (ExecApplication(pchCmdLine, FALSE)) {
            dwExecuted++;
        }

        /*
         * Advance to next name. Double 0 means end of names.
         */
        pchCmdLine = pchT + 1;
        if (*pchCmdLine == 0)
            break;
    }

    Free(pchData);

    return dwExecuted ;
}


/***************************************************************************\
* SearchAndAllocPath
*
* Version of SearchPath that allocates the return string.
*
* Returns pointer to full path of file or NULL if not found.
*
* The returned buffer should be free using Free()
*
* History:
* 09-Dec-92     Davidc  Created
*
\***************************************************************************/
LPTSTR
SearchAndAllocPath(
    LPTSTR lpPath,
    LPTSTR lpFileName,
    LPTSTR lpExtension,
    LPTSTR *lpFilePart
    )
{
    LPTSTR Buffer;
    DWORD LengthRequired;
    DWORD LengthUsed;
    DWORD BytesRequired;

    //
    // Allocate a buffer to hold the full filename
    //

    LengthRequired = MAX_PATH;
    BytesRequired = (LengthRequired * sizeof(TCHAR));

    Buffer = Alloc(BytesRequired);
    if (Buffer == NULL) {
        UIPrint(("SearchAndAllocPath: Failed to allocate %d bytes for file name", BytesRequired));
        return(NULL);
    }

    //
    // Go search for the file
    //

    LengthUsed = SearchPath(
                           lpPath,
                           lpFileName,
                           lpExtension,
                           LengthRequired,
                           Buffer,
                           lpFilePart);

    if (LengthUsed == 0) {
        VerbosePrint(("SearchAndAllocPath: Path <%S>, file <%S>, extension <%S> not found, error = %d", lpPath, lpFileName, lpExtension, GetLastError()));
        Free(Buffer);
        return(NULL);
    }

    if (LengthUsed > LengthRequired - 1) {
        UIPrint(("SearchAndAllocPath: Unexpected result from SearchPath. Length passed = %d, length used = %d (expected %d)", LengthRequired, LengthUsed, LengthRequired - 1));
        Free(Buffer);
        return(NULL);
    }

    return(Buffer);
}


/***************************************************************************\
* ExecScript
*
* Attempts to run the command script or exe lpScript in the directory lpPath.
* If path is not specified then the default windows search path is used.
*
* This routine is basically a wrapper for CreateProcess. CreateProcess always
* assumes a .exe extension for files without extensions. It will run .cmd
* and .bat files but it keys off the .cmd and .bat extension. So we must go
* search for the file first and add the extension before calling CreateProcess.
*
* Returns TRUE if the script began executing successfully.
* Returns FALSE if we can't find the script in the path specified
* or something fails.
*
* History:
* 09-Dec-92     Davidc  Created
*
\***************************************************************************/
BOOL
ExecScript(
    LPTSTR lpPath OPTIONAL,
    LPTSTR lpScript
    )
{
    BOOL Result;
    DWORD i;
    LPTSTR lpFullName;
    DWORD BytesRequired;


    //
    // First try and execute the raw script file name
    //

    if (lpPath != NULL) {

        BytesRequired = (lstrlen(lpPath) +
                         lstrlen(PATH_SEPARATOR) +
                         lstrlen(lpScript) +
                         1)
                         * sizeof(TCHAR);

        lpFullName  = Alloc(BytesRequired);
        if (lpFullName == NULL) {
            UIPrint(("ExecScript failed to allocate %d bytes for full script name", BytesRequired));
            return(FALSE);
        }

        lstrcpy(lpFullName, lpPath);
        lstrcat(lpFullName, PATH_SEPARATOR);
        lstrcat(lpFullName, lpScript);

        ASSERT(((lstrlen(lpFullName) + 1) * sizeof(TCHAR)) == BytesRequired);

    } else {
        lpFullName = lpScript;
    }


    //
    // Let CreateProcess have a hack at the raw script path and name.
    //

    Result = ExecApplication(lpFullName, TRUE);


    //
    // Free up the full name buffer
    //

    if (lpFullName != lpScript) {
        Free(lpFullName);
    }




    if (!Result) {

        //
        // Create process couldn't find it so add each script extension in
        // turn and try and execute the full script name.
        //
        // Only bother with this procedure if the script name doesn't
        // already contain an extension
        //
        BOOL ExtensionPresent = FALSE;
        LPTSTR p = lpScript;

        while (*p) {
            if (*p == EXTENSION_SEPARATOR_CHAR) {
                ExtensionPresent = TRUE;
                break;
            }
            p = CharNext(p);
        }

        if (ExtensionPresent) {
            VerbosePrint(("ExecScript: Skipping search path because script name contains extension"));
        } else {

            for (i = 0; i < sizeof(ScriptExtensions)/sizeof(ScriptExtensions[0]); i++) {

                lpFullName = SearchAndAllocPath(
                                    lpPath,
                                    lpScript,
                                    ScriptExtensions[i],
                                    NULL);

                if (lpFullName != NULL) {

                    //
                    // We found the file, go execute it
                    //

                    Result = ExecApplication(lpFullName, TRUE);

                    //
                    // Free the full path buffer
                    //

                    Free(lpFullName);

                    return(Result);
                }
            }
        }
    }


    return(Result);
}

BOOL
PrependToPath(
    IN LPTSTR lpLogonPath,
    OUT LPTSTR *lpOldPath
    )
{
    DWORD BytesRequired;
    LPTSTR lpNewPath;

    //
    // Prepend the address of the logon script to the path, so it can
    // reference other files.
    //

    *lpOldPath = AllocAndGetEnvironmentVariable( PATH );

    BytesRequired = ( lstrlen(lpLogonPath) +
                      lstrlen(*lpOldPath)   +
                      2                           // one for terminator, one for ';'
                    ) * sizeof(TCHAR);

    lpNewPath = (LPTSTR)Alloc(BytesRequired);
    if (lpNewPath == NULL) {
        UIPrint(("RunLogonScript: Failed to allocate %d bytes for modified path variable", BytesRequired));
        return(FALSE);
    }

    lstrcpy(lpNewPath, lpLogonPath);
    lstrcat(lpNewPath, DIRECTORY_SEPARATOR);
    lstrcat(lpNewPath, *lpOldPath);

//    Free( *lpOldPath );

    ASSERT(((lstrlen(lpNewPath) + 1) * sizeof(TCHAR)) == BytesRequired);

    SetEnvironmentVariable(PATH, lpNewPath);

    Free(lpNewPath);

    return(TRUE);
}


/***************************************************************************\
* RunLogonScript
*
* Starts the logon script
*
* Returns TRUE on success, FALSE on failure
*
* History:
* 21-Aug-92     Davidc  Created
*
\***************************************************************************/
BOOL
RunLogonScript(
    LPTSTR lpLogonServer OPTIONAL,
    LPTSTR lpLogonScript
    )
{
    LPTSTR lpLogonPath;
    LPTSTR lpOldPath;
    DWORD BytesRequired;
    BOOL Result;

    //
    // if the logon server exists, look for the logon scripts on
    // \\<LogonServer>\NETLOGON\<ScriptName>
    //

    if ((lpLogonServer != NULL) && (lpLogonServer[0] != 0)) {

        BytesRequired = ( lstrlen(SERVER_PREFIX) +
                          lstrlen(lpLogonServer) +
                          lstrlen(SERVER_SCRIPT_PATH) +
                          1
                        ) * sizeof(TCHAR);

        lpLogonPath = (LPTSTR)Alloc(BytesRequired);
        if (lpLogonPath == NULL) {
            UIPrint(("RunLogonScript: Failed to allocate %d bytes for remote logon script path", BytesRequired));
            return(FALSE);
        }

        lstrcpy(lpLogonPath, SERVER_PREFIX);
        lstrcat(lpLogonPath, lpLogonServer);
        lstrcat(lpLogonPath, SERVER_SCRIPT_PATH);

        ASSERT(((lstrlen(lpLogonPath) + 1) * sizeof(TCHAR)) == BytesRequired);

        Result = PrependToPath( lpLogonPath, &lpOldPath );

        if (Result) {
            VerbosePrint(("Successfully prepended <%S> to path", lpLogonPath));
        } else {
            VerbosePrint(("Cannot prepend <%S> path.",lpLogonPath));
        }

        //
        // Try and execute the app/script specified by lpLogonScript
        // in the directory specified by lpLogonPath
        //

        Result = ExecScript(lpLogonPath, lpLogonScript);

        if (Result) {
            VerbosePrint(("Successfully executed logon script <%S> in directory <%S>", lpLogonScript, lpLogonPath));
        } else {
            VerbosePrint(("Cannot start logon script <%S> on LogonServer <%S>. Trying local path.", lpLogonScript, lpLogonServer));
        }

        //
        // Put the path back the way it was
        //

        SetEnvironmentVariable(PATH, lpOldPath);

        Free(lpOldPath);

        //
        // Free up the buffer
        //

        Free(lpLogonPath);

        //
        // If the script started successfully we're done, otherwise
        // drop through and try to find the script locally
        //

        if (Result) {
            return(TRUE);
        }
    }




    //
    // Try to find the scripts on <system dir>\repl\import\scripts\<scriptname>
    //

    BytesRequired = GetSystemDirectory(NULL, 0) * sizeof(TCHAR);
    if (BytesRequired == 0) {
        UIPrint(("RunLogonScript: GetSystemDirectory failed, error = %d", GetLastError()));
        return(FALSE);
    }

    BytesRequired += ( lstrlen(LOCAL_SCRIPT_PATH)
                       // BytesRequired already includes space for terminator
                     ) * sizeof(TCHAR);

    lpLogonPath = (LPTSTR)Alloc(BytesRequired);
    if (lpLogonPath == NULL) {
        UIPrint(("RunLogonScript failed to allocate %d bytes for logon script path", BytesRequired));
        return(FALSE);
    }

    Result = FALSE;
    if (GetSystemDirectory(lpLogonPath, BytesRequired)) {

        lstrcat(lpLogonPath, LOCAL_SCRIPT_PATH);

        ASSERT(((lstrlen(lpLogonPath) + 1) * sizeof(TCHAR)) == BytesRequired);

        Result = PrependToPath( lpLogonPath, &lpOldPath );

        if (Result) {
            VerbosePrint(("Successfully prepended <%S> to path", lpLogonPath));
        } else {
            VerbosePrint(("Cannot prepend <%S> path.",lpLogonPath));
        }

        //
        // Try and execute the app/script specified by lpLogonScript
        // in the directory specified by lpLogonPath
        //

        Result = ExecScript(lpLogonPath, lpLogonScript);

        if (Result) {
            VerbosePrint(("Successfully executed logon script <%S> in directory <%S>", lpLogonScript, lpLogonPath));
        } else {
            VerbosePrint(("Cannot start logon script <%S> on local path <%S>.", lpLogonScript, lpLogonPath));
        }

        //
        // Put the path back the way it was
        //

        SetEnvironmentVariable(PATH, lpOldPath);

        Free(lpOldPath);

    } else {
        UIPrint(("RunLogonScript: GetSystemDirectory failed, error = %d", GetLastError()));
    }

    //
    // Free up the buffer
    //

    Free(lpLogonPath);

    return(Result);
}


/***************************************************************************\
* RunMprLogonScripts
*
* Starts the network provider logon scripts
* The passed string is a multi-sz - we exec each script in turn.
*
* Returns TRUE on success, FALSE on failure
*
* History:
* 21-Aug-92     Davidc  Created
*
\***************************************************************************/
BOOL
RunMprLogonScripts(
    LPTSTR lpLogonScripts
    )
{
    BOOL Result;

    if (lpLogonScripts != NULL) {

        DWORD Length;

        do {
            Length = lstrlen(lpLogonScripts);
            if (Length != 0) {

                Result = ExecScript(NULL, lpLogonScripts);

                if (Result) {
                    VerbosePrint(("Successfully executed mpr logon script <%S>", lpLogonScripts));
                } else {
                    VerbosePrint(("Cannot start mpr logon script <%S>", lpLogonScripts));
                }
            }

            lpLogonScripts += (Length + 1);

        } while (Length != 0);

    }

    return(TRUE);
}


/***************************************************************************\
* RestoreNetworkConnections
*
* Restores any saved network connections for the user.
*
* Returns TRUE on success, FALSE on failure.
*
* History:
* 21-Aug-92     Davidc     Created
*
\***************************************************************************/
DWORD
RestoreNetworkConnections(
    VOID
    )
{
    DWORD Error;

    Error = WNetRestoreConnection(NULL, NULL);

    //
    // It is not an error to not be able to open the profile,
    // it just means there are no saved connections
    //

    if (Error == ERROR_CANNOT_OPEN_PROFILE) {
        Error = ERROR_SUCCESS;
    }


    return(Error);
}


/***************************************************************************\
* AllocAndGetEnvironmentVariable
*
* Version of GetEnvironmentVariable that allocates the return buffer.
*
* Returns pointer to environment variable or NULL on failure
*
* The returned buffer should be free using Free()
*
* History:
* 09-Dec-92     Davidc  Created
*
\***************************************************************************/
LPTSTR
AllocAndGetEnvironmentVariable(
    LPTSTR lpName
    )
{
    LPTSTR Buffer;
    DWORD LengthRequired;
    DWORD LengthUsed;
    DWORD BytesRequired;

    //
    // Go search for the variable and find its length
    //

    LengthRequired = GetEnvironmentVariable(lpName, NULL, 0);

    if (LengthRequired == 0) {
        VerbosePrint(("Environment variable <%S> not found, error = %d", lpName, GetLastError()));
        return(NULL);
    }

    //
    // Allocate a buffer to hold the variable
    //

    BytesRequired = LengthRequired * sizeof(TCHAR);

    Buffer = Alloc(BytesRequired);
    if (Buffer == NULL) {
        UIPrint(("Failed to allocate %d bytes for environment variable", BytesRequired));
        return(NULL);
    }

    //
    // Go get the variable and pass a buffer this time
    //

    LengthUsed = GetEnvironmentVariable(lpName, Buffer, LengthRequired);

    if (LengthUsed == 0) {
        VerbosePrint(("Environment variable <%S> not found (should have found it), error = %d", lpName, GetLastError()));
        Free(Buffer);
        return(NULL);
    }

    if (LengthUsed != LengthRequired - 1) {
        UIPrint(("Unexpected result from GetEnvironmentVariable. Length passed = %d, length used = %d (expected %d)", LengthRequired, LengthUsed, LengthRequired - 1));
        Free(Buffer);
        return(NULL);
    }

    return(Buffer);
}


/***************************************************************************\
* AllocAndGetEnvironmentMultiSz
*
* Gets an environment variable's value that's assumed to be an
* encoded multi-sz and decodes it into an allocated return buffer.
* Variable should have been written with SetEnvironmentMultiSz() (winlogon)
*
* Returns pointer to environment variable or NULL on failure
*
* The returned buffer should be free using Free()
*
* History:
* 01-15-93      Davidc  Created
*
\***************************************************************************/

#define TERMINATOR_REPLACEMENT  TEXT(',')

LPTSTR
AllocAndGetEnvironmentMultiSz(
    LPTSTR lpName
    )
{
    LPTSTR Buffer;
    LPTSTR p, q;

    Buffer = AllocAndGetEnvironmentVariable(lpName);
    if (Buffer == NULL) {
        return(NULL);
    }

    //
    // Now decode the string - we can do this in place since the string
    // will always get smaller
    //

    p = Buffer;
    q = Buffer;

    while (*p) {

        if (*p == TERMINATOR_REPLACEMENT) {

            p ++;
            if (*p != TERMINATOR_REPLACEMENT) {
                p --;
                *p = 0;
            }
        }

        if (p != q) {
            *q = *p;
        }

        p ++;
        q ++;
    }

    ASSERT(q <= p);

    //
    // Copy terminator
    //

    if (q != p) {
        *q = 0;
    }

    return(Buffer);
}



/***************************************************************************\
* CheckVideoSelection
*
* History:
* 15-Mar-93 Andreva          Created.
\***************************************************************************/

VOID
CheckVideoSelection(
    HINSTANCE hInstance
)

{
    //
    // First check if we are in a detection mode.
    // If we are, spawn the applet and let the user pick the mode.
    //
    // Otherwise, check to see if the display was initialized properly.
    // We may want to move this to a more appropriate place at a later date.
    //
    // Andreva
    //

    NTSTATUS Status;
    HANDLE HkRegistry;
    OBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING UnicodeString;
    TCHAR achDispMode[512];
    TCHAR achDisp[512];
    TCHAR achExec[MAX_PATH];

    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet"
                         L"\\Control\\GraphicsDrivers\\DetectDisplay");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = NtOpenKey(&HkRegistry,
                       GENERIC_READ | GENERIC_WRITE | DELETE,
                       &ObjectAttributes);


    if (!NT_SUCCESS(Status)) {

        RtlInitUnicodeString(&UnicodeString,
                             L"\\Registry\\Machine\\System\\CurrentControlSet"
                             L"\\Control\\GraphicsDrivers\\InvalidDisplay");

        InitializeObjectAttributes(&ObjectAttributes,
                                   &UnicodeString,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        Status = NtOpenKey(&HkRegistry,
                           GENERIC_READ | GENERIC_WRITE | DELETE,
                           &ObjectAttributes);


        if (NT_SUCCESS(Status)) {

            LPTSTR psz = NULL;

            (VOID)LoadString(hInstance, IDS_INVALID_DISPLAY, achDisp, 512);
            (VOID)LoadString(hInstance, IDS_INVALID_DISPLAY_MODE, achDispMode, 512);

            FormatMessage(FORMAT_MESSAGE_FROM_STRING |
                              FORMAT_MESSAGE_ALLOCATE_BUFFER |
                              FORMAT_MESSAGE_MAX_WIDTH_MASK,
                          achDispMode,
                          0,
                          0,
                          (LPTSTR) (&psz),
                          1,
                          NULL);

            if (psz) {

                MessageBox(NULL,
                           psz,
                           achDisp,
                           MB_OK | MB_ICONSTOP);

                LocalFree(psz);

            }
        }
    }

    //
    // If either of the the two keys we opened successfully, then close the
    // key and spawn the applet (we only delete the invalid display key, not
    // the DetectDisplay key !)
    //

    if (NT_SUCCESS(Status)) {

        NtClose(HkRegistry);

        LoadString(hInstance, IDS_DISPLAYAPPLET, achExec, sizeof(achExec));

        ExecProcesses(TEXT("display applet"), achExec);

    }
}



#ifdef LOGGING

#define DATEFORMAT  TEXT("%d-%d %.2d:%.2d:%.2d:%.3d ")
#define FILE_HANDLE_VARIABLE TEXT("LogFileHandle")

/***************************************************************************\
* WriteLog
*
* History:
* 22-Mar-93 Robertre          Created.
\***************************************************************************/

BOOL
WriteLog(
    HANDLE LogFileHandle,
    LPCTSTR LogString
    )
{
    TCHAR Buffer[256];
    DWORD NumberOfBytesWritten;
    SYSTEMTIME st;
    TCHAR FormatString[256];


    lstrcpy( FormatString, DATEFORMAT );
    lstrcat( FormatString, LogString );
    lstrcat( FormatString, TEXT("\n") );

    GetLocalTime( &st );

    //
    // Construct the message
    //

    sprintf( Buffer,
             FormatString,
             st.wMonth,
             st.wDay,
             st.wHour,
             st.wMinute,
             st.wSecond,
             st.wMilliseconds
             );

    NumberOfBytesWritten = 0;

    return(WriteFile(LogFileHandle, Buffer, lstrlen(Buffer), &NumberOfBytesWritten, NULL ));

}

#endif


DWORD
WINAPI
AddToMessageAlias(
    PVOID IgnoreParam
    )
/***************************************************************************\
* AddToMessageAlias
*
* History:
* 10-Apr-93 Robertre       Created.
\***************************************************************************/
{
    WCHAR UserName[MAX_PATH + 1];
    DWORD UserNameLength = sizeof(UserName) / sizeof(*UserName);

    //
    // Add the user's msg alias.
    //

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Adding MsgAlias"));
#endif

    if (GetUserNameW(UserName, &UserNameLength)) {
        AddMsgAlias(UserName);
    } else {
        UIPrint(("GetUserName failed, error = %d",GetLastError()));
    }

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Finished adding MsgAlias"));
#endif

    return( NO_ERROR );
}



/***************************************************************************\
* WinMain
*
* History:
* 20-Aug-92 Davidc       Created.
\***************************************************************************/

VOID
CheckControlPanelCache();

int
WINAPI
WinMain(
    HINSTANCE  hInstance,
    HINSTANCE  hPrevInstance,
    LPSTR   lpszCmdParam,
    int     nCmdShow
    )
{
    DWORD Error;
    LPTSTR lpLogonServer;
    LPTSTR lpLogonScript;
    LPTSTR lpMprLogonScripts;
    DWORD ThreadId;
    DWORD WaitResult;
    HANDLE ThreadHandle;

#ifdef LOGGING

    LPTSTR lpLogFileHandle;
    BOOL Result;

    lpLogFileHandle = AllocAndGetEnvironmentVariable(FILE_HANDLE_VARIABLE);
    SetEnvironmentVariable(FILE_HANDLE_VARIABLE, NULL);

    LogFileHandle = (HANDLE)atol( lpLogFileHandle );

    Free(lpLogFileHandle);

    Result = WriteLog( LogFileHandle, TEXT("Userinit: Starting"));

    if ( !Result ) {
        DbgPrint("WriteLog failed, error = %d\n",GetLastError());
        ASSERT(FALSE);
    }

#endif

    //
    // Restore the user's network connections
    //

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Restoring connections"));
#endif

    Error = RestoreNetworkConnections();
    if (Error != ERROR_SUCCESS) {
        UIPrint(("Failed to restore network connections, error = %d", Error));
    }

    // net connections have been restored, so we can reload fonts
    // from the "Fonts" section of the registry that are on the net

    LoadRemoteFonts();

    //
    // Execute the logon script
    //
    // Get the logon server and script name out of the environment
    // variables, then delete these variables so child processes
    // don't inherit them.
    //

    lpLogonServer = AllocAndGetEnvironmentVariable(LOGON_SERVER_VARIABLE);
    lpLogonScript = AllocAndGetEnvironmentVariable(LOGON_SCRIPT_VARIABLE);
    lpMprLogonScripts = AllocAndGetEnvironmentMultiSz(MPR_LOGON_SCRIPT_VARIABLE);



    //
    // Delete the logon script environment variables
    //

    SetEnvironmentVariable(LOGON_SERVER_VARIABLE, NULL);
    SetEnvironmentVariable(LOGON_SCRIPT_VARIABLE, NULL);
    SetEnvironmentVariable(MPR_LOGON_SCRIPT_VARIABLE, NULL);


    //
    // Run the script
    //

    if (lpLogonScript != NULL) {
#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Running logon script"));
#endif
        RunLogonScript(lpLogonServer, lpLogonScript);
    }

    //
    // Run the provider scripts
    //

    if (lpMprLogonScripts != NULL) {
#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Running provider scripts"));
#endif
        RunMprLogonScripts(lpMprLogonScripts);
    }

    //
    // Free up the buffers
    //

    Free(lpLogonServer);
    Free(lpLogonScript);
    Free(lpMprLogonScripts);


    CheckControlPanelCache();

    //
    // Start the user shell
    //

    {
        HKEY hkeyMM;
        DWORD dwTempFile, cbTempFile, dwType;
        TCHAR achExec[MAX_PATH];


        //
        // check for no page file. If there is not one, then spawn the vm applet
        //

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szMemMan, 0, KEY_READ,
                &hkeyMM) == ERROR_SUCCESS) {

            cbTempFile = sizeof(dwTempFile);
            if (RegQueryValueEx (hkeyMM, szNoPageFile, NULL, &dwType,
                    (LPBYTE) &dwTempFile, &cbTempFile) != ERROR_SUCCESS ||
                    dwType != REG_DWORD || cbTempFile != sizeof(dwTempFile)) {
                dwTempFile = 0;
            }

            RegCloseKey(hkeyMM);
        } else
            dwTempFile = 0;

        if (dwTempFile == 1) {
            LoadString(hInstance, IDS_VMAPPLET, achExec, sizeof(achExec));
            ExecProcesses(TEXT("vmapplet"), achExec);
        }
    }

    //
    // Tell the user if he has an invalid video selection.
    //
#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Checking video selection"));
#endif

    CheckVideoSelection(hInstance);

    //
    // Attempt to intialize the spooler.  If the spooler isn't running,
    // then it will initialize itself when it starts.
    //

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Initializing spooler"));
#endif

    SpoolerInit();

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: Starting progman"));
#endif

    ExecProcesses(TEXT("shell"), TEXT("progman"));

#ifdef LOGGING
    WriteLog( LogFileHandle, TEXT("Userinit: progman started"));

    CloseHandle( LogFileHandle );
#endif

#ifdef DBCS_IME
    // Start MSIME process here.  We cannot start it earlier because the
    // HKEY_CURRENT_USER has not been determined and since MSIME is running
    // at HIGH_PRIORITY_CLASS it quickly writes initialization data to the
    // registry.  If the HKEY_CURRENT_USER hasn't been setup then it defaults
    // to writing to HKEY_USERS\.DEFAULT which is unacceptable.
    {
        IMEPRO ImePro;

        if (IMPGetIME((HWND)-1, &ImePro)) {
            IMPSetIME((HWND)-1, &ImePro);
        }
    }
#endif // DBCS_IME

    ThreadHandle = CreateThread(
                       NULL,
                       0,
                       AddToMessageAlias,
                       0,
                       0,
                       &ThreadId
                       );

    WaitResult = WaitForSingleObject( ThreadHandle, TIMEOUT_VALUE );

    if ( WaitResult == WAIT_TIMEOUT ) {

        //
        // This may never come back, so kill it.
        //

        UIPrint(("UserInit: AddToMessageAlias timeout, terminating thread\n"));
        TerminateThread( ThreadHandle, 0L );
    }

    CloseHandle( ThreadHandle );

    //
    // We're finished
    //

    return(0);
}



VOID
CheckControlPanelCache()
{


    WCHAR *pszRegCache;
    WCHAR *pszCacheValid;
    HKEY  hkeyRCache = NULL;
    DWORD j = 0;
    DWORD dwSize;
    DWORD dwType;
    DWORD dwDisposition;
    LONG Err;

    pszRegCache = L"Control Panel\\Cache";
    pszCacheValid = L"Cache Valid";


    //
    //  Check "Cache Valid" flag
    //

    Err = RegCreateKeyExW(
                            HKEY_CURRENT_USER,         // Root key
                            pszRegCache,               // Subkey to open/create
                            0L,                        // Reserved
                            NULL,                      // Class string
                            0L,                        // Options
                            KEY_ALL_ACCESS,            // SAM
                            NULL,                      // ptr to Security struct
                            &hkeyRCache,               // return handle
                            &dwDisposition             // return disposition
                         );

    if (Err == ERROR_SUCCESS ) {
        Err = RegQueryValueExW(hkeyRCache,pszCacheValid,0L,&dwType,(LPBYTE) &j,&dwSize);
        if (Err != ERROR_SUCCESS) {
            j = 0;
            }
        RegCloseKey(hkeyRCache);
        }
    else {
        j = 0;
        }

    if ( !j ) {
        ExecProcesses(TEXT("controlbuild"), TEXT("control /Build"));
        }
}
