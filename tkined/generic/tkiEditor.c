/*
 * tkiEditor.c --
 *
 *	This file contains all functions that manipulate the state
 *	of an editor object. An editor objects keeps track of all
 *	information associated with a toplevel view.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkined.h"
#include "tkiPort.h"

Tcl_DString clip;                        /* the global clipboard */
static int force = 0;                    /* force a full dump */
static int numEditors = 0;               /* the number of editor objects */
static char *defaultName = "noname.tki"; /* the default map name */

#define TKI_PAGE_RESOLUTION 5

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
do_delete	(Tki_Editor *editor, Tcl_Interp *interp, 
			     Tcl_DString *dstp);
static void
do_dump		(Tki_Editor *editor, Tcl_Interp *interp, 
			     Tki_Object *group, Tcl_DString *dstp);
static int 
do_ined		(Tki_Editor *editor, Tcl_Interp *interp,
			     char *line);
static int 
do_set		(Tki_Editor *editor, Tcl_Interp *interp, 
			     char *line);

static int
GetId		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
GetWidth	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
GetHeight	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
GetPageWidth	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
GetPageHeight	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
Toplevel	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
DirName		(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static int
FileName	(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static int
Cut		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
Copy		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
Paste		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
ClearEditor	(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
GetColor	(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static void
ReadHistory	(Tki_Editor *editor, Tcl_Interp *interp);

static void
WriteHistory	(Tki_Editor *editor, Tcl_Interp *interp);

static void
ExpandIconName	(Tki_Editor *editor, Tcl_Interp *interp,
			     int type, char *str);
static void
ReadDefaultFile	(Tki_Editor *editor, Tcl_Interp *interp,
			     char *filename);
static void
ReadDefaults	(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static int
LoadMap		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
SaveMap		(Tki_Editor *editor, Tcl_Interp *interp, 
			     int argc, char **argv);
static int
DeleteEditor	(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static int
QuitEditor	(Tki_Editor *editor, Tcl_Interp *interp,
			     int argc, char **argv);
static int 
EditorCommand	(ClientData clientData, Tcl_Interp *interp,
			     int argc, char **argv);
/*
 * Create a new editor object. No Parameters are expected.
 */

int 
Tki_CreateEditor (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    Tki_Editor *editor;
    static unsigned lastid = 0;

    sprintf(buffer, "tkined%d", lastid++);

    if (argc != 1) {
	Tcl_SetResult(interp, "wrong # args", TCL_STATIC);
        return TCL_ERROR;
    }

    editor = (Tki_Editor *) ckalloc(sizeof(Tki_Editor));

    editor->id = ckstrdup(buffer);
    editor->toplevel = ckstrdup("");

    editor->dirname = ckstrdup("");
    editor->filename = ckstrdup("");

    editor->pagesize = ckstrdup("");
    editor->landscape = 0;
    editor->width = 0; editor->height = 0;
    editor->pagewidth = 0; editor->pageheight = 0;

    editor->traceCount = 0;

    Tcl_InitHashTable (&(editor->attr), TCL_STRING_KEYS);

    /* create a tcl command for the new object */

    Tcl_CreateCommand (interp, editor->id, EditorCommand, 
		       (ClientData) editor, Tki_DeleteEditor);    

    ReadDefaults (editor, interp, 0, (char **) NULL);

    ReadHistory (editor, interp);

    /* call the tk procedure to do initialization stuff */

    Tcl_VarEval (interp, "Editor__create ", editor->id, (char *) NULL);
    Tcl_ResetResult (interp);

    /* get the colormodel for this editor */

    Tcl_Eval (interp, "winfo depth . ");
    editor->color = atoi (Tcl_GetStringResult(interp)) > 2;
    Tcl_ResetResult (interp);

    ClearEditor (editor, interp, 0, (char **) NULL);

    numEditors++;

    Tcl_SetResult(interp, editor->id, TCL_STATIC);

    return TCL_OK;
}

/*
 * Delete an editor object. Free everything allocated before 
 * destroying the structure.
 */

void 
Tki_DeleteEditor (ClientData clientData)
{
    Tki_Editor *editor = (Tki_Editor *) clientData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    numEditors--;

    ckfree (editor->id);
    ckfree (editor->toplevel);
    ckfree (editor->dirname);
    ckfree (editor->filename);
    ckfree (editor->pagesize);

    entryPtr = Tcl_FirstHashEntry(&(editor->attr), &search);
    while (entryPtr != NULL) {
	ckfree ((char *) Tcl_GetHashValue (entryPtr));
	entryPtr = Tcl_NextHashEntry (&search);
    }

    Tcl_DeleteHashTable (&(editor->attr));

    ckfree ((char*) editor);
}

/*
 * Read the current config file. It contains the list of last used
 * files.
 */

static void
ReadHistory (editor, interp) 
    Tcl_Interp *interp;
    Tki_Editor *editor;
{
    FILE *f;
    char *home = getenv ("HOME");
    char *argv[2];
    Tcl_DString dst;

    if (! home) return;

    Tcl_DStringInit (&dst);
    Tcl_DStringAppend (&dst, home, -1);
    Tcl_DStringAppend (&dst, "/.tkined/.history", -1);
    f = fopen (Tcl_DStringValue (&dst), "r");

    Tcl_DStringFree (&dst);
    if (f) {
        while (fgets(buffer, 1024, f) != NULL) {
	    int len = strlen (buffer);
	    if (buffer[len-1] == '\n') {
	        buffer[len-1] = '\0';
	    }
	    if (access (buffer, R_OK) == 0) {
		Tcl_DStringAppendElement (&dst, buffer);
	    }
	}
	fclose (f);
    }

    argv[0] = "history";
    argv[1] = Tcl_DStringValue (&dst);
    Tki_EditorAttribute (editor, interp, 2, argv);

    Tcl_DStringFree (&dst);
}

/*
 * Prepend the current editor file to the list of last used files.
 * Creates a $HOME/.tkined directory if none exists and writes to
 * $HOME/.tkined/.history . The max. size of the history is HISTSIZE.
 */

static void
WriteHistory (editor, interp) 
    Tcl_Interp *interp;
    Tki_Editor *editor;
{
#define HISTSIZE 20
    FILE *f;
    char *fname, *home = getenv ("HOME");
    char *hist[HISTSIZE];
    int i = 0;

    if (! home) return;

    if (strcmp (editor->filename, defaultName) == 0) return;

    for (i = 0; i < HISTSIZE; i++) hist[i] = NULL;
    
    fname = ckalloc (strlen(home)+30);
    strcpy (fname, home);
    strcat (fname, "/.tkined/.history");
    f = fopen (fname, "r");

    if (f) {
	i = 0;
	while ((fgets(buffer, 1024, f) != NULL) && (i < HISTSIZE)) {
	    int len = strlen (buffer);
	    if (buffer[len-1] == '\n') {
		buffer[len-1] = '\0';
	    }
	    hist[i++] = ckstrdup (buffer);
	}
	fclose (f);
    }

    f = fopen (fname, "w+");
    if (! f) {
	strcpy (fname, home);
	strcat (fname, "/.tkined");
#ifdef __WIN32__
	mkdir (fname);
#else
	mkdir (fname, 0755);
#endif
	strcat (fname, "/.history");
	f = fopen (fname, "w+");
    }

    if (f) {
	char *name = ckalloc (strlen(editor->dirname) 
			      + strlen(editor->filename) + 2);
	strcpy (name, editor->dirname);
	strcat (name, "/");
	strcat (name, editor->filename);

	fputs (name, f);
	fputs ("\n", f);
	
	for (i = 0; i < HISTSIZE; i++) {
	    if (hist[i] && (strcmp (hist[i], name) != 0)) {
		fputs (hist[i], f);
		fputs ("\n", f);
	    }
	}
	fclose (f);
    }

    ckfree (fname);

    ReadHistory (editor, interp);
}

/*
 * Return the id of an editor object.
 */

static int 
GetId (Tki_Editor *editor, Tcl_Interp *interp, int argc, char **argv)
{
    Tcl_SetResult(interp, editor->id, TCL_STATIC);
    return TCL_OK;
}

/*
 * Return or set the toplevel window of an editor object.
 */

static int 
Toplevel (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{

    if (argc > 0 ) {
        STRCOPY (editor->toplevel, argv[0]);
	Tcl_VarEval (interp, "Editor__toplevel ", editor->id, (char *) NULL);
	fprintf (stderr, Tcl_GetStringResult(interp));
	Tcl_ResetResult (interp);
    }

    Tcl_SetResult(interp, editor->toplevel, TCL_STATIC);
    return TCL_OK;
}

/*
 * Return the toplevel graph window used by graph objects.
 */

int 
Tki_EditorGraph (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{

    Tcl_VarEval (interp, "Editor__graph ", editor->id, (char *) NULL);
    return TCL_OK;
}

/*
 * Retrieve the objects that belong to this editor.
 */

int 
Tki_EditorRetrieve (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;
    int mask = (argc == 0) ? TKINED_ALL : string_to_type(argv[0]);

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor && object->type & mask) {
	    Tcl_AppendElement (interp, object->id);
	}
	entryPtr = Tcl_NextHashEntry (&search);
    }

    return TCL_OK;
}

/*
 * Return or clear the current selection.
 */

int 
Tki_EditorSelection (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;
    int clear = 0;

    if (argc > 0 ) {
	if ((argv[0][0] == 'c') && (strcmp(argv[0], "clear") == 0)) {
	    clear++;
	}
    }

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {

	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor) {
	    if (clear && object->selected) {
		m_unselect (interp, object, 0, (char **) NULL);
	    }
	    if (object->selected) {
		Tcl_AppendElement (interp, object->id);
	    }
	}

	entryPtr = Tcl_NextHashEntry (&search);
    }

    return TCL_OK;
}

/*
 * Get the current color model.
 */

static int 
GetColor (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(editor->color));
    return TCL_OK;
}

/*
 * Get the current width of the drawing area.
 */

static int
GetWidth (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(editor->width));
    return TCL_OK;
}

/*
 * Get the current height of the drawing area.
 */

static int
GetHeight (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(editor->height));
    return TCL_OK;
}

/*
 * Get the current width of the printed page in millimeter.
 */

static int
GetPageWidth (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(editor->pagewidth));
    return TCL_OK;
}

/*
 * Get the current height of the printed page in millimeter.
 */

static int
GetPageHeight (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_SetObjResult(interp, Tcl_NewIntObj(editor->pageheight));
    return TCL_OK;
}

/*
 * Get or set the current directory name.
 */

static int 
DirName (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    if (argc == 1) {
	STRCOPY (editor->dirname, argv[0]);
    }

    Tcl_SetResult (interp, editor->dirname, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get or set the current filename.
 */

static int
FileName (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    if (argc == 1) {
	STRCOPY (editor->filename, argv[0]);
	Tcl_VarEval (interp, "Editor__filename ", editor->id, (char *) NULL);
	WriteHistory (editor, interp);
    }

    Tcl_SetResult (interp, editor->filename, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get or set the current pagesize. Convert the textual name in the
 * internal width and height in canvas pixels. 
 */

int 
Tki_EditorPageSize (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    typedef struct PageSize {
	char *name;
	int width;
	int height;
    } PageSize;

    PageSize pageSizeTable[] = {
	{ "A4",		210,	 297 },
	{ "A3",		297,	 420 },
	{ "A2",		420,	 594 },
	{ "A1",		594,	 841 },
	{ "A0",		841,	1189 },
	{ "Letter",	216,	 279 },
	{ "Legal",	216,	 356 },
	{ NULL,		  0,	   0 },
    };
    
    if (argc == 1) {

	PageSize *p;

	for (p = pageSizeTable; p->name; p++) {
	    if (0 == strcmp(argv[0], p->name)) break;
	}
	if (! p->name) {		/* use first entry as default */
	    p = pageSizeTable;
	}

	STRCOPY(editor->pagesize, p->name);
	editor->pagewidth  = p->width;
	editor->pageheight = p->height;

	/* Flip the page if we should use landscape orientation. */
	if (editor->landscape) {
	    int tmp = editor->pagewidth;
	    editor->pagewidth = editor->pageheight;
	    editor->pageheight = tmp;
	}

	editor->width = editor->pagewidth * TKI_PAGE_RESOLUTION;
	editor->height = editor->pageheight * TKI_PAGE_RESOLUTION;
	
	sprintf (buffer, "Editor__pagesize %s %d %d",
		 editor->id, editor->width, editor->height);
	Tcl_Eval (interp, buffer);
	Tcl_ResetResult (interp);
    }

    Tcl_SetResult(interp, editor->pagesize, TCL_STATIC);

    return TCL_OK;
}

/*
 * Get or set the current page orientation.
 */

int 
Tki_EditorOrientation (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    if (argc == 1) {
	if (strcmp(argv[0], "portrait") == 0) {
	    if (editor->landscape) {
		int tmp = editor->pagewidth;
		editor->pagewidth = editor->pageheight;
		editor->pageheight = tmp;
	    }
	    editor->landscape = 0;
	    editor->width = editor->pagewidth * TKI_PAGE_RESOLUTION;
	    editor->height = editor->pageheight * TKI_PAGE_RESOLUTION;
	    sprintf (buffer, "Editor__pagesize %s %d %d",
		     editor->id, editor->width, editor->height);
	    Tcl_Eval (interp, buffer);
	} else {
	    if (! editor->landscape) {
		int tmp = editor->pagewidth;
                editor->pagewidth = editor->pageheight;
                editor->pageheight = tmp;
	    }
	    editor->landscape = 1;
	    editor->width = editor->pagewidth * TKI_PAGE_RESOLUTION;
	    editor->height = editor->pageheight * TKI_PAGE_RESOLUTION;
	    sprintf (buffer, "Editor__pagesize %s %d %d",
		     editor->id, editor->width, editor->height);
	    Tcl_Eval (interp, buffer);
	}
    }

    if (editor->landscape) {
	Tcl_SetResult(interp, "landscape", TCL_STATIC);
    } else {
	Tcl_SetResult(interp, "portrait", TCL_STATIC);
    }

    return TCL_OK;
}

/*
 * Get or set a generic editor attribute. They are used to
 * store default values and arbitrary status information.
 */

int 
Tki_EditorAttribute (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;

    if (argc == 0) return TCL_OK;

    if (argc == 2) {
	int isnew;

	entryPtr = Tcl_CreateHashEntry (&(editor->attr), argv[0], &isnew);
	if (! isnew) {
	    ckfree ((char *) Tcl_GetHashValue (entryPtr));
	}
	Tcl_SetHashValue (entryPtr, ckstrdup(argv[1]));
    }

    entryPtr = Tcl_FindHashEntry (&(editor->attr), argv[0]);
    if (entryPtr != NULL) {
	Tcl_SetResult(interp, (char *) Tcl_GetHashValue (entryPtr), TCL_STATIC);
    } else {
	Tcl_ResetResult (interp);
    }

    if (tki_Debug) {
	if (argc == 2) {
	    fprintf (stderr, "++ %s attribute %s = %s\n", 
		     editor->id, argv[0], argv[1]);
	} else {
	    fprintf (stderr, "-- %s attribute %s (%s)\n",
                     editor->id, argv[0], Tcl_GetStringResult(interp));
	}
    }

    return TCL_OK;
}

/*
 * Delete all the objects given by the ids in dstp.
 */

static void
do_delete (editor, interp, dstp)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    Tcl_DString *dstp;
{
    int largc, i;
    char **largv;
    
    Tcl_SplitList (interp, Tcl_DStringValue(dstp), &largc, &largv);
    for (i = 0; i < largc; i++) {
	Tcl_VarEval (interp, largv[i], " delete", (char *) NULL);
	Tcl_ResetResult (interp);
    }

    ckfree ((char *) largv);
}

/*
 * Copy the current selection on the clipboard and then call
 * do_delete to actually delete the selection.
 */

static int 
Cut (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;
    Tcl_DString dst;

    Copy (editor, interp, argc, argv);

    Tcl_DStringInit (&dst);
 
    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor && object->selected) {
	    Tcl_DStringAppendElement (&dst, object->id);
	}

	entryPtr = Tcl_NextHashEntry (&search);
    }

    do_delete (editor, interp, &dst);

    Tcl_DStringFree (&dst);

    return TCL_OK;
}

/*
 * Dump an object. Call the appropriate method and write the
 * text to the DString dstp. Group objects are saved recursively.
 * The done flag is used to prevent objects from being saved twice.
 */

static void
do_dump (editor, interp, object, dstp)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    Tki_Object *object;
    Tcl_DString *dstp;
{
    char *result;

    if (! object || object->done) return;

    switch (object->type) {
      case TKINED_NODE: 
	Tki_DumpObject (interp, object);
	break;
      case TKINED_NETWORK:
	Tki_DumpObject (interp, object);
	break;
      case TKINED_LINK:
        {
#if 1
	    if (! object->src) {
		fprintf(stderr, "*** link %s without src ***\n", object->name);
	    }
	    if (! object->dst) {
		fprintf(stderr, "*** link %s without dst ***\n", object->name);
	    }
#endif
	    if (!object->src || !object->dst) {
		break;
	    }
	    do_dump (editor, interp, object->src, dstp);
	    do_dump (editor, interp, object->dst, dstp);
        }
	Tki_DumpObject (interp, object);
	break;
      case TKINED_GROUP:
        {
	    if (object->member) {
		int i;
		for (i = 0; object->member[i]; i++) {
		    do_dump (editor, interp, object->member[i], dstp);
		}
	    }
        }
	Tki_DumpObject (interp, object);
	break;
      case TKINED_TEXT:
	Tki_DumpObject (interp, object);
	break;
      case TKINED_IMAGE:
	Tki_DumpObject (interp, object);
	break;
      case TKINED_INTERPRETER:
	Tki_DumpObject (interp, object);
	break;
      case TKINED_REFERENCE: 
	Tki_DumpObject (interp, object);
	break;
      case TKINED_STRIPCHART: 
	Tki_DumpObject (interp, object);
	break;
      case TKINED_BARCHART: 
	Tki_DumpObject (interp, object);
	break;
      case TKINED_GRAPH: 
	Tki_DumpObject (interp, object);
	break;
      default:
	Tcl_ResetResult (interp);
    }

    result = Tcl_GetStringResult(interp);
    if (*result != '\0') {
	Tcl_DStringAppend (dstp, result, -1);
	Tcl_DStringAppend (dstp, "\n", 1);
    }

    object->done = 1;

    Tcl_ResetResult (interp);
}

/*
 * Copy the current selection or all objects belonging to this editor
 * (if force is set) in text form on the clipboard. The clipboard may 
 * be used later to save the textual representation in a file or to 
 * paste it in a different editor.
 */

static int 
Copy (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;

    Tcl_DStringInit (&clip);

    /* Set all the done flag of all objects to false. Ignore
       interpreter objects because they use the done flag to
       indicate a complete command buffer. */

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor) {
	    if (object->type != TKINED_INTERPRETER) object->done = 0;
	}
	entryPtr = Tcl_NextHashEntry (&search);
    }

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor && (object->selected || force)) {
	    if (object->type != TKINED_INTERPRETER) {
		do_dump (editor, interp, object, &clip);
	    }
	}
	entryPtr = Tcl_NextHashEntry (&search);
    }

    return TCL_OK;
}

/*
 * Paste the contents of the clipboard in the window. We `evaluate'
 * the text. A bit tricky because I did not want to write a real parser
 * for this stuff...
 */

static int
do_ined (Tki_Editor *editor, Tcl_Interp *interp, char *line)
{
    int largc;
    char **largv;
    char *p;
    int i, res;
    Tki_Object dummy;

    if (Tcl_SplitList (interp, line, &largc, &largv) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_ResetResult (interp);
    
    /* expand variable names */
    
    for (i = 1; i < largc; i++) {
	if (largv[i][0] == '$') {
	    largv[i]++;
	    p = Tcl_GetVar (interp, largv[i], TCL_GLOBAL_ONLY);
	    if (p == NULL) {
		largv[i] = "";
	    } else {
		largv[i] = p;
	    }
	}
    }

    /* fake a dummy interpreter object to process the ined command */
    
    dummy.type = TKINED_INTERPRETER;
    dummy.id = "dummy";
    dummy.editor = editor;
    dummy.canvas = ckalloc(strlen(editor->toplevel)+8);
    strcpy (dummy.canvas, editor->toplevel);
    strcat (dummy.canvas, ".canvas");
    dummy.name = dummy.id;

    res = ined ((ClientData) &dummy, interp, largc, largv);

    ckfree (dummy.canvas);

    return res;
}

static int
do_set (Tki_Editor *editor, Tcl_Interp *interp, char *line)
{
    int len;
    char *var;

    line += 3;                                       /* skip the word set   */
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return TCL_ERROR;

    var = line;
    while (*line && !isspace(*line)) line++;         /* extract the varname */
    if (*line == '\0') return TCL_ERROR;

    *line++ = '\0';                              /* beginning of the command */
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return TCL_ERROR;

    if (*line != '[') return TCL_ERROR;         /* remove [ and skip spaces */
    line++;
    if (strncmp ("eval", line, 4) == 0)         /* backward compatibility   */
	    line += 4;
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return TCL_ERROR;
    
    len = strlen(line)-1;                       /* remove trailing spaces   */
    while (len > 0 && isspace(line[len])) {
	line[len] = '\0';
	len--;
    }
    if (len == 0) return TCL_ERROR;

    if (line[len] != ']') return TCL_ERROR;     /* remove ]                 */
    line[len] = '\0';

    if (do_ined (editor, interp, line) == TCL_OK) {
	line = Tcl_SetVar (interp, var, Tcl_GetStringResult(interp), TCL_GLOBAL_ONLY);
	if (line) {
	    Tki_Object *object = Tki_LookupObject (Tcl_GetStringResult(interp));
	    if (object) object->loaded = 1;
	    return TCL_OK;
	}
    }

    return TCL_ERROR;
}

static int 
Paste (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    char *line, *freeme, *p;

    freeme = ckstrdup (Tcl_DStringValue(&clip));
    line = freeme;

    while (1) {

	/* find the end of the line if any left */
	p = line;
	while ( *p != '\n' && *p != '\0') p++;
	if (*p == '\0') break;
	*p = '\0';

	/* first remove leading white space characters */
	while (*line && isspace(*line)) line++;
	
	/* evaluate the line ignoring comments and empty lines */
	if ( (*line != '\0') && (*line != '#')) {

	    if (strncmp("set", line, 3) == 0) {
		do_set (editor, interp, line);
	    } else if (strncmp("ined", line, 4) == 0) {
		do_ined (editor, interp, line);
	    } else if (strncmp("exec tkined", line, 11) == 0) {
		/* dont complain about exec commands - ugly hack */
	    } else {
		fprintf (stderr, "** Paste unknown: %s\n", line);
	    }

	}

	line = ++p;
    }

    ckfree (freeme);

    return TCL_OK;
}

/*
 * Tki_EditorPostScript() returns a PostScript dump of the 
 * current map in interp->result.
 */

int
Tki_EditorPostScript (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    return Tcl_VarEval (interp, "Editor__postscript ", 
			editor->id, (char *) NULL);

}


/*
 * Tki_EditorTnmMap() returns a Tnm map dump of the 
 * current Tkined map in interp->result.
 */

int
Tki_EditorTnmMap (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    return Tcl_VarEval (interp, "Editor__map ", 
			editor->id, (char *) NULL);

}


/*
 * Reinitialize the editor. Delete all objects currently associated
 * with this editor.
 */

static int
ClearEditor (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;
    Tcl_DString dst;
    char *cwd;

    Tcl_DStringInit (&dst);

    entryPtr = Tcl_FirstHashEntry (&tki_ObjectTable, &search);
    while (entryPtr != NULL) {

        object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
        if (object->editor == editor
	    && object->type != TKINED_LOG 
	    && object->type != TKINED_MENU
	    && (object->type != TKINED_INTERPRETER
		|| (object->type == TKINED_INTERPRETER && object->loaded))) {

	    Tcl_DStringAppendElement (&dst, object->id);
	}

        entryPtr = Tcl_NextHashEntry (&search);
    }

    do_delete (editor, interp, &dst);

    Tcl_DStringFree (&dst);

    FileName (editor, interp, 1, &defaultName);

    if (! (cwd = getcwd ((char *) NULL, 1024))) cwd = "";
    DirName (editor, interp, 1, &cwd);

    return TCL_OK;
}

/*
 * ExpandIconName() expands an icon name and searches for the file name
 * of the bitmap that belongs to the icon. If successful, an entry is
 * added to the editor attributes.
 */

static void
ExpandIconName (editor, interp, type, str)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int type;
    char *str;
{
    char *icon = str;
    char *name = str;
    char *argv[2];
    char *path;

    while (*name && !isspace(*name)) name++;
    if (*name == '\0') return;

    *name = '\0';
    name++;

    while (*name && isspace(*name)) name++;
    if (*name == '\0') return;

    /*
     * Skip leading elements that are separated with colons.
     */

    { 
	char *p;
	for (p = name; *p != '\0'; p++) {
	    if (*p == ':') {
		name = p+1;
	    }
	}
    }

    /*
     * Make our own copy since findfile will reuse the buffer space!
     */

    icon = ckstrdup (icon);
    name = ckstrdup (name);
    argv[0] = ckalloc (strlen(name)+20);
    argv[1] = NULL;

    if (type == TKINED_NETWORK) {
	strcpy (argv[0], "NETWORK-icon-");
	strcat (argv[0], name);
	argv[1] = ckstrdup (icon);
	Tki_EditorAttribute (editor, interp, 2, argv);
    }

    if (type == TKINED_GRAPH) {
	strcpy (argv[0], "GRAPH-icon-");
	strcat (argv[0], name);
	argv[1] = ckstrdup (icon);
	Tki_EditorAttribute (editor, interp, 2, argv);
    }

    path = findfile (interp, icon);
    if (path != NULL) {
	argv[1] = ckalloc (strlen(path)+2);
	argv[1][0] = '@';
	strcpy (argv[1]+1, path);

	if (type == TKINED_NODE) {
	    int len;
	    strcpy (argv[0], "NODE-icon-");
	    strcat (argv[0], name);
	    Tki_EditorAttribute (editor, interp, 2, argv);

	    /* needed to ensure backward compatibility */
            
            ckfree (argv[0]);
            argv[0] = ckalloc (strlen(icon)+15);
            strcpy (argv[0], "NODE-icon-");
            strcat (argv[0], icon);
            Tki_EditorAttribute (editor, interp, 2, argv);
            
            if ((len = strlen (argv[0])) > 3
                && argv[0][len-3] == '.'
                && argv[0][len-2] == 'b' 
                && argv[0][len-1] == 'm') {
                argv[0][len-3] = '\0';
                Tki_EditorAttribute (editor, interp, 2, argv);
            }
	} 
	
	if (type == TKINED_GROUP) {
	    int len;
	    strcpy (argv[0], "GROUP-icon-");
	    strcat (argv[0], name);
	    Tki_EditorAttribute (editor, interp, 2, argv);

	    /* needed to ensure backward compatibility */
            
            ckfree (argv[0]);
            argv[0] = ckalloc (strlen(icon)+15);
            strcpy (argv[0], "GROUP-icon-");
            strcat (argv[0], icon);
            Tki_EditorAttribute (editor, interp, 2, argv);
            
            if ((len = strlen (argv[0])) > 3
                && argv[0][len-3] == '.'
                && argv[0][len-2] == 'b' 
                && argv[0][len-1] == 'm') {
                argv[0][len-3] = '\0';
                Tki_EditorAttribute (editor, interp, 2, argv);
            }
	}

	if (type == TKINED_REFERENCE) {
	    int len;
	    strcpy (argv[0], "REFERENCE-icon-");
	    strcat (argv[0], name);
	    Tki_EditorAttribute (editor, interp, 2, argv);

	    /* needed to ensure backward compatibility */
            
            ckfree (argv[0]);
            argv[0] = ckalloc (strlen(icon)+20);
            strcpy (argv[0], "REFERENCE-icon-");
            strcat (argv[0], icon);
            Tki_EditorAttribute (editor, interp, 2, argv);
            
            if ((len = strlen (argv[0])) > 3
                && argv[0][len-3] == '.'
                && argv[0][len-2] == 'b' 
                && argv[0][len-1] == 'm') {
                argv[0][len-3] = '\0';
                Tki_EditorAttribute (editor, interp, 2, argv);
            }
	}
    }

    ckfree (argv[0]);
    if (argv[1] != NULL) ckfree (argv[1]);
    ckfree (name);
    ckfree (icon);
}

/*
 * ReadDefaultFile() reads a default file and sets up the editor 
 * attributes.
 */

static void
ReadDefaultFile (Tki_Editor *editor, Tcl_Interp *interp, char *filename)
{
    FILE *f;

    if (filename == NULL) return;

    if ((f = fopen (filename, "r")) == NULL) return;
    
    while (fgets (buffer, 1024, f) != NULL) {
	int len;
	char *p, *value;
	char *name = buffer;
	char *largv[2];
	
	while (*name && isspace(*name)) name++;
	if (*name == '\0') continue;
	
	/*
	 * ignore lines starting with a comment sign or entries that
	 * do not start with a "tkined." prefix.
	 */
	
	if (*name == '#' 
	    || *name == '!' 
	    || strlen(name) < 8 
	    || (strncmp(name, "tkined.", 7) != 0)) continue;
	
	name += 7;
	p = name;
	while (*p && *p != ':') p++;
	if (*p == '\0') continue;
	
	*p = '\0';
	value = ++p;
	
	while (*value && isspace(*value)) value++;

	/*
	 * remove trailing spaces
	 */
	
	len = strlen(value)-1;
	while (len > 0 && isspace(value[len])) {
	    value[len] = '\0';
	    len--;
	}
	
	largv[0] = name;
	largv[1] = value;
	Tki_EditorAttribute (editor, interp, 2, largv);
	
	/*
	 * Expand icon bitmap file names and add expanded entries
	 * to the editor attributes.
	 */
	
	if (strncmp (name, "node", 4) == 0) {
	    ExpandIconName (editor, interp, TKINED_NODE, value);
	} else if (strncmp (name, "group", 5) == 0) {
	    ExpandIconName (editor, interp, TKINED_GROUP, value);
	} else if (strncmp (name, "network", 7) == 0) {
	    ExpandIconName (editor, interp, TKINED_NETWORK, value);
	} else if (strncmp (name, "dashes", 6) == 0) {
	    ExpandIconName (editor, interp, TKINED_GRAPH, value);
	} else if (strncmp (name, "reference", 9) == 0) {
	    ExpandIconName (editor, interp, TKINED_REFERENCE, value);
	}
    }
    
    fclose (f);
}

/*
 * ReadDefaults() searches for tkined.default files and loads
 * the contents into editor attributes using ReadDefaultFile().
 */

static void
ReadDefaults (Tki_Editor *editor, Tcl_Interp *interp, int argc, char **argv)
{
    char *library;
    char *fname;

    library = Tcl_GetVar2(interp, "tkined", "library", TCL_GLOBAL_ONLY);
    if (library == NULL) {
	return;
    }

    fname = ckalloc (strlen(library)+30);
    strcpy (fname, library);
    strcat (fname, "/tkined.defaults");
    ReadDefaultFile (editor, interp, findfile (interp, fname));
    strcpy (fname, library);
    strcat (fname, "/site/tkined.defaults");
    ReadDefaultFile (editor, interp, findfile (interp, fname));
    ckfree (fname);

    ReadDefaultFile (editor, interp, 
		     findfile (interp, "~/.tkined/tkined.defaults"));
    ReadDefaultFile (editor, interp, 
		     findfile (interp, "tkined.defaults"));
}

/*
 * LoadMap() reads a previously saved from a file.
 */

static int
LoadMap (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    FILE *f;
    Tcl_DString tmp;
    int isok;
    char *p;

    if (argc != 1) {
	Tcl_SetResult(interp,"wrong # args", TCL_STATIC);
        return TCL_ERROR;
    }

    if ((f = fopen(argv[0], "r")) == NULL) {
	Tcl_PosixError (interp);
	return TCL_ERROR;
    }

    tmp = clip;
    Tcl_DStringInit (&clip);

    /*
     * Check if the header looks like a valid tkined file.
     */

    isok = 0;
    if (fgets(buffer, 1024, f) != NULL) {
	Tcl_DStringAppend (&clip, buffer, -1);
	if (fgets(buffer, 1024, f) != NULL) {
	    Tcl_DStringAppend (&clip, buffer, -1);
	    for (p = buffer; *p; p++) {
		if (strncmp (p, "tkined version", 14) == 0) {
		    isok = 1;
		    break;		  
		}
	    }
	}
    }

    if (! isok) {
	Tcl_DStringFree (&clip);
	clip = tmp;
	Tcl_SetResult (interp, "not a valid tkined save file", TCL_STATIC);
	return TCL_ERROR;
    }
    
    while (fgets(buffer, 1024, f) != NULL) {
	Tcl_DStringAppend (&clip, buffer, -1);
    }
    fclose (f);
    Paste (editor, interp, 0, (char **) NULL);
    Tcl_DStringFree (&clip);
    clip = tmp;

    return TCL_OK;
}

/*
 * SaveMap() writes all objects in a file. We copy the whole stuff 
 * in good old interviews tradition on the clipboard and write the
 * contents of the clipboard into a file.
 */

static int
SaveMap (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    FILE *f;
    Tcl_DString tmp;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;

    if (argc != 1) {
	Tcl_SetResult(interp, "wrong # args", TCL_STATIC);
        return TCL_ERROR;
    }

    if ((f = fopen(argv[0], "w")) == NULL) {
	Tcl_PosixError (interp);
	return TCL_ERROR;
    }

    fputs ("#!/bin/sh\n", f);
    fprintf (f, "## This file was created by tkined version %s\t%s\n",
	     TKI_VERSION, ">> DO NOT EDIT <<");
    fputs ("##\n", f);
    fputs ("## This may look like TCL code but it is definitely not! \\\n", f);
    fputs ("exec tkined \"$0\" \"$@\"\n\n", f);

    fprintf (f, "ined page %s %s\n\n", editor->pagesize,
	     editor->landscape ? "landscape" : "portrait");

    force = 1;
    tmp = clip;
    Copy (editor, interp, 0, (char **) NULL);
    if (fputs (Tcl_DStringValue(&clip), f) == EOF) {
	Tcl_PosixError (interp);
	return TCL_ERROR;
    }
    clip = tmp;
    force = 0;

    /*
     * Save all interpreter objects that have running jobs.
     */

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor && (object->type == TKINED_INTERPRETER)) {
	    if (strlen(object->action) != 0) {
		Tki_DumpObject (interp, object);
		fputs (Tcl_GetStringResult(interp), f);
		fputs ("\n", f);
		Tcl_ResetResult (interp);
	    }
	}
	entryPtr = Tcl_NextHashEntry (&search);
    }

    fclose (f);

    return TCL_OK;
}


/*
 * DeleteEditor() deletes all objects that belong to an editor.
 * We must restart the loop through the hash table since the hash 
 * table may get clobbered when an object is deleted.
 */

static int 
DeleteEditor (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tki_Object *object;

    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
    while (entryPtr != NULL) {
	object = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	if (object->editor == editor) {
	    m_delete (interp, object, 0, (char **) NULL);
	    entryPtr = Tcl_FirstHashEntry(&tki_ObjectTable, &search);
	}
	entryPtr = Tcl_NextHashEntry (&search);
    }
    
    Tcl_VarEval (interp, "Editor__delete ", editor->id, (char *) NULL);

    Tcl_DeleteCommand (interp, editor->id);

    if (numEditors == 0) {
        QuitEditor (editor, interp, 0, (char **) NULL);
    }

    return TCL_OK;
}

/*
 * QuitEditor() finishes the whole application. This is mainly here 
 * as a hook for future enhancements.
 */

static int
QuitEditor (editor, interp, argc, argv)
    Tcl_Interp *interp;
    Tki_Editor *editor;
    int argc;
    char **argv;
{
    return Tcl_Eval (interp, "destroy .");
}


/*
 * All methods are dispatched using the following table. Depending
 * on the type and the name of an object, we choose the function
 * to call. The type TKINED_ALL matches any type.
 */

typedef struct Method {
    char *cmd;
    int (*fnx)(); /* (Tki_Editor *, Tcl_Interp*, int, char**) */
} Method;

static Method methodTable[] = {

        { "id",          GetId },
	{ "toplevel",    Toplevel },
	{ "graph",       Tki_EditorGraph },
	{ "retrieve",    Tki_EditorRetrieve },
	{ "selection",   Tki_EditorSelection },

	{ "color",	 GetColor },
	{ "width",	 GetWidth },
	{ "height",	 GetHeight },
	{ "pagewidth",	 GetPageWidth },
	{ "pageheight",	 GetPageHeight },

	{ "dirname",     DirName },
	{ "filename",    FileName },
	{ "pagesize",    Tki_EditorPageSize },
	{ "orientation", Tki_EditorOrientation },

	{ "attribute",   Tki_EditorAttribute },

	{ "cut",         Cut },
	{ "copy",        Copy },
	{ "paste",       Paste },

	{ "postscript",  Tki_EditorPostScript },
	{ "map",         Tki_EditorTnmMap },

	{ "clear",       ClearEditor },
	{ "load",        LoadMap },
	{ "save",        SaveMap },
	{ "delete",      DeleteEditor },
	{ "quit",        QuitEditor },

        { 0, 0 }
    };

/*
 * Process a method of an editor object. Check the table for an
 * appropriate entry and call the desired function. We have an 
 * error if no entry matches.
 */

static int
EditorCommand (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    Tki_Editor *editor = (Tki_Editor *) clientData;
    Method *ds;

    if (argc < 2) {
	Tcl_SetResult(interp, "wrong # args", TCL_STATIC);
	return TCL_ERROR;
    }

    if (strcmp(editor->id, argv[0]) != 0) {
	fprintf (stderr, "** fatal error: %s has illegal id %s\n", 
		 argv[0], editor->id);
	fprintf (stderr, "** while doing: %s %s\n", argv[0], argv[1]);
    }

    for (ds = methodTable; ds->cmd; ds++) {
	int res;

	if ((argv[1][0] != *(ds->cmd)) || (strcmp(argv[1], ds->cmd) != 0)) 
		continue;

	res = (ds->fnx)(editor, interp, argc-2, argv+2);
	return res;
    }

    Tcl_AppendResult (interp, "unknown option \"", argv[1], 
		      "\": should be ", (char *) NULL);
    for (ds = methodTable; ds->cmd; ds++) {
	if (ds != methodTable) {
	    Tcl_AppendResult (interp, ", ", (char *) NULL);
	}
	Tcl_AppendResult (interp, ds->cmd, (char *) NULL);
    }

    return TCL_ERROR;
}

