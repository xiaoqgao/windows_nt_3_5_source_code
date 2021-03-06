/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    psp.h

Abstract:

    Private Interfaces for process structure.

Author:

    Mark Lucovsky (markl) 20-Apr-1989

Revision History:

--*/

#ifndef _PSP_
#define _PSP_

#include "ntos.h"
#include "ki.h"

#ifdef POOL_TAGGING
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'  sP')
#define ExAllocatePoolWithQuota(a,b) ExAllocatePoolWithQuotaTag(a,b,'  sP')
#endif

//
// Process Quota Charges:
//
//  PagedPool
//      Directory Base Page - PAGE_SIZE
//
//  NonPaged
//      Object Body         - sizeof(EPROCESS)
//

#define PSP_PROCESS_PAGED_CHARGE    (PAGE_SIZE)
#define PSP_PROCESS_NONPAGED_CHARGE (sizeof(EPROCESS))

//
// Thread Quota Charges:
//
//  PagedPool
//      Kernel Stack        - 0
//
//  NonPaged
//      Object Body         - sizeof(ETHREAD)
//

#define PSP_THREAD_PAGED_CHARGE     (0)
#define PSP_THREAD_NONPAGED_CHARGE  (sizeof(ETHREAD))

typedef struct _TERMINATION_PORT {
    LIST_ENTRY Links;
    PVOID Port;
} TERMINATION_PORT, *PTERMINATION_PORT;

typedef struct _GETSETCONTEXT {
    KAPC Apc;
    KPROCESSOR_MODE Mode;
    KEVENT OperationComplete;
    CONTEXT Context;
    KNONVOLATILE_CONTEXT_POINTERS NonVolatileContext;
} GETSETCONTEXT, *PGETSETCONTEXT;

typedef struct _SYSTEM_DLL {
    PVOID Section;
    PVOID DllBase;
    PKNORMAL_ROUTINE LoaderInitRoutine;
} SYSTEM_DLL, PSYSTEM_DLL;


//
// Private Entry Points
//

VOID
PspProcessDump(
    IN PVOID Object,
    IN POB_DUMP_CONTROL Control OPTIONAL
    );

VOID
PspProcessDelete(
    IN PVOID Object
    );


VOID
PspThreadDump(
    IN PVOID Object,
    IN POB_DUMP_CONTROL Control OPTIONAL
    );

VOID
PspInheritQuota(
    IN PEPROCESS NewProcess,
    IN PEPROCESS ParentProcess
    );

VOID
PspDereferenceQuota(
    IN PEPROCESS Process
    );

VOID
PspThreadDelete(
    IN PVOID Object
    );

//
// Initialization and loader entrypoints
//

BOOLEAN
PspInitPhase0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

BOOLEAN
PspInitPhase1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
PspLocateSystemDll( VOID );

NTSTATUS
PspInitializeSystemDll( VOID );

NTSTATUS
PspLookupSystemDllEntryPoint(
    IN PSZ EntryPointName,
    OUT PVOID *EntryPointAddress
    );

NTSTATUS
PspLookupKernelUserEntryPoints(
    VOID
    );

USHORT
PspNameToOrdinal(
    IN PSZ EntryPointName,
    IN ULONG DllBase,
    IN ULONG NumberOfNames,
    IN PULONG NameTableBase,
    IN PUSHORT OrdinalTableBase
    );

NTSTATUS
PspMapSystemDll(
    IN PEPROCESS Process,
    OUT PVOID *DllBase OPTIONAL
    );

//
// Internal Creation Functions
//

NTSTATUS
PspCreateProcess(
    OUT PHANDLE ProcessHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ParentProcess OPTIONAL,
    IN BOOLEAN InheritObjectTable,
    IN HANDLE SectionHandle OPTIONAL,
    IN HANDLE DebugPort OPTIONAL,
    IN HANDLE ExceptionPort OPTIONAL
    );

NTSTATUS
PspCreateThread(
    OUT PHANDLE ThreadHandle,
    IN ACCESS_MASK DesiredAccess,
    IN POBJECT_ATTRIBUTES ObjectAttributes OPTIONAL,
    IN HANDLE ProcessHandle,
    IN PEPROCESS ProcessPointer,
    OUT PCLIENT_ID ClientId OPTIONAL,
    IN PCONTEXT ThreadContext OPTIONAL,
    IN PINITIAL_TEB InitialTeb OPTIONAL,
    IN BOOLEAN CreateSuspended,
    IN PKSTART_ROUTINE StartRoutine OPTIONAL,
    IN PVOID StartContext
    );

//
// Startup Routines
//

VOID
PspUserThreadStartup(
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
    );

VOID
PspSystemThreadStartup(
    IN PKSTART_ROUTINE StartRoutine,
    IN PVOID StartContext
    );

VOID
PspReaper(
    IN PVOID StartContext
    );

VOID
PspNullSpecialApc(
    IN PKAPC Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
    );

//
// Thread Exit Support
//

VOID
PspExitThread(
    IN NTSTATUS ExitStatus
    );

NTSTATUS
PspTerminateThreadByPointer(
    IN PETHREAD Thread,
    IN NTSTATUS ExitStatus
    );

VOID
PspExitSpecialApc(
    IN PKAPC Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
    );

VOID
PspExitProcess(
    IN BOOLEAN TrimAddressSpace,
    IN PEPROCESS Process
    );


//
// Context Management
//

VOID
PspSetContext(
    OUT PKTRAP_FRAME TrapFrame,
    OUT PKNONVOLATILE_CONTEXT_POINTERS NonVolatileContext,
    IN PCONTEXT Context,
    KPROCESSOR_MODE Mode
    );

VOID
PspGetContext(
    IN PKTRAP_FRAME TrapFrame,
    IN PKNONVOLATILE_CONTEXT_POINTERS NonVolatileContext,
    IN OUT PCONTEXT Context
    );

VOID
PspGetSetContextSpecialApc(
    IN PKAPC Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
    );

VOID
PspExitNormalApc(
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

//
// private security routines
//

NTSTATUS
PspInitializeProcessSecurity(
    IN PEPROCESS Parent OPTIONAL,
    IN PEPROCESS Child
    );

VOID
PspDeleteProcessSecurity(
  IN PEPROCESS Process
  );

VOID
PspInitializeThreadSecurity(
    IN PEPROCESS Process,
    IN PETHREAD Thread
    );

VOID
PspDeleteThreadSecurity(
    IN PETHREAD Thread
    );

NTSTATUS
PspAssignPrimaryToken(
    IN PEPROCESS Process,
    IN HANDLE Token
    );


NTSTATUS
PspAssignImpersonationToken(
    IN PETHREAD Thread,
    IN HANDLE Token
    );

//
// VOID
// PspLockProcessSecurityFields(VOID)
//

#define PspLockProcessSecurityFields( ) ExAcquireFastMutex( &PspProcessSecurityLock )

//
// VOID
// PspFreeProcessSecurityFields(VOID);
//

#define PspFreeProcessSecurityFields( ) ExReleaseFastMutex( &PspProcessSecurityLock )

//
// Ldt support routines
//

#ifdef i386
NTSTATUS
PspLdtInitialize(
    );
#endif

//
// Vdm support Routines

#ifdef i386
NTSTATUS
PspVdmInitialize(
    );
#endif

NTSTATUS
PspQueryLdtInformation(
    IN PEPROCESS Process,
    OUT PVOID LdtInformation,
    IN ULONG LdtInformationLength,
    OUT PULONG ReturnLength
    );

NTSTATUS
PspSetLdtInformation(
    IN PEPROCESS Process,
    IN PVOID LdtInformation,
    IN ULONG LdtInformationLength
    );

NTSTATUS
PspSetLdtSize(
    IN PEPROCESS Process,
    IN PVOID LdtSize,
    IN ULONG LdtSizeLength
    );

VOID
PspDeleteLdt(
    IN PEPROCESS Process
    );

//
// Io handling support routines
//


NTSTATUS
PspSetProcessIoHandlers(
    IN PEPROCESS Process,
    IN PVOID IoHandlerInformation,
    IN ULONG IoHandlerLength
    );

VOID
PspDeleteVdmObjects(
    IN PEPROCESS Process
    );

NTSTATUS
PspQueryDescriptorThread (
    PETHREAD Thread,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
    );
//
//
// Global Data
//

extern PVOID PspCidTable;
extern HANDLE PspInitialSystemProcessHandle;
extern PACCESS_TOKEN PspBootAccessToken;
extern KSPIN_LOCK PspEventPairLock;
extern SYSTEM_DLL PspSystemDll;
extern FAST_MUTEX PspProcessSecurityLock;
extern FAST_MUTEX PspActiveProcessMutex;
extern FAST_MUTEX PspProcessLockMutex;

extern ULONG PspDefaultPagedLimit;
extern ULONG PspDefaultNonPagedLimit;
extern ULONG PspDefaultPagefileLimit;

extern EPROCESS_QUOTA_BLOCK PspDefaultQuotaBlock;
extern BOOLEAN PspDoingGiveBacks;

#if DBG

extern PRTL_EVENT_ID_INFO PspExitProcessEventId;
extern PRTL_EVENT_ID_INFO PspPageFaultEventId;

#endif // DBG

#endif // _PSP_
