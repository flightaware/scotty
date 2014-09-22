/*
 * tnmWinIcmp.c --
 *
 *	Make an ICMP request to a list of IP addresses. The Windows
 *	implementation uses the unofficial Microsoft ICMP DLL since
 *	there are currently no RAW sockets shipped with Windows NT
 *	3.51 and Windows '95.
 *
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * Still left to do:
 *
 * - this code sometimes crashes windows 95.
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * The following structure is used by Microsoft's ICMP.DLL.
 */

typedef struct {
   unsigned char Ttl;			/* Time To Live. */
   unsigned char Tos;			/* Type Of Service. */
   unsigned char Flags;			/* IP header flags. */
   unsigned char OptionsSize;		/* Size in bytes of options data. */
   unsigned char *OptionsData;		/* Pointer to options data. */
} IP_OPTION_INFORMATION, * PIP_OPTION_INFORMATION;

typedef struct {
   DWORD Address;			/* Replying address. */
   unsigned long  Status;		/* Reply status. */
   unsigned long  RoundTripTime;	/* RTT in milliseconds/ */
   unsigned short DataSize;		/* Echo data size. */
   unsigned short Reserved;		/* Reserved for system use. */
   void *Data;				/* Pointer to the echo data. */
   IP_OPTION_INFORMATION Options;	/* Reply options. */
} IP_ECHO_REPLY, * PIP_ECHO_REPLY;

#define IP_STATUS_BASE	11000
#define IP_SUCCESS	0
#define IP_DEST_PORT_UNREACHABLE (IP_STATUS_BASE + 5)
#define IP_TTL_EXPIRED_TRANSIT (IP_STATUS_BASE + 13)

HANDLE hIP;

/*
 * Functions exported by Microsoft's ICMP.DLL.
 */

HANDLE (WINAPI *pIcmpCreateFile) (VOID);
BOOL (WINAPI *pIcmpCloseHandle) (HANDLE);
DWORD (WINAPI *pIcmpSendEcho) (HANDLE, DWORD, LPVOID, WORD, 
			       PIP_OPTION_INFORMATION, LPVOID, DWORD, DWORD);

/*
 * Structure used to pass the arguments for a single thread:
 */

typedef struct {
    TnmIcmpRequest	*icmpPtr;
    TnmIcmpTarget	*targetPtr;
} IcmpThreadParam;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
IcmpExit	(ClientData clientData);

static DWORD
IcmpThread	(LPDWORD dwParam);


/*
 *----------------------------------------------------------------------
 *
 * IcmpExit --
 *
 *	Cleanup upon exit of the Tnm process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
IcmpExit(ClientData clientData)
{
    HANDLE hIcmp = (HANDLE) clientData;
    FreeLibrary(hIcmp);
}

/*
 *----------------------------------------------------------------------
 *
 * IcmpThread --
 *
 *	Send an ICMP request to a single destination address. This
 *	is done in its own thread so that we can query multiple
 *	hosts in parallel."
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static DWORD
IcmpThread(LPDWORD lpdwParam)
{
    IcmpThreadParam *threadParamPtr = (IcmpThreadParam *) lpdwParam;
    TnmIcmpRequest *icmpPtr = threadParamPtr->icmpPtr;
    TnmIcmpTarget *targetPtr = threadParamPtr->targetPtr;
    DWORD dwStatus;
    PIP_ECHO_REPLY pIpe;
    PIP_OPTION_INFORMATION optInfPtr = NULL;
    char *mem = NULL;
    int i;

    /*
     * We do not support ICMP mask or ICMP timestamp requests.
     */
    
    if (icmpPtr->type == TNM_ICMP_TYPE_MASK 
	|| icmpPtr->type == TNM_ICMP_TYPE_TIMESTAMP) {
	targetPtr->status = TNM_ICMP_STATUS_TIMEOUT;
	goto exit;
    }

    /*
     * Allocate the parameter block which is passed to the
     * ICMP library call.
     */

    mem = ckalloc(sizeof(IP_ECHO_REPLY) + icmpPtr->size);

    for (i = 0; i <= icmpPtr->retries; i++) {

	int timeout = (1000 * icmpPtr->timeout) * (i + 1)
	    / (icmpPtr->retries + 1);
	
#if 0
	{ char buf[80];
	  sprintf(buf, "try %d timeout %d\n", i, timeout);
	  TnmWriteMessage(buf);
	}
#endif
	  
	memset(mem, 0, sizeof(IP_ECHO_REPLY) + icmpPtr->size);

	pIpe = (PIP_ECHO_REPLY) mem;
	pIpe->Data = mem + sizeof(IP_ECHO_REPLY);
	pIpe->DataSize = icmpPtr->size;

	/*
	 * Set the TTL field if we are doing a traceroute step.
	 */
	
	if (icmpPtr->type == TNM_ICMP_TYPE_TRACE) {
	    optInfPtr = (PIP_OPTION_INFORMATION) ckalloc(sizeof(*optInfPtr));
	    memset((void *) optInfPtr, 0, sizeof(IP_OPTION_INFORMATION));
	    optInfPtr->Ttl = icmpPtr->ttl;
	}

	targetPtr->status = TNM_ICMP_STATUS_GENERROR;
	dwStatus = pIcmpSendEcho(hIP, targetPtr->dst.s_addr,
				 pIpe->Data, pIpe->DataSize, optInfPtr,
				 pIpe, sizeof(IP_ECHO_REPLY) + icmpPtr->size,
				 timeout);
	if (dwStatus) {
#if 0
	    { char buf[80];
	      sprintf(buf,
		      "Addr:%d.%d.%d.%d,\tRTT: %dms,\tTTL: %d,\tStatus: %d\n",
		      LOBYTE(LOWORD(pIpe->Address)),
		      HIBYTE(LOWORD(pIpe->Address)),
		      LOBYTE(HIWORD(pIpe->Address)),
		      HIBYTE(HIWORD(pIpe->Address)),
		      pIpe->RoundTripTime,
		      pIpe->Options.Ttl,
		      pIpe->Status);
	      TnmWriteMessage(buf);
	    }
#endif
	    if (pIpe->Status == IP_SUCCESS ||
		pIpe->Status == IP_TTL_EXPIRED_TRANSIT ||
		pIpe->Status == IP_DEST_PORT_UNREACHABLE) {
		targetPtr->status = TNM_ICMP_STATUS_NOERROR;
		targetPtr->res.s_addr = pIpe->Address;
		targetPtr->u.rtt = pIpe->RoundTripTime * 1000;
		if (icmpPtr->type == TNM_ICMP_TYPE_TRACE 
		    && pIpe->Status == IP_DEST_PORT_UNREACHABLE) {
		    targetPtr->flags |= TNM_ICMP_FLAG_LASTHOP;
		}
		break;
	    }
	}
    }

 exit:
    if (mem) ckfree((char *) mem);
    if (threadParamPtr) ckfree((char *) threadParamPtr);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmIcmp --
 *
 *	This procedure is the platform specific entry point for
 *	sending ICMP requests.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmIcmp(Tcl_Interp *interp, TnmIcmpRequest *icmpPtr)
{
    static HANDLE hIcmp = 0;	/* The handle for ICMP.DLL. */
    int j, i, code;
    DWORD nCount, dwStatus;
    HANDLE *lpHandles;

    if (! hIcmp) {
	hIcmp = LoadLibrary("ICMP.DLL");
	if (hIcmp) {
	    Tcl_CreateExitHandler(IcmpExit, hIcmp);
	    (FARPROC) pIcmpCreateFile 
		= (FARPROC) GetProcAddress(hIcmp, "IcmpCreateFile");
	    (FARPROC) pIcmpCloseHandle
		= (FARPROC) GetProcAddress(hIcmp, "IcmpCloseHandle");
	    (FARPROC) pIcmpSendEcho 
		= (FARPROC) GetProcAddress(hIcmp, "IcmpSendEcho");
	    if (! pIcmpCreateFile || ! pIcmpCloseHandle || ! pIcmpSendEcho) {
		FreeLibrary(hIcmp);
		hIcmp = 0;
	    }
	}

    }
    if (! hIcmp) {
	Tcl_SetResult(interp, "failed to load ICMP.DLL", TCL_STATIC);
	return TCL_ERROR;
    }

    hIP = pIcmpCreateFile();
    if (hIP == INVALID_HANDLE_VALUE) {
	Tcl_SetResult(interp, "failed to access ICMP.DLL", TCL_STATIC);
	return TCL_ERROR;
    }

    /*
     * Create a thread for every single target.
     */

    lpHandles = (HANDLE *) ckalloc(icmpPtr->numTargets * sizeof(HANDLE));
    code = TCL_OK;

    j = 0;
    while (j < icmpPtr->numTargets) {
      
	for (i = j, nCount = 0;
	     i < icmpPtr->numTargets && nCount < 200
		 && (! icmpPtr->window || (int) nCount < icmpPtr->window);
	     i++, nCount++) {
	
	    DWORD dwThreadId;
	    TnmIcmpTarget *targetPtr = &(icmpPtr->targets[i]);
	    IcmpThreadParam *threadParamPtr = 
		(IcmpThreadParam *) ckalloc(sizeof(IcmpThreadParam));
	    threadParamPtr->icmpPtr = icmpPtr;
	    threadParamPtr->targetPtr = targetPtr;
	    lpHandles[i] = CreateThread(NULL, 0, 
					(LPTHREAD_START_ROUTINE) IcmpThread,
					(LPDWORD) threadParamPtr, 0,
					&dwThreadId);
	    if (! lpHandles[i]) {
		Tcl_ResetResult(interp);
		Tcl_SetResult(interp, "failed to create ICMP thread",
			      TCL_STATIC);
		code = TCL_ERROR;
		break;
	    }
	}

	/* 
	 * Wait here until all threads have finished, release the handles,
	 * free the handle array and finally move on.
	 */

	dwStatus = WaitForMultipleObjects(nCount, lpHandles+j, TRUE, INFINITE);
	if (dwStatus == WAIT_FAILED) {
	    Tcl_ResetResult(interp);
	    Tcl_SetResult(interp, "failed to wait for ICMP thread",
			  TCL_STATIC);
	    code = TCL_ERROR;
	}
	for (i = j; nCount-- > 0; i++, j++) {
	    if (lpHandles[i]) {
		CloseHandle(lpHandles[i]);
	    }
	}
    }
    ckfree((char *) lpHandles);
    pIcmpCloseHandle(hIP);
    return code;
}

