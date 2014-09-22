/*
 * tnmWinLog.c --
 *
 *	Windows specific functions to write to the system logging facility.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#include <windows.h>


/*
 *----------------------------------------------------------------------
 *
 * TnmWriteLogMessage --
 *
 *	This procedure is invoked to write a message to the Windows 
 *	system logging facility. The UNIX specific logging type is
 *	converted to one of the three Windows event types.
 *
 * 	Note, Windows NT has a far more sophisticated event logging
 *	system than UNIX. But we can't make use of all the good stuff
 *	because we would need system administrator priviledges for
 *	this. See the Byte article "Windows NT Event Logging" (April
 *	1996) for an example how to hack these things.
 *
 * Results:
 *	0 on success and -1 on failure.
 *
 * Side effects:
 *	A message is written to the system logging facility.
 *
 *----------------------------------------------------------------------
 */

int
TnmWriteLogMessage(char *ident, int level, int facility, char *message)
{
    HANDLE ed;
    char *msgList[1];
    WORD type;
    WORD category = facility;
    DWORD id = getpid();

    switch (level) {
      case TNM_LOG_EMERG:
      case TNM_LOG_ALERT:
      case TNM_LOG_CRIT:
      case TNM_LOG_ERR:
	type = EVENTLOG_ERROR_TYPE;
	break;
      case TNM_LOG_WARNING:
      case TNM_LOG_NOTICE:
	type = EVENTLOG_WARNING_TYPE;
	break;
      case TNM_LOG_INFO:
      case TNM_LOG_DEBUG:
	type = EVENTLOG_INFORMATION_TYPE;
	break;
      default:
	return -1;
    }

    if (! ident) {
	ident = "scotty";
    }

    ed = RegisterEventSource(NULL, ident);
    if (ed != NULL) {
	msgList[0] = message;
	ReportEvent(ed, type, category, id, NULL, 1, 0, msgList, NULL);
	DeregisterEventSource(ed);
    }

    return 0;
}

