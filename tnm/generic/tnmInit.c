/*
 * tnmInit.c --
 *
 *	This is the initialization of the Tnm Tcl extension with
 *	commands to retrieve management information from TCP/IP
 *	networks. This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1998-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
InitVars		(Tcl_Interp *interp);

static int
SourceRcFile		(Tcl_Interp *interp, char *fileName);

static int
SourceInitFiles		(Tcl_Interp *interp);

static int
InitCmds		(Tcl_Interp *interp, int safe);

/*
 * The following global variable is used to hold the 
 * startup time stamp.
 */

Tcl_Time tnmStartTime;

/*
 * The following structure defines the commands in the Tnm extension.
 */

typedef struct {
    char *name;			/* Name of the command. */
    Tcl_ObjCmdProc *objProc;	/* Object-based procedure for command. */
    int isSafe;			/* If non-zero, command will be present
                                 * in safe interpreter. Otherwise it will
                                 * be hidden. */
} CmdInfo;

static CmdInfo tnmCmds[] = {
    { "Tnm::dns",	Tnm_DnsObjCmd,		 0 },
    { "Tnm::icmp",	Tnm_IcmpObjCmd,		 0 },
    { "Tnm::ined",	Tnm_InedObjCmd,		 0 },
    { "Tnm::job",	Tnm_JobObjCmd,		 1 },
    { "Tnm::map",	Tnm_MapObjCmd,		 1 },
    { "Tnm::mib",	Tnm_MibObjCmd,		 0 },
    { "Tnm::netdb",	Tnm_NetdbObjCmd,	 0 },
    { "Tnm::ntp",	Tnm_NtpObjCmd,		 0 },
#ifdef HAVE_SMI_H
    { "Tnm::smi",	Tnm_SmiObjCmd,		 0 },
#endif
    { "Tnm::smx",	Tnm_SmxObjCmd,		 1 },
    { "Tnm::snmp",	Tnm_SnmpObjCmd,		 0 },
    { "Tnm::sunrpc",	Tnm_SunrpcObjCmd,	 0 },
    { "Tnm::syslog",	Tnm_SyslogObjCmd,	 0 },
    { "Tnm::udp",	Tnm_UdpObjCmd,		 0 },
    { (char *) NULL,	(Tcl_ObjCmdProc *) NULL, 0 }
};


/*
 *----------------------------------------------------------------------
 *
 * InitVars --
 *
 *	This procedure initializes all global Tcl variables that are 
 *	exported by the Tnm extension.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl variables are created.
 *
 *----------------------------------------------------------------------
 */

static void
InitVars(Tcl_Interp *interp)
{
    CONST char *machine, *os, *vers, *user;
    char *tmp, *p;
    char buffer[20];
    Tcl_DString arch;
    Tcl_Obj *path;

    TnmInitPath(interp);

    Tcl_SetVar2(interp, "tnm", "version", TNM_VERSION, TCL_GLOBAL_ONLY);
    Tcl_SetVar2(interp, "tnm", "url", TNM_URL, TCL_GLOBAL_ONLY);

    /*
     * Get the startup time of the Tnm extension.
     */

    if (! tnmStartTime.sec && ! tnmStartTime.usec) {
	Tcl_GetTime(&tnmStartTime);
    }
    sprintf(buffer, "%ld", tnmStartTime.sec);
    Tcl_SetVar2(interp, "tnm", "start", buffer, TCL_GLOBAL_ONLY);

    /*
     * Check if the current version of the Tnm extension is still valid
     * or if it has expired. Note, this is only useful in distribution
     * demos or test versions. This check should be turned off on all
     * stable and final releases.
     */

#ifdef TNM_EXPIRE_TIME
    if (tnmStartTime.sec > TNM_EXPIRE_TIME) {
	Tcl_Panic("Tnm Tcl extension expired. Please upgrade to a newer version.");
    }
    sprintf(buffer, "%ld", TNM_EXPIRE_TIME);
    Tcl_SetVar2(interp, "tnm", "expire", buffer, TCL_GLOBAL_ONLY);
#endif

    /*
     * Set the host name. We are only interested in the name and not
     * in a fully qualified domain name. This makes the result
     * predictable and thus portable.
     */

    tmp = ckstrdup(Tcl_GetHostName());
    p = strchr(tmp, '.');
    if (p) *p = '\0';
    Tcl_SetVar2(interp, "tnm", "host", tmp, TCL_GLOBAL_ONLY);
    ckfree(tmp);

    /*
     * Get the user name. We try a sequence of different environment
     * variables in the hope to find something which works on all
     * systems.
     */

    user = getenv("USER");
    if (user == NULL) {
	user = getenv("USERNAME");
	if (user == NULL) {
	    user = getenv("LOGNAME");
	    if (user == NULL) {
		user = "unknown";
	    }
	}
    }
    Tcl_SetVar2(interp, "tnm", "user", user, TCL_GLOBAL_ONLY);

    /*
     * Search for a directory which allows to hold temporary files.
     * Save the directory name in the tnm(tmp) variable.
     */

    tmp = getenv("TEMP");
    if (! tmp) {
	tmp = getenv("TMP");
	if (! tmp) {
	    tmp = "/tmp";
	    if (access(tmp, W_OK) != 0) {
		tmp = ".";
	    }
	}
    }
    for (p = tmp; *p; p++) {
	if (*p == '\\') {
	    *p = '/';
	}
    }
    Tcl_SetVar2(interp, "tnm", "tmp", tmp, TCL_GLOBAL_ONLY);

    /*
     * Determine the architecture string which is used to store 
     * machine dependend files in the Tnm cache area.
     */

    machine = Tcl_GetVar2(interp, "tcl_platform", "machine", TCL_GLOBAL_ONLY);
    os = Tcl_GetVar2(interp, "tcl_platform", "os", TCL_GLOBAL_ONLY);
    vers = Tcl_GetVar2(interp, "tcl_platform", "osVersion", TCL_GLOBAL_ONLY);

    Tcl_DStringInit(&arch);
    if (machine && os && vers) {
	Tcl_DStringAppend(&arch, machine, -1);
	Tcl_DStringAppend(&arch, "-", 1);
	Tcl_DStringAppend(&arch, os, -1);
	Tcl_DStringAppend(&arch, "-", 1);
	Tcl_DStringAppend(&arch, vers, -1);
    } else {
	Tcl_DStringAppend(&arch, "unknown-os", -1);
    }

    /*
     * Initialize the tnm(cache) variable which points to a directory
     * where we can cache shared data between different instantiations
     * of the Tnm extension. We usually locate the cache in the users
     * home directory. However, if this fails (because the user does
     * not have a home), we locate the cache in the tmp file area.
     */

    path = Tcl_NewObj();
    Tcl_AppendStringsToObj(path, "~/.tnm", TNM_VERSION, NULL);
    if (Tcl_FSConvertToPathType(interp, path) == TCL_ERROR) {
	Tcl_SetStringObj(path, tmp, -1);
	Tcl_AppendStringsToObj(path, "/tnm", TNM_VERSION, NULL);
    }
    if (Tcl_FSConvertToPathType(interp, path) == TCL_OK) {
	(void) TnmMkDir(interp, path);
    }
    Tcl_SetVar2(interp, "tnm", "cache",
		Tcl_GetStringFromObj(path, NULL), TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(path);

    /*
     * Remove all white spaces and slashes from the architecture string 
     * because these characters are a potential source of problems and 
     * I really do not like white spaces in a directory name.
     */

    {
	char *d = Tcl_DStringValue(&arch);
	char *s = Tcl_DStringValue(&arch);

	while (*s) {
	    *d = *s;
	    if ((!isspace((int) *s)) && (*s != '/')) d++;
	    s++;
	}
	*d = '\0';
    } 

    Tcl_SetVar2(interp, "tnm", "arch", 
		Tcl_DStringValue(&arch), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&arch);
}

/*
 *----------------------------------------------------------------------
 *
 * SourceRcFiles --
 *
 *	This procedure evaluates a users Tnm initialization script.
 *
 * Results:
 *	1 if the file was found, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SourceRcFile(Tcl_Interp *interp, char *fileName)
{
    Tcl_DString temp;
    char *fullName;
    int result = 0;

    if (! fileName) {
	return 0;
    }

    Tcl_DStringInit(&temp);
    fullName = Tcl_TranslateFileName(interp, fileName, &temp);
    if (fullName == NULL) {
	TnmWriteMessage(Tcl_GetStringResult(interp));
	TnmWriteMessage("\n");
    } else {
	Tcl_Channel channel;
	channel = Tcl_OpenFileChannel(NULL, fullName, "r", 0);
	if (channel) {
	    Tcl_Close((Tcl_Interp *) NULL, channel);
	    result = 1;
	    if (Tcl_EvalFile(interp, fullName) != TCL_OK) {
		TnmWriteMessage(Tcl_GetStringResult(interp));
		TnmWriteMessage("\n");
	    }
	}
    }
    Tcl_DStringFree(&temp);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SourceInitFiles --
 *
 *	This procedure evaluates the Tnm initialization scripts.
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
SourceInitFiles(Tcl_Interp *interp)
{
    char *fileName;
    CONST char *library;
    Tcl_DString dst;

    library = Tcl_GetVar2(interp, "tnm", "library", TCL_GLOBAL_ONLY);
    if (! library) {
	Tcl_Panic("Tnm Tcl variable tnm(library) undefined.");
    }
    Tcl_DStringInit(&dst);
    Tcl_DStringAppend(&dst, library, -1);
    Tcl_DStringAppend(&dst, "/library/init.tcl", -1);
    if (Tcl_EvalFile(interp, Tcl_DStringValue(&dst)) != TCL_OK) {
	Tcl_DStringFree(&dst);
	return TCL_ERROR;
    }
    Tcl_DStringFree(&dst);

    /*
     * Load the user specific startup file. Check whether we
     * have a readable startup file so that we only complain
     * about errors when we are expected to complain.
     */

    fileName = getenv("TNM_RCFILE");
    if (fileName) {
	SourceRcFile(interp, fileName);
    } else {
	if (! SourceRcFile(interp, "~/.tnmrc")) {
	    SourceRcFile(interp, "~/.scottyrc");
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * InitCmds --
 *
 *	This procedure initializes the commands provided by the
 *	Tnm extension. The safe parameter determines if the command
 *	set should be restricted to the safe command subset.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl commands are created.
 *
 *----------------------------------------------------------------------
 */

static int
InitCmds(Tcl_Interp *interp, int safe)
{
    CmdInfo *cmdInfoPtr;
    Tcl_CmdInfo info;

    for (cmdInfoPtr = tnmCmds; cmdInfoPtr->name != NULL; cmdInfoPtr++) {
	/*
	 * Due to some Tcl limitations, we need to remove the Tnm
	 * namespace qualifier if we register the commands in a
	 * safe Tcl interpreter (since we can only hide commands
	 * in the global namespace). This is truely ugly - but Tcl
	 * forces me to do this.
	 */
	char *cmdName = cmdInfoPtr->name;
	if (safe && ! cmdInfoPtr->isSafe) {
	    char *newName = strstr(cmdName, "::");
	    while (newName) {
		cmdName = newName + 2;
		newName = strstr(cmdName, "::");
	    }
	}
	/*
	 * Check if the command already exists and return an error
	 * to ensure we detect name clashes while loading the Tnm
	 * extension.
	 */
	if (Tcl_GetCommandInfo(interp, cmdName, &info)) {
	    Tcl_AppendResult(interp, "command \"", cmdName,
			     "\" already exists", (char *) NULL);
	    return TCL_ERROR;
	}
	if (cmdInfoPtr->objProc) {
	    Tcl_CreateObjCommand(interp, cmdName, cmdInfoPtr->objProc,
			      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
	}
	/*
	 * Hide all unsafe commands from the interpreter
	 * if it is a safe Tcl interpreter.
	 */
	if (safe && ! cmdInfoPtr->isSafe) {
            if (Tcl_HideCommand(interp, cmdName, cmdName) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmInit --
 *
 *	This procedure is the platform independent entry point for 
 *	trusted and untrusted Tcl interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl variables are created.
 *
 *----------------------------------------------------------------------
 */

int
TnmInit(Tcl_Interp *interp, int safe)
{

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
#endif
    
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, "Tnm", TNM_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }

    Tcl_RegisterObjType(&tnmUnsigned64Type);
    Tcl_RegisterObjType(&tnmUnsigned32Type);
    Tcl_RegisterObjType(&tnmOctetStringType);
    Tcl_RegisterObjType(&tnmIpAddressType);

    InitVars(interp);
    TnmInitDns(interp);
    if (InitCmds(interp, safe) != TCL_OK) {
	return TCL_ERROR;
    }
    if (TnmSmxInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    return SourceInitFiles(interp);
}

