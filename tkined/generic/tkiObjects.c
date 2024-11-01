/*
 * tkiObjects.c --
 *
 *	This file contains the low level object handling code, mainly 
 *	object creation and communication with external processes.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tkined.h"
#include "tkiPort.h"

/*
 * Our hashtable that maps object ids to the object structure.
 */

Tcl_HashTable tki_ObjectTable;

/*
 * This variable is set to true to ignore further traces. It is used
 * to implement the -notrace option of the ined command.
 */

static int ignoretrace = 0;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int 
ObjectCommand	      (ClientData clientData, Tcl_Interp *interp,
				   int argc, const char **argv);
static void 
do_debug              (Tki_Object *object, Tcl_Interp *interp,
				   int argc, const char **argv, const char *result);
/* 
 * Find an object by its id.
 */

Tki_Object* 
Tki_LookupObject (const char *id)
{
    Tcl_HashEntry *entryPtr;

    if (id == NULL) {
	return NULL;
    }

    entryPtr = Tcl_FindHashEntry (&tki_ObjectTable, id);
    if (entryPtr == NULL) {
        return NULL;
    }

    return (Tki_Object *) Tcl_GetHashValue (entryPtr);
}

/*
 * This function gets called whenever a command is processed by
 * an object. It is used to write a trace to an interpreter which
 * can be used to drive client server interactions.
 *
 * Here is lots of room for optimizations: First, we should generate
 * the message once. Second, we should maintain our own list of trace
 * interpreters to avoid searching throught the whole hash table.
 */

void
TkiTrace (Tki_Editor *editor, Tki_Object *object, char *cmd, int argc, const char **argv, char *result)
{

    /* **** start of hack **** */

    /* if we get called with a trace for an unknown editor, we save
       the trace and put it out, if we are called with an editor but 
       without any further arguments. this hack is needed to trace 
       create commands correctly since the new objects do not have an 
       editor pointer when the trace is done */

    static Tki_Object *old_object = (Tki_Object *) NULL;
    static char *old_cmd = (char *) NULL;
    static char *old_result = (char *) NULL;
    static int old_argc = 0;
    static char **old_argv = NULL;

    int i, code;

    if (!editor && result && cmd) {
	old_object = object;
	old_cmd = ckstrdup (cmd);
	old_result = ckstrdup (result);
	old_argc = argc;
	old_argv = (char **) ckalloc (sizeof(char*) * (argc + 1));
	for (i = 0; i < argc; i++) {
	    old_argv[i] = ckstrdup (argv[i]);
	}
    }

    if (editor && result == (char *) NULL && cmd == (char *) NULL) {
	if (!old_cmd) return;
	TkiTrace (editor, old_object, old_cmd, old_argc, (const char **) old_argv, old_result);
	old_object = (Tki_Object *) NULL;
	if (old_cmd) ckfree (old_cmd);
	old_cmd = (char *) NULL;
	ckfree (old_result); 
	old_result = (char *) NULL;
	for (i = 0; i < old_argc; i++) {
	    ckfree (old_argv[i]);
	}
	ckfree ((char *) old_argv);
	old_argv = (char **) NULL;
	old_argc = 0;
	return;
    }

    /* **** end of hack **** */

    if (ignoretrace) return;

    if (editor && editor->traceCount > 0) {

	Tcl_HashEntry *entryPtr;
	Tcl_HashSearch ht_search;
	Tki_Object *obj;

	entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &ht_search);
	while (entryPtr != NULL) {

	    obj = (Tki_Object *) Tcl_GetHashValue (entryPtr);

	    if (obj->trace && (obj->editor == editor)) {
		
		int len;
		Tcl_DString dst;

		Tcl_DStringInit (&dst);
		Tcl_DStringAppend (&dst, obj->traceCmd, -1);

		Tcl_DStringAppend (&dst, " ", -1);
		Tcl_DStringAppend (&dst, cmd, -1);
		if (object != NULL) {
		    Tcl_DStringAppendElement (&dst, object->id);
		}
		for (i = 0; i < argc; i++) {
		    char *tmp = ckstrdupnn (argv[i]);
		    Tcl_DStringAppendElement (&dst, tmp);
		    ckfree (tmp);
		}
		if (result != NULL) {
		    Tcl_DStringAppendElement (&dst, ">");
		    Tcl_DStringAppendElement (&dst, result);
		}
		Tcl_DStringAppend (&dst, "\n", 1);

		len = Tcl_DStringLength(&dst);
		code = Tcl_Write(obj->channel, Tcl_DStringValue(&dst), len);
		if (code == len) {
		    code = Tcl_Flush(obj->channel);
		}
		if (code < 0) {
		    fprintf(stderr, "trace: failed to write to %s: %d\n",
			    obj->id, Tcl_GetErrno());
		}

		Tcl_DStringFree (&dst);

	    }
	    entryPtr = Tcl_NextHashEntry (&ht_search);
	}

#if 1
	/*
	 * Handle all pending events.
	 */
	
	while (Tk_DoOneEvent(TK_DONT_WAIT) != 0) {
	  /* Empty loop body */
	}
#endif
    }
}

/*
 * This function gets called to write debug messages whenever a 
 * command is processed by an object.
 */

static void 
do_debug (Tki_Object *object, Tcl_Interp *interp, int argc, const char **argv, const char *result)
{
    int i;

    if (!tki_Debug) return;

    if (object != NULL) {
	if (object->editor != NULL) {
	    printf ("# %s:%s ", object->editor->id, object->id);
	} else {
	    printf ("# (?):%s ", object->id);
	}
    } else {
	printf ("#  ");
    }
    for (i = 0; i < argc; i++) {
	printf ("%s ", argv[i]);
    }
    if (result != NULL) {
	printf ("> %s\n", result);
    } else {
	printf ("\n");
    }
}

/*
 * Tki_CreateObject() creates a new object. The first argument is the type 
 * name e.g. NODE and the second argument is the keyword create. Further
 * arguments are type specific. Consult the Tkined man page for more details
 * about this. The create functions initializes a new object and registers
 * a command that can be used later to query or modify the object state.
 */

int 
Tki_CreateObject (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    Tki_Object *object;
    Tcl_HashEntry *entryPtr;
    int flag;

    if (argc < 2) {
	Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
        return TCL_ERROR;
    }

    object = (Tki_Object *) ckalloc(sizeof(Tki_Object));
    memset ((void *) object, 0, sizeof (Tki_Object));

    object->type = string_to_type (argv[0]);
    if (object->type == TKINED_NONE) {
	ckfree ((char *) object);
	Tcl_SetResult (interp, "unknown object type", TCL_STATIC);
        return TCL_ERROR;
    }

    object->id = ckstrdup("");
    object->name = ckstrdup("");
    object->address = ckstrdup("");
    object->action = ckstrdup("");
    object->icon = ckstrdup("");
    object->font = ckstrdup("fixed");
    object->color = ckstrdup("black");
    object->label = ckstrdup("");
    object->text = ckstrdup("");
    object->canvas = ckstrdup("");
    object->items = ckstrdup("");    
    object->size = ckstrdup("");
    object->links = ckstrdup("");
    object->scale = 100;
    object->points = ckstrdup("");

    Tcl_InitHashTable (&(object->attr), TCL_STRING_KEYS);

    /* call the create member function to do type specific initialization */

    flag = ObjectCommand ((ClientData) object, interp, argc, argv);
    if (flag != TCL_OK) return flag;

    /* throw the new object in the hash table */

    entryPtr = Tcl_CreateHashEntry (&tki_ObjectTable, object->id, &flag);
    if (flag == 0) {
	Tcl_ResetResult (interp);
	Tcl_AppendResult (interp, "failed to create hash entry for ",
			  object->id, (char *) NULL);
	return TCL_ERROR;
    } else {
	Tcl_SetHashValue (entryPtr, (ClientData) object);
    }
    
    /* create a tcl command for the new object */

    Tcl_CreateCommand (interp, object->id, (Tcl_CmdProc *) ObjectCommand,
		       (ClientData) object, Tki_DeleteObject);

    if (tki_Debug) 
	    do_debug ((Tki_Object *) NULL, interp, argc, argv, object->id);

    Tcl_SetResult (interp, object->id, TCL_STATIC);
    return TCL_OK;
}


/*
 * Tki_DeleteObject destroys an object. It frees everything allocated
 * before releasing the object structure.
 */

void 
Tki_DeleteObject (ClientData clientData)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch ht_search;
    Tki_Object *object = (Tki_Object *) clientData;

    entryPtr = Tcl_FindHashEntry (&tki_ObjectTable, object->id);
    if (entryPtr != NULL) Tcl_DeleteHashEntry (entryPtr);

    ckfree (object->id);
    ckfree (object->name);
    ckfree (object->address);
    ckfree (object->action);
    ckfree (object->icon);
    ckfree (object->font);
    ckfree (object->color);
    ckfree (object->label);
    ckfree (object->text);
    ckfree (object->canvas);
    ckfree (object->items);
    ckfree (object->size);
    ckfree (object->links);
    ckfree (object->points);
    if (object->valuePtr != NULL) {
	ckfree ((char *) object->valuePtr);
    }

    entryPtr = Tcl_FirstHashEntry(&(object->attr), &ht_search);
    while (entryPtr != NULL) {
        ckfree ((char *) Tcl_GetHashValue (entryPtr));
        entryPtr = Tcl_NextHashEntry (&ht_search);
    }

    Tcl_DeleteHashTable (&(object->attr));
    
    if (object->type == TKINED_INTERPRETER) {

	Tcl_UnregisterChannel((Tcl_Interp *) NULL, object->channel);

	if (object->cmd) {
	    Tcl_DStringFree (object->cmd);
	    ckfree ((char *) object->cmd);
	}

	/* 
	 * Hack against zombies - it does help sometimes.
	 */

	Tcl_ReapDetachedProcs();
    }
    
    ckfree ((char*) object);
}

/*
 * Execute a received ined command. The interpreter object
 * is given by object. Commands that affect the user interface
 * are processed by calling the appropriate tk procedure. Commands
 * that change an object status are handled by calling the appropriate
 * member function.
 */

int 
ined (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    Tki_Object *object = (Tki_Object *) clientData;
    char *cmd;
    const char *tmp;
    int result = TCL_ERROR;
    int i;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch ht_search;
    int update = 1;
    Tcl_CmdInfo	info;

    char tbuf[1024];
    
    /* ignore everything not starting with the key word 'ined' */
    
    if ((argc < 2) || (argv[0][0] != 'i') || strcmp(argv[0], "ined") != 0) 
    {
	fprintf (stderr, "ined() called without any arguments!\n");
	return TCL_CONTINUE;
    }
    
    /* check for the -noupdate and -notrace option */
    
    while ( (argc > 2) && (argv[1][0] == '-')) {
	if (strcmp (argv[1], "-noupdate") == 0) {
	    update = 0;
	} else if (strcmp (argv[1], "-notrace") == 0) {
	    ignoretrace = 1;
	} else break;
	for ( argc--, i = 1; i < argc; i++) argv[i] = argv[i+1];
    }

    /* process 'ined queue <n>' messages */
    
    if (   (argc == 3) 
	&& (argv[1][0] == 'q')
        && (strcmp(argv[1], "queue") == 0)) {
	int len;
        if (Tcl_GetInt (interp, argv[2], &len) == TCL_OK) {
	    object->queue = len;
	    sprintf (buffer, "%s__queue %s %d", 
		     type_to_string (object->type), 
		     object->id, object->queue);
	    Tcl_Eval (interp, buffer);
	}
	ignoretrace = 0;
	return TCL_RETURN; /* do not send an acknowledge! */
    }

    /* process 'ined restart' messages */

    if (   (argc > 1) 
	&& (argv[1][0] == 'r')
        && (strcmp(argv[1], "restart") == 0)) {

	/* save a list of message that is sent to an interpreter if it
	   gets restarted from a saved file */

	if (argc == 3) {
	    STRCOPY(object->action, argv[2]);
	}

	Tcl_SetResult (interp, object->action, TCL_VOLATILE);
	ignoretrace = 0;
	return TCL_OK;
    }
    
    /* process the page command */

    if (   (argc > 1) 
	&& (argv[1][0] == 'p')
        && (strcmp(argv[1], "page") == 0)) {
	if (object->editor) {
	    if (argc > 2) {
		Tki_EditorPageSize (object->editor, interp, 1, &argv[2]);
		if (argc > 3) {
		    Tki_EditorOrientation (object->editor, interp, 
					   1, &argv[3]);
		}
	    }
	    Tcl_ResetResult (interp);
	    snprintf (tbuf, sizeof(tbuf), "%s %s", object->editor->pagesize, 
		      object->editor->landscape ? "landscape" : "portrait");
            Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
	}
	ignoretrace = 0;
	return TCL_OK;
    }
    
    /* check for a dialog command by searching for Dialog__<cmd> */

    if (argc > 1) {
	buffersize(strlen(argv[1])+10);
	sprintf (buffer, "Dialog__%s", argv[1]);
	if (Tcl_GetCommandInfo (interp, buffer, &info) == 1) {
	    cmd = Tcl_Merge (argc-2, argv+2);
	    result = Tcl_VarEval (interp, buffer, " ",
				  object->canvas, " ", cmd, (char *) NULL);
	    if ((result == TCL_OK) && tki_Debug) {
		do_debug (object, interp, argc, argv, (char *) NULL);
	    }
	    ckfree (cmd);
	    ignoretrace = 0;
	    return result;
	}
	Tcl_ResetResult (interp);
    }

    /* catch the retrieve command with no arguments. */

    if (   (argc == 2)
	&& (argv[1][0] == 'r')
        && (strcmp (argv[1], "retrieve") == 0)) {

	Tki_Object *obj;
        Tcl_DString ds;

	Tcl_DStringInit (&ds);
	entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &ht_search);
	while (entryPtr != NULL) {
	    obj = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	    if (strcmp (obj->canvas, object->canvas) == 0) {
		result = m_retrieve (interp, obj, 0, (char **) NULL);
		if (result == TCL_OK) {
		    Tcl_DStringAppendElement(&ds, Tcl_GetStringResult(interp));
		}
		Tcl_ResetResult (interp);
	    }
	    entryPtr = Tcl_NextHashEntry (&ht_search);
	}
	Tcl_DStringResult (interp, &ds);
	Tcl_DStringFree (&ds);
	ignoretrace = 0;
	return TCL_OK;
    }

    if (   (argc == 2)
	&& (argv[1][0] == 'p')
        && (strcmp (argv[1], "postscript") == 0)) {

	if (object->editor) {
	    Tki_EditorPostScript (object->editor, interp, 0, (char **) NULL);
	}

        Tcl_SetResult (interp, ckstrdupnn(Tcl_GetStringResult(interp)), TCL_DYNAMIC);
	return TCL_OK;
    }

    /* catch the size command with no arguments */

    if ((argc == 2) 
	&& (argv[1][0] == 's')
        && (strcmp (argv[1], "size") == 0)) {
	if (object->editor) {
	    Tcl_ResetResult (interp);
	    snprintf(tbuf, sizeof(tbuf), "0 0 %d %d", 
		     object->editor->width, object->editor->height);
            Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
	    if (tki_Debug) 
	      do_debug (object, interp, argc, argv, tbuf);
	}
	ignoretrace = 0;
	return TCL_OK;
    }

    /* catch the select command with no arguments */

    if (   (argc == 2)
	&& (argv[1][0] == 's')
        && (strcmp (argv[1], "select") == 0)) {
	Tcl_ResetResult (interp);
	Tki_EditorSelection (object->editor, interp, 0, (char **) NULL);
	ignoretrace = 0;
	return TCL_OK;
    }

    /* catch the trace command. I use the member pointer to
       save the name of the callback procedure. */

    if ((argc == 3) 
	&& (argv[1][0] == 't')
        && (strcmp (argv[1], "trace") == 0)) {
	if (object->traceCmd) {
	    ckfree (object->traceCmd);
	}
	object->traceCmd = ckstrdup (argv[2]);
	if (object->editor) {
	    if (strlen(argv[2]) > 0) {
		object->editor->traceCount++;
		object->trace = 1;
	    } else {
		object->editor->traceCount--;
		object->trace = 0;
	    }
	}
	ignoretrace = 0;
	return TCL_OK;
    }
    
    /* map the external syntax to internal syntax */

    if (argc > 2) {

	Tki_Object *obj;

        tmp = argv[1]; argv[1] = argv[2]; argv[2] = tmp;
	Tcl_ResetResult (interp);
	obj = Tki_LookupObject (argv[1]);

	if (obj != NULL) {
	    
	    /* Do object related commands here. */

	    result = ObjectCommand ((ClientData) obj, interp, argc-1, argv+1);

	} else {

	    /* Check for create commands. Make sure that new created
	       objects get their canvas attribute setup to the canvas
	       of the interpreter object. */
	    
	    if ((argv[2][0] == 'c') && (strcmp(argv[2], "create") == 0)) {

		cmd = Tcl_Merge (argc-1, argv+1);
#if defined(DEBUG) && defined(stderr) && defined(__FILE__) && defined(__LINE__)
                fprintf(stderr, "file: %s +%d cmd: %s\n", __FILE__, __LINE__, cmd);
#endif /* DEBUG && stderr && __FILE__ && __LINE__ */
		result = Tcl_Eval (interp, cmd);
		ckfree (cmd);

		if (result == TCL_OK) {
		    obj = Tki_LookupObject(Tcl_GetStringResult(interp));
		    if (obj->type == TKINED_MENU) {
			STRCOPY (obj->links, object->id);
		    }
		    if (obj->type == TKINED_LOG) {
			STRCOPY (obj->links, object->id);
		    }
		    obj->editor = object->editor;
		    TkiTrace (obj->editor, (Tki_Object *) NULL, 
			    (char *) NULL, 0, (const char **) NULL, (char *) NULL);
		    if (obj->type == TKINED_GRAPH) {
			Tki_EditorGraph (obj->editor, interp, 
					 0, (char **) NULL);
			Tcl_AppendResult (interp, ".blt", (char *) NULL);
#if 0
			m_canvas (interp, obj, 1, &interp->result);
#endif /* 0 */
			
			/* FIXME: the following appears to be wrong, very wrong.
			 * m_canvas expects char **argv, probably with trailing
			 * NULL pointer while we just provide a const char *,
			 * which sets off -Wincompatible-pointer-types
			 */
			m_canvas (interp, obj, 1, Tcl_GetStringResult(interp));
		    } else {
			m_canvas (interp, obj, 1, &object->canvas);
		    }
		    Tcl_SetResult (interp, obj->id, TCL_STATIC);
		}
          } else if ((argv[2][0] == 'e') && (strcmp(argv[2], "eval") == 0)) {

	      char *p;
              const char *tp;

              /* 
	       * Hand the rest of the command off for evaluation in
	       * the context of tkined.  Meanwhile we have to reorder
	       * the argument list to suit the external syntax.
               */

              tmp = argv[1]; argv[1] = argv[2]; argv[2] = tmp;

              cmd = Tcl_Merge (argc-2, argv+2);
              result = Tcl_Eval (interp, cmd);
              ckfree(cmd);

	      /*
	       * It seems we also have to replace any newlines with
	       * semicolons to keep the Tcl parser happy later on.
	       */

              /* XXX probably it should be Tcl_GetStringResult / SetResult */
	      //for (p = interp->result; *p; p++) {
              tp = Tcl_GetStringResult(interp);
	      /* TODO: are we really allowed to poke around in the
		 string returned by Tcl_GetStringResult()? */
              for (p = tp; *p; p++) {
		  if (*p == '\n') *p = ';';
	      }
              Tcl_SetResult(interp, (char *) tp, TCL_VOLATILE);

              tmp = argv[1]; argv[1] = argv[2]; argv[2] = tmp;

	    } else {

#if 0
		/* these errors are not always welcome - we don't use them */
		tmp = argv[1]; argv[1] = argv[2]; argv[2] = tmp;
		Tcl_AppendResult (interp, "unknown object or command: \"",
				  argv[0], (char *) NULL);
		for (i = 1; i < argc ; i++) {
		    Tcl_AppendResult (interp, " ", argv[i], (char *) NULL);
		}
		Tcl_AppendResult (interp, "\"", (char *) NULL);
		result = TCL_ERROR;
#else
		Tcl_ResetResult (interp);
		result = TCL_OK;
#endif
	    }
	}

        tmp = argv[1]; argv[1] = argv[2]; argv[2] = tmp;
    }

    if (update) {
	tmp = ckstrdup (Tcl_GetStringResult(interp));
        Tcl_Eval (interp, "update idletask");
	Tcl_SetResult (interp, (char *) tmp, TCL_DYNAMIC);
    }

    ignoretrace = 0;
    return result;
}

/*
 * This function gets called whenver an interpreter gets readable.
 * All available characters are read and added to the command buffer.
 * Complete commands are evaluated. 
 */

void 
receive(clientData, mask)
    ClientData clientData;    /* Describes command to execute. */
    int        mask;          /* Not used */
{
#define BUFFER_SIZE 4000
    Tki_Object *object = (Tki_Object *) clientData;
    Tcl_Interp *interp = object->interp;
    char input[BUFFER_SIZE+1];
    char *cmd;
    char *line;
    char *p;
    int count, len, code;
    int argc;
    const char **argv;
    Tcl_DString buf;

    if (object->done) {
	Tcl_DStringFree (object->cmd);
#if 0
	Tcl_DStringInit (object->cmd);
#endif
    }

    count = Tcl_Read(object->channel, input, BUFFER_SIZE);
#if 0
    {
	Tcl_Channel c = Tcl_GetStdChannel(TCL_STDERR);
	if (c) {
	    Tcl_Write(c, "** Tcl_Read() >", -1);
	    Tcl_Write(c, input, count);
	    Tcl_Write(c, "<\n", -1);
	}
    }
#endif

    if (count <= 0) {
	if (object->done) {   
	    m_delete (interp, object, 0, (const char **) NULL);
	    return;
	} else {
	    input[0] = 0;
	}
    } else {
	input[count] = 0;
    }

    cmd = Tcl_DStringAppend(object->cmd, input, count);

    if ( (!Tcl_CommandComplete(cmd)) || cmd[strlen(cmd)-1] != '\n') {
	object->done = 0;
        return;
    }
    object->done = 1;

    if (tki_Debug) {
	fprintf (stderr, "%s >> %s", object->id, cmd);
    }

    /* split the buffer with newlines and process each piece */
    for (line = cmd, p = cmd; *p != 0; p++) {

	if (*p != '\n') continue;
	*p = 0;

	if (Tcl_SplitList (interp, line, &argc, &argv) != TCL_OK) {
	    Tcl_ResetResult (interp);
	    puts (line);
	    line = p+1;
	    cmd = p+1;
	    continue;
	}

        Tcl_DStringInit (&buf);
	
	if ((argc > 1) && (strcmp(argv[0], "ined") == 0)) {

	    int res = ined ((ClientData) object, interp, argc, argv);

	    switch (res) {
	case TCL_OK: Tcl_DStringAppend (&buf, "ined ok ", -1); break;
	case TCL_ERROR: Tcl_DStringAppend (&buf, "ined error ", -1); break;
	    }
	} else {
	    puts (line);
	}

	ckfree ((char*) argv);

	/* write back an acknowledge and the result */
	    
	if (Tcl_DStringLength (&buf) > 0) {
	    Tcl_DStringAppend (&buf, Tcl_GetStringResult(interp), -1);
	    Tcl_DStringAppend (&buf, "\n", 1);
	    
	    len = Tcl_DStringLength (&buf);
	    code = Tcl_Write(object->channel, Tcl_DStringValue(&buf), len);
	    if (code == len)  {
		code = Tcl_Flush(object->channel);
	    }
	    if (code < 0) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "write to ", object->id,
				 " failed: ", Tcl_PosixError (interp),
				 (char *) NULL);
		return;
	    }
	    
	    if (tki_Debug) {
		fprintf (stderr, "%s << %s", object->id, 
			 Tcl_DStringValue (&buf));
		fflush (stderr);
	    }
	}

	Tcl_DStringFree (&buf);

	line = p+1;
	cmd = p+1;
    }
}

/*
 * All methods are dispatched using the following table. Depending
 * on the type and the name of an object, we choose the function
 * to call. The type TKINED_ALL matches any type.
 */

typedef struct Method {
    int type;
    char *cmd;
    int (*fnx)(); /* (Tcl_Interp*, Tki_Object*, int, char**) */
} Method;

static Method methodTable[] = {

        { TKINED_ALL,         "create",      m_create },

	{ TKINED_ALL,         "retrieve",    m_retrieve },

        { TKINED_ALL,         "id",          m_id },

        { TKINED_ALL,         "type",        m_type },

        { TKINED_ALL,	      "parent",	     m_parent },

	{ TKINED_ALL,         "name",        m_name },

	{ TKINED_ALL,         "canvas",      m_canvas },

	{ TKINED_ALL,         "editor",      m_editor },

	{ TKINED_ALL,         "items",       m_items },

	{ TKINED_ALL,         "attribute",   m_attribute },

	{ TKINED_NODE | TKINED_NETWORK | TKINED_GROUP | TKINED_LOG |
	  TKINED_REFERENCE | TKINED_STRIPCHART | TKINED_BARCHART |
	  TKINED_GRAPH,
	    "address",     m_address },

        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE,
	    "oid",         m_oid },

        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "select",      m_select },

        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "unselect",    m_unselect },

        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "selected",    m_selected },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LOG | 
          TKINED_REFERENCE | TKINED_GRAPH,
	    "icon",        m_icon },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "label",       m_label },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_TEXT |
	  TKINED_REFERENCE | TKINED_STRIPCHART | TKINED_BARCHART,
	    "font",        m_font },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "color",       m_color },

        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART,
	    "move",        m_move },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART,
	    "raise",       m_raise },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART,
	    "lower",       m_lower },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART,
	    "size",        m_size },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_INTERPRETER | TKINED_LOG |
          TKINED_REFERENCE | TKINED_STRIPCHART | TKINED_BARCHART |
	  TKINED_GRAPH | TKINED_DATA,
	    "dump",        m_dump },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "postscript",  m_postscript },

	{ TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
          TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE |
          TKINED_STRIPCHART | TKINED_BARCHART,
	    "flash",       m_flash },

	{ TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH | 
          TKINED_LOG ,
	    "clear",       m_clear },

	{ TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH,
	    "scale",       m_scale },

	{ TKINED_STRIPCHART | TKINED_BARCHART | TKINED_GRAPH | TKINED_DATA,
	    "values",      m_values },

	{ TKINED_STRIPCHART,
	    "jump",        m_jump },

	{ TKINED_LINK,
	    "src",         m_src },
	{ TKINED_LINK,
	    "dst",         m_dst },

	{ TKINED_LOG,
	    "append",      m_append },
	{ TKINED_LOG,
	    "hyperlink",   m_hyperlink },
	{ TKINED_LOG,
	    "interpreter", m_interpreter },

        { TKINED_TEXT,
	    "text",        m_text },

	{ TKINED_GROUP,       
	    "member",      m_member },
	{ TKINED_GROUP,       
	    "collapse",    m_collapse },
	{ TKINED_GROUP,       
	    "expand",      m_expand },
	{ TKINED_GROUP,       
	    "collapsed",   m_collapsed },
	{ TKINED_GROUP,       
	    "ungroup",     m_ungroup },

	{ TKINED_NODE | TKINED_NETWORK,
	    "links",       m_links },

	{ TKINED_LINK | TKINED_NETWORK,
	    "points",      m_points },

	{ TKINED_NETWORK,     
	    "labelxy",     m_network_labelxy },

        { TKINED_MENU,
	    "interpreter", m_interpreter },

        { TKINED_INTERPRETER, 
	    "send",        m_send },

	{ TKINED_EVENT,
	    "bell",	   m_bell },

        { TKINED_ALL,         
	    "delete",      m_delete },

#if 0
        { TKINED_NODE | TKINED_GROUP | TKINED_NETWORK | TKINED_LINK |
	  TKINED_TEXT | TKINED_IMAGE | TKINED_REFERENCE,
	    "action",      m_action },
#endif

        { 0, 0, 0 }
    };

/*
 * Process a method of a Tki_Object. Check the table for an
 * appropriate entry and call the desired function. We have an 
 * error if no entry matches.
 */

static int
ObjectCommand (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    Tki_Object *object = (Tki_Object *) clientData;
    Method *ds;
    int res;

    if (argc < 2) {
	Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    for (ds = methodTable; ds->cmd; ds++) {

	if (! (ds->type & object->type)) continue;

	if ( (argv[1][0] != *(ds->cmd)) || (strcmp(argv[1], ds->cmd) != 0)) 
		continue;

	res = (ds->fnx)(interp, object, argc-2, argv+2);
	if (res == TCL_OK) {
	    if (tki_Debug && (strcmp(argv[1], "create") != 0)) {
		do_debug (object, interp, argc-1, argv+1, Tcl_GetStringResult(interp));
	    }
	}

#if 0
	{ char dummy[100], *tmp = ckstrdup (interp->result);
	  Tcl_Eval (interp, "update");
	  Tcl_SetResult (interp, tmp, TCL_DYNAMIC);
	  fprintf (stdout, Tcl_Merge (argc, argv));
	  fflush (stdout);
	  gets (dummy);
        }
#endif

	return res;
    }

    Tcl_ResetResult (interp);
    
    Tcl_AppendResult (interp, "unknown option \"", argv[1],
                      "\": should be ", (char *) NULL);
    for (ds = methodTable; ds->cmd; ds++) {
	if (! (ds->type & object->type)) continue;
        if (ds != methodTable) {
            Tcl_AppendResult (interp, ", ", (char *) NULL);
        }
        Tcl_AppendResult (interp, ds->cmd, (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 * Invoke a method for a given object and make sure that this call
 * does not get forwarded through the trace mechanism.
 */

int
TkiNoTrace (int (*method)(), Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int nt = ignoretrace;
    int res;

    ignoretrace = 1;
    res = (method)(interp, object, argc, argv);
    ignoretrace = nt;

    return res;
}

