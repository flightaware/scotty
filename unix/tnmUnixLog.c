/*
 * tnmUnixLog.c --
 *
 *	UNIX specific functions to write to the syslog facility.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#include <syslog.h>


/*
 *----------------------------------------------------------------------
 *
 * TnmWriteLogMessage --
 *
 *	This procedure is invoked to write a message to the UNIX 
 *	syslog facility.
 *
 * Results:
 *	0 on success and -1 on failure.
 *
 * Side effects:
 *	A message is written to the syslog facility.
 *
 *----------------------------------------------------------------------
 */

int
TnmWriteLogMessage(ident, level, facility, message)
    char *ident;
    int level;
    int facility;
    char *message;
{
    switch (level) {
      case TNM_LOG_EMERG:
	level = LOG_EMERG; 
	break;
      case TNM_LOG_ALERT:
	level = LOG_ALERT;
	break;
      case TNM_LOG_CRIT:
	level = LOG_CRIT;
	break;
      case TNM_LOG_ERR:
	level = LOG_ERR;
	break;
      case TNM_LOG_WARNING:
	level = LOG_WARNING;
	break;
      case TNM_LOG_NOTICE:
	level = LOG_NOTICE;
	break;
      case TNM_LOG_INFO:
	level = LOG_INFO;
	break;
      case TNM_LOG_DEBUG:
	level = LOG_DEBUG;
	break;
      default:
	return -1;
    }

    switch (facility) {
      case TNM_LOG_KERN:
        facility = LOG_KERN;
	break;
      case TNM_LOG_USER:
        facility = LOG_USER;
	break;
      case TNM_LOG_MAIL:
        facility = LOG_MAIL;
	break;
      case TNM_LOG_DAEMON:
        facility = LOG_DAEMON;
	break;
      case TNM_LOG_AUTH:
        facility = LOG_AUTH;
	break;
      case TNM_LOG_SYSLOG:
        facility = LOG_SYSLOG;
	break;
      case TNM_LOG_LPR:
        facility = LOG_LPR;
	break;
      case TNM_LOG_NEWS:
        facility = LOG_NEWS;
	break;
      case TNM_LOG_UUCP:
        facility = LOG_UUCP;
	break;
      case TNM_LOG_CRON:
        facility = LOG_CRON;
	break;
      case TNM_LOG_AUTHPRIV:
#ifdef LOG_AUTHPRIV
        facility = LOG_AUTHPRIV;
#else
	facility = LOG_AUTH;
#endif
	break;
      case TNM_LOG_FTP:
#ifdef LOG_FTP
        facility = LOG_FTP;
#else
	facility = LOG_DAEMON;
#endif
	break;
      case TNM_LOG_NTP:
#ifdef LOG_NTP
        facility = LOG_NTP;
#else
        facility = LOG_DAEMON;
#endif
	break;
      case TNM_LOG_AUDITPRIV:
#ifdef LOG_AUDITPRIV
        facility = LOG_AUDITPRIV;
#else
        facility = LOG_AUTH;
#endif
	break;
      case TNM_LOG_ALERTPRIV:
#ifdef LOG_ALERTPRIV
        facility = LOG_ALERTPRIV;
#else
        facility = LOG_AUTH;
#endif
	break;
      case TNM_LOG_CRONPRIV:
#ifdef LOG_ALERTPRIV
        facility = LOG_CRONPRIV;
#else
        facility = LOG_CRON;
#endif
	break;
      case TNM_LOG_LOCAL0:
        facility = LOG_LOCAL0;
	break;
      case TNM_LOG_LOCAL1:
        facility = LOG_LOCAL1;
	break;
      case TNM_LOG_LOCAL2:
        facility = LOG_LOCAL2;
	break;
      case TNM_LOG_LOCAL3:
        facility = LOG_LOCAL3;
	break;
      case TNM_LOG_LOCAL4:
        facility = LOG_LOCAL4;
	break;
      case TNM_LOG_LOCAL5:
        facility = LOG_LOCAL5;
	break;
      case TNM_LOG_LOCAL6:
        facility = LOG_LOCAL6;
	break;
      case TNM_LOG_LOCAL7:
        facility = LOG_LOCAL7;
	break;
    default:
	return -1;
    }

    if (! ident) {
	ident = "scotty";
    }

    if (message != NULL) {
#ifdef ultrix
	openlog(ident, LOG_PID);
#else
	openlog(ident, LOG_PID, facility);
#endif
	syslog(level, message);
	closelog();
    }

    return 0;
}
