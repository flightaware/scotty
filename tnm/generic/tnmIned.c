/*
 * tnmIned.c --
 *
 *	Extend a Tcl command interpreter with an ined command. See the
 *	documentation of Tkined for more info about the ined command.
 *	This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1998-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmIned.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * The list that is used to queue received messages.
 */

typedef struct Message {
    char *msg;
    struct Message *nextPtr;
} Message;

static Tcl_Channel tkiChannel = NULL;

/*
 * Every Tcl interpreter has an associated InedControl record. It
 * keeps track of the queue of messages to be processed by the
 * interpreter. The name tnmInedControl is used to get/set the 
 * InedControl record.
 */

static char tnmInedControl[] = "tnmInedControl";

typedef struct InedControl {
    Message *queue;		/* The queue of messages to be processed. */
} InedControl;

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(inedMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static int
InedInitialize	(Tcl_Interp *interp);

static void 
InedFatal	(void);

static void
InedQueue	(Tcl_Interp *interp);

static void
InedFlushProc	(ClientData clientData);

static void
InedFlushQueue	(Tcl_Interp *);

static void 
InedAppendQueue	(Tcl_Interp *interp, char *msg);

static char*
InedGets	(Tcl_Interp *interp);

static int 
InedCompCmd	(char *cmd, Tcl_Interp *interp, 
			     int argc, char **argv);
static void
InedReceiveProc	(ClientData clientData, int mask);

/*
 * The following tkined object type definitions must be in sync 
 * with the tkined sources.
 */

#define TKINED_NONE         0
#define TKINED_ALL          1
#define TKINED_NODE         2
#define TKINED_GROUP        3
#define TKINED_NETWORK      4
#define TKINED_LINK         5
#define TKINED_TEXT         6
#define TKINED_IMAGE        7
#define TKINED_INTERPRETER  8
#define TKINED_MENU         9
#define TKINED_LOG         10
#define TKINED_REFERENCE   11
#define TKINED_STRIPCHART  12
#define TKINED_BARCHART    13
#define TKINED_GRAPH	   14
#define TKINED_HTML	   15
#define TKINED_DATA	   16
#define TKINED_EVENT	   17

static TnmTable tkiTypeTable[] = {
    { TKINED_NONE,	  "NONE" },
    { TKINED_ALL,	  "ALL" },
    { TKINED_NODE,	  "NODE" },
    { TKINED_GROUP,	  "GROUP" },
    { TKINED_NETWORK,	  "NETWORK" },
    { TKINED_LINK,	  "LINK" },
    { TKINED_TEXT,	  "TEXT" },
    { TKINED_IMAGE,	  "IMAGE" },
    { TKINED_INTERPRETER, "INTERPRETER" },
    { TKINED_MENU,	  "MENU" },
    { TKINED_LOG,	  "LOG" },
    { TKINED_REFERENCE,	  "REFERENCE" },
    { TKINED_STRIPCHART,  "STRIPCHART" },
    { TKINED_BARCHART,	  "BARCHART" },
    { TKINED_GRAPH,	  "GRAPH" },
    { TKINED_HTML,	  "HTML" },
    { TKINED_DATA,	  "DATA" },
    { TKINED_EVENT,	  "EVENT" },
    { 0, NULL }
};

/*
 *----------------------------------------------------------------------
 *
 * AssocDeleteProc --
 *
 *	This procedure is called when a Tcl interpreter gets destroyed
 *	so that we can clean up the data associated with this interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
AssocDeleteProc(ClientData clientData, Tcl_Interp *interp)
{
    InedControl *control = (InedControl *) clientData;

    /*
     * Note, we do not care about job objects since Tcl first
     * removes all commands before calling this delete procedure.
     * However, we have to delete the timer to make sure that
     * no further events are processed for this interpreter.
     */

    if (control) {
	Message **p;
	for (p = &(control->queue); *p; ) {
	    Message *m = *p;
	    p = &(*p)->nextPtr;
	    ckfree(m->msg);
	    ckfree((char *) m);
	}
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InedInitialize --
 *
 *	This procedure initializes the ined module. It registers
 *	the stdin channel so that we can send and receive message
 *	to/from the tkined editor. We also modify the auto_path
 *	variable so that applications are found automatically.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The stdin channel is registered in the event loop and the
 *	Tcl auto_path variable is modified.
 *
 *----------------------------------------------------------------------
 */

static int
InedInitialize(Tcl_Interp *interp)
{
    Tcl_Channel channel;
    char *path = NULL, *tmp, *p;
    CONST char *tkiPath;
    CONST char *autoPath;

    tmp = getenv("TNM_INED_TCPPORT");
    if (tmp) {

	/*
	 * Try to connect to Tkined via a TCP channel. Make the 
	 * channel unbuffered.
	 */

	int port = atoi(tmp);
	if (tkiChannel) {
	    Tcl_UnregisterChannel((Tcl_Interp *) NULL, tkiChannel);
	}
	channel = Tcl_OpenTcpClient(interp, port, "localhost", NULL, 0, 0);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetChannelOption(interp, channel, "-buffering", "line");
	tkiChannel = channel;
	Tcl_RegisterChannel((Tcl_Interp *) NULL, tkiChannel);
    } else {

	/*
	 * Make stdin/stdout unbuffered and register a channel handler to
	 * receive commands from the tkined editor. Make sure that an
	 * already existing channel handler is removed first (make wish
	 * happy).
	 */
    
	channel = Tcl_GetChannel(interp, "stdout", NULL);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetChannelOption(interp, channel, "-buffering", "line");
	
	channel = Tcl_GetChannel(interp, "stdin", NULL);
	if (channel == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetChannelOption(interp, channel, "-buffering", "none");
    }

    Tcl_CreateChannelHandler(channel, TCL_READABLE, 
			     InedReceiveProc, (ClientData) interp);
    InedFlushQueue(interp);
    
    /* 
     * Adjust the auto_path to take care of the environment variable
     * TKINED_PATH, $HOME/.tkined and the default Tkined library.
     */

    tkiPath = Tcl_GetVar2(interp, "tkined", "library", TCL_GLOBAL_ONLY);
    
    autoPath = Tcl_GetVar(interp, "auto_path", TCL_GLOBAL_ONLY);
    if (autoPath) {
	path = ckstrdup(autoPath);
    }
    
    Tcl_SetVar(interp, "auto_path", "", TCL_GLOBAL_ONLY);
    
    if ((p = getenv("TKINED_PATH"))) {
	tmp = ckstrdup(p);
	for (p = tmp; *p; p++) {
	    if (*p == ':') {
		*p = ' ';
	    }
	}
	Tcl_SetVar(interp, "auto_path", tmp, TCL_GLOBAL_ONLY);
	ckfree(tmp);
    }
    
    if ((p = getenv("HOME"))) {
	tmp = ckalloc(strlen(p) + 20);
	sprintf(tmp, "%s/.tkined", p);
	Tcl_SetVar(interp, "auto_path", tmp, 
		   TCL_APPEND_VALUE | TCL_LIST_ELEMENT | TCL_GLOBAL_ONLY);
	ckfree(tmp);
    }

    if (tkiPath) {
	tmp = ckalloc(strlen(tkiPath) + 20);
	sprintf(tmp, "%s/site", tkiPath);
	Tcl_SetVar(interp, "auto_path", tmp,
		   TCL_APPEND_VALUE| TCL_LIST_ELEMENT | TCL_GLOBAL_ONLY);
	sprintf(tmp, "%s/apps", tkiPath);
	Tcl_SetVar(interp, "auto_path", tmp,
		   TCL_APPEND_VALUE| TCL_LIST_ELEMENT | TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "auto_path", tkiPath, 
		   TCL_APPEND_VALUE | TCL_LIST_ELEMENT | TCL_GLOBAL_ONLY);
	ckfree(tmp);
    }
    
    if (path) {
	Tcl_SetVar(interp, "auto_path", " ",
		   TCL_APPEND_VALUE | TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "auto_path", path, 
		   TCL_APPEND_VALUE | TCL_GLOBAL_ONLY);
	ckfree(path);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InedFatal --
 *
 *	This procedure handles errors that occur while talking to
 *	tkined. We simply print a warning and exit this process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The process is terminated.
 *
 *----------------------------------------------------------------------
 */

static void
InedFatal()
{
    TnmWriteMessage("Tnm: lost connection to Tkined\n");
    Tcl_Exit(1);
}

/*
 *----------------------------------------------------------------------
 *
 * InedQueue --
 *
 *	This procedure writes a queue message to the tkined editor.
 *	No acknowledge is expected. Queue messages are used to let 
 *	tkined know how busy we are.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
InedQueue(Tcl_Interp *interp)
{
    Tcl_Channel channel;
    char msg[256];
    int len = 0;
    Message *p;

    InedControl *control = (InedControl *)
	Tcl_GetAssocData(interp, tnmInedControl, NULL);

    if (! control) {
	return;
    }

    for (p = control->queue; p != NULL; p = p->nextPtr) len++;

    sprintf(msg, "ined queue %d\n", len);
    len = strlen(msg);

    channel = tkiChannel ? tkiChannel : Tcl_GetChannel(interp, "stdout", NULL);
    if (channel == NULL) {
	InedFatal();
	return;
    }

    if (Tcl_Write(channel, msg, (int) strlen(msg)) < 0) {
	Tcl_Flush(channel);
	InedFatal();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InedFlushProc --
 *
 *	This procedure is called from the event loop to flush the
 *	ined queue. It simply calls InedFlushQueue to do the job.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
InedFlushProc(ClientData clientData)
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    InedFlushQueue(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * InedFlushQueue --
 *
 *	This procedure processes all queued commands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary Tcl commands are evaluated.
 *
 *----------------------------------------------------------------------
 */

static void
InedFlushQueue(Tcl_Interp *interp)
{
    Message *p, *m;
    
    InedControl *control = (InedControl *)
	Tcl_GetAssocData(interp, tnmInedControl, NULL);

    if (! control || ! control->queue) return;

    InedQueue(interp);
    for (p = control->queue; p; ) {
	m = p, p = p->nextPtr;
	if (Tcl_GlobalEval(interp, m->msg) != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	ckfree(m->msg);
	ckfree((char *) m);
    }
    control->queue = NULL;

    InedQueue(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * InedAppendQueue --
 *
 *	This procedure appends the command given by msg to the
 *	queue of commands that need to be processed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
InedAppendQueue(Tcl_Interp *interp, char *msg)
{
    Message *np, *p;
    
    InedControl *control = (InedControl *)
	Tcl_GetAssocData(interp, tnmInedControl, NULL);

    if (msg == NULL) {
	return;
    }

    if (! control) {
	control = (InedControl *) ckalloc(sizeof(InedControl));
	memset((char *) control, 0, sizeof(InedControl));
	Tcl_SetAssocData(interp, tnmInedControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    np = (Message *) ckalloc(sizeof(Message));
    np->msg = msg;
    np->nextPtr = NULL;

    if (control->queue == NULL) {
        control->queue = np;
        return;
    }

    for (p = control->queue; p->nextPtr; p = p->nextPtr) ;
    p->nextPtr = np;

    InedQueue(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * InedGets --
 *
 *	This procedure reads a message from the Tkined editor.
 *
 * Results:
 *	A pointer to a malloced buffer containing the received 
 *	message or NULL on EOF.
 *
 * Side effects:
 *	Causes on Error an exit() via InedFatal().
 *
 *----------------------------------------------------------------------
 */

static char*
InedGets(Tcl_Interp *interp)
{
    Tcl_Channel channel;
    Tcl_DString line;
    char *buffer = NULL;
    int len;

    channel = tkiChannel ? tkiChannel : Tcl_GetChannel(interp, "stdin", NULL);
    if (channel == NULL) {
	InedFatal();
	/* not reached */
	return NULL;
    }

    Tcl_DStringInit(&line);
    len = Tcl_Gets(channel, &line);
    if (len < 0 && Tcl_Eof(channel)) {
	return NULL;
    }

    if (len < 0) {
	InedFatal();
	/* not reached */
	return NULL;
    }

    buffer = ckstrdup(Tcl_DStringValue(&line));
    Tcl_DStringFree(&line);
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * InedCompCmd --
 *
 *	This procedure checks if we can evaluate a command based on
 *	the Tcl list representation of a tkined object. This allows
 *	to save some interactions between scotty and tkined.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
InedCompCmd(char *cmd, Tcl_Interp *interp, int argc, char **argv)
{
    int type = TnmGetTableKey(tkiTypeTable, argv[0]);
    if (type < 0 || (type == TKINED_NONE) || (type == TKINED_ALL)) {
	return TCL_ERROR;
    }

    if ((strcmp(cmd, "type") == 0) && (argc > 0)) {
	Tcl_SetResult(interp, argv[0], TCL_VOLATILE);
	return TCL_OK;

    } else if ((strcmp(cmd, "id") == 0) && (argc > 1)) {
	Tcl_SetResult(interp, argv[1], TCL_VOLATILE);
	return TCL_OK;

    } else if ((strcmp(cmd, "name") == 0) && (argc > 2)) {
        if ((type == TKINED_NODE) || (type == TKINED_NETWORK)
	    || (type == TKINED_BARCHART) || (type == TKINED_STRIPCHART)
	    || (type == TKINED_GROUP) || (type == TKINED_REFERENCE)
	    || (type == TKINED_MENU) || (type == TKINED_LOG) 
	    || (type == TKINED_GRAPH) || (type == TKINED_HTML)
	    || (type == TKINED_DATA) || (type == TKINED_EVENT) )
	    Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
        return TCL_OK;

    } else if ((strcmp(cmd, "address") == 0) && (argc > 3)) {
        if ((type == TKINED_NODE) || (type == TKINED_NETWORK)
	    || (type == TKINED_BARCHART) || (type == TKINED_STRIPCHART)
	    || (type == TKINED_REFERENCE) || (type == TKINED_GRAPH)
	    || (type == TKINED_DATA))
	    Tcl_SetResult(interp, argv[3], TCL_VOLATILE);
        return TCL_OK;

    } else if (strcmp(cmd, "oid") == 0) {
        if ((type == TKINED_GROUP) && (argc > 3)) {
	    Tcl_SetResult(interp, argv[3], TCL_VOLATILE);
	}
        if ((type == TKINED_NODE || type == TKINED_NETWORK) && (argc > 4)) {
	    Tcl_SetResult(interp, argv[4], TCL_VOLATILE);
	}
	return TCL_OK;

    } else if ((strcmp(cmd, "links") == 0) && (argc > 5)) {
        if ((type == TKINED_NODE) || (type == TKINED_NETWORK))
	    Tcl_SetResult(interp, argv[5], TCL_VOLATILE);
        return TCL_OK;

    } else if ((strcmp(cmd, "member") == 0) && (argc > 4)) {
        if (type == TKINED_GROUP)
	    Tcl_SetResult(interp, argv[4], TCL_VOLATILE);
        return TCL_OK;

    } else if ((strcmp(cmd, "src") == 0) && (argc > 2)) {
        if (type == TKINED_LINK)
	    Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
        return TCL_OK;

    } else if ((strcmp(cmd, "dst") == 0) && (argc > 3)) {
        if (type == TKINED_LINK)
	    Tcl_SetResult(interp, argv[3], TCL_VOLATILE);
        return TCL_OK;

    } else if ((strcmp(cmd, "text") == 0) && (argc > 2)) {
        if (type == TKINED_LINK)
	    Tcl_SetResult(interp, argv[2], TCL_VOLATILE);
        return TCL_OK;

    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_InedObjCmd --
 *
 *	This procedure is invoked to process the "ined" command.
 *	See the user documentation for details on what it does.
 *
 *	We send the command to the tkined editor and waits for a
 *	response containing the answer or the error description.
 *	Everything received while waiting for the response is
 *	queued for later execution.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tnm_InedObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Channel channel;
    int i;
    char *p;
    static int initialized = 0;

    Tcl_MutexLock(&inedMutex);
    if (! initialized) {
	if (InedInitialize(interp) != TCL_OK) {
	    Tcl_MutexUnlock(&inedMutex);
	    return TCL_ERROR;
	}
	initialized = 1;
    }
    Tcl_MutexUnlock(&inedMutex);

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command ?arg ...?");
	return TCL_ERROR;
    }

    /* 
     * Check for commands that can be implemented locally (based on 
     * the list representation of tkined objects).
     */

    if (objc == 3) {
        int largc;
	char **largv;
        int rc = Tcl_SplitList(interp, Tcl_GetStringFromObj(objv[2], NULL),
			       &largc, &largv);
	if (rc == TCL_OK && largc > 0) {
	    if (InedCompCmd(Tcl_GetStringFromObj(objv[1], NULL),
			    interp, largc, largv) == TCL_OK) {
		ckfree((char *) largv);
		return TCL_OK;
	    }
 	    ckfree((char *) largv);
	}
    }

    /*
     * Write the command string to the tkined editor.
     */

    Tcl_MutexUnlock(&inedMutex);
    
    channel = tkiChannel ? tkiChannel : Tcl_GetChannel(interp, "stdout", NULL);
    if (channel == NULL) {
	Tcl_MutexUnlock(&inedMutex);
	InedFatal();
	/* not reached */
	return TCL_ERROR;
    }

    for (i = 0; i < objc; i++) {
	if (Tcl_Write(channel, "{", 1) < 0) {
	    Tcl_MutexUnlock(&inedMutex);
	    InedFatal();
	}
        for (p = Tcl_GetStringFromObj(objv[i], NULL); *p; p++) {
	    if (*p == '\r') {
		continue;
	    } else if (*p == '\n') {
	        if (Tcl_Write(channel, "\\n", 2) < 0) {
		    Tcl_MutexUnlock(&inedMutex);
		    InedFatal();
		}
	    } else {
	        if (Tcl_Write(channel, p, 1) < 0) {
		    Tcl_MutexUnlock(&inedMutex);
		    InedFatal();
		}
	    }
	}
        if (Tcl_Write(channel, "} ", 2) < 0) {
	    Tcl_MutexUnlock(&inedMutex);
	    InedFatal();
	}
    }
    if (Tcl_Write(channel, "\n", 1) < 0) {
	Tcl_MutexUnlock(&inedMutex);
	InedFatal();
    } 
    Tcl_Flush(channel);
 
    /*
     * Wait for the response.
     */

    channel = tkiChannel ? tkiChannel : Tcl_GetChannel(interp, "stdin", NULL);
    if (channel == NULL) {
	Tcl_MutexUnlock(&inedMutex);
	InedFatal();
	/* not reached */
	return TCL_ERROR;
    }

    while ((p = InedGets(interp)) != (char *) NULL) {
        if (*p == '\0') continue;
	if (strncmp(p, "ined ok", 7) == 0) {
	    char *r = p+7;
	    while (*r && isspace(*r)) r++;
	    Tcl_SetResult(interp, r, TCL_VOLATILE);
	    ckfree(p);
	    Tcl_MutexUnlock(&inedMutex);
	    return TCL_OK;
	} else if (strncmp(p, "ined error", 10) == 0) {
	    char *r = p+10;
	    while (*r && isspace(*r)) r++;
	    Tcl_SetResult(interp, r, TCL_VOLATILE);
	    ckfree(p);
	    Tcl_MutexUnlock(&inedMutex);
	    return TCL_ERROR;
	} else {
	    InedAppendQueue(interp, p);
	    Tcl_CreateTimerHandler(0, InedFlushProc, (ClientData) interp);
	}
    }

    Tcl_MutexUnlock(&inedMutex);

    /* EOF reached */
    Tcl_Exit(1);

    /* not reached */
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InedReceiveProc --
 *
 *	This procedure is called from the event handler whenever
 *	a command can be read from the Tkined editor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
InedReceiveProc (ClientData clientData, int mask)
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    char *cmd = InedGets(interp);

    if (! cmd) {
        /* EOF reached */
        Tcl_Exit(1);
    }

    Tcl_MutexLock(&inedMutex);
    InedAppendQueue(interp, cmd);
    Tcl_MutexUnlock(&inedMutex);
    InedFlushQueue(interp);
}

