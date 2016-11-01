/*
 * tkiMethods.c --
 *
 *	This file contains all functions that manipulate the state
 *	of tkined objects. These functions get called whenever a user
 *	invokes a command, when applying a tool, or when a command is send
 *	from an interpreter. Every method calls a corresponding tk 
 *	procedure that is responsible to manipulate the display according
 *	to the changes. The naming convention is that a command like `move'
 *	is implemented as the method m_move (written in C) which calls
 *	<type>__move to let tk take the appropriate action.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkined.h"
#include "tkiPort.h"

#ifndef USE_TCL_STUBS
/*
 * The following prototype is normally in tclIntPlatDecls.h, which
 * does not appear to be installed on many systems.
 */

EXTERN void TclGetAndDetachPids (Tcl_Interp * interp, 
                                             Tcl_Channel chan);
#else
/*
 * This internal header is needed for TclGetAndDetachPids() :-(
 */
#include <TclPort.h>
#endif 
 

#define USE_TCP_CHANNEL
/* #define NOTYET */

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
dump_move             (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_icon             (Tcl_Interp *interp,
				   Tki_Object *object);
static void
dump_name             (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_address          (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_oid              (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_font             (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_color            (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_label            (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_scale            (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_size             (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
dump_attributes       (Tcl_Interp *interp,
                                   Tki_Object *object);

static void
parent_resize         (Tcl_Interp *interp, 
				   Tki_Object *object);
static void
update_links	      (Tcl_Interp *interp,
				   Tki_Object *object);

static void
m_network_link_end    (Tcl_Interp *interp, Tki_Object *network,
				   double *sx, double *sy);
static int 
m_link_update         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);

static int 
m_node_create         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_group_create        (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_network_create      (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_link_create         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_text_create         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_image_create        (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_interpreter_create  (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_menu_create         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, const char **argv);
static int 
m_log_create          (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_ref_create          (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_strip_create        (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static int 
m_bar_create          (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
static int 
m_graph_create        (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
static int
m_data_create         (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
static int
m_event_create        (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);

static int 
m_node_retrieve       (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_group_retrieve      (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_network_retrieve    (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_link_retrieve       (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_text_retrieve       (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_image_retrieve      (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_interpreter_retrieve (Tcl_Interp* interp, Tki_Object *object,
				    int argc, char** argv);
static int 
m_menu_retrieve       (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_log_retrieve        (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_ref_retrieve        (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_strip_retrieve      (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_bar_retrieve        (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int 
m_graph_retrieve      (Tcl_Interp* interp, Tki_Object *object,
				   int argc, char** argv);
static int
m_data_retrieve       (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
static int
m_event_retrieve      (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);

static void 
m_linked_delete       (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static void 
m_link_delete         (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static void 
m_group_delete        (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);
static void 
m_interpreter_delete  (Tcl_Interp *interp, Tki_Object *object,
				   int argc, char **argv);

static int 
m_node_dump        (Tcl_Interp *interp, Tki_Object *object);

static int 
m_group_dump       (Tcl_Interp *interp, Tki_Object *object);

static int
m_network_dump     (Tcl_Interp *interp, Tki_Object *object);

static int
m_link_dump        (Tcl_Interp *interp, Tki_Object *object);

static int
m_text_dump        (Tcl_Interp *interp, Tki_Object *object);

static int
m_image_dump       (Tcl_Interp *interp, Tki_Object *object);

static int
m_interpreter_dump (Tcl_Interp *interp, Tki_Object *object);

static int
m_log_dump	   (Tcl_Interp *interp, Tki_Object *object);

static int
m_ref_dump         (Tcl_Interp *interp, Tki_Object *object);

static int
m_strip_dump       (Tcl_Interp *interp, Tki_Object *object);

static int
m_bar_dump         (Tcl_Interp *interp, Tki_Object *object);

static int
m_graph_dump       (Tcl_Interp *interp, Tki_Object *object);

static int
m_data_dump        (Tcl_Interp *interp, Tki_Object *object);

static void
RemoveObject	   (Tki_Object **table, Tki_Object *object);

#ifdef USE_TCP_CHANNEL
static void
AcceptProc	   (ClientData clientData, Tcl_Channel channel, 
				char *hostName, int port);
static void
TimeOutProc	   (ClientData clientData);
#endif

/* 
 * Remove the references to the given object from a table
 * of objects.
 */

static void
RemoveObject (Tki_Object **table, Tki_Object *object)
{
    int i, j;

    for (i = 0, j = 0; table[i]; i++) {
	if (table[i] != object) {
	    table[j++] = table[i];
	}
    }
    while (j < i) {
	table[j++] = NULL;
    }
}


/*
 * The following set of functions is used to write object state
 * into the iterpreter result string. They are used to dump objects
 * into an ascii string.
 */

static void
dump_move (Tcl_Interp *interp, Tki_Object *object)
{
    sprintf (buffer, "ined -noupdate move $%s %.2f %.2f\n",
	     object->id, object->x, object->y);
    Tcl_AppendResult (interp, buffer, (char *)NULL);
}

static void
dump_icon (Tcl_Interp *interp, Tki_Object *object)
{
    if (*(object->icon) != '\0') {

	char *p = strrchr (object->icon, '/');
    
        Tcl_AppendResult (interp, "ined -noupdate icon $",
			  object->id, (char *) NULL);
	Tcl_AppendElement (interp, (p) ? ++p : object->icon);
	Tcl_AppendResult (interp, "\n", (char *) NULL);
    }
}

static void
dump_name (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "ined -noupdate name $", 
		      object->id, (char *) NULL);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendResult (interp, "\n", (char *) NULL);
}

static void
dump_address (Tcl_Interp *interp, Tki_Object *object)
{
    if (*(object->address) != '\0') {
        Tcl_AppendResult (interp, "ined -noupdate address $",
			  object->id, (char *) NULL);
	Tcl_AppendElement (interp, object->address);
	Tcl_AppendResult (interp, "\n", (char *) NULL);
    }
}
    
static void
dump_oid (Tcl_Interp *interp, Tki_Object *object)
{
    if (object->oid != 0) {
	sprintf (buffer, "ined -noupdate oid $%s \"%d\"\n",
		 object->id, object->oid);
	Tcl_AppendResult (interp, buffer, (char *)NULL);
    }
}

static void
dump_font (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "ined -noupdate font $", 
		      object->id, (char *) NULL);
    Tcl_AppendElement (interp, object->font);
    Tcl_AppendResult (interp, "\n", (char *) NULL);
}

static void
dump_color (Tcl_Interp *interp, Tki_Object *object)
{
    if (strlen(object->color) > 0 && strcmp(object->color, "black") != 0) {
        Tcl_AppendResult (interp, "ined -noupdate color $",
			  object->id, (char *) NULL);
        Tcl_AppendElement (interp, object->color);
        Tcl_AppendResult (interp, "\n", (char *) NULL);
    }
}

static void
dump_label (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "ined -noupdate label $", object->id, 
		      (char *) NULL);
    Tcl_AppendElement (interp, object->label);
    Tcl_AppendResult (interp, "\n", (char *) NULL);
}

static void
dump_scale (Tcl_Interp *interp, Tki_Object *object)
{
    sprintf (buffer, "ined -noupdate scale $%s %.2f\n", 
	     object->id, object->scale);
    Tcl_AppendResult (interp, buffer, (char *) NULL);
}

static void
dump_size (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "ined -noupdate size $", object->id, " ", 
		      object->size, "\n", (char *) NULL);
}

static void
dump_attributes (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_HashEntry *ht_entry;
    Tcl_HashSearch ht_search;

    ht_entry = Tcl_FirstHashEntry(&(object->attr), &ht_search);
    while (ht_entry != NULL) {
	Tcl_AppendResult (interp, "ined -noupdate attribute $", object->id,
			  (char *) NULL);
	Tcl_AppendElement (interp, 
			   Tcl_GetHashKey (&(object->attr), ht_entry));
	Tcl_AppendElement (interp, (char *) Tcl_GetHashValue (ht_entry));
	Tcl_AppendResult (interp, "\n", (char *) NULL);
	ht_entry = Tcl_NextHashEntry (&ht_search);
    }
}

/* 
 * Update the size of an expanded parent object.
 */

#ifdef NOTYET      

typedef struct ObjectElem {
    Tki_Object *object;
    struct ObjectElem *nextPtr;
} ObjectElem;

static ObjectElem *resizeList = NULL;

static void
ParentUpdateProc (ClientData clientData)
{
    char *tmp = "reset";

    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    ObjectElem *elem;

    for (elem = resizeList; elem; elem = elem->nextPtr) {
	Tcl_VarEval (interp, type_to_string (elem->object->type),
		     "__resize ", elem->object->id, (char *) NULL);
	m_label (interp, elem->object, 1, &tmp);
    }

    /*
     * Ugly weay of doing a cleanup of the hash table.
     */

    elem = resizeList;
    while (elem) {
	ObjectElem *freeme = elem;
	elem = elem->nextPtr;
	ckfree ((char *) freeme);
    }
    resizeList = NULL;
}

static void
parent_resize (Tcl_Interp *interp, Tki_Object *object)
{
    Tki_Object *parent = object->parent;

    if (parent && (! parent->collapsed)) {
	ObjectElem *elem;
	for (elem = resizeList; elem; elem = elem->nextPtr) {
	    if (elem->object == parent) return;
	}
	elem = (ObjectElem *) ckalloc (sizeof (ObjectElem));
	elem->object = parent;
	elem->nextPtr = resizeList;
	if (! resizeList) {
	    Tk_DoWhenIdle (ParentUpdateProc, (ClientData) interp);
	}
	resizeList = elem;
    }    
}
#endif

static void
#ifdef NOTYET
old_parent_resize (interp, object)
#else
parent_resize (interp, object)
#endif
    Tcl_Interp *interp;
    Tki_Object *object;
{
    char *tmp = "reset";
    Tki_Object *parent = object->parent;

    if ((parent != NULL) && (! (parent->collapsed))) {
	Tcl_VarEval (interp, type_to_string (parent->type),
		     "__resize ", parent->id, (char *) NULL);
	m_label (interp, parent, 1, &tmp);
	parent_resize (interp, parent);
    }
}

/*
 * Update all links to this object (if any).
 */

static void
update_links (Tcl_Interp *interp, Tki_Object *object)
{
    switch (object->type) {
      case TKINED_LINK: 
	m_link_update (interp, object, 0, (char **) NULL);
	break;
      case TKINED_NODE:
      case TKINED_NETWORK:
	{
	    int largc, i;
            const char **largv;
            Tcl_SplitList (interp, object->links, &largc, &largv);
            for (i=0; i < largc; i++) {
                Tki_Object *link = Tki_LookupObject (largv[i]);
                if (link != NULL) {
                    m_link_update (interp, link, 0, (char **) NULL);
                }
            }
	    ckfree ((char*) largv);
	}
      case TKINED_GROUP:
	{
	    if (object->member) {
		int i;
		for (i = 0; object->member[i]; i++) {
		    update_links (interp, object->member[i]);
		}
	    }
	}
    }
}

/*
 * This utility function gets the coordinates of a network
 * and puts them into two arrays named x and y. The next step 
 * is to find a network segment which allows us to draw a horizontal 
 * or vertical line. If this fails, we connect to the endpoint that 
 * is closest to our initial position.
 */

static void 
m_network_link_end (Tcl_Interp *interp, Tki_Object *network, double *sx, double *sy)
{
    int found = 0;
    int i, j, n;
    int largc;
    char **largv;
    double *x;
    double *y;
    double rx = 0, ry = 0;
    double d = -1;

    Tcl_SplitList (interp, network->points, &largc, &largv);

    x = (double *) ckalloc (largc * sizeof(double));
    y = (double *) ckalloc (largc * sizeof(double));

    if (x == NULL || y == NULL) {
	ckfree ((char *) largv);
	return;
    }

    for (n = 0, i = 0; i < largc; i++) {
	if ((i%2) == 0) {
	    Tcl_GetDouble (interp, largv[i], &x[n]);
	    x[n] += network->x;
	} else {
	    Tcl_GetDouble (interp, largv[i], &y[n]);
	    y[n] += network->y;
	    n++;
	}
    }

    for (i = 1, j = 0, found = 0; i < n; i++, j++) {

	double lo_x = (x[i] < x[j]) ? x[i] : x[j];
	double up_x = (x[i] < x[j]) ? x[j] : x[i];
	double lo_y = (y[i] < y[j]) ? y[i] : y[j];
        double up_y = (y[i] < y[j]) ? y[j] : y[i];

	if ((lo_x <= *sx) && (up_x >= *sx)) {
	    double nd = (*sy > y[i]) ? *sy - y[i] : y[i] - *sy;
	    if (d < 0 || nd < d ) {
		rx = *sx; ry = y[i];
		d = nd;
		found++;
	    }
	}

	if ((lo_y <= *sy) && (up_y >= *sy)) {
	    double nd = (*sx > x[i]) ? *sx - x[i] : x[i] - *sx;
	    if (d < 0 || nd < d ) {
                rx = x[i]; ry = *sy;
                d = nd;
		found++;
	    }
        }
    }

    /* If we can not make a horizontal or vertical link
       or if one of the fixed points is much nearer, we 
       simply make a link to the nearest fixed point */
    
    for (i = 0; i < n; i++) {	
	double nd = ((x[i] > *sx) ? x[i] - *sx : *sx - x[i])
		  + ((y[i] > *sy) ? y[i] - *sy : *sy - y[i]);
	if (!found || (nd < d)) {
	    rx = x[i]; ry = y[i];
	    d = nd;
	    found++;
	}
    }
    
    ckfree ((char *) x);
    ckfree ((char *) y);
    ckfree ((char *) largv);

    *sx = rx; *sy = ry;

    return;
}

/*
 * Update the link position. This must get called whenever the link
 * moves or one of the connected objects moves.
 */

#ifdef NOTYET

static Tcl_HashTable *linkUpdateTablePtr = NULL;

static void
LinkUpdateProc (ClientData clientData)
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    if (!linkUpdateTablePtr) return;

    entryPtr = Tcl_FirstHashEntry (linkUpdateTablePtr, &search);
    while (entryPtr) {
	Tki_Object *link = (Tki_Object *) Tcl_GetHashValue (entryPtr);
	m_old_link_update (interp, link, 0, 0);
	parent_resize (interp, link);
	entryPtr = Tcl_NextHashEntry (&search);
    }

    /*
     * Ugly weay of doing a cleanup of the hash table.
     */

    Tcl_DeleteHashTable (linkUpdateTablePtr);
    Tcl_InitHashTable (linkUpdateTablePtr, TCL_ONE_WORD_KEYS);
}

static int
m_link_update (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int dummy;
    Tcl_HashEntry *entryPtr;

    if (!linkUpdateTablePtr) {
	linkUpdateTablePtr = (Tcl_HashTable *) ckalloc (sizeof(Tcl_HashTable));
	Tcl_InitHashTable (linkUpdateTablePtr, TCL_ONE_WORD_KEYS);
    }

    entryPtr = Tcl_CreateHashEntry (linkUpdateTablePtr, 
				    (char *) object, &dummy);
    if (entryPtr) {
	Tcl_SetHashValue (entryPtr, (char *) object);
    }
    
    Tk_DoWhenIdle (LinkUpdateProc, (ClientData) interp);

    return TCL_OK;
}

#endif

static int 
#ifdef NOTYET
m_old_link_update (interp, object, argc, argv)
#else
m_link_update (interp, object, argc, argv)
#endif
    Tcl_Interp *interp;
    Tki_Object *object;
    int argc;
    char **argv;
{
    char *tmp;
    char buf_a[20], buf_b[20];
    int n;
    double *x;
    double *y;
    Tki_Object *src, *dst;
    int selected = object->selected;
    double x_a, y_a, x_b, y_b;

    /* Get the objects linked by this link object. Search for 
       parent objects if there is currently no canvas. */

    src = object->src; 
    while (src != NULL && (*(src->canvas) == 0)) {
	src = src->parent;
    }

    dst = object->dst;
    while (dst != NULL && (*(dst->canvas) == 0)) {
	dst = dst->parent;
    }

    if (src == NULL || dst == NULL) {
	Tcl_SetResult (interp, "update link: can not find linked objects",
		       TCL_STATIC);
	return TCL_ERROR;
    }

    if (src->type == TKINED_NETWORK) {
	x_a = dst->x;
	y_a = dst->y;
	m_network_link_end (interp, src, &x_a, &y_a);
    } else {
	x_a = src->x;
	y_a = src->y;
    }

    if (dst->type == TKINED_NETWORK) {
	x_b = src->x;
	y_b = src->y;
	m_network_link_end (interp, dst, &x_b, &y_b);
    } else {
	x_b = dst->x;
	y_b = dst->y;
    }

    /* handle fixed points if any */

    tmp = NULL;
    if (strlen(object->points) > 0) {
	int i,largc;
	char **largv;
        char tbuf[128];
	Tcl_SplitList (interp, object->points, &largc, &largv);

	if (largc > 0) {
	    x = (double *) ckalloc (largc * sizeof(double));
	    y = (double *) ckalloc (largc * sizeof(double));
	
	    if (x == NULL || y == NULL) {
		ckfree ((char*) largv);
		Tcl_ResetResult (interp);
		snprintf(tbuf, sizeof(tbuf), "%f %f", object->x, object->y);
                Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
		return TCL_OK;
	    }
	    
	    for (n = 0, i = 0; i < largc; i++) {
		if ((i%2) == 0) {
		    Tcl_GetDouble (interp, largv[i], &x[n]);
		    x[n] += object->x;
		} else {
		    Tcl_GetDouble (interp, largv[i], &y[n]);
		    y[n] += object->y;
		    n++;
		}
	    }
	    
	    if (x[0] == x[1]) {
	        y[0] = y_a;
	    } else {
	        x[0] = x_a;
	    }
	    
	    if (x[n-1] == x[n-2]) {
	        y[n-1] = y_b;
	    } else {
	        x[n-1] = x_b;
	    }

	    if (n == 1) {
	        x[0] = x_a;
	        y[0] = y_b;
	    }

	    tmp = ckalloc((size_t) (n*32));
	    *tmp = 0;
	    for (i = 0; i < n; i++) {
		sprintf (buffer, "%.2f %.2f ", x[i], y[i]);
		strcat (tmp, buffer);
	    }

	    ckfree ((char *) x);
	    ckfree ((char *) y);
	}
	ckfree ((char *) largv);
    }

    sprintf (buf_a, "%.2f %.2f ", x_a, y_a);
    sprintf (buf_b, "%.2f %.2f ", x_b, y_b);

    if (selected) {
	m_unselect (interp, object, 0, (char **) NULL);
    }
    
    Tcl_VarEval (interp, "foreach item [", object->id, " items] {",
		 "if {[", object->canvas, " type $item]==\"line\"} break }; ",
		 "eval ", object->canvas, " coords $item ", 
		 buf_a, (tmp == NULL) ? "" : tmp, buf_b,
		 (char *) NULL);

    if (tmp != NULL) ckfree (tmp);

    if (selected) {
        m_select (interp, object, 0, (char **) NULL);
    }
    return TCL_OK;
}


/*
 * Create a NODE object. Initialize the id and name fields.
 */

static int 
m_node_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "node%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create NODE", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a GROUP object. Initialize the id and name fields.
 * Set the member to the ids given in argv.
 */

static int 
m_group_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "group%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, buffer);
    object->collapsed = 0;

    m_member (interp, object, argc, argv);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create GROUP", argc, argv, object->id);
    
    return TCL_OK;
}

/*
 * Create a NETWORK object. Initialize the id and name fields.
 * Store the points in the member field and set the position
 * to the first x and y coordinates.
 */

static int 
m_network_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "network%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, buffer);

    if (argc > 1) {
	int i;
	Tcl_GetDouble (interp, argv[0], &(object->x));
	Tcl_GetDouble (interp, argv[1], &(object->y));
	buffersize ((size_t) (argc*12));
	*buffer = '\0';
	for (i = 0; i < (argc/2)*2; i++) {
	    char tmp[20];
	    double val;
	    Tcl_GetDouble (interp, argv[i++], &val);
	    sprintf (tmp, "%f ", val - object->x);
	    strcat (buffer, tmp);
	    Tcl_GetDouble (interp, argv[i], &val);
	    sprintf (tmp, "%f ", val - object->y);
	    strcat (buffer, tmp);
	}
	STRCOPY (object->points, buffer);
    } else {
	/* default length and position */
	STRCOPY (object->points, "0 0 130 0");
	object->x = 50;
	object->y = 50;
    }

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create NETWORK", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a LINK object. Initialize the id and name fields.
 * Initialize the fields that contain the id of the connected
 * objects.
 */

static int 
m_link_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    if (argc < 2) {
        Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    sprintf(buffer, "link%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, buffer);
    object->src = Tki_LookupObject (argv[0]);
    object->dst = Tki_LookupObject (argv[1]);

    if (argc > 3) {
	char *freeme = Tcl_Merge (argc-2, argv+2);
        STRCOPY (object->points, freeme);
        ckfree (freeme);
    }

    if (object->src) {
	lappend (&object->src->links, object->id);
    }

    if (object->dst) {
	lappend (&object->dst->links, object->id);
    }

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create LINK", argc, argv, object->id);
    
    return TCL_OK;
}

/*
 * Create a TEXT object. Initialize the id and name fields.
 * Map any newline characters to the sequence \n.
 */

static int 
m_text_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    if (argc == 1) {
	sprintf(buffer, "text%d", lastid++);
	STRCOPY (object->id, buffer);
	
	m_text (interp, object, 1, &argv[0]);

	TkiTrace (object->editor, (Tki_Object *) NULL, 
	       "ined create TEXT", argc, argv, object->id);
    }

    return TCL_OK;
}

/*
 * Create an IMAGE object. Initialize the id and name fields,
 * which contains the path to the bitmap file.
 */

static int 
m_image_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;
    char *file;

    if (argc < 1) {
        Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    file = findfile (interp, argv[0]);
    if (file == NULL) {
	file = argv[0];
    }

    STRCOPY (object->name, file);
    sprintf(buffer, "image%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create IMAGE", argc, argv, object->id);

    return TCL_OK;
}

#ifdef USE_TCP_CHANNEL
/*
 * Accept an application that connects back to the Tkined editor.
 */

static void
AcceptProc(ClientData clientData, Tcl_Channel channel, char *hostName, int port)
{
    int code;
    Tki_Object *object = (Tki_Object *) clientData;

    if ((strcmp(hostName, "127.0.0.1") != 0) && (strcmp(hostName, "::1") != 0)) {
	Tcl_Channel errChannel = Tcl_GetStdChannel(TCL_STDERR);
	if (errChannel) {
	    Tcl_Write(errChannel, "tkined: connection from ", -1);
	    Tcl_Write(errChannel, hostName, -1);
	    Tcl_Write(errChannel, " refused", -1);
	}
	Tcl_Close((Tcl_Interp *) NULL, channel);
	return;
    }

    code = Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, 
				"-blocking", "0");
    if (code != TCL_OK) {
	Tcl_Close((Tcl_Interp *) NULL, channel);
        return;
    }

    Tcl_RegisterChannel((Tcl_Interp *) NULL, channel);
    Tcl_CreateChannelHandler(channel, TCL_READABLE, 
			     receive, (ClientData) object);
    Tcl_UnregisterChannel((Tcl_Interp *) NULL, object->channel);
    object->channel = channel;
    object->incomplete = 0;
}

/*
 * A dummy procedure used to timeout while waiting for a new tool to
 * register itself.
 */

static void
TimeOutProc(ClientData clientData)
{
    Tki_Object *object = (Tki_Object *) clientData;

    object->timeout = 1;
}
#endif

/*
 * Create an INTERPRETER object. Initialize the id and name fields.
 * Initialize a command buffer and fork a process to do the real work.
 */

static int 
m_interpreter_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

#ifndef __WIN32__
    FILE *in;
#endif
    int pargc = 0;
    char **pargv = NULL;
    char *file;
    int largc;
    char **largv = NULL;
    char *bang = NULL;
    int i;
#ifndef USE_TCP_CHANNEL
    int code;
#endif

    if (argc != 1) {
        Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    if (Tcl_SplitList (interp, argv[0], &largc, &largv) != TCL_OK) {
	return TCL_ERROR;
    }

    if (largc == 0) {
	Tcl_SetResult (interp, "empty arg", TCL_STATIC);
	ckfree ((char *) largv);
	return TCL_ERROR;
    }

    if ((file = findfile (interp, largv[0])) == (char *) NULL) {
	m_delete (interp, object, 0, (char **) NULL);
	Tcl_ResetResult (interp);
	Tcl_AppendResult (interp, largv[0], " not found", (char *) NULL);
	ckfree ((char *) largv);
	return TCL_ERROR;
    }

    STRCOPY (object->name, file);
    sprintf(buffer, "interpreter%d", lastid++);
    STRCOPY (object->id, buffer);
    object->cmd = (Tcl_DString *) ckalloc (sizeof (Tcl_DString));
    Tcl_DStringInit (object->cmd);
    object->done = 1;
    object->interp = interp;

    /*
     * start #! scripts by hand to allow long pathes 
     */

    pargv = (char **) ckalloc ((largc + 5) * sizeof (char *));
    memset ((char *) pargv, 0, (largc + 5) * sizeof (char *));

#ifdef __WIN32__
    {
	char *path;
	path = Tcl_GetVar2(interp, "tkined", "tclsh", TCL_GLOBAL_ONLY);
	if (! path) {
	    Tcl_SetResult(interp, "unable to find the tclsh interpreter",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	pargv[pargc++] = path;
	pargv[pargc++] = object->name;
    }
#else
    if ((in = fopen (object->name, "r"))) {
	bang = fgets (buffer, 512, in);
	fclose (in);
    }

    if (bang && bang[0] == '#' && bang[1] == '!') {
	char *p;
	for (p = bang + 2; isspace(*p); p++) ;
	pargv[pargc++] = p;
	while (*p && !isspace(*p)) p++;
	while (*p && isspace(*p)) *p++ = 0;
	pargv[pargc] = p;
	while (*p && !isspace(*p)) p++;
	*p = 0;
	pargc += (strlen (pargv[pargc]) != 0);
	pargv[pargc++] = object->name;
    }
#endif
	
    for (i = 1; i < largc; i++) {
	pargv[pargc++] = largv[i];
    }

#ifdef USE_TCP_CHANNEL
    { 
	int port, start;
	static char buffer[80];
	Tcl_Channel channel;
	Tcl_TimerToken token;

	/*
	 * First, open a server channel for this connection.
	 */

	start = 1700 + (rand() % 1000);
	for (port = start; port < start + 100; port++) {
	    object->channel = Tcl_OpenTcpServer(interp, port, NULL,
						AcceptProc,
						(ClientData) object);
	    if (object->channel) break;
	}
	if (! object->channel) {
	    ckfree((char *) pargv);
	    ckfree((char *) largv);
	    goto errorExit;
	}

	/*
	 * Pass the TCP port number to the client via an 
	 * environment variable.
	 */

	sprintf(buffer, "TNM_INED_TCPPORT=%d", port);
	Tcl_PutEnv(buffer);

	/*
	 * Fork a process that runs in the background and we are done.
	 */

	pargv[pargc++] = "&";
	channel = Tcl_OpenCommandChannel(interp, pargc, pargv, 0);
	ckfree((char *) pargv);
	ckfree((char *) largv);

	if (channel == NULL) {
	    goto errorExit;
	}

	/*
         * Get the list of PIDs from the pipeline into interp->result and
         * detach the PIDs (instead of waiting for them).
         */

        TclGetAndDetachPids(interp, channel);

	/*
	 * Wait for the interpreter to connect...
	 */

	token = Tcl_CreateTimerHandler(10000, TimeOutProc, (ClientData) object);
	object->incomplete = 1;
	object->timeout = 0;
	while (object->incomplete && ! object->timeout) {
	    Tcl_DoOneEvent(TCL_FILE_EVENTS|TCL_TIMER_EVENTS);
	}
	Tcl_DeleteTimerHandler(token);

	if (object->incomplete) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "failed to accept connection from \"",
			     object->name, "\"", (char *) NULL);
	    Tcl_Close((Tcl_Interp *) NULL, channel);
	    Tcl_Close((Tcl_Interp *) NULL, object->channel);
	    goto errorExit;
	}
    }
#else

    object->channel = Tcl_OpenCommandChannel(interp, pargc, pargv, 
					     TCL_STDIN|TCL_STDOUT);
    ckfree ((char *) pargv);
    ckfree ((char *) largv);

    if (object->channel == NULL) {
        goto errorExit;
    }

    /*
     * Make the channel non blocking so that we can use Tcl_Read().
     */

    code = Tcl_SetChannelOption(interp, object->channel, "-blocking", "0");
    if (code != TCL_OK) {
        goto errorExit;
    }

    Tcl_CreateChannelHandler(object->channel, TCL_READABLE, 
			     receive, (ClientData) object);
#endif

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create INTERPRETER", argc, argv, object->id);

    return TCL_OK;

  errorExit:
    bang = ckstrdup(Tcl_GetStringResult(interp));
    m_delete(interp, object, 0, (char **) NULL);
    Tcl_SetResult(interp, bang, TCL_DYNAMIC);
    return TCL_ERROR;
}

/*
 * Create a MENU object. Initialize the id and name fields.
 * The items field contains the commands of the menu and 
 * the links field contains the id of the interpreter object.
 */

static int 
m_menu_create (Tcl_Interp *interp, Tki_Object *object, int argc, const char **argv)
{
    static unsigned lastid = 0;
    char *freeme;

    if (argc < 2) {
        Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    sprintf (buffer, "menu%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, argv[0]);
    freeme = Tcl_Merge (argc-1, argv+1);
    STRCOPY (object->items, freeme);
    ckfree (freeme);

    return TCL_OK;
}

/*
 * Create a LOG object (window). Initialize the id and name fields.
 */

static int
m_log_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    char *user;
    static unsigned lastid = 0;

    sprintf(buffer, "log%d", lastid++);
    STRCOPY (object->id, buffer);
    STRCOPY (object->name, buffer);

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

    STRCOPY (object->address, user);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create LOG", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a REFERENCE object pointing to another tkined map. 
 * Initialize the id.
 */

static int
m_ref_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "reference%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create REFERENCE", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a STRIPCHART object showing statistics in a stripchart.
 */

static int
m_strip_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "stripchart%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create STRIPCHART", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a BARCHART object showing statistics in a barchart.
 */

static int
m_bar_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "barchart%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create BARCHART", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a GRAPH which uses the BLT graph widget to allow some
 * simple form of data analysis.
 */

static int
m_graph_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "graph%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create GRAPH", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create a DATA stream object. Monitored variables are written to a
 * data stream object which provides basic statistics and forwards
 * values to stripchart, barchart or whatever objects will be defined.
 */

static int
m_data_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "data%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create DATA", argc, argv, object->id);

    return TCL_OK;
}

/*
 * Create an EVENT object.
 */

static int
m_event_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    static unsigned lastid = 0;

    sprintf(buffer, "event%d", lastid++);
    STRCOPY (object->id, buffer);

    TkiTrace (object->editor, (Tki_Object *) NULL, 
	   "ined create EVENT", argc, argv, object->id);

    return TCL_OK;
}

/*
 * The create method just dispatches to one of those object type
 * specific functions.
 */

int
m_create (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    switch (object->type) {
 case TKINED_NODE: 
	return m_node_create (interp, object, argc, argv);
 case TKINED_GROUP: 
	return m_group_create (interp, object, argc, argv);
 case TKINED_NETWORK: 
	return m_network_create (interp, object, argc, argv);
 case TKINED_LINK:
	return m_link_create (interp, object, argc, argv);
 case TKINED_TEXT:
	return m_text_create (interp, object, argc, argv);
 case TKINED_IMAGE:
	return m_image_create (interp, object, argc, argv);
 case TKINED_INTERPRETER:
	return m_interpreter_create (interp, object, argc, argv);
 case TKINED_MENU:
	return m_menu_create (interp, object, argc, argv);
 case TKINED_LOG:
	return m_log_create (interp, object, argc, argv);
 case TKINED_REFERENCE:
	return m_ref_create (interp, object, argc, argv);
 case TKINED_STRIPCHART:
	return m_strip_create (interp, object, argc, argv);
 case TKINED_BARCHART:
	return m_bar_create (interp, object, argc, argv);
 case TKINED_GRAPH:
	return m_graph_create (interp, object, argc, argv);
 case TKINED_DATA:
	return m_data_create (interp, object, argc, argv);
 case TKINED_EVENT:
	return m_event_create (interp, object, argc, argv);
    }
    return TCL_OK;
}

/*
 * Retrieve the external representation of a NODE object.
 */

static int 
m_node_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    sprintf (buffer, "%u", object->oid);
    Tcl_AppendElement (interp, "NODE");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);
    Tcl_AppendElement (interp, buffer);
    Tcl_AppendElement (interp, object->links);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a GROUP object.
 */

static int 
m_group_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    sprintf (buffer, "%u", object->oid);
    Tcl_AppendElement (interp, "GROUP");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, buffer);
    if (object->member) {
	int i;
	Tcl_DString dst;
	Tcl_DStringInit (&dst);
	Tcl_DStringGetResult (interp, &dst);
	Tcl_DStringStartSublist (&dst);
	for (i = 0; object->member[i]; i++) {
	    Tcl_DStringAppendElement (&dst, object->member[i]->id);
	}
	Tcl_DStringEndSublist (&dst);
	Tcl_DStringResult (interp, &dst);
    }

    return TCL_OK;
}

/*
 * Retrieve the external representation of a NETWORK object.
 */

static int
m_network_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{    
    sprintf (buffer, "%u", object->oid);
    Tcl_AppendElement (interp, "NETWORK");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);
    Tcl_AppendElement (interp, buffer);
    Tcl_AppendElement (interp, object->links);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a LINK object.
 */

static int
m_link_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "LINK");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->src ? object->src->id : "");
    Tcl_AppendElement (interp, object->dst ? object->dst->id : "");

    return TCL_OK;
}

/*
 * Retrieve the external representation of a TEXT object.
 */

static int
m_text_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "TEXT");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->text);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a IMAGE object.
 */

static int
m_image_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "IMAGE");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a INTERPRETER object.
 */

static int
m_interpreter_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "INTERPRETER");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a MENU object.
 */

static int
m_menu_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "MENU");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->items);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a LOG object.
 */

static int
m_log_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "LOG");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a REFERENCE object.
 */

static int
m_ref_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "REFERENCE");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a STRIPCHART object.
 */

static int
m_strip_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "STRIPCHART");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a BARCHART object.
 */

static int
m_bar_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "BARCHART");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a GRAPH object.
 */

static int
m_graph_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "GRAPH");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a DATA object.
 */

static int
m_data_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "DATA");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendElement (interp, object->address);

    return TCL_OK;
}

/*
 * Retrieve the external representation of a EVENT object.
 */

static int
m_event_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_AppendElement (interp, "EVENT");
    Tcl_AppendElement (interp, object->id);
    Tcl_AppendElement (interp, object->name);

    return TCL_OK;
}

/*
 * Retrieve the external representation of an object.
 */

int
m_retrieve (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    switch (object->type) {
 case TKINED_NODE: 
	return m_node_retrieve (interp, object, argc, argv);
 case TKINED_GROUP: 
	return m_group_retrieve (interp, object, argc, argv);
 case TKINED_NETWORK: 
	return m_network_retrieve (interp, object, argc, argv);
 case TKINED_LINK:
	return m_link_retrieve (interp, object, argc, argv);
 case TKINED_TEXT:
	return m_text_retrieve (interp, object, argc, argv);
 case TKINED_IMAGE:
	return m_image_retrieve (interp, object, argc, argv);
 case TKINED_INTERPRETER:
	return m_interpreter_retrieve (interp, object, argc, argv);
 case TKINED_MENU:
	return m_menu_retrieve (interp, object, argc, argv);
 case TKINED_LOG:
	return m_log_retrieve (interp, object, argc, argv);
 case TKINED_REFERENCE:
	return m_ref_retrieve (interp, object, argc, argv);
 case TKINED_STRIPCHART:
	return m_strip_retrieve (interp, object, argc, argv);
 case TKINED_BARCHART:
	return m_bar_retrieve (interp, object, argc, argv);
 case TKINED_GRAPH:
	return m_graph_retrieve (interp, object, argc, argv);
 case TKINED_DATA:
	return m_data_retrieve (interp, object, argc, argv);
 case TKINED_EVENT:
	return m_event_retrieve (interp, object, argc, argv);
    }

    return TCL_OK;
}

/*
 * Return the type of an object.
 */

int
m_type (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, type_to_string (object->type), TCL_STATIC);

    return TCL_OK;
}

/*
 * Return the id of an object.
 */

int 
m_id (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, object->id, TCL_STATIC);

    return TCL_OK;
}

/*
 * Return the parent of an object.
 */

int
m_parent (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, 
		   object->parent ? object->parent->id : "", TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the name of an object. Call the tk callback if the label 
 * is set to show the name.
 */

int
m_name (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	ckfree (object->name);
	object->name = ckstrdupnn (argv[0]);

	if (object->type == TKINED_LOG) {
	    sprintf (buffer, "%s__name %s",
		     type_to_string (object->type), object->id);
	    Tcl_Eval (interp, buffer);
	}

	if (strcmp(object->label, "name") == 0) {
	    TkiNoTrace (m_label, interp, object, 1, &object->label);
	}

	TkiTrace (object->editor, object, 
	       "ined name", argc, argv, object->name);
    }

    Tcl_SetResult (interp, object->name, TCL_STATIC);
    return TCL_OK;
}

/*
 * Return the canvas of an object.
 */

int 
m_canvas (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc > 0 ) {

        STRCOPY (object->canvas, argv[0]);

	if (strlen(object->canvas) > 0) {
	    Tcl_VarEval (interp, type_to_string (object->type),
			 "__canvas ", object->id, (char *) NULL);
	    if (object->type == TKINED_LINK) {
		m_link_update (interp, object, 0, (char **) NULL);
	    }

	    if (object->scale != 0) {
		char *tmp = ckalloc(80);
		sprintf (tmp, "%f", object->scale);
		m_scale (interp, object, 1, &tmp);
		ckfree (tmp);
	    }
	}

	if (object->type == TKINED_LINK) {
	    m_lower (interp, object, 0, (char **) NULL);
	}

	/* Update all links connected to this NODE or NETWORK object */

	if (object->type == TKINED_NODE || object->type == TKINED_NETWORK) {
	    update_links (interp, object);
	}

    }

    Tcl_SetResult (interp, object->canvas, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get or set the editor of this object. This implementation
 * does not really check if the argument is a real editor object.
 */

int
m_editor (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{

    if (argc == 1) {

	int dotrace = (object->editor == NULL);

	Tcl_CmdInfo info;
	if (Tcl_GetCommandInfo (interp, argv[0], &info) > 0) {
	    object->editor = (Tki_Editor *) info.clientData;
	}

	if (dotrace) {
	    TkiTrace (object->editor, (Tki_Object *) NULL, 
		   (char *) NULL, 0 , (char **) NULL, (char *) NULL);
	}
    }

    if (object->editor != NULL) {
	Tcl_SetResult (interp, object->editor->id, TCL_STATIC);
    } else {
	Tcl_ResetResult (interp);
    }

    return TCL_OK;
}

/*
 * Get and set the tk canvas items belonging to an object.
 */

int
m_items (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {
        STRCOPY (object->items, argv[0]);
    }
    
    Tcl_SetResult (interp, object->items, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the address of an object. Call the tk callback if the
 * label is set to show the address.
 */

int
m_address (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	ckfree (object->address);
	object->address = ckstrdupnn (argv[0]);
	
	if (strcmp(object->label, "address") == 0) {
	    TkiNoTrace (m_label, interp, object, 1, &object->label);
	}

	TkiTrace (object->editor, object, 
	       "ined address", argc, argv, object->address);
    }

    Tcl_SetResult (interp, object->address, TCL_STATIC);
    return TCL_OK;
}


/*
 * Get and set the oid of an object. There is no callback here.
 */

int
m_oid (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int result;
    
    if (argc == 1) {

	if (Tcl_GetInt (interp, argv[0], &result) != TCL_OK) 
		return TCL_ERROR;

	object->oid = result;

	TkiTrace (object->editor, object, 
	       "ined oid", argc, argv, argv[0]);
    }

    Tcl_ResetResult (interp);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(object->oid));
    return TCL_OK;
}

/*
 * Get and set the actions bound to an tkined object.
 */

int
m_action (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	STRCOPY (object->action, argv[0]);

	TkiTrace (object->editor, object, 
	       "ined action", argc, argv, object->action);
    }

    Tcl_SetResult (interp, object->action, TCL_STATIC);
    return TCL_OK;
}

/*
 * Select an object. Mark it as selected and call the tk
 * procedure to actually do the selection. Skip objects
 * that are invisible (that is have no canvas).
 */

int
m_select (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if ((!object->selected) && (*(object->canvas) != '\0')) {
	object->selected = 1;
	
	Tcl_VarEval (interp, type_to_string (object->type),
		     "__select ", object->id, (char *) NULL);
    }

    Tcl_ResetResult (interp);
    if (object->editor) {
	Tki_EditorSelection (object->editor, interp, 0, (char **) NULL);
    }
    return TCL_OK;
}

/*
 * Unselect on object. Mark it as unselected and call the tk
 * procedure to actually do the unselection.
 */

int
m_unselect (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->selected) {
	object->selected = 0;
	
	Tcl_VarEval (interp, type_to_string (object->type),
		     "__unselect ", object->id, (char *) NULL);
    }

    Tcl_ResetResult (interp);
    if (object->editor) {
	Tki_EditorSelection (object->editor, interp, 0, (char **) NULL);
    }
    return TCL_OK;
}

/*
 * Return a boolean indicating if the object is selected or not.
 */

int
m_selected (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_ResetResult (interp);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(object->selected));
    return TCL_OK;
}

/*
 * Get and set the icon of an object. Call tkined_<TYPE>_icon
 * to let tk update the canvas.
 */

int
m_icon (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    char *tmp = "reset";
    const char *objectIcon;

    int selected = object->selected;

    if (argc == 1) {

	Tki_Editor *editor = object->editor;

	buffersize (strlen(argv[0])+40);
	sprintf (buffer, "%s-icon-%s", 
		 type_to_string (object->type), argv[0]);

	Tki_EditorAttribute (editor, interp, 1, &buffer);
        objectIcon = Tcl_GetStringResult(interp);
	if (*objectIcon != '\0') {
	    STRCOPY (object->icon, objectIcon);
	} else {
	    if (argv[0][0] != '\0') {
		STRCOPY (object->icon, argv[0]);
	    } else {
		switch (object->type) {
	    case TKINED_NODE:
		    STRCOPY (object->icon, "node");
		    break;
	    case TKINED_GROUP:
		    STRCOPY (object->icon, "group");
		    break;
	    case TKINED_NETWORK:
		    STRCOPY (object->icon, "network");
		    break;
	    case TKINED_LOG:
		    STRCOPY (object->icon, "");
		    break;
	    case TKINED_REFERENCE:
		    STRCOPY (object->icon, "reference");
		    break;
	    case TKINED_GRAPH:
		    STRCOPY (object->icon, "solid");
		    break;
		}
	    }
	}
	Tcl_ResetResult (interp);

	if (object->type == TKINED_NETWORK) {
	    int width;
	    if (Tcl_GetInt (interp, object->icon, &width) != TCL_OK) {
		STRCOPY (object->icon, "3");
	    }
	}

	if (object->type == TKINED_GRAPH) {
	    int width;
	    if (Tcl_GetInt (interp, object->icon, &width) != TCL_OK) {
		STRCOPY (object->icon, "0");
	    }
	}

	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}

        Tcl_VarEval (interp, type_to_string (object->type), "__icon ", 
		     object->id, " ", object->icon, (char *) NULL);

      	TkiNoTrace (m_label, interp, object, 1, &tmp);

	/* adjust the size of a parent group if necessary */
      
	parent_resize (interp, object);

	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}

	TkiTrace (object->editor, object, 
	       "ined icon", argc, argv, object->icon);
    }
    
    Tcl_SetResult (interp, object->icon, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the label of an object. Call tkined_<TYPE>_label
 * to let tk update the canvas.
 */

int
m_label (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc > 0) {

	if ( strcmp(argv[0], "clear") == 0 ) {
	    STRCOPY (object->label, argv[0]);
	    Tcl_VarEval (interp, type_to_string (object->type),
			 "__clearlabel ", object->id, (char *) NULL);
	    Tcl_ResetResult (interp);
	    TkiTrace (object->editor, object, 
		   "ined label", argc, argv, (char *) NULL);
	} else if ( strcmp(argv[0], "reset") == 0) {
	    Tcl_VarEval (interp, type_to_string (object->type),
			 "__clearlabel ", object->id, (char *) NULL);
	    Tcl_ResetResult (interp);
	    TkiNoTrace (m_label, interp, object, 1, &object->label);
	} else {
	    char *txt = NULL;
	    if (strcmp(argv[0], "name") == 0) { 
		txt = object->name;
	    } else if (strcmp(argv[0], "address") == 0 ) {
		txt = object->address;
	    } else {
		Tcl_HashEntry *ht_entry;
		ht_entry = Tcl_FindHashEntry (&(object->attr), argv[0]);
		if (ht_entry) {
		    txt = (char *) Tcl_GetHashValue (ht_entry);
		}
	    }
	    if (txt != NULL) {
	        STRCOPY (object->label, argv[0]);
		/* XXX fix this: this quoting breaks any " characters */
		Tcl_VarEval (interp, type_to_string (object->type),
			     "__label ", object->id, " \"", txt, "\"",
			     (char *) NULL);
		Tcl_ResetResult (interp);
		TkiTrace (object->editor, object,
                       "ined label", argc, argv, (char *) NULL);
	    }
	}
    }

    Tcl_SetResult (interp, object->label, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the font of an object. Call <TYPE>__font
 * to let tk update the canvas.
 */

int
m_font (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int selected = (object->selected && object->type == TKINED_TEXT);

    if (argc == 1) {

#if 0
	Tki_Editor *editor = object->editor;

	buffersize (strlen(argv[0])+8);
	sprintf (buffer, "font-%s", argv[0]);

	Tki_EditorAttribute (editor, interp, 1, &buffer);
	if (*interp->result != '\0') {
	    STRCOPY (object->font, interp->result);
	} else {
	    if (argv[0][0] != '\0') {
		STRCOPY (object->font, argv[0]);
	    } else {
		STRCOPY (object->font, "fixed");
	    }
	}
	Tcl_ResetResult (interp);
#else
	STRCOPY (object->font, argv[0]);
#endif

	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}
	
	Tcl_VarEval (interp, type_to_string (object->type), 
		     "__font ", object->id, " \"", object->font, "\"",
		     (char *) NULL);

	if (selected) {
            m_select (interp, object, 0, (char **) NULL);
        }

	TkiTrace (object->editor, object, 
	       "ined font", argc, argv, object->font);
    }

    Tcl_SetResult (interp, object->font, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the color of an object. Call <TYPE>__color to update the 
 * view. We convert to X11 color name here which makes more sense and is
 * much faster.
 */

int
m_color (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	Tki_Editor *editor = object->editor;

	buffersize (strlen(argv[0])+8);
	sprintf (buffer, "color-%s", argv[0]);

	Tki_EditorAttribute (editor, interp, 1, &buffer);
	if (*Tcl_GetStringResult(interp) != '\0') {
	    STRCOPY (object->color, Tcl_GetStringResult(interp));
	} else {
	    if (argv[0][0] != '\0') {
		STRCOPY (object->color, argv[0]);
	    } else {
		STRCOPY (object->color, "black");
	    }
	}
	Tcl_ResetResult (interp);

	if (editor->color) {
	    Tcl_VarEval (interp, type_to_string (object->type),
			 "__color ", object->id, " ", object->color,
			 (char *) NULL);
	} else {
	    Tcl_VarEval (interp, type_to_string (object->type),
			 "__color ", object->id, " black", (char *) NULL);
	}

	TkiTrace (object->editor, object,
	       "ined color", argc, argv, object->color);
    }

    Tcl_SetResult (interp, object->color, TCL_STATIC);
    return TCL_OK;
}

/*
 * Move an object. Return the new position and after calling
 * the tk callback <TYPE>__move.
 */

int
m_move (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    char tbuf[128];

    if (argc == 2) {

	double x, y;
	char buf[40];

	if (Tcl_GetDouble (interp, argv[0], &x) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (Tcl_GetDouble (interp, argv[1], &y) != TCL_OK) {
	    return TCL_ERROR;
	}

	/* 
	 * Don't move objects outside of the canvas! Beware of expanded
	 * groups! Fix done by Erik.
	 */

	if (object->editor 
	    && !(object->type == TKINED_GROUP && !object->collapsed)) {
	    if (object->x+x < 0) x = - object->x;
	    if (object->y+y < 0) y = - object->y;
	    if (object->x+x > object->editor->width)
		    x = object->editor->width - object->x;
	    if (object->y+y > object->editor->height) 
		    y = object->editor->height - object->y;
	}

	object->x += x;
	object->y += y;

	if ( *(object->canvas) != '\0') {
	    if (object->type == TKINED_LINK) {
		m_link_update (interp, object, 0, (char **) NULL);
	    } else {
		sprintf (buffer, "%s__move %s %f %f", 
			 type_to_string (object->type), object->id, x, y);
		Tcl_Eval (interp, buffer);
	    }
	}

	/*
	 * Move all members of a group object.
	 */

	if (object->type == TKINED_GROUP && !object->collapsed) {
	    object->x -= x;
	    object->y -= y;
	    if (object->member) {
	        int i;
	        for (i = 0; object->member[i]; i++) {
		    TkiNoTrace (m_move, interp, object->member[i], argc, argv);
		}
	    }
	}

	/* 
	 * Now update all visible links to this object and adjust
	 * the size of the parent group if necessary
	 */

	update_links (interp, object);
	parent_resize (interp, object);
      
	sprintf (buf, "%f %f", object->x, object->y);
	TkiTrace (object->editor, object, 
	       "ined move", argc, argv, buf);
    }

    Tcl_ResetResult (interp);
    snprintf (tbuf, sizeof(tbuf), "%f %f", object->x, object->y);
    Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * Raise all items belonging to an object.
 */

int
m_raise (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_VarEval (interp, type_to_string (object->type),
		 "__raise ", object->id, (char *) NULL);

    TkiTrace (object->editor, object, 
	   "ined raise", argc, argv, (char *) NULL);

    Tcl_SetResult (interp, object->id, TCL_STATIC);
    return TCL_OK;
}

/*
 * Lower all items belonging to an object. Make sure that images
 * are always in the background. The loop through all objects
 * should be optimized.
 */

int
m_lower (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{

    Tcl_VarEval (interp, type_to_string (object->type),
		 "__lower ", object->id, (char *) NULL);

    if (object->type != TKINED_IMAGE) {
	Tcl_HashEntry *ht_entry;
	Tcl_HashSearch ht_search;

	ht_entry = Tcl_FirstHashEntry(&tki_ObjectTable, &ht_search);
	while (ht_entry != NULL) {
	    Tki_Object *any;
	    any = (Tki_Object *) Tcl_GetHashValue (ht_entry);
	    if (any->type == TKINED_IMAGE) {
		TkiNoTrace (m_lower, interp, any, 0, (char **) NULL);
	    }
	    ht_entry = Tcl_NextHashEntry (&ht_search);
	}
    }

    TkiTrace (object->editor, object, 
	   "ined lower", argc, argv, (char *) NULL);

    Tcl_SetResult (interp, object->id, TCL_STATIC);
    return TCL_OK;
}

/*
 * Return the size of an object by the coordinates of the bounding box.
 */

int
m_size (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int ret;

    if (argc == 4 && (object->type & (TKINED_STRIPCHART | TKINED_BARCHART))) {
	int selected = object->selected;
	char *tmp = "reset";
	double x1, y1, x2, y2;
	
	if (Tcl_GetDouble (interp, argv[0], &x1) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetDouble (interp, argv[1], &y1) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetDouble (interp, argv[2], &x2) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetDouble (interp, argv[3], &y2) != TCL_OK) return TCL_ERROR;

	/* The bounding box is 1 point larger. This is why
	   we make the real size a little bit smaller. */

	x1 += 1; y1 += 1; x2 -= 1; y2 -= 1;

	object->x = (x2+x1)/2;
	object->y = (y2+y1)/2;

	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}
	sprintf (buffer, " %f %f %f %f", x1, y1, x2, y2);
	ret = Tcl_VarEval (interp, type_to_string (object->type), "__resize ",
			   object->id, buffer, (char *) NULL);

	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}
	TkiNoTrace (m_label, interp, object, 1, &tmp);

	TkiTrace (object->editor, object, 
	       "ined size", argc, argv, (char *) NULL);
    }

    ret = Tcl_VarEval (interp, type_to_string (object->type),
		       "__size ", object->id, (char *) NULL);

    if (ret == TCL_OK && strlen(Tcl_GetStringResult(interp)) > 0) {
	STRCOPY (object->size, Tcl_GetStringResult(interp));
    }

    Tcl_SetResult (interp, object->size, TCL_STATIC);
    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the node
 * object.
 */

static int 
m_node_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create NODE ]\n", (char *) NULL);

    dump_move       (interp, object);
    dump_icon       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_oid        (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the group
 * object. The user of the string must ensure that the variables 
 * for the member objects exist.
 */

static int
m_group_dump (Tcl_Interp *interp, Tki_Object *object)
{
    int i;
    double dx = 0, dy = 0;

    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create GROUP ", (char *) NULL);
    if (object->member) {
	for (i = 0; object->member[i]; i++) {
	    Tcl_AppendResult (interp, " $", object->member[i]->id,
			      (char *) NULL);
	    dx += object->member[i]->x;
	    dy += object->member[i]->y;
	}
	dx = dx / -i;
	dy = dy / -i;
    }
    Tcl_AppendResult (interp, " ]\n", (char *) NULL);

    /*
     * If the group has members, make sure to move the group back to 
     * the starting position. We simply move it very far to the top
     * left because Tkined will not move it off the canvas.
     */

    if (dx != 0 || dy != 0) {
	sprintf (buffer, "ined -noupdate move $%s -9999999 -9999999\n",
		 object->id);
	Tcl_AppendResult (interp, buffer, (char *)NULL);
    }
    dump_move (interp, object);

    dump_icon       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_name       (interp, object);
    dump_oid        (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    if (!object->collapsed) {
	Tcl_AppendResult (interp, "ined -noupdate expand $", 
			  object->id, "\n", (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the network object.
 */

static int
m_network_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create NETWORK ", 
		      object->points, " ]\n", (char *) NULL);

    dump_move       (interp, object);
    dump_icon       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_oid        (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the link object.
 */

static int
m_link_dump(Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create LINK $", object->src->id, 
		      " $", object->dst->id, " ", object->points, " ]\n", 
		      (char *) NULL);

    dump_color	    (interp, object);
    dump_attributes (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the text.
 */

static int
m_text_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create TEXT ", (char *) NULL);
    Tcl_AppendElement (interp, object->text);
    Tcl_AppendResult (interp, " ]\n", (char *) NULL);

    dump_move  (interp, object);
    dump_font  (interp, object);
    dump_color (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the image.
 */

static int
m_image_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create IMAGE ", (char *) NULL);
    Tcl_AppendElement (interp, object->name);
    Tcl_AppendResult (interp, " ]\n", (char *) NULL);

    dump_move  (interp, object);
    dump_color (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the interpreter.
 */

static int
m_interpreter_dump (Tcl_Interp *interp, Tki_Object *object)
{
    /* remove the absolute path name to make saved files more portable */

    char *p = strrchr (object->name, '/');
    
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create INTERPRETER ", 
		      (p) ? ++p : object->name, 
		      " ]\n", (char *) NULL);

    if (strlen(object->action) > 0) {
	int largc, i;
	const char **largv;

	Tcl_SplitList (interp, object->action, &largc, &largv);
	for (i = 0; i < largc; i++) {
	    Tcl_AppendResult (interp, "ined send $", object->id,
			      " ", largv[i], "\n", (char *) NULL);
	}

	ckfree ((char *) largv);
    }

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the log object.
 */

static int
m_log_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create LOG ]\n", (char *) NULL);

    dump_icon    (interp, object);
    dump_name    (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the reference.
 */

static int
m_ref_dump (Tcl_Interp *interp, Tki_Object *object)
{
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create REFERENCE ]\n", (char *) NULL);

    dump_move       (interp, object);
    dump_icon       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_oid        (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the stripchart.
 */

static int
m_strip_dump (Tcl_Interp *interp, Tki_Object *object)
{
#if 0
    char *values = NULL;
#endif

    /* get the current size and the values of the chart */

    m_size (interp, object, 0, (char **) NULL);
#if 0
    Tcl_VarEval (interp, type_to_string (object->type), "__values ",
		 object->id, (char *) NULL);
    values = ckstrdup (interp->result);
#endif

    Tcl_ResetResult (interp);
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create STRIPCHART ]\n", 
		      (char *) NULL);

    dump_move       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_scale      (interp, object);
    dump_size       (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

#if 0
    Tcl_AppendResult (interp, "ined -noupdate values $",
		      object->id, " ", values, "\n", (char *) NULL);

    ckfree (values);
#endif
    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the barchart.
 */

static int
m_bar_dump (Tcl_Interp *interp, Tki_Object *object)
{
#if 0
    char *values = NULL;
#endif

    /* get the current size and the values of the chart */

    m_size (interp, object, 0, (char **) NULL);
#if 0
    Tcl_VarEval (interp, type_to_string (object->type), "__values ",
		 object->id, (char *) NULL);
    values = ckstrdup (interp->result);
#endif

    Tcl_ResetResult (interp);
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create BARCHART ]\n", (char *) NULL);

    dump_move       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_scale      (interp, object);
    dump_size       (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

#if 0
    Tcl_AppendResult (interp, "ined -noupdate values $",
		      object->id, " ", values, "\n", (char *) NULL);

    ckfree (values);
#endif

    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the graph.
 */

static int
m_graph_dump (Tcl_Interp *interp, Tki_Object *object)
{
    int i;
    
    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create GRAPH ]\n", (char *) NULL);

    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_icon       (interp, object);
    dump_color      (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    if (object->numValues > 0) {
	Tcl_AppendResult (interp, "ined -noupdate values $",
			  object->id, " ", (char *) NULL);
	for (i = 0; i < object->numValues; i++) {
	    Tcl_PrintDouble (interp, object->valuePtr[i], buffer);
	    Tcl_AppendResult (interp, "{", buffer, " ", (char *) NULL);
	    Tcl_PrintDouble (interp, object->valuePtr[i], buffer);
	    Tcl_AppendResult (interp, buffer, "} ", (char *) NULL);
	}
	Tcl_AppendResult (interp, "\n", (char *) NULL);
    }
	
    return TCL_OK;
}

/*
 * Return a string of ined commands that rebuilds the DATA object.
 */

static int
m_data_dump (Tcl_Interp *interp, Tki_Object *object)
{
    char *values = NULL;

    /* get the current size and the values of the chart */

    m_size (interp, object, 0, (char **) NULL);
    Tcl_VarEval (interp, type_to_string (object->type), "__values ",
		 object->id, (char *) NULL);
    values = ckstrdup (Tcl_GetStringResult(interp));
    Tcl_ResetResult (interp);

    Tcl_AppendResult (interp, "set ", object->id, 
		      " [ ined -noupdate create DATA ]\n", (char *) NULL);

    dump_move       (interp, object);
    dump_font       (interp, object);
    dump_color      (interp, object);
    dump_scale      (interp, object);
    dump_size       (interp, object);
    dump_name       (interp, object);
    dump_address    (interp, object);
    dump_attributes (interp, object);
    dump_label      (interp, object);

    Tcl_AppendResult (interp, "ined -noupdate values $",
		      object->id, " ", values, "\n", (char *) NULL);

    ckfree (values);

    return TCL_OK;
}

void
Tki_DumpObject (Tcl_Interp *interp, Tki_Object *object)
{
    switch (object->type) {
 case TKINED_NODE:
	m_node_dump (interp, object); 
	break;
 case TKINED_GROUP:
	m_group_dump (interp, object); 
	break;
 case TKINED_NETWORK:
	m_network_dump (interp, object); 
	break;
 case TKINED_LINK:
	m_link_dump (interp, object); 
	break;
 case TKINED_TEXT:
	m_text_dump (interp, object); 
	break;
 case TKINED_IMAGE:
	m_image_dump (interp, object); 
	break;
 case TKINED_INTERPRETER:
	m_interpreter_dump (interp, object); 
	break;
 case TKINED_LOG:
	m_log_dump (interp, object);
        break;
 case TKINED_REFERENCE:
	m_ref_dump (interp, object); 
	break;
 case TKINED_STRIPCHART:
	m_strip_dump (interp, object); 
	break;
 case TKINED_BARCHART:
	m_bar_dump (interp, object); 
	break;
 case TKINED_GRAPH:
	m_graph_dump (interp, object); 
	break;
 case TKINED_DATA:
	m_data_dump (interp, object); 
	break;
    }
}

/*
 * Dump an arbitrary object. This function calls the type specific 
 * function and replaces every \n with a ; to seperate the tcl commands. 
 * This conversion is needed to transport the result string through the
 * protocol to applications.
 */

int m_dump (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    char *p;
    char *tp;

    Tki_DumpObject (interp, object);

    //for (p = interp->result; *p; p++) {
    tp = Tcl_GetStringResult(interp);
    for (p = tp; *p; p++) {
	if (*p == '\n') *p = ';';
    }
    Tcl_SetResult(interp, tp, TCL_VOLATILE);

    return TCL_OK;
}

/*
 * Get the starting point (object) of a link.
 */

int
m_src (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, object->src ? object->src->id : "", TCL_STATIC);
    return TCL_OK;
}

/*
 * Get the end point (object) of a link.
 */

int
m_dst (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, object->dst ? object->dst->id : "", TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the text of a text object. This calls
 * TEXT__text to update the view.
 */

int
m_text (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	int selected = object->selected;

	ckfree (object->text);
	object->text = ckstrdupnn (argv[0]);
	
	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}
	Tcl_VarEval (interp, type_to_string (object->type),
		     "__text ", object->id, (char *) NULL);
	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}

	TkiTrace (object->editor, object, 
	       "ined text", argc, argv, object->text);
    }

    Tcl_SetResult (interp, object->text, TCL_STATIC);
    return TCL_OK;
}

/*
 * Append some text to the LOG window. The tk callback LOG__append
 * will be called to let the GUI do its actions. A backslash followed
 * by a n character will be expanded to a space and a newline character.
 */

int
m_append (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int i;
    char *p;

    for (i = 0; i < argc; i++) {

	for (p = argv[i]; (p[0] != '\0') && (p[1] != '\0'); p++) {
	    if ((p[0] == '\\') && (p[1] == 'n')) {
		p[0] = ' ', p[1] = '\n';
	    }
	}

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__append ", object->id, " {", argv[i], "}",
		     (char *) NULL);

	TkiTrace (object->editor, object, 
	       "ined append", argc, argv, (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Create a hyperlink in a LOG window. The tk callback LOG__bind
 * will be called to let the GUI do its actions.
 */

int
m_hyperlink (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int i;
    char *p;

    for (i = 1; i < argc; i++) {

	for (p = argv[i]; (p[0] != '\0') && (p[1] != '\0'); p++) {
	    if ((p[0] == '\\') && (p[1] == 'n')) {
		p[0] = ' ', p[1] = '\n';
	    }
	}

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__bind ", object->id, " {", argv[0], "} ",
		     " {", argv[i], "}", (char *) NULL);

	argv[0][0] = '\0';
	TkiTrace (object->editor, object, 
	       "ined append", argc, argv, (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Get the interpreter of a log or menu object. Its name is stored in 
 * the links attribute.
 */

int
m_interpreter (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_SetResult (interp, object->links, TCL_STATIC);
    return TCL_OK;
}

/*
 * Clear the text inside of a LOG window. The tk callback LOG__clear
 * will be called to let the GUI do its actions. This is also understood
 * by stripcharts and barcharts.
 */

int
m_clear (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->type == TKINED_GRAPH) {
	if (object->valuePtr) {
	    ckfree ((char *) object->valuePtr);
	    object->valuePtr = NULL;
	}
	object->numValues = 0;
    }
    Tcl_VarEval (interp, type_to_string (object->type),
		 "__clear ", object->id, (char *) NULL);

    if (object->type == TKINED_LOG) {
	Tcl_VarEval (interp, type_to_string (object->type),
		     "__unbind ", object->id, (char *) NULL);
    }

    TkiTrace (object->editor, object, 
	   "ined clear", argc, argv, (char *) NULL);

    return TCL_OK;
}

/*
 * Get or set the scaling of a strip- or barchart.
 */

int
m_scale (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	if (Tcl_GetDouble (interp, argv[0], &object->scale) != TCL_OK)
		return TCL_ERROR;

	Tcl_VarEval (interp, type_to_string (object->type), "__scale ",
		     object->id, " ", argv[0], (char *) NULL);

	TkiTrace (object->editor, object, 
	       "ined scale", argc, argv, (char *) NULL);
    }

    Tcl_ResetResult (interp);
    //sprintf (interp->result, "%f", object->scale);
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj(object->scale));
    return TCL_OK;
}

/*
 * Get or set the values of a strip- or barchart.
 */

int
m_values (Tcl_Interp *interp, Tki_Object *object, int argc, const char **argv)
{
#define MAX_STATIC_POINTS 256
    if (object->type == TKINED_GRAPH) {

	int largc, i;
	const char **largv;
	double Xval, Yval;
	char buf[80];
	Tcl_DString dst;

	Tcl_DStringInit (&dst);

	for (i = 0; i < argc; i++) {

	    /* allocate an initial array for data points */
	    
	    if (object->valuePtr == NULL) {
		object->numValues = 0;
		object->valuePtr = (double *) ckalloc(MAX_STATIC_POINTS *
						      sizeof(double));
		object->allocValues = MAX_STATIC_POINTS;
	    }
	    
	    /* copy the values in the array and add the current time */
	    
	    if (Tcl_SplitList (interp, argv[i], &largc, &largv) != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    if (largc == 1) {
		time_t clock = time ((time_t *) NULL);
		Xval = clock;
		sprintf (buf, "%ld", clock);
		Tcl_GetDouble(interp, largv[0], &Yval);
		Tcl_DStringAppendElement (&dst, buf);
		Tcl_DStringAppendElement (&dst, argv[0]);
	    } else {
		Tcl_GetDouble(interp, largv[0], &Xval);
		Tcl_GetDouble(interp, largv[1], &Yval); 
		Tcl_DStringAppendElement (&dst, argv[0]);
		Tcl_DStringAppendElement (&dst, argv[1]);
	    }
	
	    if (object->numValues + 2 > object->allocValues) {
		object->allocValues += MAX_STATIC_POINTS;
		object->valuePtr = (double *) ckrealloc (
			  (char *) object->valuePtr,
			  (unsigned) (sizeof(double) * object->allocValues));
	    }
	    
	    object->valuePtr[object->numValues] = Xval;
	    object->valuePtr[object->numValues+1] = Yval;
	    object->numValues+=2;

	    ckfree ((char*) largv);
	}

#ifdef HAVE_BLT
	Blt_GraphElement (interp, object->canvas, object->id, 
			  object->numValues, object->valuePtr);
#endif

	Tcl_DStringFree (&dst);
	
    } else {
	
	char *args = Tcl_Merge (argc, argv);
	Tcl_VarEval (interp, type_to_string (object->type),
		     "__values ", object->id, " ", args, (char *) NULL);
	ckfree (args);
    }
	
    TkiTrace (object->editor, object, 
	   "ined values", argc, argv, (char *) NULL);

    return TCL_OK;
}

/*
 * Get or set the jump distance of a stripchart.
 */

int
m_jump (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	int num;

	if (Tcl_GetInt (interp, argv[0], &num) != TCL_OK) 
		return TCL_ERROR;

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__jump ", object->id, " ", argv[0], (char *) NULL);

	TkiTrace (object->editor, object, 
	       "ined jump", argc, argv, (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Get and set the member variable of a group object. This does not
 * work yet. This function should only affect objects that are removed
 * or added and it must take care that these objects reappear.
 */

int
m_member (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc > 0) {

	Tki_Object *member = NULL;
        int selected = object->selected;

	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}

	/* release all members if any */

	if (object->member) {
	    int i;
	    for (i = 0; object->member[i]; i++) {
		member = object->member[i];
		if (member->parent != NULL) {
		    /* should become member of parent group? */
		    if (*member->canvas == '\0') {
			TkiNoTrace (m_canvas, interp, member, 1, &object->canvas);
		    if (strcmp (member->color, "Black") != 0) {
			TkiNoTrace (m_color, interp, member, 1, &member->color);
		    }
			if (strcmp (member->icon, "machine") != 0) {
			    TkiNoTrace (m_icon, interp, member, 1, &member->icon);
			}
			if (strcmp (member->font, "default") != 0) {
			    TkiNoTrace (m_font,   interp, member, 1, &member->font);
			}
			TkiNoTrace (m_label,  interp, member, 1, &member->label);
		    }
		    member->parent = NULL;
		}
	    }
	    ckfree ((char *) object->member);
	    object->member = NULL;
	}

	/* create a new member list */

	{
	    int i, c;
	    size_t size = (argc + 1) * sizeof(Tki_Object *);
	    object->member = (Tki_Object **) ckalloc (size);
	    memset((char *) object->member, 0, size);
	    for (i = 0, c = 0; c < argc; c++) {
		member = Tki_LookupObject (argv[c]);
		if (member && !member->parent) {
		    object->member[i++] = member;
		    member->parent = object;
		}
	    }
	}

	if (object->collapsed) {
	    object->collapsed = 0;
	    TkiNoTrace (m_collapse,  interp, object, 0, (char **) NULL);
	} else {
	    if (object->member && object->member[0]) {
		parent_resize (interp, object->member[0]);
	    }
	}
	
	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}
	
	TkiTrace (object->editor, object, 
	       "ined member", argc, argv, (char *) NULL);
    }
    
    if (object->member) {
	int i;
	for (i = 0; object->member[i]; i++) {
	    Tcl_AppendElement (interp, object->member[i]->id);
	}
    } else {
	Tcl_SetResult (interp, "", TCL_STATIC);
    }
    return TCL_OK;
}

/*
 * Collapse a group to an icon.
 */

int
m_collapse (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (!object->collapsed) {

	int i;
	double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        int selected = object->selected;

        object->collapsed = 1;
      
        if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}

	if (object->member) {
	    for (i = 0; object->member[i]; i++) {
		Tki_Object *member = object->member[i];
		
		if (member->selected) {
		    m_unselect (interp, member, 0, (char **) NULL);
		}
		
		member->parent = object;
		if (member->type == TKINED_GROUP && !member->collapsed) {
		    TkiNoTrace(m_collapse, interp, member, 0, (char **) NULL);
		}
		
		/* calculate the initial position of the group icon */
		
		if (object->x == 0 && object->y == 0) {
		    int sargc;
		    const char **sargv;
		    m_size (interp, member, 0, (char **) NULL);
		    Tcl_SplitList (interp, member->size, &sargc, &sargv);
		    if (sargc == 4) {
			double mx1, my1, mx2, my2;
			Tcl_GetDouble (interp, sargv[0], &mx1);
			Tcl_GetDouble (interp, sargv[1], &my1);
			Tcl_GetDouble (interp, sargv[2], &mx2);
			Tcl_GetDouble (interp, sargv[3], &my2);
			if (x1 == 0 || mx1 < x1) x1 = mx1;
			if (y1 == 0 || my1 < y1) y1 = my1;
			if (mx2 > x2) x2 = mx2;
			if (my2 > y2) y2 = my2;
		    }
		    ckfree ((char *) sargv);
		}
		
		STRCOPY (member->canvas, "");
	    }
	}

	/* set the initial position of the icon */

	if (object->member && object->x == 0 && object->y == 0) {
	    object->x = x1+(x2-x1)/2;
	    object->y = y1+(y2-y1)/2;
	}

	/* 
	 * Update all links pointing to this group object.
	 */

	update_links (interp, object);

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__collapse ", object->id, (char *) NULL);

        TkiNoTrace (m_icon,  interp, object, 1, &object->icon);
        TkiNoTrace (m_color, interp, object, 1, &object->color);
        TkiNoTrace (m_font,  interp, object, 1, &object->font);
        TkiNoTrace (m_label, interp, object, 1, &object->label);

	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}
	
	TkiTrace (object->editor, object, 
	       "ined collapse", argc, argv, (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Expand a group to show its members.
 */

int
m_expand (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->collapsed) {

	int i;
        int selected = object->selected;
	object->collapsed = 0;
      
	if (selected) {
	    m_unselect (interp, object, 0, (char **) NULL);
	}
	
	if (object->member) {
	    for (i = 0; object->member[i]; i++) {
		Tki_Object *member = object->member[i];
		if (member->type == TKINED_GROUP && member->collapsed) {
		    member->collapsed = 0;
		}
		TkiNoTrace (m_canvas, interp, member, 1, &object->canvas);
		if (strcmp (member->color, "Black") != 0) {
		    TkiNoTrace (m_color, interp, member, 1, &member->color);
		}
		if (strcmp (member->icon, "machine") != 0) {
		    TkiNoTrace (m_icon, interp, member, 1, &member->icon);
		}
		if (strcmp (member->font, "default") != 0) {
		    TkiNoTrace (m_font,   interp, member, 1, &member->font);
		}
		TkiNoTrace (m_label,  interp, member, 1, &member->label);
	    }
	}

	Tcl_VarEval (interp, type_to_string (object->type),
		     "__expand ", object->id, (char *) NULL);

	TkiNoTrace (m_color, interp, object, 1, &object->color);
	TkiNoTrace (m_font,  interp, object, 1, &object->font);
	TkiNoTrace (m_label, interp, object, 1, &object->label);

	/* adjust the size of a parent group if necessary */
      
	parent_resize (interp, object);
      
	if (selected) {
	    m_select (interp, object, 0, (char **) NULL);
	}

	TkiTrace (object->editor, object, 
	       "ined expand", argc, argv, (char *) NULL);
    }

    return TCL_OK;
}

/*
 * Return a boolean indicating if the group is collapsed or not.
 */

int
m_collapsed (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_ResetResult (interp);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(object->collapsed));
    return TCL_OK;
}

/*
 * Ungroup. First expand the group and then delete the group object.
 */

int
m_ungroup (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int i;

    if (object->collapsed) {
	m_expand (interp, object, argc, argv);
    }

    if (object->member) {
	for (i = 0; object->member[i]; i++) {
	    object->member[i]->parent = NULL;
	}

	ckfree ((char *) object->member);
	object->member = NULL;
    }

    TkiTrace (object->editor, object, 
	   "ined ungroup", argc, argv, (char *) NULL);

    return m_delete (interp, object, argc, argv);
}

/*
 * Get and set the links that are connected to a node or network
 * object.
 */

int
m_links (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {
        STRCOPY (object->links, argv[0]);
    }

    Tcl_SetResult (interp, object->links, TCL_STATIC);
    return TCL_OK;
}

/*
 * Get and set the fixed points stored in the member variable of a 
 * link or network object.
 */

int
m_points (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

        STRCOPY (object->points, argv[0]);

	if (object->type == TKINED_NETWORK) {
	    char *tmp = "reset";

	    if (object->selected) {
		m_unselect (interp, object, 0, (char **) NULL);
		m_select (interp, object, 0, (char **) NULL);
	    }
	    
	    TkiNoTrace (m_label, interp, object, 1, &tmp);
	    
	    /* 
	     * Now update all visible links to this object and adjust
	     * the size of the parent group if necessary
	     */

	    update_links (interp, object);
	    parent_resize (interp, object);

	    TkiTrace (object->editor, object, 
		   "ined points", argc, argv, (char *) NULL);
	}
    }

    Tcl_SetResult (interp, object->points, TCL_STATIC);
    return TCL_OK;
}

/*
 * Computer the label coordinates of a network object.
 * This is done in C because computations are complicated
 * and slow in TCL.
 *
 * m_network_labelxy searches for the longest horizontal network segment 
 * that is at least longer than 100 points. If there is no such segment, 
 * we return the coordinates of the lowest endpoint of the vertical line
 * endpoint.
 */

int 
m_network_labelxy (Tcl_Interp *interp, Tki_Object *network, int argc, char **argv)
{
    int found = 0;
    int i, j, n;
    int largc;
    const char **largv;
    double *x;
    double *y;
    double lx, ly;
    double sx = 0, sy = 0, slen = 0;
    char tbuf[1024];

    Tcl_SplitList (interp, network->points, &largc, &largv);

    x = (double *) ckalloc (largc * sizeof(double));
    y = (double *) ckalloc (largc * sizeof(double));

    if (x == NULL || y == NULL) {
	ckfree ((char*) largv);
	Tcl_ResetResult (interp);
	snprintf (tbuf, sizeof(tbuf), "%f %f", network->x, network->y);
        Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
	return TCL_OK;
    }

    for (n = 0, i = 0; i < largc; i++) {
	if ((i%2) == 0) {
	    Tcl_GetDouble (interp, largv[i], &x[n]);
	    x[n] += network->x;
	} else {
	    Tcl_GetDouble (interp, largv[i], &y[n]);
	    y[n] += network->y;
	    n++;
	}
    }

    for (i = 1, j = 0; i < n; i++, j++) {
	lx = (x[i]>x[j]) ? x[i]-x[j] : x[j]-x[i];
	ly = (y[i]>y[j]) ? y[i]-y[j] : y[j]-y[i];
	if (!found) {
            if (y[i] > sy) {
		sx = (x[i]+x[j])/2;
		sy = y[i];
	    }
	    if (y[j] > sy) {
                sx = (x[i]+x[j])/2;
                sy = y[j];
            }
	}
	if (lx > slen) {
            sx = (x[i]+x[j])/2;
	    sy = (y[i]+y[j])/2;
	    slen = lx;
	    found = (slen > 100);
	}
    }
    sy += 3;

    ckfree ((char *) x);
    ckfree ((char *) y);
    ckfree ((char *) largv);

    Tcl_ResetResult (interp);

    snprintf (tbuf, sizeof(tbuf), "%f %f", sx, sy+1);
    Tcl_SetResult(interp, tbuf, TCL_VOLATILE);
    return TCL_OK;
}

/*
 * Send a message (usually a command) to an interpreter.
 * Append a list of the current selection to it.
 */

int
m_send (Tcl_Interp *interp, Tki_Object *object, int argc, const char **argv)
{
    int len, code;
    char *args;
    char *tbuf;

    if (argc > 0) {

        args = Tcl_Merge(argc, argv);
        len = strlen(args);
        tbuf = ckalloc(len+2);
        strncpy(tbuf, args, len);
        //fprintf(stderr, "file: %s +%d cmd: %s\n", __FILE__, __LINE__, args);
        ckfree(args);

	tbuf[len] = '\n';
	tbuf[++len] = '\0';
	//fprintf(stderr, "file: %s +%d cmd: %s\n", __FILE__, __LINE__, tbuf);
	code = Tcl_Write(object->channel, tbuf, len);

#if 0
    {
	Tcl_Channel c = Tcl_GetStdChannel(TCL_STDERR);
	if (c) {
	    char buf[80];
	    sprintf(buf, "** Tcl_Write() len = %d, code = %d\n", len, code);
	    Tcl_Write(c, buf, -1);
	}
    }
#endif

	if (code == len) {
	    code = Tcl_Flush(object->channel);
	}
	if (code < 0) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "write failed: ", 
			     Tcl_PosixError(interp), (char *) NULL);
	    ckfree(tbuf);
            return TCL_ERROR;
	}

    	ckfree(tbuf);
    }

    return TCL_OK;
}

/*
 * Ring the bell.
 */

int
m_bell (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_VarEval (interp, type_to_string (object->type),
		 "__bell ", object->id, (char *) NULL);

    TkiTrace (object->editor, object, "ined bell", argc, argv, "");

    Tcl_ResetResult (interp);
    return TCL_OK;
}

/*
 * This is called for network and node objects. Before they
 * get deleted, they must delete all links connected to them.
 */

static void
m_linked_delete (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int i, largc;
    const char **largv;
    Tki_Object *link;

    Tcl_SplitList (interp, object->links, &largc, &largv);
    for (i = 0; i < largc; i++) {
	link = Tki_LookupObject (largv[i]);
	if (link != NULL) {
	    TkiNoTrace (m_delete, interp, link, 0, (char **) NULL);
	    Tcl_ResetResult (interp);
	}
    }
    ckfree ((char*) largv);
}

/*
 * When deleting a link object, we have to update the links
 * stored by the objects connected by this link.
 */

static void
m_link_delete (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->src) {
	ldelete (interp, object->src->links, object->id);
    }

    if (object->dst) {
	ldelete (interp, object->dst->links, object->id);
    }
}

/*
 * When deleting a group object, make sure that everything in 
 * it gets deleted.
 */

static void
m_group_delete (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->member) {	
	while (object->member[0]) {
	    TkiNoTrace (m_delete, interp, object->member[0], 0, (char **) NULL);
	    Tcl_ResetResult (interp);
	}
	ckfree ((char *) object->member);
    }
}

/*
 * Make sure to delete all menus associated with this interpreter.
 */

static void
m_interpreter_delete (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (object->trace && object->editor) 
	object->editor->traceCount--;

    if (object->type == TKINED_INTERPRETER) {
	Tcl_HashEntry *ht_entry;
	Tcl_HashSearch ht_search;
	ht_entry = Tcl_FirstHashEntry(&tki_ObjectTable, &ht_search);
	while (ht_entry != NULL) {
	    Tki_Object *obj = (Tki_Object *) Tcl_GetHashValue (ht_entry);
	    if ((obj->type == TKINED_MENU) 
		&& (strcmp(obj->links, object->id) == 0)) {
		TkiNoTrace (m_delete, interp, obj, 0, (char **) NULL);
		Tcl_ResetResult (interp);
	    }
	    if ((obj->type == TKINED_LOG) 
		&& (strcmp(obj->links, object->id) == 0)) {
		Tcl_VarEval (interp, type_to_string (obj->type),
			     "__unbind ", obj->id, (char *) NULL);
		Tcl_ResetResult (interp);
	    }
	    ht_entry = Tcl_NextHashEntry (&ht_search);
	}
    }
}

/*
 * Delete an object. This is understood by all objects.
 */

int
m_delete (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    switch (object->type) {
 case TKINED_NODE: 
 case TKINED_NETWORK: 
	m_linked_delete (interp, object, argc, argv);
	break;
 case TKINED_GROUP: 
	m_group_delete (interp, object, argc, argv);
	break;
 case TKINED_LINK:
	m_link_delete (interp, object, argc, argv);
	break;
 case TKINED_INTERPRETER:
	m_interpreter_delete (interp, object, argc, argv);
	break;
 case TKINED_MENU:
	Tcl_ReapDetachedProcs();
	break;
    }

    if (object->selected) {
	m_unselect (interp, object, 0, (char **) NULL);
    }

    Tcl_VarEval (interp, type_to_string (object->type),
		 "__delete ", object->id, (char *) NULL);

    /* 
     * Remove the reference to this object if it is a member
     * of a group (that if it has a valid parent).
     */

    if (object->parent) {
       if (object->parent->member) { /* object-ip-trouble-netmask-close-panic */
#if 0
	int i, j;
	for (i = 0, j = 0; object->parent->member[i]; i++) {
	    if (object->parent->member[i] != object) {
		object->parent->member[j++] = object->parent->member[i];
	    }
	}
	while (j < i) {
	    object->parent->member[j++] = NULL;
	}
#else
	  RemoveObject (object->parent->member, object);
       }
#endif
    }

    parent_resize (interp, object);

    TkiTrace (object->editor, object, 
	   "ined delete", argc, argv, (char *) NULL);

    Tcl_DeleteCommand (interp, object->id);

    return TCL_OK;
}

/*
 * Experimental stuff.
 */

int
m_postscript (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    int rc;

    if (object->type == TKINED_GRAPH) {
	rc = Tcl_VarEval (interp, type_to_string (object->type),
			  "__postscript ", object->id, (char *) NULL);
    } else {
	rc = Tcl_VarEval (interp, "__postscript ", object->id, (char *) NULL);
    }

    Tcl_SetResult (interp, ckstrdupnn(Tcl_GetStringResult(interp)), TCL_DYNAMIC);
    return rc;
}

/*
 * Get or set a generic object attribute. They are used to
 * store default values and arbitrary status information.
 */

int 
m_attribute (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    Tcl_HashEntry *ht_entry;

    if (argc == 0) {

	Tcl_HashSearch ht_search;

	ht_entry = Tcl_FirstHashEntry(&(object->attr), &ht_search);
	while (ht_entry != NULL) {
	    Tcl_AppendElement (interp, 
			       Tcl_GetHashKey (&(object->attr), ht_entry));
	    ht_entry = Tcl_NextHashEntry (&ht_search);
	}

	return TCL_OK;
    }

    if (argc == 2) {
	int isnew;

	ht_entry = Tcl_CreateHashEntry (&(object->attr), argv[0], &isnew);
	if (! isnew) {
	    ckfree ((char *) Tcl_GetHashValue (ht_entry));
	}

	if (argv[1][0] == '\0') {
	    char *tmp = "name";
	    Tcl_DeleteHashEntry (ht_entry);
	    if (strcmp(object->label, argv[0]) == 0) {
		TkiNoTrace (m_label, interp, object, 1, &tmp);
	    }
	} else {
	    Tcl_SetHashValue (ht_entry, ckstrdup(argv[1]));
	    if (strcmp(object->label, argv[0]) == 0) {
		TkiNoTrace (m_label, interp, object, 1, &object->label);
	    }
	}

	TkiTrace (object->editor, object, 
	       "ined attribute", argc, argv, argv[0]);
    }

    ht_entry = Tcl_FindHashEntry (&(object->attr), argv[0]);
    if (ht_entry != NULL) {
	Tcl_SetResult(interp, (char *) Tcl_GetHashValue (ht_entry), TCL_VOLATILE);
    }

    return TCL_OK;
}

/*
 * Flashing icons to get the attention of the user. All parents of
 * the object are also flashed so that you can group and collapse
 * objects without loosing flash notices.
 */

int 
m_flash (Tcl_Interp *interp, Tki_Object *object, int argc, char **argv)
{
    if (argc == 1) {

	int secs;
	Tki_Object *anObject;

	if (Tcl_GetInt (interp, argv[0], &secs) != TCL_OK) {
	    return TCL_ERROR;
	}

	secs *= 2; /* we flash two times in one second */

	anObject = object;
	while (anObject) {

	    if (anObject->flash > 0) {
	        anObject->flash = (secs > anObject->flash) ? 
		                   secs : anObject->flash;
	    } else {
	        anObject->flash = secs;
		TkiFlash (interp, anObject);
	    }
	
	    if (*anObject->canvas == '\0') {
	        anObject = anObject->parent;
	    } else {
	        anObject = NULL;
	    }
	}
	
	TkiTrace (object->editor, object, 
	       "ined flash ", argc, argv, argv[0]);
    }

    return TCL_OK;
}


