/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    connect.c

Abstract:

    This module contains tree connections routines supported by
    NetWare Workstation service.

Author:

    Rita Wong  (ritaw)   15-Feb-1993

Revision History:

--*/

#include <nw.h>
#include <handle.h>
#include <nwauth.h>

#define NW_ENUM_EXTRA_BYTES    256

//-------------------------------------------------------------------//
//                                                                   //
// Local Function Prototypes                                         //
//                                                                   //
//-------------------------------------------------------------------//

DWORD
NwAllocAndGetUncName(
    IN LPWSTR LocalName,
    IN DWORD LocalNameLength,
    OUT LPWSTR *UncName
    );


//-------------------------------------------------------------------//



DWORD
NwrCreateConnection(
    IN LPWSTR Reserved OPTIONAL,
    IN LPWSTR LocalName OPTIONAL,
    IN LPWSTR RemoteName,
    IN DWORD Type,
    IN LPWSTR Password OPTIONAL,
    IN LPWSTR UserName OPTIONAL
    )
/*++

Routine Description:

    This function creates a tree connection to the specified RemoteName
    (UNC name) and maps it to the LocalName (local device name), if
    it is specified.  The password and user name are the credentials
    used to create the connection, if specified; otherwise, the
    interactive logged on user's credentials are used by default.

Arguments:

    Reserved - Must be NULL.

    LocalName - Supplies the local device name to map to the created tree
        connection.  Only drive letter device names are accepted.  (No
        LPT or COM).

    RemoteName - Supplies the UNC name of the remote resource in the format
        of Server\Volume\Directory.  It must be a disk resource.

    Type - Supplies the connection type.

    Password - Supplies the password to use to make the connection to the
        server.

    UserName - Supplies the user name to use to make the connection.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Out of memory allocating internal work buffers.

    WN_BAD_NETNAME - Remote resource name is invalid.

    WN_BAD_LOCALNAME - Local DOS device name is invalid.

    ERROR_BAD_NETPATH - The UNC name does not exist on the network.

    ERROR_INVALID_PARAMETER - LPT or COM LocalName was specified.

    Other errors from the redirector.

--*/
{
    DWORD status;

    DWORD LocalLength;

    LPWSTR Local = NULL;
    LPWSTR Unc = NULL;
    LPWSTR User = NULL;

    UNICODE_STRING TreeConnectStr;
    HANDLE TreeConnection;
    BOOL Impersonate = FALSE ;

    UNICODE_STRING EncodedPassword;



    UNREFERENCED_PARAMETER(Reserved);

    TreeConnectStr.Buffer = NULL;

    EncodedPassword.Length = 0;

    //
    // If local device is an empty string, it will be treated as a pointer to
    // NULL.
    //
    if (LocalName != NULL && *LocalName != 0) {

        //
        // Local device name is not NULL, canonicalize it
        //
#if DBG
        IF_DEBUG(CONNECT) {
            KdPrint(("\nNWWORKSTATION: NwrCreateConnection: LocalName %ws\n", LocalName));
        }
#endif

        if ((status = NwLibCanonLocalName(
                          LocalName,
                          &Local,     // Must be freed with LocalFree when done.
                          &LocalLength
                          )) != NO_ERROR) {

            return WN_BAD_LOCALNAME;
        }
    }

#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("NWWORKSTATION: NwrCreateConnection: RemoteName %ws\n", RemoteName));
    }
#endif

    //
    // Canonicalize the remote name, if it is not \\Server.
    //
    if ((status = NwLibCanonRemoteName(
                      Local,
                      RemoteName,
                      &Unc,           // Must be freed with LocalFree when done.
                      NULL
                      )) != NO_ERROR) {

        status = WN_BAD_NETNAME;
        goto CleanExit;
    }


    //
    // Canonicalize user name.
    //
    if (UserName != NULL) {

        //
        // Canonicalize username
        //
#if DBG
        IF_DEBUG(CONNECT) {
            KdPrint(("NWWORKSTATION: NwrCreateConnection: UserName %ws\n",
                     UserName));
        }
#endif

        if ((status = NwLibCanonUserName(
                          UserName,
                          &User,     // Must be freed with LocalFree when done.
                          NULL
                          )) != NO_ERROR) {

#ifdef QFE_BUILD
            //
            // if not valid, just ignore the username. this works 
            // around MPR bug where if you pass say domain\user to NWRDR
            // as first provider, and he throws it out, then the next one
            // doesnt get a chance. 
            //
            // BUGBUG - this should be removed when MPR bug #4051 is fixed
            // and all platforms we ship NWRDR have that fix.
            //
            UserName = NULL ; 
            status = NO_ERROR;
#else
            status = WN_BAD_VALUE;
            goto CleanExit;
#endif
        }
    }

    //
    // For password any syntax or length is accepted.
    //
    if (Password != NULL) {

#if DBG
        IF_DEBUG(CONNECT) {
            KdPrint(("NWWORKSTATION: NwrCreateConnection: Password %ws\n",
                     Password));
        }
#endif
        //
        // Decode the password
        //
        RtlInitUnicodeString(&EncodedPassword, Password);
        RtlRunDecodeUnicodeString(NW_ENCODE_SEED3, &EncodedPassword);
    }

    //
    // Create an NT-style tree connection name
    //
    if ((status = NwCreateTreeConnectName(
                      Unc,
                      Local,
                      &TreeConnectStr
                      )) != NO_ERROR) {
        goto CleanExit;
    }

    //
    // Impersonate the client
    //
    if ((status = NwImpersonateClient()) != NO_ERROR)
    {
        goto CleanExit;
    }

    Impersonate = TRUE ;

    if (Local != NULL) {

        //
        // Create symbolic link for local device name.
        //
        if ((status = NwCreateSymbolicLink(
                          Local,
                          TreeConnectStr.Buffer
                          )) != NO_ERROR) {

            goto CleanExit;
        }
    }

    //
    // Create the tree connection while impersonating the client so
    // that redirector can get to caller's logon id.
    //
    status = NwOpenCreateConnection(
                 &TreeConnectStr,
                 User,
                 Password,
                 Unc,
                 SYNCHRONIZE | GENERIC_WRITE,
                 FILE_CREATE,          // Fail if already exist
                 FILE_CREATE_TREE_CONNECTION |
                     FILE_SYNCHRONOUS_IO_NONALERT,
                 Type,
                 &TreeConnection,
                 NULL
                 );

    //
    // If there's a problem creating the tree connection, remove symbolic
    // link if any.
    //
    if (status != NO_ERROR) {

        if (  (status == ERROR_NOT_CONNECTED) 
           || (status == ERROR_FILE_NOT_FOUND )
           )
        {
            status = ERROR_BAD_NETPATH;
        }

        //
        // Delete the symbolic link we created.
        //
        NwDeleteSymbolicLink(
            Local,
            TreeConnectStr.Buffer
            );
    }
    else {

        //
        // Just close the connection handle.
        //
        (void) NtClose(TreeConnection);
    }

CleanExit:
    if (Local != NULL) {
        (void) LocalFree((HLOCAL) Local);
    }

    if (Unc != NULL) {
        (void) LocalFree((HLOCAL) Unc);
    }

    if (User != NULL) {
        (void) LocalFree((HLOCAL) User);
    }

    if (TreeConnectStr.Buffer != NULL) {
        (void) LocalFree((HLOCAL) TreeConnectStr.Buffer);
    }
 
    if (Impersonate) {
        (void) NwRevertToSelf() ;
    }

    //
    // Put the password back the way we found it.
    //
    if (EncodedPassword.Length != 0) {

        UCHAR Seed = NW_ENCODE_SEED3;

        RtlRunEncodeUnicodeString(&Seed, &EncodedPassword);
    }


#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("NWWORKSTATION: NwrCreateConnection returns %lu\n", status));
    }
#endif

    return status;
}


DWORD
NwrDeleteConnection(
    IN LPWSTR Reserved OPTIONAL,
    IN LPWSTR ConnectionName,
    IN DWORD UseForce
    )
/*++

Routine Description:

    This function deletes an existing connection.

Arguments:

    Reserved - Must be NULL.

    ConnectionName - Supplies the local device name or UNC name which
        specifies the connection to delete.  If UNC name is specified,
        the UNC connection must exist.


    UseForce - Supplies a flag which if TRUE specifies to tear down
        the connection eventhough files are opened.  If FALSE, the
        connection is deleted only if there are no opened files.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Out of memory allocating internal work buffers.

    ERROR_INVALID_NAME - ConnectionName is invalid.

    ERROR_BAD_NETPATH - The UNC name does not exist on the network.

    ERROR_INVALID_PARAMETER - LPT or COM LocalName was specified.

    Other errors from the redirector.

--*/
{
    DWORD status;

    LPWSTR ConnectName = NULL;
    DWORD ConnectLength;

    LPWSTR LocalName;
    LPWSTR UncName = NULL;

    BOOL Impersonate = FALSE ;

    UNREFERENCED_PARAMETER(Reserved);

    if (*ConnectionName == 0) {
        return ERROR_INVALID_PARAMETER;
    }

#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("\nNWWORKSTATION: NwrDeleteConnection: ConnectionName %ws, Force %lu\n",
                 ConnectionName, UseForce));
    }
#endif

    if ((status = NwLibCanonLocalName(
                      ConnectionName,
                      &ConnectName,
                      &ConnectLength
                      )) == NO_ERROR) {

        //
        // Get the UNC name mapped to this drive letter so that we can
        // open a handle to it for deletion.
        //
        if ((status = NwAllocAndGetUncName(
                          ConnectName,
                          ConnectLength,
                          &UncName
                          )) != NO_ERROR) {

            if (status == WN_NOT_CONNECTED && 
                NwGetGatewayResource(ConnectName,
                                     NULL, 
                                     0, 
                                     NULL) == WN_MORE_DATA)
            {
                status = ERROR_DEVICE_IN_USE ;
            }

            (void) LocalFree((HLOCAL) ConnectName);

            return status;
        }

        LocalName = ConnectName;

    }
    else {

        //
        // Not a device name.  See if it is a UNC name.
        //
        if ((status = NwLibCanonRemoteName(
                          NULL,
                          ConnectionName,
                          &ConnectName,
                          NULL
                          )) != NO_ERROR) {

            return status;
        }

        UncName = ConnectName;
        LocalName = NULL;

    }

    if ((status = NwImpersonateClient()) != NO_ERROR)
    {
        goto CleanExit;
    }
    Impersonate = TRUE ;

    //
    // To delete a connection, a tree connection handle must be opened to
    // it so that the handle can be specified to the redirector to delete
    // the connection.
    //
    status = NwOpenHandleToDeleteConn(
                 UncName,
                 LocalName,
                 UseForce,
                 FALSE
                 );

    if ( status == ERROR_FILE_NOT_FOUND )
        status = ERROR_BAD_NETPATH;

CleanExit:

    if (Impersonate) {
        (void) NwRevertToSelf() ;
    }
    if (UncName != NULL && UncName != ConnectName) {
        (void) LocalFree((HLOCAL) UncName);
    }

    if (ConnectName != NULL) {
        (void) LocalFree((HLOCAL) ConnectName);
    }

#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("NWWORKSTATION: NwrDeleteConnection returns %lu\n", status));
    }
#endif

    return status;
}


DWORD
NwrQueryServerResource(
    IN LPWSTR Reserved OPTIONAL,
    IN LPWSTR LocalName,
    OUT LPWSTR RemoteName,
    IN DWORD RemoteNameLen,
    OUT LPDWORD CharsRequired
    )
/*++

Routine Description:

    This function looks up the UNC name associated with the given DOS
    device name.

Arguments:

    Reserved - Must be NULL.

    LocalName - Supplies the local device name to look up.

    RemoteName - Receives the UNC name mapped to the LocalName.

    RemoteNameLen - Supplies the length of the RemoteName buffer.

    CharsRequired - Receives the length required of the RemoteName buffer
        to get the UNC name.  This value is only returned if the return
        code is ERROR_MORE_DATA.

Return Value:

    NO_ERROR - Operation was successful.

    WN_BAD_LOCALNAME - LocalName was invalid.

    ERROR_INVALID_PARAMETER - LPT or COM LocalName was specified.

    ERROR_MORE_DATA - RemoteName buffer was too small.

    ERROR_NOT_CONNECTED - LocalName does not map to any server resource.

--*/
{
    DWORD status;

    LPWSTR Local;
    DWORD LocalLength;

    BOOL Impersonate = FALSE ;

    UNREFERENCED_PARAMETER(Reserved);

#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("\nNWWORKSTATION: NwrQueryServerResource: LocalName %ws, RemoteNameLen %lu\n",
                 LocalName, RemoteNameLen));
    }
#endif

    //
    // Canonicalize the LocalName
    //
    if ((status = NwLibCanonLocalName(
                      LocalName,
                      &Local,
                      &LocalLength
                      )) != NO_ERROR) {

        return WN_BAD_LOCALNAME;
    }

    if ((status = NwImpersonateClient()) != NO_ERROR)
    {
        goto CleanExit;
    }

    Impersonate = TRUE ;

    status = NwGetServerResource(
                 Local,
                 LocalLength,
                 RemoteName,
                 RemoteNameLen,
                 CharsRequired
                 );

    if (status == WN_NOT_CONNECTED)
    {
        status = NwGetGatewayResource(
                     Local,
                     RemoteName,
                     RemoteNameLen,
                     CharsRequired
                     );
    }

CleanExit:

    if (Impersonate) {
        (void) NwRevertToSelf() ;
    }

    (void) LocalFree((HLOCAL) Local);

#if DBG
    IF_DEBUG(CONNECT) {
        KdPrint(("NWWORKSTATION: NwrQueryServerResource returns %lu\n", status));

        if (status == NO_ERROR) {
            KdPrint(("              RemoteName is %ws\n", RemoteName));
        }
        else if (status == ERROR_MORE_DATA) {
            KdPrint(("              RemoteNameLen %lu too small.  Need %lu\n",
                     RemoteNameLen, *CharsRequired));
        }
    }
#endif

    return status;
}


DWORD
NwrOpenEnumConnections(
    IN LPWSTR Reserved OPTIONAL,
    IN DWORD ConnectionType,
    OUT LPNWWKSTA_CONTEXT_HANDLE EnumHandle
    )
/*++

Routine Description:

    This function creates a new context handle and initializes it
    for enumerating the connections.

Arguments:

    Reserved - Unused.

    EnumHandle - Receives the newly created context handle.

Return Value:

    ERROR_NOT_ENOUGH_MEMORY - if the memory for the context could
        not be allocated.

    NO_ERROR - Call was successful.

--*/
{
    LPNW_ENUM_CONTEXT ContextHandle;


    UNREFERENCED_PARAMETER(Reserved);

#if DBG
    IF_DEBUG(CONNECT) {
       KdPrint(("\nNWWORKSTATION: NwrOpenEnumConnections\n"));
    }
#endif

    //
    // Allocate memory for the context handle structure.
    //
    ContextHandle = (PVOID) LocalAlloc(
                                LMEM_ZEROINIT,
                                sizeof(NW_ENUM_CONTEXT)
                                );

    if (ContextHandle == NULL) {
        KdPrint(("NWWORKSTATION: NwrOpenEnumConnections LocalAlloc Failed %lu\n",
                 GetLastError()));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    //
    // Initialize contents of the context handle structure.
    //
    ContextHandle->Signature = NW_HANDLE_SIGNATURE;
    ContextHandle->HandleType = NwsHandleListConnections;
    ContextHandle->ResumeId = 0;
    ContextHandle->ConnectionType = 0;

    if ( ConnectionType == RESOURCETYPE_ANY ) {
        ContextHandle->ConnectionType = CONNTYPE_ANY;
    }
    else {
        
        if ( ConnectionType & RESOURCETYPE_DISK ) 
            ContextHandle->ConnectionType |= CONNTYPE_DISK;

        if ( ConnectionType & RESOURCETYPE_PRINT ) 
            ContextHandle->ConnectionType |= CONNTYPE_PRINT;
    }
         
         

    //
    // Return the newly created context.
    //
    *EnumHandle = (LPNWWKSTA_CONTEXT_HANDLE) ContextHandle;

    return NO_ERROR;
}




DWORD
NwAllocAndGetUncName(
    IN LPWSTR LocalName,
    IN DWORD LocalNameLength,
    OUT LPWSTR *UncName
    )
/*++

Routine Description:

    This function calls an internal routine to ask the redirector for the
    UNC name of a given DOS device name.  It also allocates the output
    buffer to hold the UNC name.

Arguments:

    LocalName - Supplies the DOS device name.

    LocalNameLength - Supplies the length of the DOS device name (chars).

    UncName - Receives a pointer to the output buffer allocated by this
        routine which contains the UNC name of the DOS device.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Could not allocate output buffer.

    Other errors from the redirector.
--*/
{
    DWORD status;
    DWORD UncNameLength;



    *UncName = (PVOID) LocalAlloc(
                           LMEM_ZEROINIT,
                           (MAX_PATH + 1) * sizeof(WCHAR)
                           );

    if (*UncName == NULL) {
        KdPrint(("NWWORKSTATION: NwAllocAndGetUncName LocalAlloc Failed %lu\n",
                 GetLastError()));
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    status = NwGetServerResource(
                 LocalName,
                 LocalNameLength,
                 *UncName,
                 MAX_PATH + 1,
                 &UncNameLength
                 );

    if ((status == ERROR_MORE_DATA) || (status == ERROR_INSUFFICIENT_BUFFER)) {

        //
        // Our output buffer was too small.  Try again.
        //
        (void) LocalFree((HLOCAL) *UncName);

        *UncName = (PVOID) LocalAlloc(
                               LMEM_ZEROINIT,
                               UncNameLength * sizeof(WCHAR)
                               );

        if (*UncName == NULL) {
            KdPrint(("NWWORKSTATION: NwAllocAndGetUncName LocalAlloc Failed %lu\n",
                     GetLastError()));
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        status = NwGetServerResource(
                     LocalName,
                     LocalNameLength,
                     *UncName,
                     UncNameLength,
                     &UncNameLength
                     );

    }

    //
    // callers will only free this if success.
    //
    if (status != NO_ERROR) 
    {
        (void) LocalFree((HLOCAL) *UncName);
        *UncName = NULL ;
    }

    return status;
}


DWORD
NwOpenHandleToDeleteConn(
    IN LPWSTR UncName,
    IN LPWSTR LocalName OPTIONAL,
    IN DWORD UseForce,
    IN BOOL IsStopWksta
    )
/*++

Routine Description:

    This function deletes an active connection by opening a tree connection
    handle to the connection first, and specifying this handle to the
    redirector to delete.  This is because the workstation service does
    not keep any connection information.

Arguments:

    UncName - Supplies the UNC name of the connection to delete.

    LocalName - Supplies the DOS device name of the connection, if any.

    UseForce - Supplies a flag which if TRUE specifies to tear down
        the connection eventhough files are opened.  If FALSE, the
        connection is deleted only if there are no opened files.

    IsStopWksta - Supplies a flag which if TRUE indicates that we must
        delete the symbolic link, even when we have failed to delete the
        connection in the redirector.  As much as possible must be cleaned
        up because the workstation service is stopping.  A value of FALSE,
        indicates that the delete is aborted if we cannot delete it in
        the redirector.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Could not allocate output buffer.

    Other errors from the redirector.
--*/
{
    DWORD status;

    UNICODE_STRING TreeConnectStr;
    HANDLE TreeConnection = NULL;



    TreeConnectStr.Buffer = NULL;

    //
    // Create an NT-style tree connection name, either: \Device\Nwrdr\Server\Vol
    // or \Device\Nwrdr\X:\Server\Vol, if LocalName is specified.
    //
    if ((status = NwCreateTreeConnectName(
                      UncName,
                      LocalName,
                      &TreeConnectStr
                      )) != NO_ERROR) {
        return status;
    }

    status = NwMapStatus(NwCallNtOpenFile( &TreeConnection, 
                                           SYNCHRONIZE | DELETE, 
                                           &TreeConnectStr, 
                                           FILE_CREATE_TREE_CONNECTION  
                                           | FILE_SYNCHRONOUS_IO_NONALERT
                                           | FILE_DELETE_ON_CLOSE
                                         ));

    if (status == NO_ERROR) {

        //
        // Ask the redirector to delete the tree connection.
        //
        status = NwNukeConnection(
                     TreeConnection,
                     UseForce
                     );

        (void) CloseHandle(TreeConnection);
    }

    if (ARGUMENT_PRESENT(LocalName) &&
        (status == NO_ERROR || IsStopWksta)) {

        //
        // Delete the symbolic link we created.
        //
        NwDeleteSymbolicLink(
            LocalName,
            TreeConnectStr.Buffer
            );
    }

    if (TreeConnectStr.Buffer != NULL) {
        (void) LocalFree((HLOCAL) TreeConnectStr.Buffer);
    }

    return status;
}


VOID
DeleteAllConnections(
    VOID
    )
/*++

Routine Description:

    This function deletes all active connections returned by the
    redirector ENUMERATE_CONNECTIONS fsctl on workstation termination.

Arguments:

    None.

Return Value:

    None.

--*/
{
    DWORD status;
    NWWKSTA_CONTEXT_HANDLE EnumHandle;
    LPNETRESOURCEW NetR = NULL;

    DWORD BytesNeeded = 256;
    DWORD EntriesRead;


    status = NwrOpenEnumConnections(NULL, RESOURCETYPE_ANY, &EnumHandle);
    if ( status != NO_ERROR )
        return;

    //
    // Allocate buffer to get connection list.
    //
    if ((NetR = (LPVOID) LocalAlloc(
                             0,
                             BytesNeeded
                             )) == NULL) {

        status = ERROR_NOT_ENOUGH_MEMORY;
        goto CleanExit;
    }

    do {

        status = NwEnumerateConnections(
                     &((LPNW_ENUM_CONTEXT) EnumHandle)->ResumeId,
                     0xFFFFFFFF,
                     (LPBYTE) NetR,
                     BytesNeeded,
                     &BytesNeeded,
                     &EntriesRead,
                     CONNTYPE_ANY 
                     );

        if (status == NO_ERROR) {

            DWORD i;
            LPNETRESOURCEW SavePtr = NetR;
            LPWSTR Local;


            for (i = 0; i < EntriesRead; i++, NetR++) {

                Local = NetR->lpLocalName;

                if (NetR->lpLocalName && *(NetR->lpLocalName) == 0) {
                    Local = NULL;
                }

                (void) NwOpenHandleToDeleteConn(
                           NetR->lpRemoteName,
                           Local,
                           TRUE,
                           TRUE
                           );
            }

            NetR = SavePtr;

        }
        else if (status == WN_MORE_DATA) {

            //
            // Original buffer was too small.  Free it and allocate
            // the recommended size and then some to get as many
            // entries as possible.
            //

            (void) LocalFree((HLOCAL) NetR);

            BytesNeeded += NW_ENUM_EXTRA_BYTES;

            if ((NetR = (LPVOID) LocalAlloc(
                                     0,
                                     BytesNeeded
                                     )) == NULL) {

                status = ERROR_NOT_ENOUGH_MEMORY;
                goto CleanExit;
            }
        }
        else {
            // give up if see any other return code
            break ;
        }

    } while (status != WN_NO_MORE_ENTRIES);

CleanExit:
    (void) NwrCloseEnum(&EnumHandle);

    if (NetR != NULL) {
        (void) LocalFree((HLOCAL) NetR);
    }
}



DWORD
NwCreateSymbolicLink(
    IN  LPWSTR Local,
    IN  LPWSTR TreeConnectStr
    )
/*++

Routine Description:

    This function creates a symbolic link object for the specified local
    device name which is linked to the tree connection name that has a
    format of \Device\NwRdr\Device:\Server\Volume\Directory.

Arguments:

    Local - Supplies the local device name.

    TreeConnectStr - Supplies the tree connection name string which is
        the link target of the symbolick link object.

Return Value:

    NO_ERROR or reason for failure.

--*/
{
    WCHAR TempBuf[64];

    DWORD LocalLength = wcslen(Local);


    if (LocalLength > 2) {

        LPWSTR UncName;


        //
        // Local device is LPTn:
        //

        //
        // Check to see if we already have this UNC name mapped.
        //
        if (NwAllocAndGetUncName(
                Local,
                LocalLength,
                &UncName
                ) == NO_ERROR) {

            LocalFree((HLOCAL) UncName);
            return ERROR_ALREADY_ASSIGNED;
        }

    }
    else {

        //
        // Local device is X:
        //

        if (! QueryDosDeviceW(
                  Local,
                  TempBuf,
                  64
                  )) {

            if (GetLastError() != ERROR_FILE_NOT_FOUND) {

                //
                // Most likely failure occurred because our output
                // buffer is too small.  It still means someone already
                // has an existing symbolic link for this device.
                //

                return ERROR_ALREADY_ASSIGNED;
            }

            //
            // ERROR_FILE_NOT_FOUND (translated from OBJECT_NAME_NOT_FOUND)
            // means it does not exist and we can redirect this device.
            //
        }
        else {

            //
            // QueryDosDevice successfully an existing symbolic link--
            // somebody is already using this device.
            //
            return ERROR_ALREADY_ASSIGNED;
        }
    }

    //
    // Create a symbolic link object to the device we are redirecting
    //
    if (! DefineDosDeviceW(
              DDD_RAW_TARGET_PATH,
              Local,
              TreeConnectStr
              )) {

        return GetLastError();
    }

    return NO_ERROR;
}



VOID
NwDeleteSymbolicLink(
    IN  LPWSTR LocalDeviceName,
    IN  LPWSTR TreeConnectStr
    )
/*++

Routine Description:

    This function deletes the symbolic link we had created earlier for
    the device.

Arguments:

    LocalDeviceName - Supplies the local device name string of which the
        symbolic link object is created.

    TreeConnectStr - Supplies a pointer to the Unicode string which
        contains the link target string we want to match and delete.

Return Value:

    None.

--*/
{
    if (LocalDeviceName != NULL) {

        if (! DefineDosDeviceW(
                  DDD_REMOVE_DEFINITION  |
                     DDD_RAW_TARGET_PATH |
                     DDD_EXACT_MATCH_ON_REMOVE,
                  LocalDeviceName,
                  TreeConnectStr
                  )) {

#if DBG
            IF_DEBUG(CONNECT) {
                KdPrint(("NWWORKSTATION: DefineDosDevice DEL of %ws %ws returned %lu\n",
                         LocalDeviceName, TreeConnectStr, GetLastError()));
            }
#endif
        }
#if DBG
        else {
            IF_DEBUG(CONNECT) {
                KdPrint(("NWWORKSTATION: DefineDosDevice DEL of %ws %ws returned successful\n",
                         LocalDeviceName, TreeConnectStr));
            }

        }
#endif

    }
}

DWORD
NwCreateGWConnection(
    IN LPWSTR RemoteName,
    IN LPWSTR UserName,
    IN LPWSTR Password,
    IN BOOL   KeepConnection
    )
/*++

Routine Description:

    This function creates a tree connection to the specified RemoteName
    (UNC name). It is only used by the Gateway and DOES NOT impersonate.

Arguments:

    RemoteName - Supplies the UNC name of the remote resource in the format
        of Server\Volume\Directory.  It must be a disk resource.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Out of memory allocating internal work buffers.

    WN_BAD_NETNAME - Remote resource name is invalid.

    WN_BAD_LOCALNAME - Local DOS device name is invalid.

    ERROR_BAD_NETPATH - The UNC name does not exist on the network.

    ERROR_INVALID_PARAMETER - LPT or COM LocalName was specified.

    Other errors from the redirector.

--*/
{
    DWORD status;
    LPWSTR Unc = NULL;
    LPWSTR User = NULL;
    UNICODE_STRING TreeConnectStr;
    HANDLE TreeConnection;

    TreeConnectStr.Buffer = NULL;

    //
    // Canonicalize the remote name, if it is not \\Server.
    //
    if ((status = NwLibCanonRemoteName(
                      NULL,
                      RemoteName,
                      &Unc,           // Must be freed with LocalFree when done.
                      NULL
                      )) != NO_ERROR) 
    {
        status = WN_BAD_NETNAME;
        goto CleanExit;
    }

    if (UserName != NULL) {

        //
        // Canonicalize username
        //
        if ((status = NwLibCanonUserName(
                          UserName,
                          &User,     // Must be freed with LocalFree when done.
                          NULL
                          )) != NO_ERROR) {

            status = WN_BAD_VALUE;
            goto CleanExit;
        }
    }

    //
    // Create an NT-style tree connection name
    //
    if ((status = NwCreateTreeConnectName(
                      Unc,
                      NULL,
                      &TreeConnectStr
                      )) != NO_ERROR) 
    {
        goto CleanExit;
    }


    //
    // Create the tree connection while impersonating the client so
    // that redirector can get to caller's logon id.
    //
    status = NwOpenCreateConnection(
                 &TreeConnectStr,
                 User,
                 Password,
                 Unc,
                 SYNCHRONIZE | GENERIC_WRITE,
                 FILE_CREATE,          // Fail if already exist
                 FILE_CREATE_TREE_CONNECTION |
                     FILE_SYNCHRONOUS_IO_NONALERT,
                 RESOURCETYPE_DISK,
                 &TreeConnection,
                 NULL
                 );


    if (status != NO_ERROR) {

        if (  (status == ERROR_NOT_CONNECTED) 
           || (status == ERROR_FILE_NOT_FOUND )
           )
        {
            status = ERROR_BAD_NETPATH;
        }
    }
    else 
    {

        //
        // Just close the connection handle.
        //
        (void) NtClose(TreeConnection);


        //
        // delete the connect we just created. ignore this error.
        //
        if (!KeepConnection)
        {
            (void) NwOpenHandleToDeleteConn(
                         RemoteName,
                         NULL,
                         FALSE,
                         FALSE
                         );
        }
    }


CleanExit:

    if (User != NULL) {
        (void) LocalFree((HLOCAL) User);
    }

    if (Unc != NULL) {
        (void) LocalFree((HLOCAL) Unc);
    }

    if (TreeConnectStr.Buffer != NULL) {
        (void) LocalFree((HLOCAL) TreeConnectStr.Buffer);
    }

    return status;
}

DWORD
NwDeleteGWConnection(
    IN LPWSTR ConnectionName
    )
/*++

Routine Description:

    This function deletes an existing connection.

Arguments:

    ConnectionName - Supplies the local device name or UNC name which
        specifies the connection to delete.  If UNC name is specified,
        the UNC connection must exist.

Return Value:

    NO_ERROR - Operation was successful.

    ERROR_NOT_ENOUGH_MEMORY - Out of memory allocating internal work buffers.

    ERROR_INVALID_NAME - ConnectionName is invalid.

    ERROR_BAD_NETPATH - The UNC name does not exist on the network.

    ERROR_INVALID_PARAMETER - LPT or COM LocalName was specified.

    Other errors from the redirector.

--*/
{
    DWORD status;

    LPWSTR ConnectName = NULL;
    DWORD ConnectLength;

    if (!ConnectionName || *ConnectionName == 0) {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // See if it is a UNC name.
    //
    if ((status = NwLibCanonRemoteName(
                          NULL,
                          ConnectionName,
                          &ConnectName,
                          NULL
                          )) != NO_ERROR) {

        return status;
    }

    //
    // To delete a connection, a tree connection handle must be opened to
    // it so that the handle can be specified to the redirector to delete
    // the connection.
    //
    status = NwOpenHandleToDeleteConn(
                 ConnectName,
                 NULL,
                 TRUE,
                 FALSE
                 );

    if ( status == ERROR_FILE_NOT_FOUND )
        status = ERROR_BAD_NETPATH;

CleanExit:

    if (ConnectName != NULL) {
        (void) LocalFree((HLOCAL) ConnectName);
    }

    return status;
}

