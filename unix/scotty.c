/*
 * scotty.c --
 *
 *	This is scotty, a simple event-driven Tcl interpreter with 
 *	some special commands to get network management information
 *	about TCP/IP networks. 
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

static Tcl_Interp *interp;      /* Interpreter for this application. */
static Tcl_DString command;	/* Used to assemble lines of terminal input
				 * into Tcl commands. */
static Tcl_DString line;	/* Used to read the next line from the
                                 * terminal input. */
static int tty;			/* Non-zero means standard input is a
				 * terminal-like device.  Zero means it's
				 * a file. */

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
EventLoop		(Tcl_Interp *interp);

static void
Prompt			(Tcl_Interp *interp, int partial);

static void		
StdinProc		(ClientData clientData, int mask);


/*
 *----------------------------------------------------------------------
 *
 * EventLoop --
 *
 *	This procedure is main event loop. Tcl_DoOneEvent() favours 
 *      timer events over file events. This can create problems if 
 *	e.g. async. SNMP requests become timed out before the answer 
 *	is processed. We therefore check for a dending file event 
 *	inside of the while loop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes and events are processed.
 *
 *----------------------------------------------------------------------
 */

static void
EventLoop(interp)
    Tcl_Interp *interp;
{
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS)) {
        Tcl_DoOneEvent(TCL_DONT_WAIT|TCL_FILE_EVENTS);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(interp)
    Tcl_Interp *interp;
{
    if (Tcl_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_PkgRequire(interp, "Tnm", TNM_VERSION, 1) == NULL) {
	if (Tcl_StringMatch(Tcl_GetStringResult(interp), "*can't find package*")) {
	    Tcl_AppendResult(interp, "\n",
   "This usually means that you have to define the TCLLIBPATH environment\n",
   "variable to point to the tnm library directory or you have to include\n",
   "the path to the tnm library directory in Tcl's auto_path variable.",
			     (char *) NULL);
	}
        return TCL_ERROR;
    }

    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(argc, argv)
    int argc;
    const char *const *argv;
{
    char buffer[512], *args; 
    const char *fileName = NULL;
    Tcl_Channel inChannel, errChannel;
    int code, exitCode = 0;

    /* 
     * Create and initialize the Tcl interpreter. 
     */

    Tcl_FindExecutable(argv[0]);
    interp = Tcl_CreateInterp();
#ifdef TCL_MEM_DEBUG
    Tcl_InitMemory(interp);
#endif

    /*
     * Make command-line arguments available in the Tcl variables "argc"
     * and "argv".  If the first argument doesn't start with a "-" then
     * strip it off and use it as the name of a script file to process.
     */

    fileName = NULL;
    if ((argc > 1) && (argv[1][0] != '-')) {
        fileName = argv[1];
        argc--;
        argv++;
    }
    args = Tcl_Merge(argc-1, argv+1);
    Tcl_SetVar(interp, "argv", args, TCL_GLOBAL_ONLY);
    ckfree(args);
    sprintf(buffer, "%d", argc-1);
    Tcl_SetVar(interp, "argc", buffer, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "argv0", (fileName != NULL) ? fileName : argv[0],
	       TCL_GLOBAL_ONLY);

    /* 
     * Set the "tcl_interactive" variable. 
     */

    tty = isatty(0);
    Tcl_SetVar(interp, "tcl_interactive",
	       ((fileName == NULL) && tty) ? "1" : "0", TCL_GLOBAL_ONLY);

    /*
     * Invoke application-specific initialization.
     */

    if (Tcl_AppInit(interp) == TCL_ERROR) {
	errChannel = Tcl_GetChannel(interp, "stderr", NULL);
	if (errChannel) {
	    Tcl_Write(errChannel, "initialization failed: ", -1);
	    Tcl_Write(errChannel, Tcl_GetStringResult(interp), -1);
	    Tcl_Write(errChannel, "\n", 1);
	}
	exitCode = 1;
	goto done;
    }

    /*
     * If a script file was specified then just source that file
     * and fall into the event loop.
     */

    if (fileName != NULL) {
	code = Tcl_EvalFile(interp, fileName);
	if (code != TCL_OK) {
	    goto error;
	}
	EventLoop(interp);
	goto done;
    }
    
    /*
     * We're running interactively. Source a user-specific startup
     * file if the application specified one and if the file exists.
     */

    Tcl_SourceRCFile(interp);

    /*
     * Commands will come from standard input, so set up an event
     * handler for standard input.
     */

    inChannel = Tcl_GetChannel(interp, "stdin", NULL);
    if (inChannel) {
	Tcl_CreateChannelHandler(inChannel, TCL_READABLE, StdinProc,
				 (ClientData) inChannel);
	if (tty) {
	    Prompt(interp, 0);
	}
    }

    Tcl_DStringInit(&command);
    Tcl_DStringInit(&line);
    Tcl_ResetResult(interp);

    /*
     * Loop infinitely, waiting for commands to execute.  When there
     * are event sources left, EventLoop returns and we exit.
     */

    EventLoop(interp);
    goto done;

error:
    errChannel = Tcl_GetChannel(interp, "stderr", NULL);
    if (errChannel) {
	/*
	 * The following statement guarantees that the errorInfo
	 * variable is set properly.
	 */
	Tcl_AddErrorInfo(interp, "");
	Tcl_Write(errChannel, 
		  Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY), -1);
	Tcl_Write(errChannel, "\n", 1);
    }
    exitCode = 1;

done:

    /*
     * Rather than calling exit, invoke the "exit" command so that
     * users can replace "exit" with some other command to do additional
     * cleanup on exit.  The Tcl_Eval call should never return.
     */

    sprintf(buffer, "exit %d", exitCode);
    Tcl_Eval(interp, buffer);
    return exitCode;
}

/*
 *----------------------------------------------------------------------
 *
 * StdinProc --
 *
 *	This procedure is invoked by the event dispatcher whenever
 *	standard input becomes readable.  It grabs the next line of
 *	input characters, adds them to a command being assembled, and
 *	executes the command if it's complete.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Could be almost arbitrary, depending on the command that's
 *	typed.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static void
StdinProc(clientData, mask)
    ClientData clientData;		/* Not used. */
    int mask;				/* Not used. */
{
    static int gotPartial = 0;
    char *cmd;
    int code, count;
    Tcl_Channel chan = (Tcl_Channel) clientData;

    count = Tcl_Gets(chan, &line);

    if (count < 0) {
	if (!gotPartial) {
	    if (tty) {
		Tcl_Exit(0);
	    } else {
		Tcl_DeleteChannelHandler(chan, StdinProc, (ClientData) chan);
	    }
	    return;
	} else {
	    count = 0;
	}
    }

    (void) Tcl_DStringAppend(&command, Tcl_DStringValue(&line), -1);
    cmd = Tcl_DStringAppend(&command, "\n", -1);
    Tcl_DStringFree(&line);
    
    if (count != 0) {
	if (!Tcl_CommandComplete(cmd)) {
	    gotPartial = 1;
	    goto prompt;
	}
    }
    gotPartial = 0;

    /*
     * Disable the stdin channel handler while evaluating the command;
     * otherwise if the command re-enters the event loop we might
     * process commands from stdin before the current command is
     * finished.  Among other things, this will trash the text of the
     * command being evaluated.
     */

    Tcl_CreateChannelHandler(chan, 0, StdinProc, (ClientData) chan);
    code = Tcl_RecordAndEval(interp, cmd, TCL_EVAL_GLOBAL);
    Tcl_CreateChannelHandler(chan, TCL_READABLE, StdinProc,
	    (ClientData) chan);
    Tcl_DStringFree(&command);
    if (*Tcl_GetStringResult (interp) != '\0') {
	if ((code != TCL_OK) || (tty)) {
	    /*
	     * The statement below used to call "printf", but that resulted
	     * in core dumps under Solaris 2.3 if the result was very long.
             *
             * NOTE: This probably will not work under Windows either.
	     */

	    puts(Tcl_GetStringResult (interp));
	}
    }

    /*
     * Output a prompt.
     */

    prompt:
    if (tty) {
	Prompt(interp, gotPartial);
    }
    Tcl_ResetResult(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Prompt --
 *
 *	Issue a prompt on standard output, or invoke a script
 *	to issue the prompt.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A prompt gets output, and a Tcl script may be evaluated
 *	in interp.
 *
 *----------------------------------------------------------------------
 */

static void
Prompt(interp, partial)
    Tcl_Interp *interp;			/* Interpreter to use for prompting. */
    int partial;			/* Non-zero means there already
					 * exists a partial command, so use
					 * the secondary prompt. */
{
    CONST char *promptCmd;
    int code;
    Tcl_Channel outChannel, errChannel;

    outChannel = Tcl_GetChannel(interp, "stdout", NULL);
    errChannel = Tcl_GetChannel(interp, "stderr", NULL);

    promptCmd = Tcl_GetVar(interp,
	partial ? "tcl_prompt2" : "tcl_prompt1", TCL_GLOBAL_ONLY);
    if (promptCmd == NULL) {
defaultPrompt:
	if (!partial) {

            /*
             * We must check that outChannel is a real channel - it
             * is possible that someone has transferred stdout out of
             * this interpreter with "interp transfer".
             */

            if (outChannel != (Tcl_Channel) NULL) {
	        Tcl_Write(outChannel, "% ", 2);
	    }
	}
    } else {
	code = Tcl_Eval(interp, promptCmd);
	if (code != TCL_OK) {
	    Tcl_AddErrorInfo(interp,
		    "\n    (script that generates prompt)");
            /*
             * We must check that errChannel is a real channel - it
             * is possible that someone has transferred stderr out of
             * this interpreter with "interp transfer".
             */
	    
            if (errChannel != (Tcl_Channel) NULL) {
	        Tcl_Write(errChannel, Tcl_GetStringResult (interp), -1);
	        Tcl_Write(errChannel, "\n", 1);
	    }
	    goto defaultPrompt;
	}
    }
    if (outChannel != (Tcl_Channel) NULL) {
        Tcl_Flush(outChannel);
    }
}
