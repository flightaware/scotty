/*
 * tkiFlash.c --
 *
 *	Some utility functions to implement flashing icons.
 *
 * Copyright (c) 1995-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkined.h"
#include "tkiPort.h"

typedef struct FlashItem {
    char *id;
    struct FlashItem *nextPtr;
} FlashItem;

static FlashItem *flashList = NULL;
static char *flashIcon = "optionFlashIcon";

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
FlashProc		_ANSI_ARGS_((ClientData clientData));

/*
 * The FlashProc callback flashes every object in the flashList 
 * and calls it again if there are any jobs left.
 */

static void
FlashProc (clientData)
    ClientData clientData;
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    Tki_Object *object;
    char *color;
    FlashItem *p;
    int max = 0;

    Tk_Window window;
    Tk_Window tkwin = Tk_MainWindow(interp);

    for (p = flashList; p != NULL; p = p->nextPtr) {

	if (p->id == NULL) continue;

	object = Tki_LookupObject (p->id);
	if (object == NULL) continue;

	window = Tk_NameToWindow(interp, object->editor->toplevel, tkwin);
	if (! window) {
	    continue;
	}

	if (! object->editor->color) {
	    if ((object->flash) % 2) {
		color = "black";
	    } else {
		color = "white";
	    }
	} else {
	    color = object->color;
	    if ((object->flash) % 2) {
		if (strcasecmp(color, "white") == 0) color = "black";
	    } else {
		color = "white";
	    }
	}

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__color ", object->id, " ", color, 
		     (char *) NULL);

#if 1
	if (object->editor) {
	    Tki_EditorAttribute(object->editor, interp, 1, &flashIcon);
	    if ((*interp->result != '\0') &&
		(strcmp(interp->result, "yes") == 0 
		 || strcmp(interp->result, "true") == 0
		 || strcmp(interp->result, "on") == 0
		 || strcmp(interp->result, "1") == 0)) {
		char *buf = (object->flash % 2) ? "icon" : "noicon";
		Tcl_VarEval(interp, "if ![winfo ismapped ", 
			    object->editor->toplevel,
			    "] {", "wm iconbitmap ", object->editor->toplevel,
			    " ", buf, "}", (char *) NULL);
	    }
	}
#endif

	object->flash -= 1;

	if (object->flash == 0) {
	    TkiNoTrace (m_color, interp, object, 1, &object->color);
	    ckfree (p->id);
	    p->id = NULL;
	}

	max = ( object->flash > max ) ? object->flash : max;
    }

    if (max <= 0) {      /* everything is done - remove the flashList */

	FlashItem *q;

        for (p = flashList; p != NULL; p = q) {
            q = p->nextPtr;
            if (p->id != NULL) ckfree (p->id);
            ckfree ((char *) p);
        }

        flashList = NULL;
    }

    Tcl_Eval (interp, "update");

    if (max > 0) {
	Tk_CreateTimerHandler (500, FlashProc, (ClientData) interp);
    }
}

/*
 * Add a new flash node to our list of flashing objects.
 * Start flash callback if we create the first entry.
 */

void
TkiFlash (interp, object)
    Tcl_Interp *interp;
    Tki_Object *object;
{
    FlashItem *p;

    if (flashList == NULL) {
	
        flashList = (FlashItem *) ckalloc (sizeof(FlashItem));
	p = flashList;
	p->id = ckstrdup(object->id);
	p->nextPtr = NULL;
	Tk_CreateTimerHandler (500, FlashProc, (ClientData) interp);

    } else {

	/* 
	 * Move to the end of the list and check if it exists already.
	 */

	for (p = flashList; p->nextPtr != NULL; p = p->nextPtr) {
	    if (p->id && strcmp (p->id, object->id) == 0) return;
	}
	if (p->id && strcmp (p->id, object->id) == 0) {
	    return;
	}

	/* 
	 * Create a new entry for the flash list.
	 */

        p->nextPtr = (FlashItem *) ckalloc (sizeof(FlashItem));
	p = p->nextPtr;
	p->id = ckstrdup(object->id);
	p->nextPtr = NULL;
    }
}
