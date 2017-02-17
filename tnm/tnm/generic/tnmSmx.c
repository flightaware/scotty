/*
 * tnmSmx.c --
 *
 *	Extend a Tcl command interpreter with a command to act as
 *	a script MIB runtime engine by implementing the SMX protocol
 *	defined in RFC 2593bis.
 *
 * Copyright (c) 2000-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#define TNM_SMX_VERSION		"SMX/1.1"

/*
 * Definitions for SMX replies as defined in RFC 2593bis section 5.3.
 */

#define TNM_SMX_REPL_IDENTITY		211
#define TNM_SMX_REPL_STATUS		231
#define TNM_SMX_REPL_ABORT		232
#define TNM_SMX_REPL_SYNTAX_ERROR	401
#define TNM_SMX_REPL_WRONG_CMD		402
#define TNM_SMX_REPL_WRONG_SCRIPT	421
#define TNM_SMX_REPL_WRONG_RUNID	431
#define TNM_SMX_REPL_WRONG_PROFILE	432
#define TNM_SMX_REPL_WRONG_ARGUMENT	433
#define TNM_SMX_REPL_STATUS_FAILED	434
#define TNM_SMX_NTFY_NOTICE		511
#define TNM_SMX_NTFY_STATUS_CHANGE	531
#define TNM_SMX_NTFY_RESULT		532
#define TNM_SMX_NTFY_RESULT_EVENT	533
#define TNM_SMX_NTFY_SCRIPT_TERM	534	/* deprecated in RFC 2593bis */
#define TNM_SMX_NTFY_SCRIPT_ABORT	535	/* deprecated in RFC 2593bis */
#define TNM_SMX_NTFY_ERROR		536	/* added in RFC 2593bis */
#define TNM_SMX_NTFY_ERROR_EVENT	537	/* added in RFC 2593bis */
#define TNM_SMX_NTFY_TERMINATION	538	/* added in RFC 2593bis */

/*
 * Definitions of run states as defined in RFC 2592 (smRunState)
 */

#define TNM_SMX_RUN_STATE_INITIALIZING	1
#define TNM_SMX_RUN_STATE_EXECUTING	2
#define TNM_SMX_RUN_STATE_SUSPENDING	3
#define TNM_SMX_RUN_STATE_SUSPENDED	4
#define TNM_SMX_RUN_STATE_RESUMING	5
#define TNM_SMX_RUN_STATE_ABORTING	6
#define TNM_SMX_RUN_STATE_TERMINATED	7

/*
 * Definitions of exit codes as defined in RFC 2592 (smExitCode)
 */

#define TNM_SMX_EXIT_CODE_NOERROR	1
#define TNM_SMX_EXIT_CODE_HALTED	2
#define TNM_SMX_EXIT_CODE_LIFETIME	3
#define TNM_SMX_EXIT_CODE_NORESOURCES	4
#define TNM_SMX_EXIT_CODE_LANG_ERROR	5
#define TNM_SMX_EXIT_CODE_RUN_ERROR	6
#define TNM_SMX_EXIT_CODE_ARG_ERROR	7
#define TNM_SMX_EXIT_CODE_SEC_ERROR	8
#define TNM_SMX_EXIT_CODE_GEN_ERROR	9

static TnmTable smxExitCodeTable[] = {
    { TNM_SMX_EXIT_CODE_NOERROR,	"noError" },
    { TNM_SMX_EXIT_CODE_HALTED,		"halted" },
    { TNM_SMX_EXIT_CODE_LIFETIME,	"lifeTimeExceeded" },
    { TNM_SMX_EXIT_CODE_NORESOURCES,	"noResourcesLeft" },
    { TNM_SMX_EXIT_CODE_LANG_ERROR,	"languageError" },
    { TNM_SMX_EXIT_CODE_RUN_ERROR,	"runtimeError" },
    { TNM_SMX_EXIT_CODE_ARG_ERROR,	"invalidArgument" },
    { TNM_SMX_EXIT_CODE_SEC_ERROR,	"securityViolation" },
    { TNM_SMX_EXIT_CODE_GEN_ERROR,	"genericError" },
    { 0, NULL }
};

/*
 * Macros to test whether a character is a VCHAR or a WSP
 * as defined in RFC 2234:
 *
 *     VCHAR          =  %x21-7E
 *     WSP            =  SP / HTAB
 *     SP             =  %x20
 *     HTAB           =  %x09
 */

#define VCHAR(c)	(c >= 0x21 && c <= 0x7e)
#define WSP(c)		(c == 0x20 || c == 0x09)

/*
 * Data structure for a running script instance. Each running script
 * executes in its own Tcl interpreter.
 */

typedef struct Run {
    unsigned	runid;		/* The identity of this running script. */
    int		runstate;	/* The status as defined in RFC 2592. */
    int		exitcode;	/* The exit code as defined in RFC 2592. */
    Tcl_Interp	*interp;	/* Tcl interpreter executing this script. */
    struct Run	*nextPtr;	/* Next run in our list of running scripts. */
} Run;

/*
 * Every SMX controlled Tcl interpreter has an associated SmxControl
 * record. It keeps track of the smx channel, the list of running
 * scripts and the list of runtime security profiles known by the
 * interpreter. The name tnmSmxControl is used to get/set the
 * SmxControl record.
 */

static char tnmSmxControl[] = "tnmSmxControl";

typedef struct SmxControl {
    Tcl_Channel smx;		/* The channel to the SMX peer. */
    Run* runList;		/* The list of running scripts. */
    Tcl_Obj *profileList;	/* Tcl list of known runtime profiles. */
} SmxControl;

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(smxMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc		(ClientData clientData,
				     Tcl_Interp *interp);
static Run*
SmxNewRun		(SmxControl *control, unsigned runid);

static Run*
SmxGetRunFromId		(SmxControl *control, unsigned runid);

static Run*
SmxGetRunFromInterp	(SmxControl *control, Tcl_Interp *interp);

static void
SmxDeleteRun		(SmxControl *control, unsigned runid);

static void
SmxAbortRun		(Tcl_Interp *interp, SmxControl *control,
				     Run *runPtr);
static void
SmxHello		(Tcl_Interp *interp, int id);

static void
SmxStart		(Tcl_Interp *interp,
				     int id, unsigned runid, char *script,
                                     char *profile, char *argument);
static void
SmxSuspend		(Tcl_Interp *interp,
				     int id, unsigned runid);
static void
SmxResume		(Tcl_Interp *interp,
				     int id, unsigned runid);
static void
SmxAbort		(Tcl_Interp *interp,
				     int id, unsigned runid);
static void
SmxStatus		(Tcl_Interp *interp,
				     int id, unsigned runid);
static void
SmxAppendHexString	(char *dst, CONST char *src, int len);

static void
SmxAppendQuotedString	(char *dst, CONST char *src, int len);

static void
SmxAppendHexOrQuotedString (char *dst, CONST char *src, int len);

static void
SmxReply		(SmxControl *control,
				     int reply, int id, Run *run,
				     CONST char *msg, int msglen);
static char*
SmxParseRunId		(char *line, int id, unsigned *runid);

static char*
SmxParseQuotedString	(char *line, char **dst, int *len);

static char*
SmxParseHexString	(char *line, char **dst, int *len);

static char*
SmxParseHexOrQuotedString (char *line, char **dst, int *len);

static char*
SmxParseProfileString	(char *line, char **dst);

static int
SmxParse		(char *line, Tcl_Interp *interp);

static void
SmxReceiveProc		(ClientData clientData, int mask);


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
    SmxControl *control = (SmxControl *) clientData;

    if (control) {
	Tcl_DecrRefCount(control->profileList);
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SmxNewRun --
 *
 *	This procedure inserts a new Run pointer into the list of
 *	concurrent runs.
 *
 * Results:
 *	A pointer to the newly created Run structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Run* SmxNewRun(SmxControl *control, unsigned runid)
{
    Run *runPtr;

    runPtr = (Run *) ckalloc(sizeof(Run));
    memset((char *) runPtr, 0, sizeof(Run));
    runPtr->runid = runid;

    runPtr->nextPtr = control->runList;
    runPtr->runstate = TNM_SMX_RUN_STATE_INITIALIZING;
    runPtr->exitcode = TNM_SMX_EXIT_CODE_NOERROR;
    control->runList = runPtr;
    return runPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxGetRunFromId --
 *
 *	This procedure finds the Run pointer for a given runid.
 *
 * Results:
 *	A pointer to the Run structure or NULL if there is none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Run* SmxGetRunFromId(SmxControl *control, unsigned runid)
{
    Run *runPtr;

    for (runPtr = control->runList; runPtr; runPtr = runPtr->nextPtr) {
	if (runPtr->runid == runid) break;
    }

    return runPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxGetRunFromInterp --
 *
 *	This procedure finds the Run pointer for a given
 *	Tcl interpreter.
 *
 * Results:
 *	A pointer to the Run structure or NULL if there is none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Run* SmxGetRunFromInterp(SmxControl *control, Tcl_Interp *interp)
{
    Run *runPtr;


    for (runPtr = control->runList; runPtr; runPtr = runPtr->nextPtr) {
	if (runPtr->interp == interp) break;
    }

    return runPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxDeleteRun --
 *
 *	This procedure deletes all Runs with the given runid from the
 *	internal list of concurrent runs.
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
SmxDeleteRun(SmxControl *control, unsigned runid)
{
    Run **rPtrPtr, *runPtr;

    rPtrPtr = &(control->runList);
    while (*rPtrPtr && (*rPtrPtr)->runid != runid) {
	rPtrPtr = &(*rPtrPtr)->nextPtr;
    }
    if (*rPtrPtr) {
	runPtr = *rPtrPtr;
	*rPtrPtr = (*rPtrPtr)->nextPtr;
	ckfree((char *) runPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SmxAbortRun --
 *
 *	This procedure kills a running script. It heuristically tries
 *	to get the proper exit code by classifying the error message
 *	left in the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies the exitcode in the runPtr.
 *
 *----------------------------------------------------------------------
 */


static void
SmxAbortRun(Tcl_Interp *interp, SmxControl *control, Run *runPtr)
{
    CONST char *msg;
    int i;

    /*
     * An array of Tcl error message prefixes which we classify as
     * language errors. This probably catches 90 % of the cases, but is
     * not really nice. The prefixes are taken from tclBasic.c,
     * tclIndexObj.c, tclParse.c, and tclParseExpr.c.
     */

    static char* tclLangErrorPrefixes[] = {
	"wrong # args:",
	"invalid command name",
	"bad option",
	"syntax error",
	"extra characters after close-quote",
	"extra characters after close-brace",
	"missing close-bracket",
	"missing close-brace",
	"missing close-brace for variable name",
	"missing )",
	"missing \"",
	NULL
    };

    /*
     * How do we distinguish error codes? In general, we can not do
     * that since Tcl does not distinguish between language, runtime
     * and security errors. So the default is to report errors as
     * runtime errors since Tcl does not really distinguish between
     * syntax parsing and execution. However, we can make a best guess
     * by looking at the error string left in the interpreter and try
     * to recognize common syntax error messages.
     *
     * It is left for further study whether a similar approach can be
     * used to identify security violations.
     */

    runPtr->runstate = TNM_SMX_RUN_STATE_TERMINATED;
    runPtr->exitcode = TNM_SMX_EXIT_CODE_RUN_ERROR;
    msg = Tcl_GetStringResult(interp);
    for (i = 0; tclLangErrorPrefixes[i]; i++) {
	if (strncmp(msg, tclLangErrorPrefixes[i],
		    strlen(tclLangErrorPrefixes[i])) == 0) {
	    runPtr->exitcode = TNM_SMX_EXIT_CODE_LANG_ERROR;
	    break;
	}
    }
    msg = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
    SmxReply(control, TNM_SMX_NTFY_ERROR, 0, runPtr, msg, -1);
    SmxReply(control, TNM_SMX_NTFY_TERMINATION, 0, runPtr, NULL, 0);
    Tcl_DeleteInterp(runPtr->interp);
    SmxDeleteRun(control, runPtr->runid);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxHello --
 *
 *	This procedure implements the SMX hello command as
 *	defined in RFC 2593bis section 6.1.1.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxHello(Tcl_Interp *interp, int id)
{
    char *smxCookie = getenv("SMX_COOKIE");
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);
    
    SmxReply(control, TNM_SMX_REPL_IDENTITY, id, NULL, smxCookie, -1);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxStart --
 *
 *	This procedure implements the SMX start command as
 *	defined in RFC 2593bis section 6.1.2.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxStart(Tcl_Interp *interp, int id, unsigned runid, char *script, char *profile, char *argument)
{
    Run *runPtr;
    int i, code, objc, argc;
    Tcl_Obj **objv;
    char buffer[80], slaveName[80];
    CONST char **argv;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    /* Check whether the runid is already in use. */

    runPtr = SmxGetRunFromId(control, runid);
    if (runPtr) {
	SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	return;
    }

    /*
     * Check whether the script file can be read. Note that this test
     * is not giving correct results in all cases since the file
     * permissions might be changed until a subsequent open occurs.
     * But there is no security risk associated with this test since
     * the subsequent open may still fail. But we must be careful to
     * return the correct SMX reply in this case.
     */

    if (access(script, R_OK) != 0) {
	SmxReply(control, TNM_SMX_REPL_WRONG_SCRIPT, id, NULL, NULL, 0);
	return;
    }

    /*
     * Check whether the security profile is known.
     */

    if (TCL_OK != Tcl_ListObjGetElements((Tcl_Interp *) NULL,
					 control->profileList,
					 &objc, &objv)) {
	SmxReply(control, TNM_SMX_REPL_WRONG_PROFILE, id , NULL, NULL, 0);
	return;
    }

    for (i = 0; i < objc; i++) {
	if (strcmp(profile, Tcl_GetString(objv[i])) == 0) {
	    break;
	}
    }
    if (i == objc) {
	SmxReply(control, TNM_SMX_REPL_WRONG_PROFILE, id , NULL, NULL, 0);
	return;
    }

    /*
     * Check whether the argument string is well-formed and can be
     * tokenized into something we can pass safely to the script.
     */

    if (Tcl_SplitList(interp, argument, &argc, &argv) != TCL_OK) {
	SmxReply(control, TNM_SMX_REPL_WRONG_ARGUMENT, id, NULL, NULL, 0);
	return;
    }
    sprintf(buffer, "%d", argc);
    ckfree((char *) argv);

    /*
     * Start the script by creating a new slave interpreter
     * and return a positive reply.
     */

    runPtr = SmxNewRun(control, runid);
    SmxReply(control, TNM_SMX_REPL_STATUS, id, runPtr, NULL, 0);

    sprintf(slaveName, "run%d", runid);
    runPtr->interp = Tcl_CreateSlave(interp, slaveName, 1);
    
    /*
     * This should really be an call to Tcl_AppInit(). But this did
     * not really work and I guess it is a fault in Tcl itself.
     */
    if (TnmInit(runPtr->interp, 1) != TCL_OK) {
	SmxAbortRun(runPtr->interp, control, runPtr);
	return;
    }

    /*
     * Let the master interpreter install the runtime security
     * profile. Note that an error exit here will end the game
     * before execution really starts.
     */

    Tcl_Preserve((ClientData) interp);
    code = Tcl_VarEval(interp, profile, " ", slaveName, (char *) NULL);
    if (code != TCL_OK) {
	SmxAbortRun(interp, control, runPtr);
	Tcl_Release((ClientData) interp);
	return;
    }
    Tcl_Release((ClientData) interp);

    /*
     * Initialize the global variables arc, argv and argv0 which hold
     * the script arguments.
     */

    (void) Tcl_SetVar(runPtr->interp, "argc", buffer, TCL_GLOBAL_ONLY);
    (void) Tcl_SetVar(runPtr->interp, "argv", argument, TCL_GLOBAL_ONLY);
    (void) Tcl_SetVar(runPtr->interp, "argv0", script, TCL_GLOBAL_ONLY);
    
    runPtr->runstate = TNM_SMX_RUN_STATE_EXECUTING;
    SmxReply(control, TNM_SMX_NTFY_STATUS_CHANGE, id, runPtr, NULL, 0);
    {
	Tcl_Interp *runInterp;
	runInterp = runPtr->interp;
	Tcl_Preserve((ClientData) runInterp);
	code = Tcl_EvalFile(runInterp, script);
	if (code == TCL_ERROR) {
	    SmxAbortRun(runInterp, control, runPtr);
	    /* runPtr is invalid now - so do not use it anymore */
	}
	Tcl_Release((ClientData) runInterp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SmxSuspend --
 *
 *	This procedure implements the SMX suspend command as
 *	defined in RFC 2593bis section 6.1.3.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxSuspend(Tcl_Interp *interp, int id, unsigned runid)
{
    Run *runPtr;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);
    
    /* Check whether the runid is valid. */

    runPtr = SmxGetRunFromId(control, runid);
    if (! runPtr) {
	SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	return;
    }

    /*
     * Return a negative reply since we currently do not support
     * suspend/resume semantics.
     */

    SmxReply(control, TNM_SMX_REPL_STATUS_FAILED, id, runPtr, NULL, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxResume --
 *
 *	This procedure implements the SMX resume command as
 *	defined in RFC 2593bis section 6.1.4.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxResume(Tcl_Interp *interp, int id, unsigned runid)
{
    Run *runPtr;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    /* Check whether the runid is valid. */

    runPtr = SmxGetRunFromId(control, runid);
    if (! runPtr) {
	SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	return;
    }

    /*
     * Return a negative reply since we currently do not support
     * suspend/resume semantics.
     */

    SmxReply(control, TNM_SMX_REPL_STATUS_FAILED, id, runPtr, NULL, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxAbort --
 *
 *	This procedure implements the SMX abort command as
 *	defined in RFC 2593bis section 6.1.5.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxAbort(Tcl_Interp *interp, int id, unsigned runid)
{
    Run *runPtr;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    /* Check whether the runid is valid. */

    runPtr = SmxGetRunFromId(control, runid);
    if (! runPtr) {
	SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	return;
    }

    runPtr->exitcode = TNM_SMX_EXIT_CODE_HALTED;
    runPtr->runstate = TNM_SMX_RUN_STATE_TERMINATED;
    Tcl_DeleteInterp(runPtr->interp);
    SmxDeleteRun(control, runPtr->runid);
    SmxReply(control, TNM_SMX_REPL_ABORT, id, runPtr, NULL, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxStatus --
 *
 *	This procedure implements the SMX status command as
 *	defined in RFC 2593bis section 6.1.6.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxStatus(Tcl_Interp *interp, int id, unsigned runid)
{
    Run *runPtr;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    /* Check whether the runid is valid. */

    runPtr = SmxGetRunFromId(control, runid);
    if (! runPtr) {
	SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	return;
    }

    SmxReply(control, TNM_SMX_REPL_STATUS, id, runPtr, NULL, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxAppendHexString --
 *
 *	This procedure converts the len bytes pointed to at src
 *	into a hex string and appends it to dst. The caller must
 *	ensure that dst is large enough to hold the result.
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
SmxAppendHexString(char *dst, CONST char *src, int len)
{
    char *p;
    int i;

    if (! dst) return;

    p = dst + strlen(dst);
    for (i = 0; i < len; i++, p += 2) {
	sprintf(p, "%02x", src[i]);
    }
    *p = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxAppendQuotedString --
 *
 *	This procedure converts the len bytes pointed to at src
 *	into a quoted string and appends it to dst. The caller
 *	must ensure that dst is large enough to hold the result.
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
SmxAppendQuotedString(char *dst, CONST char *src, int len)
{
    char *p;
    int i;

    if (! dst) return;

    /*
     * According to RFC 2592bis, some characters are quoted
     * as follows:
     *
     *	`\\'   backslash character       (`%x5C')
     *  `\t'   tab character             (`HTAB')
     *  `\n'   newline character         (`LF')
     *  `\r'   carriage-return character (`CR')
     *  `\"'   quote character           (`DQUOTE')
     */
    
    p = dst + strlen(dst);
    *p++ = '"';
    for (i = 0; i < len; i++, p++) {
	switch (src[i]) {
	case 0x5c:	/* '\\' */
	    *p++ = '\\'; *p = '\\';
	    break;
	case 0x09:	/* '\t' */
	    *p++ = '\\'; *p = 't';
	    break;
	case 0x0a:	/* '\n' */
	    *p++ = '\\'; *p = 'n';
	    break;
	case 0x0c:	/* '\r' */
	    *p++ = '\\'; *p = 'r';
	    break;
	case '"':	/* '"'  */
	    *p++ = '\\'; *p = '"';
	    break;
	default:
	    *p = src[i];
	    break;
	}
    }
    *p++ = '"';
    *p = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxAppendHexOrQuotedString --
 *
 *	This procedure converts the len bytes pointed to at src
 *	into a quoted or hex string and appends it to dst. The
 *	caller must ensure that dst is large enough to hold the
 *	result.
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
SmxAppendHexOrQuotedString(char *dst, CONST char *src, int len)
{
    int i;

    /*
     * Test whether the string conforms to a QuotedString and
     * call either the QuotedString or HexString encoder. A
     * QuotedString is defined in RFC 2593bis as follows:
     *
     *     QuotedString  = DQUOTE *(VCHAR / WSP) DQUOTE
     */

    for (i = 0; i < len; i++) {
	if (! (VCHAR(src[i]) || WSP(src[i])
	       || src[i] == '\t' || src[i] == '\r' || src[i] == '\n')) {
	    break;
	}
    }

    if (i == len) {
        SmxAppendQuotedString(dst, src, len);
    } else {
	SmxAppendHexString(dst, src, len);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SmxReply --
 *
 *	This procedure generates an SMX reply message based on the
 *	parameters provided by the caller.
 *
 * Results:
 *	A dynamically allocated SMX reply message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void SmxReply(SmxControl *control, int reply, int id, Run *runPtr, CONST char *msg, int msglen)
{
    char *line;
    int n;

    if (! msg) {
	msglen = 0;
    }
    if (msg && msglen < 0) {
	msglen = strlen(msg);
    }

    /*
     * Make sure the line buffer is large enough if we need to
     * encode the message. If new messages are ever added to the
     * SMX protocol, then make sure that the size estimation here
     * is still correct and on the safe side.
     */

    line = ckalloc((size_t) (80 + (msg ? msglen * 2 : 0)));
    if (! line) {
	return;
    }
    strcpy(line, "");

    /*
     * Construct the reply message.
     */

    switch (reply) {
    case TNM_SMX_REPL_IDENTITY:
	sprintf(line, "%d %d %s %s\r\n", reply, id, TNM_SMX_VERSION, msg);
	break;
    case TNM_SMX_REPL_STATUS:
	sprintf(line, "%d %d %d\r\n", reply, id,
		runPtr ? runPtr->runstate : 0);
	break;
    case TNM_SMX_REPL_ABORT:
    case TNM_SMX_REPL_SYNTAX_ERROR:
    case TNM_SMX_REPL_WRONG_CMD:
    case TNM_SMX_REPL_WRONG_SCRIPT:
    case TNM_SMX_REPL_WRONG_RUNID:
    case TNM_SMX_REPL_WRONG_PROFILE:
    case TNM_SMX_REPL_WRONG_ARGUMENT:
    case TNM_SMX_REPL_STATUS_FAILED:
	sprintf(line, "%d %d\r\n", reply, id);
	break;
    case TNM_SMX_NTFY_NOTICE:
	sprintf(line, "%d 0 ", reply);
	SmxAppendQuotedString(line, msg, msglen);
	strcat(line, "\r\n");
	break;
    case TNM_SMX_NTFY_STATUS_CHANGE:
	sprintf(line, "%d 0 %u %d\r\n", reply,
		runPtr ? runPtr->runid : 0,
		runPtr ? runPtr->runstate : 0);
	break;
    case TNM_SMX_NTFY_RESULT:
    case TNM_SMX_NTFY_RESULT_EVENT:
    case TNM_SMX_NTFY_ERROR:
    case TNM_SMX_NTFY_ERROR_EVENT:
	sprintf(line, "%d 0 %u %d ", reply,
		runPtr ? runPtr->runid : 0,
		runPtr ? runPtr->runstate : 0);
	SmxAppendHexOrQuotedString(line, msg, msglen);
	strcat(line, "\r\n");
	break;
    case TNM_SMX_NTFY_SCRIPT_TERM:
	sprintf(line, "%d 0 %u ", reply, runPtr ? runPtr->runid : 0);
	SmxAppendHexOrQuotedString(line, msg, msglen);
	strcat(line, "\r\n");
	break;
    case TNM_SMX_NTFY_SCRIPT_ABORT:
	sprintf(line, "%d 0 %u %d ", reply,
		runPtr ? runPtr->runid : 0,
		runPtr ? runPtr->exitcode : 0);
	SmxAppendHexOrQuotedString(line, msg, msglen);
	strcat(line, "\r\n");
	break;
    case TNM_SMX_NTFY_TERMINATION:
	sprintf(line, "%d 0 %u %d\r\n", reply,
		runPtr ? runPtr->runid : 0,
		runPtr ? runPtr->exitcode : 0);
	break;
    default:
	sprintf(line, "%d 0 \"unknown error code in SmxReply()\"\r\n",
		TNM_SMX_NTFY_NOTICE);
	break;
    }

    /*
     * Submit message to the SMX peer. This is the only Tcl specific
     * code here.
     */

    if (control->smx) {
	Tcl_MutexLock(&smxMutex);
	n = Tcl_Write(control->smx, line, -1);
	if (n < 0) {
	    (void) Tcl_UnregisterChannel((Tcl_Interp *) NULL, control->smx);
	    control->smx = NULL;
	    TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_DAEMON,
		       "SMX connection terminated abnormally: write failure");
	}
	Tcl_MutexUnlock(&smxMutex);
    }

    ckfree(line);
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParseRunId --
 *
 *	This procedure extracts a run identifier from an SMX
 *	message.
 *
 * Results:
 *	The remainder of the input line.
 *
 * Side effects:
 *	Generates an SmxReply if the run identifier is illegal.
 *
 *----------------------------------------------------------------------
 */

static char *SmxParseRunId(char *line, int id, unsigned *runid)
{
    char *ptr;
    
    *runid = strtoul(line, &ptr, 10);
    if (ptr == line) {
	return NULL;
    }
    return ptr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParseQuotedString --
 *
 *	This procedure extracts a quoted string as defined in RFC 2593bis
 *	section 5.1 and returns it as a proper null-terminated string
 *	plus its length.
 *
 * Results:
 *	The remainder of the input line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *SmxParseQuotedString(char *line, char **dst, int *len)
{
    char *ptr, last;

    /* First eat up any white space. */

    for (ptr = line; *ptr == ' ' || *ptr == '\t'; ptr++) ;

    if (*ptr != '"') {
	return NULL;
    }

    for (*dst = ++ptr, *len = 0, last = 0;
	 (VCHAR(*ptr) || WSP(*ptr)) && (*ptr != '"' || last == '\\');
	 last = *ptr++) {
	if (last == '\\') {
	    switch (*ptr) {
	    case 't':
		*ptr = '\t';
		break;
	    case 'n':
		*ptr = '\n';
 		break;
	    case 'r':
		*ptr = '\r';
		break;
	    default:
		break;
	    }
	}
	(*dst)[*len] = *ptr;
	if (*ptr != '\\' || last == '\\') {
	    (*len)++;
	}
    }
    if (*ptr != '"') {
	return NULL;
    }
    (*dst)[*len] = 0;
    return ++ptr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParseHexString --
 *
 *	This procedure extracts a hexadecimal string and returns it
 *	as a proper null-terminated string and its length.
 *
 * Results:
 *	The remainder of the input line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *SmxParseHexString(char *line, char **dst, int *len)
{
    char *ptr;
    int i, c;
    
    /* First eat up any white space. */
    
    for (ptr = line; *ptr == ' ' || *ptr == '\t'; ptr++) ;

    /* Check whether we have an even number of hex digits. */

    for (i = 0; ptr[i]; i++) {
	if (!isxdigit((int) ptr[i])) return NULL;
    }
    if (i % 2 || i == 0) return NULL;

    for (*dst = ptr, *len = 0;
	 1 == sscanf(ptr, "%2x", &c);
	 ptr += 2, (*len)++) {
	(*dst)[*len] = c;
    }
    if (*ptr && !isspace((int) *ptr)) {
	return NULL;
    }
    (*dst)[*len] = 0;
    return ptr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParseHexOrQuotedString --
 *
 *	This procedure extracts a hexadecimal string or a quoted string
 *	and returns it as a proper null-terminated string and its length.
 *
 * Results:
 *	The remainder of the input line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *SmxParseHexOrQuotedString(char *line, char **dst, int *len)
{
    char *ptr;

    /* First eat up any white space. */

    for (ptr = line; *ptr == ' ' || *ptr == '\t'; ptr++) ;

    if (*ptr == '"') {
	return SmxParseQuotedString(line, dst, len);
    } else {
	return SmxParseHexString(line, dst, len);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParseProfileString --
 *
 *	This procedure extracts a profile string and returns it as
 *	a proper null-terminated string.
 *
 * Results:
 *	The remainder of the input line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *SmxParseProfileString(char *line, char **dst)
{
    char *ptr;
    
    /* First eat up any white space. */
    
    for (ptr = line; *ptr == ' ' || *ptr == '\t'; ptr++) ;

    for (*dst = ptr; isalnum((int) *ptr)
	     || *ptr == '-' || *ptr == '.' || *ptr == ',' || *ptr == '/'
	     || *ptr == '_' || *ptr == ':';
	 ptr++) {
    }
    if (*ptr && !isspace((int) *ptr)) {
	return NULL;
    }
    *ptr = 0;
    return ++ptr;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxParse --
 *
 *	This procedure parses an SMX string and calls one of the
 *	functions to handle the command. Error replys are generated
 *	if the input string is malformed.
 *
 * Results:
 *	A non-zero result indicates a parsing or processing failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int SmxParse(char *line, Tcl_Interp *interp)
{
    char *ptr;
    char *cmd, *script, *profile, *argument;
    int id = 0;
    unsigned runid = 0;
    int len;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    /*
     * First get the command name and the transaction identifier as
     * described in RFC 2593bis section 6.1. We assume that all existing
     * and future SMX commands only contain alpha characters. (We
     * should actually restrict that to the C locale.)
     */

    for (ptr = line; isalpha((int) *ptr); ptr++) ;
    if (ptr == line) {
	SmxReply(control, TNM_SMX_NTFY_NOTICE, 0, NULL,
		 "failed to extract command", -1);
	return -1;
    }
    cmd = line; *ptr = 0; line = ++ptr;

    id = strtol(line, &ptr, 10);
    if (ptr == line) {
	SmxReply(control, TNM_SMX_NTFY_NOTICE, 0, NULL,
		 "failed to extract transaction id", -1);
	return -1;
    }
    line = ptr;

    /*
     * Now extract the arguments for each command and call the
     * appropriate function to handle the command. We are liberal
     * in what we accept and thus we just ignore any superfluous
     * arguments.
     */
    
    if (strcasecmp(cmd, "hello") == 0) {
	SmxHello(interp, id);
    } else if (strcasecmp(cmd, "start") == 0) {
	if (! (line = SmxParseRunId(line, id, &runid))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	    return -1;
	}
	if (! (line = SmxParseQuotedString(line, &script, &len))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_SCRIPT, id, NULL, NULL, 0);
	    return -1;
	}
	if (! (line = SmxParseProfileString(line, &profile))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_PROFILE, id, NULL, NULL, 0);
	    return -1;
	}
	if (! *profile) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_PROFILE, id, NULL, NULL, 0);
	    return -1;
	}
	if (! (line = SmxParseHexOrQuotedString(line, &argument, &len))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_ARGUMENT, id, NULL, NULL, 0);
	    return -1;
	}
	SmxStart(interp, id, runid, script, profile, argument);
    } else if (strcasecmp(cmd, "suspend") == 0) {
	if (! (line = SmxParseRunId(line, id, &runid))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	    return -1;
	}
	SmxSuspend(interp, id, runid);
    } else if (strcasecmp(cmd, "resume") == 0) {
	if (! (line = SmxParseRunId(line, id, &runid))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	    return -1;
	}
	SmxResume(interp, id, runid);
    } else if (strcasecmp(cmd, "abort") == 0) {
	if (! (line = SmxParseRunId(line, id, &runid))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	    return -1;
	}
	SmxAbort(interp, id, runid);
    } else if (strcasecmp(cmd, "status") == 0) {
	if (! (line = SmxParseRunId(line, id, &runid))) {
	    SmxReply(control, TNM_SMX_REPL_WRONG_RUNID, id, NULL, NULL, 0);
	    return -1;
	}
	SmxStatus(interp, id, runid);
    } else {
	SmxReply(control, TNM_SMX_REPL_WRONG_CMD, id, NULL, NULL, 0);
	return -1;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SmxReceiveProc --
 *
 *	This procedure is called to receive a message from the SMX
 *	master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Processes the SMX command as described in the SMX documentation.
 *
 *----------------------------------------------------------------------
 */

static void
SmxReceiveProc(ClientData clientData, int mask)
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    static Tcl_DString *in = NULL;
    int len;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    Tcl_MutexLock(&smxMutex);
    
    if (! control->smx) {
	Tcl_MutexUnlock(&smxMutex);
	return;
    }

    if (! in) {
	in = (Tcl_DString *) ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(in);
    } else {
	Tcl_DStringFree(in);
    }

    len = Tcl_Gets(control->smx, in);
    if (len < 0) {
	(void) Tcl_UnregisterChannel((Tcl_Interp *) NULL, control->smx);
	control->smx = NULL;
	Tcl_MutexUnlock(&smxMutex);
	/*
	 * What to do in this case? Perhaps we should try to abort
	 * all running scripts first? Right now, we just terminate
	 * the whole runtime engine.
	 */
	TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_DAEMON,
		   "SMX connection terminated: read failure");
	Tcl_Exit(1);
	return;
    }

    (void) SmxParse(Tcl_DStringValue(in), interp);
    Tcl_MutexUnlock(&smxMutex);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSmxInit --
 *
 *	Try to establish a TCP connection to the SMX peer.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates an open channel to the SMX peer and registers a
 *	channel handler for the new channel.
 *
 *----------------------------------------------------------------------
 */

int
TnmSmxInit(Tcl_Interp *interp)
{
    char *smxPort = getenv("SMX_PORT");
    char *smxCookie = getenv("SMX_COOKIE");
    int i;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);
    
    /*
     * We only initialize the SMX protocol if we are running in
     * SMX mode. We are in SMX mode whenever we can read the SMX
     * environment variables.
     */

    if (! smxPort || ! smxCookie) {
	return TCL_OK;
    }

    /*
     * Validate the cookie to make sure it conforms to a HexString
     * as specified in RFC 2593bis.
     */

    for (i = 0; smxCookie[i]; i++) {
	if (!isxdigit((int) smxCookie[i])) {
	    i = 0;
	    break;
	}
    }
    if (i % 2 || i == 0) {
	Tcl_AppendResult(interp, "illegal smx cookie \"", smxCookie,
			 "\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create a control record where we keep all the information
     * needed to handle the SMX protocol and the execution of scripts
     * under SMX control. Note that we only create a control record
     * for Tcl interpreters that have no master interpreter. Otherwise,
     * our slave interpreters would end up with their own control
     * record, which obviously makes no sense.
     */

    if (! control && ! Tcl_GetMaster(interp)) {
	control = (SmxControl *) ckalloc(sizeof(SmxControl));
	memset((char *) control, 0, sizeof(SmxControl));
	control->profileList = Tcl_NewListObj(0, NULL);
    	Tcl_SetAssocData(interp, tnmSmxControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    /*
     * Every thing is fine. Lets try to connect back to the
     * SMX_PORT on localhost.
     */

    Tcl_MutexLock(&smxMutex);
    if (control && ! control->smx) {
	control->smx = Tcl_OpenTcpClient(interp, atoi(smxPort), "localhost",
					 NULL, 0, 0);
	if (! control->smx) {
	    Tcl_MutexUnlock(&smxMutex);
	    return TCL_ERROR;
	}
	Tcl_RegisterChannel(interp, control->smx);
	(void) Tcl_SetChannelOption((Tcl_Interp *) NULL, control->smx,
				    "-buffering", "none");
	(void) Tcl_SetChannelOption((Tcl_Interp *) NULL, control->smx,
				    "-translation", "crlf binary");
	(void) Tcl_CreateChannelHandler(control->smx, TCL_READABLE,
					SmxReceiveProc, (ClientData) interp);
    }
    Tcl_MutexUnlock(&smxMutex);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SmxObjCmd --
 *
 *	This procedure is invoked to process the "smx" command.
 *	See the user documentation for details on what it does.
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
Tnm_SmxObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    char *msg;
    int msglen, result, code;
    Run *runPtr = NULL;
    SmxControl *control = (SmxControl *)
	Tcl_GetAssocData(interp, tnmSmxControl, NULL);

    enum commands {
	cmdError, cmdExit, cmdLog, cmdProfiles, cmdResult
    } cmd;

    static CONST char *cmdTable[] = {
	"error", "exit", "log", "profiles", "result", (char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    if (! control) {
	Tcl_Interp *master;
	for (master = Tcl_GetMaster(interp);
	     master; master = Tcl_GetMaster(master)) {
	    control = (SmxControl *)
		Tcl_GetAssocData(master, tnmSmxControl, NULL);
	    if (control) break;
	}
    }

    if (control) {
	runPtr = SmxGetRunFromInterp(control, interp);
    }

    switch ((enum commands) cmd) {
    case cmdExit:
	if (objc < 2 || objc > 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?exitCode?");
	    return TCL_ERROR;
	}
	code = TNM_SMX_EXIT_CODE_NOERROR;
	if (objc == 3) {
	    code = TnmGetTableKeyFromObj(interp, smxExitCodeTable, objv[2],
					 "exit code");
	    if (code < 0) {
		return TCL_ERROR;
	    }
	}

	if (runPtr) {
	    runPtr->runstate = TNM_SMX_RUN_STATE_TERMINATED;
	    runPtr->exitcode = code;
	    SmxReply(control, TNM_SMX_NTFY_TERMINATION, 0, runPtr, NULL, 0);
	    Tcl_DeleteInterp(runPtr->interp);
	    SmxDeleteRun(control, runPtr->runid);
	} else {
	    Tcl_Channel channel;
	    char buffer[80];
	    channel = Tcl_GetChannel(interp, "stdout", NULL);
	    if (channel) {
		Tcl_Write(channel, "smRunExitCode:\t", -1);
		Tcl_Write(channel,
			  TnmGetTableValue(smxExitCodeTable, (unsigned) code), -1);
 		Tcl_Write(channel, "\n", 1);
		Tcl_Flush(channel);
	    }
	    sprintf(buffer, "exit %d", code);
	    Tcl_Eval(interp, buffer);
	}
	result = TCL_OK;
	break;
	
    case cmdLog:
        if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "message");
	    return TCL_ERROR;
	}
	if (control) {
	    msg = Tcl_GetStringFromObj(objv[2], &msglen);
	    SmxReply(control, TNM_SMX_NTFY_NOTICE, 0, NULL, msg, msglen);
	}
	result = TCL_OK;
	break;

    case cmdProfiles:
	if (objc < 2 || objc > 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?list?");
	    return TCL_ERROR;
	}
	if (runPtr) {
	    Tcl_SetResult(interp,
			  "option \"profiles\" may not be used "
			  "in slaves executing scripts",
			  TCL_STATIC);
	    return TCL_ERROR;
	}

	/*
	 * Check whether the argument is a well-formed Tcl list.
	 * We do this by getting the length of the list (which
	 * implicitely does a conversion into a Tcl_ListObj).
	 */

	if (objc == 3) {
	    int len;
	    if (Tcl_ListObjLength(interp, objv[2], &len) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	
	if (control) {
	    if (objc == 3) {
		Tcl_DecrRefCount(control->profileList);
		control->profileList = objv[2];
		Tcl_IncrRefCount(control->profileList);
	    }
	    Tcl_SetObjResult(interp, control->profileList);
	}
	result = TCL_OK;
	break;

    case cmdResult:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?-notify? string");
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    if (strcmp(Tcl_GetString(objv[2]), "-notify") != 0) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-notify? string");
		return TCL_ERROR;
	    }
	}
	if (runPtr) {
	    msg = Tcl_GetStringFromObj((objc == 4) ? objv[3] : objv[2],
				       &msglen);
	    SmxReply(control, (objc == 4)
		     ? TNM_SMX_NTFY_RESULT_EVENT
		     : TNM_SMX_NTFY_RESULT,
		     0, runPtr, msg, msglen);
	} else {
	    Tcl_Channel channel;
	    msg = Tcl_GetStringFromObj((objc == 4) ? objv[3] : objv[2],
				       &msglen);
	    channel = Tcl_GetChannel(interp, "stdout", NULL);
	    if (channel) {
		Tcl_Write(channel, "smRunResult:\t\"", -1);
		Tcl_Write(channel, msg, msglen);
		Tcl_Write(channel, "\"\n", 2);
		Tcl_Flush(channel);
	    }
	}

	result = TCL_OK;
	break;

    case cmdError:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?-notify? string");
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    if (strcmp(Tcl_GetString(objv[2]), "-notify") != 0) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-notify? string");
		return TCL_ERROR;
	    }
	}
	if (runPtr) {
	    msg = Tcl_GetStringFromObj((objc == 4) ? objv[3] : objv[2],
				       &msglen);
	    SmxReply(control, (objc == 4)
		     ? TNM_SMX_NTFY_ERROR_EVENT
		     : TNM_SMX_NTFY_ERROR,
		     0, runPtr, msg, msglen);
	} else {
	    Tcl_Channel channel;
	    msg = Tcl_GetStringFromObj((objc == 4) ? objv[3] : objv[2],
				       &msglen);
	    channel = Tcl_GetChannel(interp, "stdout", NULL);
	    if (channel) {
		Tcl_Write(channel, "smRunError:\t\"", -1);
		Tcl_Write(channel, msg, msglen);
		Tcl_Write(channel, "\"\n", 2);
		Tcl_Flush(channel);
	    }
	}
	break;
    }

    return result;
}

