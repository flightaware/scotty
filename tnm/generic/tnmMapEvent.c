/*
 * tnmMapEvent.c --
 *
 *	This file implements map events, bindings and messages.
 *	This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMapEvent.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(mapEventMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
EventDeleteProc	(ClientData clientData);

static void
BindDeleteProc	(ClientData clientData);

static void
MsgDeleteProc	(ClientData clientData);

static int 
EventObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int 
BindObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int
EvalBinding	(TnmMapEvent *eventPtr, TnmMapBind *bindList);

static int
SaveMsg		(TnmMapMsg *msgPtr);

static int 
MsgObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);

/*
 * The following table maps internal events to strings.
 */

static TnmTable eventTable[] = {
    { TNM_MAP_CREATE_EVENT,	"<CreateItem>" },
    { TNM_MAP_DELETE_EVENT,	"<DeleteItem>" },
    { TNM_MAP_CONFIGURE_EVENT,	"<ConfigureItem>" },
    { TNM_MAP_NAME_EVENT,	"<NameItem>" },
    { TNM_MAP_ADDRESS_EVENT,	"<AddressItem>" },
    { TNM_MAP_MOVE_EVENT,	"<MoveItem>" },
    { TNM_MAP_COLLAPSE_EVENT,	"<CollapseItem>" },
    { TNM_MAP_EXPAND_EVENT,	"<ExpandItem>" },
    { TNM_MAP_ATTRIBUTE_EVENT,	"<AttributeItem>" },
    { TNM_MAP_LABEL_EVENT,	"<LabelItem>" },
    { 0, NULL }
};


/*
 *----------------------------------------------------------------------
 *
 * EventDeleteProc --
 *
 *	This procedure is invoked to destroy an event object.
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
EventDeleteProc(clientData)
    ClientData clientData;
{
    TnmMapEvent **eventPtrPtr;
    TnmMapEvent *eventPtr = (TnmMapEvent *) clientData;

    /*
     * Update the event lists that reference this event.
     */

    if (eventPtr->itemPtr) {
	eventPtrPtr = &eventPtr->itemPtr->eventList;
	while (*eventPtrPtr && (*eventPtrPtr) != eventPtr) {
	    eventPtrPtr = &(*eventPtrPtr)->nextPtr;
	}
	if (*eventPtrPtr) {
	    (*eventPtrPtr) = eventPtr->nextPtr;
	}
    }

    if (eventPtr->mapPtr) {
	eventPtrPtr = &eventPtr->mapPtr->eventList;
	while (*eventPtrPtr && (*eventPtrPtr) != eventPtr) {
	    eventPtrPtr = &(*eventPtrPtr)->nextPtr;
	}
	if (*eventPtrPtr) {
	    (*eventPtrPtr) = eventPtr->nextPtr;
	}
    }

    ckfree((char *) eventPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapCreateUserEvent --
 *
 *	This procedure is called to create user events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMapEvent*
TnmMapCreateUserEvent(mapPtr, itemPtr, name, args)
    TnmMap *mapPtr;
    TnmMapItem *itemPtr;
    char *name;
    char *args;
{
    size_t size;
    TnmMapEvent *eventPtr;
    static unsigned nextId = 0;

    size = sizeof(TnmMapEvent) + strlen(name) + 1;
    size += (name) ? strlen(name) + 1 : 0;
    size += (args) ? strlen(args) + 1 : 0;

    eventPtr = (TnmMapEvent *) ckalloc(size);
    memset((char *) eventPtr, 0, size);

    eventPtr->type = TNM_MAP_USER_EVENT | TNM_MAP_EVENT_QUEUE;
    Tcl_GetTime(&eventPtr->eventTime);
    if (itemPtr) {
	eventPtr->itemPtr = itemPtr;
	eventPtr->mapPtr = itemPtr->mapPtr;
	eventPtr->interp = itemPtr->mapPtr->interp;
    }
    if (mapPtr && ! eventPtr->mapPtr) {
	eventPtr->mapPtr = mapPtr;
	eventPtr->interp = mapPtr->interp;
    }

    eventPtr->eventName = (char *) eventPtr + sizeof(TnmMapEvent);
    strcpy(eventPtr->eventName, name);
    if (args) {
	eventPtr->eventData = eventPtr->eventName + strlen(name) + 1;
	strcpy(eventPtr->eventData, args);
    }

    /*
     * Create a new Tcl command for this event object.
     */

    if (eventPtr->interp) {
	/* Don't worry: TnmGetHandle() is thread-safe... */
	char *name = TnmGetHandle(eventPtr->interp, "event", &nextId);
	eventPtr->token = Tcl_CreateObjCommand(eventPtr->interp, name, 
			  EventObjCmd, (ClientData) eventPtr, EventDeleteProc);
	Tcl_SetResult(eventPtr->interp, name, TCL_STATIC);
    }
    
    return eventPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapCreateEvent --
 *
 *	This procedure is called to create an event.
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
TnmMapCreateEvent(type, itemPtr, eventData)
    int type;
    TnmMapItem *itemPtr;
    char *eventData;
{
    TnmMapEvent event, *eventPtr = &event;
    char *eventName;

    /*
     * Ignore requests for events that are not known in the event table.
     */

    eventName = TnmGetTableValue(eventTable, (unsigned) type);
    if (eventName == NULL) {
	return;
    }

    /*
     * Fill out an event structure and raise the event. Note that the
     * TNM_MAP_EVENT_QUEUE bit is not set. It is therefore save to
     * allocate the event structure on the stack.
     */

    memset((char *) eventPtr, 0, sizeof(TnmMapEvent));
    eventPtr->type = type;
    eventPtr->mapPtr = itemPtr->mapPtr;
    eventPtr->itemPtr = itemPtr;
    eventPtr->eventName = eventName;
    Tcl_GetTime(&eventPtr->eventTime);
    TnmMapRaiseEvent(eventPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapRaiseEvent --
 *
 *	This procedure is called to raise an event. The event is 
 *	queued in the event history if this is desired. All the
 *	Tcl scripts bound to the event are evaluated if it is a
 *	TNM_MAP_USER_EVENT.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An event to be queued in the event history must be allocated
 *	using malloc. It is owned by the event queue code and freed
 *	using ckfree() if it is not needed anymore.
 *
 *----------------------------------------------------------------------
 */

void
TnmMapRaiseEvent(eventPtr)
    TnmMapEvent *eventPtr;
{
    TnmMapItem *itemPtr;
    TnmMap *mapPtr;
    int code;

    if (eventPtr->type & TNM_MAP_EVENT_QUEUE) {
	if (eventPtr->itemPtr) {
	    eventPtr->nextPtr = eventPtr->itemPtr->eventList;
	    eventPtr->itemPtr->eventList = eventPtr;
	} else if (eventPtr->mapPtr) {
	    eventPtr->nextPtr = eventPtr->mapPtr->eventList;
	    eventPtr->mapPtr->eventList = eventPtr;
	} else {
	    ckfree((char *) eventPtr);
	    return;
	}
    }

    if ((eventPtr->type & TNM_MAP_EVENT_MASK) == TNM_MAP_USER_EVENT) {

	for (itemPtr = eventPtr->itemPtr; itemPtr; itemPtr = itemPtr->parent) {
	    mapPtr = itemPtr->mapPtr;
	    code = EvalBinding(eventPtr, itemPtr->bindList);
	    if (code == TCL_BREAK) {
		return;
	    }
	}

	EvalBinding(eventPtr, eventPtr->mapPtr->bindList);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapExpireEvents --
 *
 *	This procedure removes all events from an eventList which
 *	are older than expireTime.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The eventList is modified.
 *
 *----------------------------------------------------------------------
 */

void
TnmMapExpireEvents(eventListPtr, expireTime)
    TnmMapEvent **eventListPtr;
    long expireTime;
{
    TnmMapEvent *evtPtr;

nextEvent:
    for (evtPtr = *eventListPtr; evtPtr; evtPtr = evtPtr->nextPtr) {
	if (evtPtr->token && evtPtr->interp 
	    && evtPtr->eventTime.sec < expireTime) {
	    Tcl_DeleteCommandFromToken(evtPtr->interp, evtPtr->token);
	    goto nextEvent;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * EventObjCmd --
 *
 *	This procedure implements the event object command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
EventObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    TnmMapEvent *eventPtr = (TnmMapEvent *) clientData;
    char buffer[20];
    int result;

    enum commands {
	cmdArgs, cmdDestroy, cmdItem, cmdMap, cmdTag, cmdTime, cmdType
    } cmd;

    static CONST char *cmdTable[] = {
	"args",	"destroy", "item", "map", "tag", "time", "type", (char *) NULL
    };

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdArgs:
	if (eventPtr->eventData) {
	    Tcl_SetResult(interp, eventPtr->eventData, TCL_STATIC);
	}
	break;
    case cmdDestroy:
	if (eventPtr) {
	    Tcl_DeleteCommandFromToken(interp, eventPtr->token);
	}
	break;
    case cmdItem:
	if (eventPtr->itemPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, eventPtr->itemPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdMap:
	if (eventPtr->mapPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, eventPtr->mapPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdTag:
	if (eventPtr->eventName) {
	    Tcl_SetResult(interp, eventPtr->eventName, TCL_STATIC);
	}
	break;
    case cmdTime:
	sprintf(buffer, "%lu", eventPtr->eventTime.sec);
	Tcl_SetResult(interp, buffer, TCL_VOLATILE);
	break;
    case cmdType:
	Tcl_SetResult(interp, "event", TCL_STATIC);
	break;
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * BindDeleteProc --
 *
 *	This procedure is invoked to destroy an binding object.
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
BindDeleteProc(clientData)
    ClientData clientData;
{
    TnmMapBind **bindPtrPtr;
    TnmMapBind *bindPtr = (TnmMapBind *) clientData;

    /*
     * Update the bind lists that reference this event.
     */

    if (bindPtr->itemPtr) {
	bindPtrPtr = &bindPtr->itemPtr->bindList;
	while (*bindPtrPtr && (*bindPtrPtr) != bindPtr) {
	    bindPtrPtr = &(*bindPtrPtr)->nextPtr;
	}
	if (*bindPtrPtr) {
	    (*bindPtrPtr) = bindPtr->nextPtr;
	}
    }

    if (bindPtr->mapPtr) {
	bindPtrPtr = &bindPtr->mapPtr->bindList;
	while (*bindPtrPtr && (*bindPtrPtr) != bindPtr) {
	    bindPtrPtr = &(*bindPtrPtr)->nextPtr;
	}
	if (*bindPtrPtr) {
	    (*bindPtrPtr) = bindPtr->nextPtr;
	}
    }

    ckfree((char *) bindPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapUserBinding --
 *
 *	This procedure creates a new event binding of type
 *	TNM_MAP_USER_EVENT.
 *
 * Results:
 *	A pointer to the new TnmMapBind object or NULL if the 
 *	parameters are not valid.
 *
 * Side effects:
 *	The list of bindings of the map or the item is modified.
 *
 *----------------------------------------------------------------------
 */

TnmMapBind*
TnmMapUserBinding(mapPtr, itemPtr, pattern, script)
    TnmMap *mapPtr;
    TnmMapItem *itemPtr;
    char *pattern;
    char *script;
{
    TnmMapBind *bindPtr;
    size_t size;
    static unsigned nextId = 0;

    if (mapPtr == NULL && itemPtr == NULL) {
	return NULL;
    }

    size = sizeof(TnmMapBind) + strlen(pattern) + strlen(script) + 2;
    bindPtr = (TnmMapBind *) ckalloc(size);
    memset((char *) bindPtr, 0, size);

    bindPtr->type = TNM_MAP_USER_EVENT;
    bindPtr->mapPtr = mapPtr;
    bindPtr->itemPtr = itemPtr;
    if (mapPtr) {
	bindPtr->interp = mapPtr->interp;
    }
    if (itemPtr) {
	bindPtr->interp = itemPtr->mapPtr->interp;
    }
    bindPtr->pattern = (char *) bindPtr + sizeof(TnmMapBind);
    strcpy(bindPtr->pattern, pattern);
    bindPtr->bindData = bindPtr->pattern + strlen(bindPtr->pattern) + 1;
    strcpy(bindPtr->bindData, script);

    /*
     * Create a new Tcl command for this bind object.
     */

    if (bindPtr->interp) {
	/* Don't worry: TnmGetHandle() is thread-safe... */
	char *name = TnmGetHandle(bindPtr->interp, "bind", &nextId);
	bindPtr->token = Tcl_CreateObjCommand(bindPtr->interp, name,
			     BindObjCmd, (ClientData) bindPtr, BindDeleteProc);
	Tcl_SetResult(bindPtr->interp, name, TCL_STATIC);
    }

    /*
     * Put the new binding into the bind list of the requesting object.
     */

    if (itemPtr) {
	bindPtr->nextPtr = itemPtr->bindList;
	itemPtr->bindList = bindPtr;
    } else if (mapPtr) {
	bindPtr->nextPtr = mapPtr->bindList;
        mapPtr->bindList = bindPtr;
    }
    
    return bindPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * EvalBinding --
 *
 *	This procedure evaluates a Tcl binding for a map event. It
 *	substitues the % escape sequences as described in the user
 *	documentation.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Arbitrary Tcl commands are evaluated.
 *
 *----------------------------------------------------------------------
 */

static int
EvalBinding(eventPtr, bindList)
    TnmMapEvent *eventPtr;
    TnmMapBind *bindList;
{
    TnmMapBind *bindPtr;
    char buf[20];
    int	code;
    Tcl_DString tclCmd;
    char *startPtr, *scanPtr;
    Tcl_Interp *interp;

    if (!eventPtr->mapPtr || !eventPtr->mapPtr->interp) {
	return TCL_OK;
    }

    interp = eventPtr->mapPtr->interp;
    Tcl_Preserve((ClientData) interp);

    Tcl_DStringInit(&tclCmd);

    for (bindPtr = bindList; bindPtr; bindPtr = bindPtr->nextPtr) {

	if (bindPtr->type != (eventPtr->type & TNM_MAP_EVENT_MASK) || 
	    Tcl_StringMatch(eventPtr->eventName, bindPtr->pattern) == 0) {
	    continue;
	}

	startPtr = bindPtr->bindData;
	for (scanPtr = startPtr; *scanPtr != '\0'; scanPtr++) {
	    if (*scanPtr != '%') {
		continue;
	    }
	    Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);
	    scanPtr++;
	    startPtr = scanPtr + 1;
	    switch (*scanPtr) {
	    case 'M':
		if (eventPtr->mapPtr) {
		    Tcl_DStringAppend(&tclCmd,
		      Tcl_GetCommandName(interp, 
					 eventPtr->mapPtr->token), -1);
		}
		break;
	    case 'I':
		if (eventPtr->itemPtr) {
		    Tcl_DStringAppend(&tclCmd,
		      Tcl_GetCommandName(interp, 
					 eventPtr->itemPtr->token), -1);
		}
		break;
	    case 'N':
		Tcl_DStringAppend(&tclCmd, eventPtr->eventName, -1);
		break;
	    case 'E':
		if (eventPtr->token) {
		    Tcl_DStringAppend(&tclCmd,
				      Tcl_GetCommandName(interp, 
							 eventPtr->token), -1);
		}
		break;
	    case 'P':
		Tcl_DStringAppend(&tclCmd, bindPtr->pattern, -1);
		break;
	    case 'A':
		if (eventPtr->eventData) {
		    Tcl_DStringAppend(&tclCmd, eventPtr->eventData, -1);
		}
		break;
	    case 'B':
		if (bindPtr->token) {
		    Tcl_DStringAppend(&tclCmd,
				      Tcl_GetCommandName(interp, 
							 bindPtr->token), -1);
		}
		break;
	    case '%':
		Tcl_DStringAppend(&tclCmd, "%", -1);
		break;
	    default:
		sprintf(buf, "%%%c", *scanPtr);
		Tcl_DStringAppend(&tclCmd, buf, -1);
	    }
	}
	Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);

	/*
	 * Now evaluate the callback function and issue a background
	 * error if the callback fails for some reason. Return the
	 * original error message and code to the caller.
	 */
	
	Tcl_AllowExceptions(interp);
	code = Tcl_GlobalEval(interp, Tcl_DStringValue(&tclCmd));
	Tcl_DStringFree(&tclCmd);

	switch (code) {
	case TCL_BREAK:
	    Tcl_Release((ClientData) interp);
	    return TCL_BREAK;
	case TCL_CONTINUE:
	    Tcl_Release((ClientData) interp);
	    return TCL_OK;
	case TCL_ERROR: 
	    {
		char *errorMsg = ckstrdup(Tcl_GetStringResult(interp));
		Tcl_AddErrorInfo(interp, "\n    (");
		if (bindPtr->itemPtr) {
		    Tcl_AddErrorInfo(interp, 
			 Tcl_GetCommandName(interp, bindPtr->itemPtr->token));
		} else {
		    Tcl_AddErrorInfo(interp, 
			 Tcl_GetCommandName(interp, bindPtr->mapPtr->token));
		}
		Tcl_AddErrorInfo(interp, " event binding ");
		Tcl_AddErrorInfo(interp, 
				 Tcl_GetCommandName(interp, bindPtr->token));
		Tcl_AddErrorInfo(interp, ")");
		Tcl_BackgroundError(interp);
		Tcl_SetResult(interp, errorMsg, TCL_DYNAMIC);
		Tcl_Release((ClientData) interp);
		return TCL_ERROR;
	    }
	}
    }

    Tcl_Release((ClientData) interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BindObjCmd --
 *
 *	This procedure implements the bind object command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
BindObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[]; 
{
    TnmMapBind *bindPtr = (TnmMapBind *) clientData;
    int result;

    enum commands {
	cmdDestroy, cmdItem, cmdMap, cmdPattern, cmdScript, cmdType
    } cmd;

    static CONST char *cmdTable[] = {
	"destroy", "item", "map", "pattern", "script", "type", (char *) NULL
    };

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdDestroy:
	if (bindPtr) {
	    Tcl_DeleteCommandFromToken(interp, bindPtr->token);
	}
	break;
    case cmdItem:
	if (bindPtr->itemPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, bindPtr->itemPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdMap:
	if (bindPtr->mapPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, bindPtr->mapPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdPattern:
	Tcl_SetResult(interp, bindPtr->pattern, TCL_STATIC);
	break;
    case cmdScript:
	Tcl_SetResult(interp, bindPtr->bindData, TCL_STATIC);
	break;
    case cmdType:
	Tcl_SetResult(interp, "binding", TCL_STATIC);
	break;
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * MsgDeleteProc --
 *
 *	This procedure is invoked to destroy a message object.
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
MsgDeleteProc(clientData)
    ClientData clientData;
{
    TnmMapMsg **msgPtrPtr;
    TnmMapMsg *msgPtr = (TnmMapMsg *) clientData;

    /*
     * Update the message lists that reference this event.
     */

    if (msgPtr->itemPtr) {
	msgPtrPtr = &msgPtr->itemPtr->msgList;
	while (*msgPtrPtr && (*msgPtrPtr) != msgPtr) {
	    msgPtrPtr = &(*msgPtrPtr)->nextPtr;
	}
	if (*msgPtrPtr) {
	    (*msgPtrPtr) = msgPtr->nextPtr;
	}
    }

    if (msgPtr->mapPtr) {
	msgPtrPtr = &msgPtr->mapPtr->msgList;
	while (*msgPtrPtr && (*msgPtrPtr) != msgPtr) {
	    msgPtrPtr = &(*msgPtrPtr)->nextPtr;
	}
	if (*msgPtrPtr) {
	    (*msgPtrPtr) = msgPtr->nextPtr;
	}
    }

    Tcl_DecrRefCount(msgPtr->msg);
    Tcl_DecrRefCount(msgPtr->tag);
    ckfree((char *) msgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapCreateMsg --
 *
 *	This procedure is called to add a new message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMapMsg*
TnmMapCreateMsg(mapPtr, itemPtr, tag, message)
    TnmMap *mapPtr;
    TnmMapItem *itemPtr;
    Tcl_Obj *tag;
    Tcl_Obj *message;
{
    size_t size;
    TnmMapMsg *msgPtr;
    static unsigned nextId = 0;

    size = sizeof(TnmMapMsg);
    msgPtr = (TnmMapMsg *) ckalloc(size);
    memset((char *) msgPtr, 0, size);

    Tcl_GetTime(&msgPtr->msgTime);
    msgPtr->mapPtr = mapPtr;
    msgPtr->itemPtr = itemPtr;
    if (mapPtr) {
	msgPtr->interp = mapPtr->interp;
    }
    if (itemPtr) {
	msgPtr->interp = itemPtr->mapPtr->interp;
    }
    msgPtr->tag = tag;
    Tcl_IncrRefCount(msgPtr->tag);
    msgPtr->msg = message;
    Tcl_IncrRefCount(msgPtr->msg);

    if (itemPtr) {
	msgPtr->nextPtr = itemPtr->msgList;
	itemPtr->msgList = msgPtr;
    } else {
	msgPtr->nextPtr = mapPtr->msgList;
	mapPtr->msgList = msgPtr;
    }

    /*
     * Create a new Tcl command for this message object.
     */

    if (msgPtr->interp) {
	/* Don't worry: TnmGetHandle() is thread-safe... */
	char *name = TnmGetHandle(msgPtr->interp, "msg", &nextId);
	msgPtr->token = Tcl_CreateObjCommand(msgPtr->interp, name,
			    MsgObjCmd, (ClientData) msgPtr, MsgDeleteProc);
	Tcl_SetResult(msgPtr->interp, name, TCL_STATIC);
    }
    
    return msgPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapMsgCmd --
 *
 *	This procedure is called to add a new message to a map
 *	or a map item.
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
TnmMapMsgCmd(interp, mapPtr, itemPtr, objc, objv)
    Tcl_Interp *interp;
    TnmMap *mapPtr;
    TnmMapItem *itemPtr;
    int objc;
    Tcl_Obj *CONST objv[]; 
{
    int result, optHealth = 0, optInterval = 0;
    TnmMapMsg *msgPtr;

    enum msgOpts { msgOptHealth, msgOptInterval } opt;

    static CONST char *msgOptTable[] = {
	"-health", "-interval", (char *) NULL
    };

    if (objc < 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "?options? tag message");
	return TCL_ERROR;
    }
    
    /*
     * Process any options for the message command.
     */
    
    while (objc > 4) {
	result = Tcl_GetIndexFromObj(interp, objv[2], msgOptTable, 
				     "option", TCL_EXACT, (int *) &opt);
	if (result != TCL_OK) {
	    return result;
	}
	objc--; objv++;
	switch (opt) {
	case msgOptInterval:
	    if (TnmGetUnsignedFromObj(interp, objv[2], &optInterval)
		!= TCL_OK) {
		    return TCL_ERROR;
	    }
	    objc--; objv++;
	    break;
	case msgOptHealth:
	    if (TnmGetIntRangeFromObj(interp, objv[2], 
				      -100, 100, &optHealth) != TCL_OK) {
		return TCL_ERROR;
	    }
	    optHealth *= 1000;
	    objc--; objv++;
	    break;
	}
    }
    
    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "?options? tag message");
	return TCL_ERROR;
    }

    msgPtr = TnmMapCreateMsg(mapPtr, itemPtr, objv[2], objv[3]);
    msgPtr->health = optHealth;
    msgPtr->interval = optInterval;
    return TCL_OK;
}    

/*
 *----------------------------------------------------------------------
 *
 * TnmMapMsgSave --
 *
 *	This procedure tries to write a message to a file.
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
SaveMsg(msgPtr)
    TnmMapMsg *msgPtr;
{
    Tcl_Obj *path = NULL;
    int code;
    Tcl_Channel c;
    char buffer[80];
    Tcl_Obj *obj;
    char *str;
    int len;

    if (! path && msgPtr->itemPtr) {
	path = msgPtr->itemPtr->path;
    }
    if (! path && msgPtr->mapPtr) {
	path = msgPtr->mapPtr->path;
    }

    if (msgPtr->tag && path && !(msgPtr->flags & TNM_MSG_SAVED)) {
	struct tm *t = localtime((time_t *) &msgPtr->msgTime.sec);
	sprintf(buffer, "/%4d-%02d-%02d", 
		1900 + t->tm_year, 1 + t->tm_mon, t->tm_mday);
	obj = Tcl_NewObj();
	Tcl_AppendStringsToObj(obj, Tcl_GetString(path), buffer, NULL);
	Tcl_IncrRefCount(obj);
	code = TnmMkDir(msgPtr->interp, obj);
	if (code != TCL_OK) {
	    Tcl_DecrRefCount(obj);
	    return -1;
	}
	Tcl_AppendStringsToObj(obj, "/", Tcl_GetString(msgPtr->tag), NULL);
	c = Tcl_OpenFileChannel((Tcl_Interp *) NULL, 
				Tcl_GetStringFromObj(obj, NULL), "a", 0666);
	Tcl_DecrRefCount(obj);
	if (! c) {
	    return -2;
	}

	str = Tcl_GetStringFromObj(msgPtr->msg, &len);
	sprintf(buffer, "%lu\t%u\t", msgPtr->msgTime.sec, msgPtr->interval);
	Tcl_Write(c, buffer, (int) strlen(buffer));
	Tcl_Write(c, str, len);
	Tcl_Write(c, "\n", 1);
	Tcl_Close((Tcl_Interp *) NULL, c);
    }

    return 0;
}

int
MatchMsg(msgPtr, storeList)
    TnmMapMsg *msgPtr;
    Tcl_Obj *storeList;
{
    int i, code, objc;
    Tcl_Obj **objv;

    if (Tcl_ListObjGetElements(NULL, storeList, &objc, &objv) == TCL_OK) {
	for (i = 0; i < objc; i++) {
	    code = Tcl_RegExpMatchObj(NULL, msgPtr->tag, objv[i]);
	    if (code == 1) {
		return 1;
	    }
	}
    }
    return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * TnmMapExpireMsgs --
 *
 *	This procedure removes all messages from a msgList which
 *	are older than expireTime.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The msgList is modified.
 *
 *----------------------------------------------------------------------
 */

void
TnmMapExpireMsgs(msgListPtr, expireTime)
    TnmMapMsg **msgListPtr;
    long expireTime;
{
    TnmMapMsg *msgPtr;
    char *s;
    int len;

    for (msgPtr = *msgListPtr; msgPtr; msgPtr = msgPtr->nextPtr) {
        if (msgPtr->token && msgPtr->interp) {

	    /*
	     * Messages with an empty tag are never saved. They expire
	     * immediately if the expire timer has passed. Check also
	     * if the msg tag patches a pattern in the storage list of
	     * this item.
	     */

	    s = Tcl_GetStringFromObj(msgPtr->tag, &len);
	    if (len == 0) {
		msgPtr->flags |= TNM_MSG_SAVED;
	    } else if (msgPtr->itemPtr) {
		if (! MatchMsg(msgPtr, msgPtr->itemPtr->storeList)) {
		    msgPtr->flags |= TNM_MSG_SAVED;
		}
	    } else if (msgPtr->mapPtr) {
		if (! MatchMsg(msgPtr, msgPtr->mapPtr->storeList)) {
		    msgPtr->flags |= TNM_MSG_SAVED;
		}
	    }

	    if (! (msgPtr->flags & TNM_MSG_SAVED)) {
		(void) SaveMsg(msgPtr);
		msgPtr->flags |= TNM_MSG_SAVED;
	    }

	    /*
	     * Expire messages that have been saved. This might result
	     * in a memory leak if messages somehow are never saved.
	     * I definitely need a better way to handle errors here.
	     */

	    if (msgPtr->msgTime.sec < expireTime
		&& (msgPtr->flags & TNM_MSG_SAVED)) {
		msgPtr->flags |= TNM_MSG_EXPIRED;
	    }
	}
    }

nextMsg:
    for (msgPtr = *msgListPtr; msgPtr; msgPtr = msgPtr->nextPtr) {
        if (msgPtr->token && msgPtr->interp
            && msgPtr->flags & TNM_MSG_EXPIRED) {
	    Tcl_DeleteCommandFromToken(msgPtr->interp, msgPtr->token);
	    goto nextMsg;
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MsgObjCmd --
 *
 *	This procedure implements the message object command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
MsgObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[]; 
{
    TnmMapMsg *msgPtr = (TnmMapMsg *) clientData;
    char buffer[20];
    int result;

    enum commands {
	cmdDestroy, cmdHealth, cmdInterval, cmdItem, cmdMap, 
	cmdTag, cmdText, cmdTime, cmdType
    } cmd;

    static CONST char *cmdTable[] = {
	"destroy", "health", "interval", "item", "map",
	"tag", "text", "time", "type", (char *) NULL
    };

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdDestroy:
	if (msgPtr) {
	    Tcl_DeleteCommandFromToken(interp, msgPtr->token);
	}
	break;
    case cmdItem:
	if (msgPtr->itemPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, msgPtr->itemPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdMap:
	if (msgPtr->mapPtr) {
	    Tcl_AppendResult(interp,
		     Tcl_GetCommandName(interp, msgPtr->mapPtr->token),
			     (char *) NULL);
	}
	break;
    case cmdHealth:
	Tcl_SetIntObj(Tcl_GetObjResult(interp), msgPtr->health / 1000);
	break;
    case cmdInterval:
	sprintf(buffer, "%u", msgPtr->interval);
	Tcl_SetResult(interp, buffer, TCL_VOLATILE);
	break;
    case cmdTag:
	Tcl_SetObjResult(interp, msgPtr->tag);
	break;
    case cmdText:
	Tcl_SetObjResult(interp, msgPtr->msg);
	break;
    case cmdTime:
	sprintf(buffer, "%lu", msgPtr->msgTime.sec);
	Tcl_SetResult(interp, buffer, TCL_VOLATILE);
	break;
    case cmdType:
	Tcl_SetResult(interp, "message", TCL_STATIC);
	break;
    }

    return result;
}
