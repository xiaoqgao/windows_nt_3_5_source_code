/********************************************************************/
/**               Copyright(c) 1989 Microsoft Corporation.	   **/
/********************************************************************/

//***
//
// Filename:	callback.c
//
// Description: Contains FSM code to handle and callback control protocol
//
// History:
//	April 11,1993.	NarenG		Created original version.
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>     // needed for winbase.h

#include <windows.h>    // Win32 base API's
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <lmcons.h>
#include <raserror.h>
#include <rasman.h>
#include <errorlog.h>
#include <pppcp.h>
#include <rasppp.h>
#include <raspppe.h>
#include <ppp.h>
#include <auth.h>
#include <callback.h>
#include <smevents.h>
#include <smaction.h>
#include <lcp.h>
#include <timer.h>
#include <util.h>
#include <worker.h>

//**
//
// Call:	CbStart
//
// Returns:	none
//
// Description: Called to initiatialze the callback control protocol and to
//		initiate to callback negotiation.
//
VOID
CbStart( 
    IN PCB * pPcb,
    IN DWORD CpIndex
)
{
    DWORD   	 dwRetCode;
    PPPCB_INPUT  PppCbInput;
    CPCB *  	 pCpCb = &(pPcb->CpCb[CpIndex]);
  
    PppCbInput.fServer 	           = pPcb->fServer;
    PppCbInput.bfCallbackPrivilege = (BYTE)(pPcb->fCallbackPrivilege);
    PppCbInput.CallbackDelay       = ( pPcb->ConfigInfo.dwConfigMask & 
                                     PPPCFG_UseCallbackDelay )
                                     ? pPcb->ConfigInfo.dwCallbackDelay
                                     : 0;

    PppCbInput.pszCallbackNumber   = pPcb->szCallbackNumber;

    PppLog( 2, "CallbackPriv in CB = %x\r\n", PppCbInput.bfCallbackPrivilege );
    
    dwRetCode = (CpTable[CpIndex].RasCpBegin)(&(pCpCb->pWorkBuf), &PppCbInput);

    if ( dwRetCode != NO_ERROR )
    {
	pPcb->CpCb[LCP_INDEX].dwError = dwRetCode;

	FsmClose( pPcb, LCP_INDEX );

	return;
    }

    InitRestartCounters( pPcb, CpIndex );

    CbWork( pPcb, CpIndex, NULL, NULL );
}

//**
//
// Call:	CbStop
//
// Returns:	none
//
// Description:	Called to stop the callback control protocol machine.
//
VOID
CbStop( 
    IN PCB * pPcb,
    IN DWORD CpIndex
)
{
    CPCB * pCpCb = &(pPcb->CpCb[CpIndex]);

    RemoveFromTimerQ( pPcb->hPort, 
                      pCpCb->LastId, 
                      CpTable[CpIndex].Protocol,
                      TIMER_EVENT_TIMEOUT );

    if ( pCpCb->pWorkBuf != NULL )
    	(CpTable[CpIndex].RasCpEnd)( pCpCb->pWorkBuf );
}

//**
//
// Call:	CbWork
//
// Returns:	none
//
// Description:	Called when and callback control protocol packet was received or
//		a timeout ocurred or to initiate callback negotiation.
//
VOID
CbWork(
    IN PCB * 	     pPcb,
    IN DWORD 	     CpIndex,
    IN PPP_CONFIG *  pRecvConfig,
    IN PPPCB_INPUT * pCbInput
)
{
    DWORD	 dwRetCode;
    CPCB *  	 pCpCb 	     = &(pPcb->CpCb[CpIndex]);
    PPP_CONFIG * pSendConfig = (PPP_CONFIG*)(pPcb->pSendBuf->Information);
    PPPCB_RESULT CbResult;
    DWORD	 dwLength;

    dwRetCode = (CpTable[CpIndex].RasApMakeMessage)( pCpCb->pWorkBuf,
						     pRecvConfig,
						     pSendConfig,
    				  		     LCP_DEFAULT_MRU 
						     - PPP_PACKET_HDR_LEN,
    				  		     (PPPAP_RESULT*)&CbResult,
                                                     (PPPAP_INPUT*)pCbInput );

    if ( dwRetCode != NO_ERROR )
    {
        if ( dwRetCode == ERROR_PPP_INVALID_PACKET )
        {
            PppLog(1,
                   "Silently discarding invalid callback packet on port %d\r\n",
                   pPcb->hPort );

            return;
        }
        else
        {
	    pPcb->CpCb[LCP_INDEX].dwError = dwRetCode;

	    FsmClose( pPcb, LCP_INDEX );

	    return;
        }
    }

    switch( CbResult.Action )
    {

    case APA_Send:
    case APA_SendWithTimeout:
    case APA_SendWithTimeout2:
    case APA_SendAndDone:

    	HostToWireFormat16( (WORD)CpTable[CpIndex].Protocol, 
			    (PBYTE)(pPcb->pSendBuf->Protocol) );

    	dwLength = WireToHostFormat16( pSendConfig->Length );

        LogPPPPacket(FALSE,pPcb,pPcb->pSendBuf,dwLength+PPP_PACKET_HDR_LEN);

    	//
    	// If RasPortSend fails we assume that the receive that is posted for 
	// this port will complete and the dispatch thread will generate a 
	// LineDown event which will do the clean up. Hence all we do here 
	// is send a notification to the client.
    	//

    	if ( ( dwRetCode = RasPortSend( pPcb->hPort, 
				    	(CHAR*)(pPcb->pSendBuf), 
				        (WORD)(dwLength + PPP_PACKET_HDR_LEN )))
					!= NO_ERROR )
    	{
	    NotifyCaller( pPcb, 
			  pPcb->fServer 
			  ? E2DMSG_SrvPppFailure 
			  : E2DMSG_PppFailure, 
			  &dwRetCode );
	    return;
        }

	pCpCb->LastId = CbResult.bIdExpected;

        if ( ( CbResult.Action == APA_SendWithTimeout ) ||
             ( CbResult.Action == APA_SendWithTimeout2 ) )
	{
    	    InsertInTimerQ( pPcb->hPort, 
			    pCpCb->LastId, 
			    CpTable[CpIndex].Protocol,
                            TIMER_EVENT_TIMEOUT,
			    pPcb->RestartTimer );

            //
            // For SendWithTimeout2 we increment the ConfigRetryCount. This 
            // means send with infinite retry count
            //

            if ( CbResult.Action == APA_SendWithTimeout2 ) 
            {
	    	(pCpCb->ConfigRetryCount)++;
            }
 	}

	if ( CbResult.Action != APA_SendAndDone )
	    break;

    case APA_Done:

        if ( CbResult.bfCallbackPrivilege == RASPRIV_NoCallback )
        {
            //
            // If no callback was negotiated we continue on to the next
            // phase.
            //

	    FsmThisLayerUp( pPcb, CpIndex );
        }
        else
        {
            if ( pPcb->fServer )
            {
                //
                // If we are the server side we save the callback info
                //

                strcpy( pPcb->szCallbackNumber, CbResult.szCallbackNumber );

                pPcb->ConfigInfo.dwCallbackDelay = CbResult.CallbackDelay;

            }
            else
            {
                //
                // We are the client side so, we tell the server that we 
                // bringing the link down and we tell the client to 
                // prepare for callback
                //

                FsmClose( pPcb, LCP_INDEX );
            }

            pPcb->fDoingCallback = TRUE;
        }

	break;

    case APA_NoAction:

        //
        // If we are on the client then we need to get the callback number 
        // from the user.
        //

        if ( ( !(pPcb->fServer) ) && ( CbResult.fGetCallbackNumberFromUser ) ) 
        {
            NotifyCaller( pPcb, E2DMSG_CallbackRequest, NULL );
        }

        break;

    default:

	break;
    }

}
