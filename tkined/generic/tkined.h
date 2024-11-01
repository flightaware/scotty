/*
 * tkined.h --
 *
 *	This file contains common declarations for tkined.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TKINED
#define _TKINED
#define tkiDebug

/*
 *----------------------------------------------------------------
 * The following definitions set up the proper options for Windows
 * compilers.  We use this method because there is no autoconf
 * equivalent. This is mostly copied from the tcl.h header file.
 *----------------------------------------------------------------
 */

#if defined(_WIN32) && !defined(__WIN32__)
#   define __WIN32__
#endif

#ifdef __WIN32__
#   ifndef STRICT
#	define STRICT
#   endif
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef HAS_STDARG
#	define HAS_STDARG 1
#   endif
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef USE_TCLALLOC
#	define USE_TCLALLOC 1
#   endif
#endif /* __WIN32__ */

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.
 */

#ifndef __WIN32__
#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif
#else /* __WIN32__ */
/*
 * The following code is copied from winnt.h
 */
#ifndef VOID
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif
#endif /* __WIN32__ */

/*
 *----------------------------------------------------------------
 * Here start the common definitions for the tkined extension.
 *----------------------------------------------------------------
 */

#define TKI_VERSION "1.6.0"

#include <tcl.h>
#include <tk.h>

/*
 * The support follows the convention that a macro called BUILD_xxxx, where
 * xxxx is the name of a library we are building, is set on the compile line
 * for sources that are to be placed in the library.
 */

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_tnm
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 *----------------------------------------------------------------
 * The following functions are not officially exported by Tcl. 
 * They are used anyway because they make this code simpler and 
 * platform independent.
 *----------------------------------------------------------------
 */

#define TkiGetTime TclpGetTime

EXTERN void
TclpGetTime		(Tcl_Time *timePtr);

/*
 *----------------------------------------------------------------
 * The external declaration of the canvas item extensions 
 * stripchart and barchart. 
 *
 * Actually the internal registration functions (so that we can
 * perform special initializations (we query the Tk stub table))
 *----------------------------------------------------------------
 */

EXTERN void Tki_StripchartInit (void);
EXTERN void Tki_BarchartInit   (void);

/*
 * This structure is used to hold all information belonging to an
 * editor window. Every object shown on the editor canvas keeps a 
 * pointer to an editor object.
 */

typedef struct Tki_Editor {
    char *id;               /* The unique identifier for an editor object.  */
    char *toplevel;         /* The toplevel window of the editor object.    */

    char *dirname;          /* The current directory name.                  */
    char *filename;         /* The filename for the current map.            */ 
    char *pagesize;         /* The size of the current map.                 */
    int  width, height;     /* The width and height in canvas pixels.       */
    int  pagewidth;         /* The page width in millimeters.               */
    int  pageheight;        /* The page height in millimeters.              */
    int  landscape;         /* The orientation (landscape or portrait).     */
    int  color;             /* The indicating the colormodel of the editor. */

    int  traceCount;        /* The number of traces for this editor.        */

    Tcl_HashTable attr;     /* A hash table of editor attributes.           */
} Tki_Editor;

/*
 * These are the constructor and destructor functions for 
 * editor objects.
 */

int 
Tki_CreateEditor	(ClientData clientData, Tcl_Interp *interp,
				     int argc, const char **argv);
void
Tki_DeleteEditor	(ClientData clientData);

int
Tki_EditorPageSize	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, const char **argv);
int
Tki_EditorOrientation	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, const char **argv);
int
Tki_EditorPostScript	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);
int
Tki_EditorTnmMap	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);
int
Tki_EditorGraph		(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);
int
Tki_EditorRetrieve	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);
int
Tki_EditorSelection	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);
int
Tki_EditorAttribute	(Tki_Editor *editor, Tcl_Interp *interp,
				     int argc, char **argv);


/*
 * The following structure is used to represent objects. I decided
 * to uses a flat structure since most tkined objects are very similar.
 */

typedef struct Tki_Object {
    unsigned type;      /* Type of the object. See below for object types    */
    char *id;           /* The unique identifier for this object             */
    char *name;         /* The name of the object (if any)                   */
    char *address;      /* The address of the object (if any)                */
    unsigned oid;       /* The external object identifier (if any)           */
    double x, y;        /* The position of the object (if any)               */
    char *icon;         /* The icon of the object (if any)                   */
    char *font;         /* The font of the object (if any)                   */
    char *color;        /* The color of the object (if any)                  */
    char *label;        /* The label of the object (if any)                  */
    char *text;         /* The text of a label or a text object              */
    const char *canvas; /* The widget name of the canvas we belong to        */
    char *items;        /* Items on the canvas belonging to this object.     */

    struct Tki_Object *parent;	/* The parent group object of an object.     */
    struct Tki_Object **member; /* A vector of all group member.             */

    struct Tki_Object *src;	/* The source object of a link object.	     */
    struct Tki_Object *dst;	/* The destination object of a link object.  */
    char *links;                /* The list of links of the object (if any). */

    char *points;		/* A list of points (network,link).          */

    char *traceCmd;		/* The command called during a trace.	     */

    char *size;         /* The size occupied by an object                    */
    char *action;       /* The action bound to this object                   */

    int queue;          /* The queue as reported by the interpreter          */
    Tcl_Channel channel; /* The IO channel to an interpreter object.         */
    Tcl_DString *cmd;   /* The command buffer used by an interpreter object. */
    Tcl_Interp *interp; /* The Tcl interpreter used by an interpreter object.*/
    unsigned done:1;    /* Flag indicating if the Buffer is empty            */
    unsigned trace:1;   /* Flag indicating if the interpreter needs a trace  */
    unsigned selected:1;/* Not zero if object currently selected             */
    unsigned collapsed:1;/* Not zero if group object currently collapsed     */
    unsigned loaded:1;  /* Not zero if object was read from a file           */
    unsigned incomplete:1;/* Not zero if object is incomplete                */
    unsigned timeout:1; /* Not zero if object caused a timeout               */
    double scale;       /* The scaling factor for a strip- or barchart       */
    int flash;          /* The number of seconds the objects flashes         */
    int allocValues;    /* Number of allocated doubles to hold values        */
    int numValues;	/* The number of values stored in valuePtr	     */
    double *valuePtr;	/* Dynamically allocated memory for XY values        */

    Tki_Editor *editor;		/* The pointer to our editor object.	     */
    Tcl_HashTable attr;		/* A hash table used to store attributes.    */
} Tki_Object;

/*
 * Legal values for the type field of a Tki_Object. See tkined.1
 * for more details on tkined objects.
 */

#define TKINED_NONE         0x0000
#define TKINED_NODE         0x0001
#define TKINED_GROUP        0x0002
#define TKINED_NETWORK      0x0004
#define TKINED_LINK         0x0008
#define TKINED_TEXT         0x0010
#define TKINED_IMAGE        0x0020
#define TKINED_INTERPRETER  0x0040
#define TKINED_MENU         0x0080
#define TKINED_LOG          0x0100
#define TKINED_REFERENCE    0x0200
#define TKINED_STRIPCHART   0x0400
#define TKINED_BARCHART     0x0800
#define TKINED_GRAPH	    0x1000
#define TKINED_HTML         0x2000
#define TKINED_DATA         0x4000
#define TKINED_EVENT        0x8000
#define TKINED_ALL          0xffff

/*
 * These are the constructor and destructor functions for 
 * tkined objects. Tki_DumpObject() returns a string that can
 * be used to reconstruct an object.
 */

int
Tki_CreateObject	(ClientData clientData, Tcl_Interp *interp,
				     int argc, const char **argv);
void
Tki_DeleteObject	(ClientData clientData);

void
Tki_DumpObject		(Tcl_Interp *interp, Tki_Object *object);

/*
 * Declaration of all functions that implement methods for tkined
 * objects. See the dispatch table in objects.c for more information
 * about which method can be executed for a given object type.
 */

int m_create           (Tcl_Interp *interp, Tki_Object *object,
                                    int argc, const char **argv);
int m_dump             (Tcl_Interp *interp, Tki_Object *object,
                                    int argc, char **argv);
int m_retrieve         (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_delete           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);

int m_id               (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_type             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_parent           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_name             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_canvas           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_editor           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_items            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_address          (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_oid              (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_action           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_select           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_unselect         (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_selected         (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_icon             (Tcl_Interp *interp, Tki_Object *object,
		 		    int argc, const char **argv);
int m_label            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_font             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_color            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_move             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_raise            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_lower            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_size             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_src              (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_dst              (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_text             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_append           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_hyperlink        (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
#if 1
int m_html_append      (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
#endif
int m_clear            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_scale            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_values           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_jump             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_member           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_collapse         (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_expand           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_collapsed        (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_ungroup          (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_links            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_postscript       (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_points           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_network_labelxy  (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_interpreter      (Tcl_Interp *interp, Tki_Object *object,
				    int argc, char **argv);
int m_send             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_bell             (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_editor           (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_attribute        (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);
int m_flash            (Tcl_Interp *interp, Tki_Object *object,
				    int argc, const char **argv);

/*
 * More utility functions.
 */

extern int TkiInit	    (Tcl_Interp *interp);

extern void TkiInitPath	    (Tcl_Interp *interp);

extern void receive         (ClientData clientData, int mask);
extern int  ined            (ClientData clientData,
					 Tcl_Interp *interp,
					 int argc, const char **argv);

extern void TkiFlash        (Tcl_Interp *interp,
					 Tki_Object *object);

extern char *findfile       (Tcl_Interp *interp, const char *name);
extern char *type_to_string (unsigned type);
extern unsigned string_to_type   (const char *str);

extern void TkiMarkRectangle (Display *display,
				  Drawable drawable, GC gc,
				  int x1, int y1, int x2, int y2 );

/*
 * Delete or append an item from/to a list stored as a tcl string list.
 */

extern void ldelete (Tcl_Interp *interp, char *slist, char *item);
extern void lappend (char **slist, char *item);

/*
 * The trace mechanism is implemented by a function that is called
 * whenever a trace relevant event happens. The arguments point to
 * the editor needed to handle the trace and to the object that caused
 * the trace. The command gets appended with the strings stored in argv.
 *
 * The function TkiNoTrace() is just a shorthand to invoke methods that
 * dont get traced. For example, moving a group will call the move
 * method for all group members, but this is internal and should not
 * be traced. Therefore we use the TkiNoTrace() function to invoke further
 * moves.
 */

extern void TkiTrace (Tki_Editor *editor, Tki_Object *object,
				  char *cmd, int argc, const char **argv, 
				  char *result);

extern int TkiNoTrace (int (*method)(), Tcl_Interp *interp, 
				   Tki_Object *object, int argc, char **argv);

/*
 * The clipboard contains a global buffer of ined commands that
 * is used to implement cut&paste and load&save commands.
 */

extern Tcl_DString clip;

/*
 * Our hashtable that maps object ids to the object structure.
 */

EXTERN Tcl_HashTable tki_ObjectTable;

EXTERN Tki_Object* 
Tki_LookupObject	(const char *id);

/*
 * This variable shows if we run in debug mode. It corresponds to
 * the tcl variable tkined_debug which contains a boolean to
 * indicate debug mode.
 */

EXTERN int tki_Debug;

/*
 * Copy a string from b to a. This macro makes sure that the copy 
 * operation is only done when a and b are not the same string.
 */

#define STRCOPY(A,B) if (A != B) { ckfree (A); A = ckstrdup(B); }

/*
 * Here comes some portability stuff. There is no strdup in the
 * ultrix C library. So we fake one.
 */

#define ckstrdup(s)	strcpy (ckalloc (strlen (s) + 1), s)

/*
 * A ckstrdupnn that replaces all newlines with "\n" sequences.
 */

extern char *ckstrdupnn (const char*);

/*
 * A general purpose sprintf buffer of 1024 bytes. Please use it
 * when you are absolutely sure 1024 bytes are enough. The function 
 * buffer_size() may be called to increase the buffer in case one 
 * needs more space. It will never shrink below 1024 bytes.
 */

EXTERN char *buffer;
EXTERN void buffersize (size_t size);

#endif /* _TKINED */
