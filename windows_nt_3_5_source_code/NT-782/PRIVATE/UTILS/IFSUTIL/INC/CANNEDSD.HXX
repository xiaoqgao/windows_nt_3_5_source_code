/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    cannedsd.hxx

Abstract:

    This module contains declarations for the CANNED_SECURITY
    class, which is a repository for the canned Security Descriptors
    used by the utilities.

    Initializing an object of this type generates the canned
    security descriptors used by the utilities, which can
    then be gotten from the object.

    These security descriptors are all in the self-relative
    format.

Author:

    Bill McJohn (billmc) 04-March-1992

--*/

#if ! defined ( _CANNED_SECURITY_DEFN )

#define _CANNED_SECURITY_DEFN

// The IFS utilities use the following kinds of canned Security Descriptors:
//
//  NoAccess    --  No one is granted any access (empty ACL).
//  NoAcl       --  The file has no ACL.
//  ReadOnly    --  System and Admins can read the file.
//  ReadWrite   --  System and Admins can read and write the file.
//  Edit        --  System and Admins can read and write the file,
//                  and can also change its permissions.
//
typedef enum _CANNED_SECURITY_TYPE {

    NoAccessCannedSd,
    NoAclCannedSd,
    ReadCannedSd,
    WriteCannedSd,
    EditCannedSd
};

// These security descriptors need the SID's for System and Administrators.
//

//#define WELL_KNOWN_NAME_SYSTEM L"System"
//#define WELL_KNOWN_NAME_ADMINS L"Administrators"

#define WELL_KNOWN_NAME_SYSTEM L"SYSTEM"
#define WELL_KNOWN_NAME_ADMINS L"ADMINS"



DEFINE_TYPE( _CANNED_SECURITY_TYPE, CANNED_SECURITY_TYPE );


class CANNED_SECURITY : public OBJECT {

    public:

        DECLARE_CONSTRUCTOR( CANNED_SECURITY );

        ~CANNED_SECURITY(
            );

        NONVIRTUAL
        BOOLEAN
        Initialize(
            );

        NONVIRTUAL
        PVOID
        GetCannedSecurityDescriptor(
            IN  CANNED_SECURITY_TYPE    Type,
            OUT PULONG                  SecurityDescriptorLength
            );

    private:

        NONVIRTUAL
        VOID
        Construct(
            );

        NONVIRTUAL
        VOID
        Destroy(
            );

        STATIC
        BOOLEAN
        QuerySystemSid(
            OUT    PSID     NewSid,
            IN OUT PULONG   Length
            );

        STATIC
        BOOLEAN
        QueryAdminsSid(
            OUT    PSID     NewSid,
            IN OUT PULONG   Length
            );

        STATIC
        PVOID
        GenerateCannedSd(
            IN  BOOLEAN     DaclPresent,
            IN  ACCESS_MASK GrantedAccess,
            IN  PSID        AdminsSid,
            IN  PSID        SystemSid,
            IN  HANDLE      TokenHandle,
            OUT PULONG      Length
            );

        STATIC
        BOOLEAN
        GenerateCannedAcl(
            IN  PACL        AclBuffer,
            IN  ULONG       BufferLength,
            IN  ACCESS_MASK GrantedAccess,
            IN  PSID        AdminsSid,
            IN  PSID        SystemSid
            );

        ULONG _NoAccessLength, _NoAclLength, _ReadLength,
                _WriteLength, _EditLength;
        PVOID _NoAccessSd, _NoAclSd, _ReadSd, _WriteSd, _EditSd;
};


#endif
