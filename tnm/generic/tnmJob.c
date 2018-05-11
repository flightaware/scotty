/*
 * tnmJob.c --
 *
 *	The simple job scheduler used to implement monitoring scripts.
 *	This version is derived from the job scheduler originally
 *	written in Tcl by Stefan Schoek <schoek@ibr.cs.tu-bs.de>.
 *	This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmJob.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tnmInt.h"
#include "tnmPort.h"

/* #define TNM_CAL */

/*
 * Structure used to describe a job.
 */

typedef struct Job {
    Tcl_Obj *cmd;		/* The command to evaluate. */
    Tcl_Obj *newCmd;		/* The new command to replace the current. */
    Tcl_Obj *exitCmd;		/* The command to cleanup exiting jobs. */
    Tcl_Obj *errorCmd;		/* The command to handle errors. */
    int interval;		/* The time interval value in ms. */
    int iterations;		/* The number of iterations of this job. */
#ifdef TNM_CAL
#if 0
    char weekdays;
    short months;
    int days;
    int hours;
    int minutes[2];
#endif
    struct tm cal;		/* The calendar based schedule point. */
#endif
    int remtime;		/* The remaining time in ms. */
    unsigned status;		/* The status of this job (see below). */
    Tcl_Obj *tagList;		/* The tags associated with this job. */
    Tcl_HashTable attributes;	/* The has table of job attributes. */
    Tcl_Command token;		/* The command token used by Tcl. */
    Tcl_Interp *interp;		/* The interpreter which owns this job. */
    struct Job *nextPtr;	/* Next job in our list of jobs. */
} Job;

/*
 * Every Tcl interpreter has an associated JobControl record. It
 * keeps track of the timers and the list of jobs known by the
 * interpreter. The name tnmJobControl is used to get/set the 
 * JobControl record.
 */

static char tnmJobControl[] = "tnmJobControl";

typedef struct JobControl {
    Job *jobList;		/* The list of jobs for this interpreter. */
    Job *currentJob;		/* The currently active job (if any). */
    Tcl_TimerToken timer;	/* The token for the Tcl timer. */
    Tcl_Time lastTime;		/* The last time stamp. */
} JobControl;

/* 
 * These are all possible status codes of an existing job.
 */

enum status { 
    suspended, waiting, running, expired 
};

static TnmTable statusTable[] =
{
    { suspended,	"suspended" },
    { waiting,		"waiting" },
    { running,		"running" },
    { expired,		"expired" },
    { 0, NULL }
};

#ifdef TNM_CAL
/*
 * Tables to map month names and weekday names to numbers and back.
 */

static char *months[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static char *weekdays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", 
    "Thursday", "Friday", "Saturday"
};

#define Periodic(x)	(x->cal.tm_sec < 0)
#define Calendar(x)	(x->cal.tm_sec >= 0)
#define SetPeriodic(x)	(x->cal.tm_sec = -1)
#define SetCalendar(x)	(x->cal.tm_sec = 1)
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static void
DeleteProc	(ClientData clientData);

static void
DestroyProc	(char *memPtr);

static void
ScheduleProc	(ClientData clientData);

static void 
NextSchedule	(Tcl_Interp *interp, JobControl *control);

static void 
AdjustTime	(JobControl *control);

static void
Schedule	(Tcl_Interp *interp, JobControl *control);

static int
CreateJob	(Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
static Tcl_Obj*
GetOption	(Tcl_Interp *interp, ClientData object, 
			     int option);
static int
SetOption	(Tcl_Interp *interp, ClientData object, 
			     int option, Tcl_Obj *objPtr);
static int
JobObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
static int
FindJobs	(Tcl_Interp *interp, JobControl *control,
			     int objc, Tcl_Obj *const objv[]);

/*
 * The options used to configure job objects.
 */

enum options { 
    optCommand, optExit, optError, optInterval, optIterations, 
    optStatus, optTags, optTime
#ifdef TNM_CAL
    , optWeekDay, optMonth, optDay, optHour, optMinute
#endif
};

static TnmTable optionTable[] = {
    { optCommand,	"-command" },
    { optError,		"-error" },
    { optExit,		"-exit" },
    { optInterval,	"-interval" },
    { optIterations,	"-iterations" },
    { optStatus,	"-status" },
    { optTags,		"-tags" },
    { optTime,		"-time" },
#ifdef TNM_CAL
    { optWeekDay,	"-weekday" },
    { optMonth,		"-month" },
    { optDay,		"-day" },
    { optHour,		"-hour" },
    { optMinute,	"-minute" },
#endif
    { 0, NULL }
};

static TnmConfig config = {
    optionTable,
    SetOption,
    GetOption
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
    JobControl *control = (JobControl *) clientData;

    /*
     * Note, we do not care about job objects since Tcl first
     * removes all commands before calling this delete procedure.
     * However, we have to delete the timer to make sure that
     * no further events are processed for this interpreter.
     */

    if (control) {
	if (control->timer) {
	    Tcl_DeleteTimerHandler(control->timer);
	}
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteProc --
 *
 *	This procedure removes the job from the list of active jobs and 
 *	releases all memory associated with a job object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A job is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteProc(ClientData clientData)
{
    Job **jobPtrPtr;
    Job *jobPtr = (Job *) clientData;
    JobControl *control = (JobControl *)
	Tcl_GetAssocData(jobPtr->interp, tnmJobControl, NULL);

    /*
     * First, update the list of all known jobs.
     */

    jobPtrPtr = &control->jobList;
    while (*jobPtrPtr && (*jobPtrPtr) != jobPtr) {
	jobPtrPtr = &(*jobPtrPtr)->nextPtr;
    }

    if (*jobPtrPtr) {
	(*jobPtrPtr) = jobPtr->nextPtr;
    }

    Tcl_EventuallyFree((ClientData) jobPtr, DestroyProc);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a job at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the job is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyProc(char *memPtr)
{
    Job *jobPtr = (Job *) memPtr;

    TnmAttrClear(&jobPtr->attributes);
    Tcl_DeleteHashTable(&jobPtr->attributes);

    Tcl_DecrRefCount(jobPtr->cmd);
    if (jobPtr->newCmd) {
	Tcl_DecrRefCount(jobPtr->newCmd);
    }
    if (jobPtr->errorCmd) {
	Tcl_DecrRefCount(jobPtr->errorCmd);
    }
    if (jobPtr->exitCmd) {
	Tcl_DecrRefCount(jobPtr->exitCmd);
    }
    if (jobPtr->tagList) {
	Tcl_DecrRefCount(jobPtr->tagList);
    }
    ckfree((char *) jobPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ScheduleProc --
 *
 *	This procedure is the callback of the Tcl event loop that
 *	invokes the scheduler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The scheduler gets called.
 *
 *----------------------------------------------------------------------
 */

static void
ScheduleProc(ClientData clientData)
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    JobControl *control = (JobControl *) 
	Tcl_GetAssocData(interp, tnmJobControl, NULL);

    if (control) {
	Tcl_Preserve((ClientData) interp);
	Schedule(interp, control);
	Tcl_Release((ClientData) interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NextSchedule --
 *
 *	This procedure registers a timer handler that will call us when
 *	the next job should be executed. An internal token is used to 
 *	make sure that we do not have more than one timer event pending
 *	for the job scheduler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new timer handler might be registered.
 *
 *----------------------------------------------------------------------
 */

static void
NextSchedule(Tcl_Interp *interp, JobControl *control)
{
    Job *jobPtr;
    int ms;

    if (control->timer) {
	Tcl_DeleteTimerHandler(control->timer);
	control->timer = NULL;
    }

    /* 
     * Calculate the minimum of the remaining times of all jobs
     * waiting. Tell the event manager to call us again if we found
     * a waiting job with remaining time >= 0. We also wait for 
     * expired jobs so that they will get removed from the job list.
     */

    ms = -1;
    for (jobPtr = control->jobList; jobPtr != NULL; jobPtr = jobPtr->nextPtr) {
        if (jobPtr->status == waiting || jobPtr->status == expired) {
	    if (ms < 0 || jobPtr->remtime < ms) {
		ms = (jobPtr->remtime < 0) ? 0 : jobPtr->remtime;
	    }
	}
    }
    
    if (ms < 0) {
	control->lastTime.sec = 0;
	control->lastTime.usec = 0;
    } else {
        control->timer = Tcl_CreateTimerHandler(ms, ScheduleProc, 
						(ClientData) interp);
    }
}

#ifdef TNM_CAL
int
NextSchedulePoint(JobControl *control, Job *jobPtr)
{
    struct tm *time;
    int secs;
    
    time = gmtime(&control->lastTime.sec);
    secs = mktime(time);
    if (jobPtr->cal.tm_min >= 0) {
	if (jobPtr->cal.tm_min > time->tm_min) {
	    time->tm_min = jobPtr->cal.tm_min;
	    goto done;
	}
	time->tm_min = jobPtr->cal.tm_min;
	time->tm_hour = (time->tm_hour + 1) % 24;
    }

done:
    time->tm_sec = 0;
    secs = mktime(time) - secs;
    fprintf(stderr, "seconds: %d\n", secs);
    return secs * 1000;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * AdjustTime --
 *
 *	This procedure updates the remaining time of all jobs in the 
 *	queue that are not suspended and sets lastTime to the current 
 *	time.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The remaining times of all active jobs is updated.
 *
 *----------------------------------------------------------------------
 */

static void
AdjustTime(JobControl *control)
{
    int delta;
    Job *jobPtr;
    Tcl_Time currentTime;

    if (control->lastTime.sec == 0 && control->lastTime.usec == 0) {
	Tcl_GetTime(&control->lastTime);
	return;
    }

    Tcl_GetTime(&currentTime);

    delta = (currentTime.sec - control->lastTime.sec) * 1000 
	    + (currentTime.usec - control->lastTime.usec) / 1000;

    control->lastTime = currentTime;

    /*
     * Do not increase remaining times of any jobs if the clock
     * has moved backwards.
     */

    if (delta <= 0) {
	return;
    }

    for (jobPtr = control->jobList; jobPtr; jobPtr = jobPtr->nextPtr) {
        if (jobPtr->status != suspended) {
	    jobPtr->remtime -= delta;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Schedule --
 *
 *	This procedure is called to schedule the next job. It first 
 *	checks for jobs that must be processed. It finally tells the 
 *	event mechanism to repeat itself when the next job needs
 *	attention. This function also cleans up the job queue by 
 *	removing all jobs that are waiting in the state expired.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Jobs might be activated and expired jobs are removed.
 *
 *----------------------------------------------------------------------
 */

static void
Schedule(Tcl_Interp *interp, JobControl *control)
{
    Job *jobPtr;
    int code, len;

    /*
     * Refresh the remaining time of all active jobs.
     */

    AdjustTime(control);

    /*
     * Execute waiting jobs with remaining time less or equal 0. 
     * Set the job status to expired if the number of iterations
     * reaches zero.
     */

  restart:
    for (jobPtr = control->jobList; jobPtr != NULL; jobPtr = jobPtr->nextPtr) {

	if (jobPtr->newCmd) {
	    Tcl_DecrRefCount(jobPtr->cmd);
	    jobPtr->cmd = jobPtr->newCmd;
	    jobPtr->newCmd = NULL;
	}

	if ((jobPtr->status == waiting) && (jobPtr->remtime <= 0)) {

	    Tcl_Preserve((ClientData) jobPtr);
	    control->currentJob = jobPtr;
	    jobPtr->status = running;

	    Tcl_AllowExceptions(interp);
	    code = Tcl_GlobalEvalObj(interp, jobPtr->cmd);
	    if (code == TCL_ERROR) {
		(void) Tcl_GetStringFromObj(jobPtr->errorCmd, &len);
		if (len > 0) {
		    Tcl_GlobalEvalObj(interp, jobPtr->errorCmd);
		} else {
		    const char *name;
		    name = Tcl_GetCommandName(interp, jobPtr->token);
		    Tcl_AddErrorInfo(interp, "\n    (script bound to job - ");
		    Tcl_AddErrorInfo(interp, name);
		    Tcl_AddErrorInfo(interp, " deleted)");
		    Tcl_BackgroundError(interp);
		    jobPtr->status = expired;
		}
	    }
    
	    Tcl_ResetResult(interp);
	    if (jobPtr->status == running) {
		jobPtr->status = waiting;
	    }
	    control->currentJob = NULL;

#ifdef TNM_CAL
	    if (Periodic(jobPtr)) {
		jobPtr->remtime = jobPtr->interval;
	    } else {
		jobPtr->remtime = NextSchedulePoint(control, jobPtr);
	    }
#else	    
	    jobPtr->remtime = jobPtr->interval;
#endif
	    if (jobPtr->iterations > 0) {
		jobPtr->iterations--; 
		if (jobPtr->iterations == 0) {
		    jobPtr->status = expired;
		}
	    }
	    Tcl_Release((ClientData) jobPtr);
	    goto restart;
	}
    }

    /*
     * Delete all jobs which have reached the status expired.
     * We must restart the loop for every deleted job as the
     * job list is modified by calling Tcl_DeleteCommand().
     */

  repeat:
    for (jobPtr = control->jobList; jobPtr != NULL; jobPtr = jobPtr->nextPtr) {
        if (jobPtr->status == expired) {
	    (void) Tcl_GetStringFromObj(jobPtr->exitCmd, &len);
	    if (len > 0) {
		(void) Tcl_GlobalEvalObj(interp, jobPtr->exitCmd);
	    }
	    Tcl_DeleteCommandFromToken(interp, jobPtr->token);
	    goto repeat;
        }
    }
    
    /*
     * Compute and subtract the time needed to execute the jobs
     * and schedule the next pass through the scheduler.
     */

    AdjustTime(control);
    NextSchedule(interp, control);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateJob --
 *
 *	This procedure creates a new job and appends it to our job
 *	list. The scheduler is called to update the timer.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The job list is updated and the scheduler is started.
 *
 *----------------------------------------------------------------------
 */

static int
CreateJob(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static unsigned nextId = 0;
    Job *jobPtr, *p;
    char *name;
    int code;
    JobControl *control = (JobControl *) 
	Tcl_GetAssocData(interp, tnmJobControl, NULL);

    jobPtr = (Job *) ckalloc(sizeof(Job));
    memset((char *) jobPtr, 0, sizeof(Job));
    jobPtr->cmd = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(jobPtr->cmd);
    jobPtr->errorCmd = jobPtr->cmd;
    Tcl_IncrRefCount(jobPtr->errorCmd);
    jobPtr->exitCmd = jobPtr->cmd;
    Tcl_IncrRefCount(jobPtr->exitCmd);
    jobPtr->interval = 1000;
#ifdef TNM_CAL
    SetPeriodic(jobPtr);
#endif
    jobPtr->status = waiting;
    jobPtr->interp = interp;
    jobPtr->tagList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(jobPtr->tagList);
    Tcl_InitHashTable(&(jobPtr->attributes), TCL_STRING_KEYS);

    code = TnmSetConfig(interp, &config, (ClientData) jobPtr, objc, objv);
    if (code != TCL_OK) {
	Tcl_EventuallyFree((ClientData) jobPtr, DestroyProc);
        return TCL_ERROR;
    }

    /*
     * Put the new job into the job list. We add it at the end
     * to preserve the order in which the jobs were created.
     */

    if (control->jobList == NULL) {
        control->jobList = jobPtr;
    } else {
        for (p = control->jobList; p->nextPtr != NULL; p = p->nextPtr) ;
	p->nextPtr = jobPtr;
    }

    /*
     * Create a new scheduling point for this new job.
     */

    NextSchedule(interp, control);

    /*
     * Create a new Tcl command for this job object.
     */

    name = TnmGetHandle(interp, "job", &nextId);
    jobPtr->token = Tcl_CreateObjCommand(interp, name, JobObjCmd,
					 (ClientData) jobPtr, DeleteProc);
    Tcl_SetResult(interp, name, TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetOption --
 *
 *	This procedure retrieves the value of a job option.
 *
 * Results:
 *	A pointer to the value formatted as a string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetOption(Tcl_Interp *interp, ClientData object, int option)
{
    char *status;
    Job *jobPtr = (Job *) object;
    JobControl *control = (JobControl *)
	Tcl_GetAssocData(jobPtr->interp, tnmJobControl, NULL);

    switch ((enum options) option) {
    case optCommand:
 	return jobPtr->newCmd ? jobPtr->newCmd : jobPtr->cmd;
    case optError:
	return jobPtr->errorCmd;
    case optExit:
	return jobPtr->exitCmd;
    case optInterval:
 	return Tcl_NewIntObj(jobPtr->interval);
    case optIterations:
 	return Tcl_NewIntObj(jobPtr->iterations);
    case optStatus:
	status = TnmGetTableValue(statusTable, jobPtr->status);
 	return Tcl_NewStringObj(status, -1);
    case optTags:
	return jobPtr->tagList;
    case optTime:
	if (control) {
	    AdjustTime(control);
	}
 	return Tcl_NewIntObj(jobPtr->remtime);
#ifdef TNM_CAL
    case optWeekDay:
	if (Periodic(jobPtr)) {
	    return NULL;
	}
	if (jobPtr->cal.tm_wday >= 0 
	    && jobPtr->cal.tm_wday <= sizeof(weekdays)) {
	    return Tcl_NewStringObj(weekdays[jobPtr->cal.tm_wday], -1);
	} else {
	    return Tcl_NewStringObj(NULL, 0);
	}
    case optMonth:
	if (Periodic(jobPtr)) {
	    return NULL;
	}
	if (jobPtr->cal.tm_mon >= 0 
	    && jobPtr->cal.tm_mon <= sizeof(months)) {
	    return Tcl_NewStringObj(months[jobPtr->cal.tm_mon], -1);
	} else {
	    return Tcl_NewStringObj(NULL, 0);
	}
    case optDay:
	if (Periodic(jobPtr)) {
	    return NULL;
	}
	if (jobPtr->cal.tm_mday >= 1 && jobPtr->cal.tm_mday <= 31) {
	    return Tcl_NewIntObj(jobPtr->cal.tm_mday);
	} else {
	    return Tcl_NewStringObj(NULL, 0);
	}
    case optHour:
	if (Periodic(jobPtr)) {
	    return NULL;
	}
	if (jobPtr->cal.tm_hour >= 0 && jobPtr->cal.tm_hour <= 23) {
	    return Tcl_NewIntObj(jobPtr->cal.tm_hour);
	} else {
	    return Tcl_NewStringObj(NULL, 0);
	}
    case optMinute:
	if (Periodic(jobPtr)) {
	    return NULL;
	}
	return Tcl_NewIntObj(jobPtr->cal.tm_min);
#endif
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOption --
 *
 *	This procedure modifies a single option of a job object.
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
SetOption(Tcl_Interp *interp, ClientData object, int option, Tcl_Obj *objPtr)
{
    Job *jobPtr = (Job *) object;
    int num, status;
    JobControl *control = (JobControl *)
	Tcl_GetAssocData(jobPtr->interp, tnmJobControl, NULL);

    switch ((enum options) option) {
    case optCommand:
	if (jobPtr->newCmd) {
	    Tcl_DecrRefCount(jobPtr->newCmd);
	}
	jobPtr->newCmd = objPtr;
	Tcl_IncrRefCount(jobPtr->newCmd);
	break;
    case optError:
	Tcl_DecrRefCount(jobPtr->errorCmd);
	jobPtr->errorCmd = objPtr;
	Tcl_IncrRefCount(jobPtr->errorCmd);
	break;
    case optExit:
	Tcl_DecrRefCount(jobPtr->exitCmd);
	jobPtr->exitCmd = objPtr;
	Tcl_IncrRefCount(objPtr);
	break;
    case optInterval:
	if (TnmGetPositiveFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	jobPtr->interval = num;
	break;
    case optIterations:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	jobPtr->iterations = num;
	break;
    case optStatus:
 	status = TnmGetTableKeyFromObj(interp, statusTable, objPtr, "status");
	if (status < 0) {
	    return TCL_ERROR;
	}
	jobPtr->status = (status == running) ? waiting : status;

	/*
	 * Compute the current time offsets and create a new 
	 * scheduling point. A suspended job may have resumed 
	 * and we must make sure that our scheduler is running.
	 */
	
	if (control) {
	    AdjustTime(control);
	    NextSchedule(interp, control);
	}
	break;
    case optTags:
	Tcl_DecrRefCount(jobPtr->tagList);
	jobPtr->tagList = objPtr;
	Tcl_IncrRefCount(jobPtr->tagList);
	break;
    case optTime:
	break; 
#ifdef TNM_CAL
    case optWeekDay:
	/* s = Tcl_GetStringFromObj(objPtr, NULL); */
	break;
    case optMonth:
	break;
    case optDay:
	break;
    case optHour:
	break;
    case optMinute:
	if (TnmGetIntRangeFromObj(interp, objPtr, 0, 59, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	SetCalendar(jobPtr);
	jobPtr->cal.tm_min = num;
	break;
#endif
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * JobObjCmd --
 *
 *	This procedure implements the job object command.
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
JobObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int result;
    Job *jobPtr = (Job *) clientData;
    JobControl *control = (JobControl *) 
	Tcl_GetAssocData(interp, tnmJobControl, NULL);

    enum commands {
	cmdAttribute, cmdCget, cmdConfigure, cmdDestroy, cmdWait
    } cmd;

    static const char *cmdTable[] = {
	"attribute", "cget", "configure", "destroy", "wait", (char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    Tcl_Preserve((ClientData) jobPtr);

    switch (cmd) {

    case cmdAttribute:
	if (objc < 2 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?name ?value??");
	    return TCL_ERROR;
	}
	switch (objc) {
	case 2:
	    TnmAttrList(&jobPtr->attributes, interp);
	    break;
	case 3:
	    result = TnmAttrSet(&jobPtr->attributes, interp, 
				Tcl_GetStringFromObj(objv[2], NULL), NULL);
	    break;
	case 4:
	    TnmAttrSet(&jobPtr->attributes, interp, 
		       Tcl_GetStringFromObj(objv[2], NULL),
		       Tcl_GetStringFromObj(objv[3], NULL));
	    break;
	}
	break;

    case cmdConfigure:
	result = TnmSetConfig(interp, &config, (ClientData) jobPtr, 
			      objc, objv);
	break;

    case cmdCget:
	result = TnmGetConfig(interp, &config, (ClientData) jobPtr, 
			      objc, objv);
	break;

    case cmdDestroy:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	jobPtr->status = expired;
	break;

    case cmdWait:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
            break;
	}
	if (control) {
	    Job *jPtr;
	repeat:
	    for (jPtr = control->jobList; jPtr; jPtr = jPtr->nextPtr) {
		if (jPtr->status == waiting && jPtr == jobPtr) {
		    Tcl_DoOneEvent(0);
		    goto repeat;
		}
	    }
	}
	break;
    }

    Tcl_Release((ClientData) jobPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FindJobs --
 *
 *	This procedure is invoked to process the "find" command
 *	option of the job command.
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
FindJobs(Tcl_Interp *interp, JobControl *control, int objc, Tcl_Obj *const objv[])
{
    int i, result, status = -1;
    Job *jobPtr;
    Tcl_Obj *listPtr, *patList = NULL;

    enum options { optStatus, optTags } option;

    static const char *optionTable[] = {
        "-status", "-tags", (char *) NULL
    };

    if (objc % 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value? ?option value? ...");
	return TCL_ERROR;
    }

    for (i = 2; i < objc; i++) {
	result = Tcl_GetIndexFromObj(interp, objv[i++], optionTable, 
				     "option", TCL_EXACT, (int *) &option);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (option) {
	case optStatus:
	    status = TnmGetTableKeyFromObj(interp, statusTable, 
					   objv[i], "status");
	    if (status < 0) {
		return TCL_ERROR;
	    }
	    break;
	case optTags:
	    patList = objv[i];
	    break;
	}
    }

    listPtr = Tcl_GetObjResult(interp);
    for (jobPtr = control->jobList; jobPtr; jobPtr = jobPtr->nextPtr) {
	if (status >= 0 && (int) jobPtr->status != status) continue;
	if (patList) {
	    int match = TnmMatchTags(interp, jobPtr->tagList, patList);
	    if (match < 0) {
		return TCL_ERROR;
	    }
	    if (! match) continue;
	}
	{
	    const char *name = Tcl_GetCommandName(interp, jobPtr->token);
	    Tcl_Obj *elemObjPtr = Tcl_NewStringObj(name, -1);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_JobObjCmd --
 *
 *	This procedure is invoked to process the "job" command.
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
Tnm_JobObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Job *jobPtr;
    int result;
    JobControl *control = (JobControl *) 
	Tcl_GetAssocData(interp, tnmJobControl, NULL);

    enum commands { 
	cmdCreate, cmdCurrent, cmdFind, cmdSchedule, cmdWait
    } cmd;

    static const char *cmdTable[] = {
	"create", "current", "find", "schedule", "wait", (char *) NULL
    };

    if (! control) {
	control = (JobControl *) ckalloc(sizeof(JobControl));
	memset((char *) control, 0, sizeof(JobControl));
	Tcl_SetAssocData(interp, tnmJobControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {

    case cmdCreate:
	result = CreateJob(interp, objc, objv);
	break;

    case cmdCurrent:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	if (control->currentJob && control->currentJob->interp == interp) {
	    const char *name;
	    name = Tcl_GetCommandName(interp, control->currentJob->token);
	    Tcl_SetResult(interp, (char *) name, TCL_STATIC);
	}
	break;

    case cmdFind:
	result = FindJobs(interp, control, objc, objv);
	break;

    case cmdSchedule:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	Schedule(interp, control);
	break;

    case cmdWait:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
    repeat:
	for (jobPtr = control->jobList; jobPtr; jobPtr = jobPtr->nextPtr) {
	    if (jobPtr->status == waiting) {
		Tcl_DoOneEvent(0);
		goto repeat;
	    }
	}
	break;
    }

    return result;
}

