/*
 * tnmMap.h --
 *
 *	Common definitions for the network map Tnm Tcl extension.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMap.h,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#ifndef _TNMMAP
#define _TNMMAP

#include <tcl.h>

/*
 *----------------------------------------------------------------
 * This structure is used to hold all information belonging to
 * a network map. Every map item keeps a pointer to its map 
 * structure.
 *----------------------------------------------------------------
 */

typedef struct TnmMap {
    Tcl_Obj *name;		 /* The name for the current map. */
    int width, height;		 /* The width and height in pixels. */
    Tcl_Obj *path;		 /* The path where statistics are kept. */
    Tcl_HashTable attributes;	 /* A hash table of map attributes. */
    Tcl_Command token;		 /* The command token used by Tcl. */
    Tcl_Interp *interp;		 /* The interpreter which owns this map. */
    int interval;		 /* The timer interval in milliseconds. */
    Tcl_TimerToken timer;	 /* The timer for updating this map. */
    Tcl_Time lastTick;		 /* The time stamp of the last tick. */
    int expire;			 /* Time in secs used to expire events. */
    int numItems;		 /* Number of existing items. */
    unsigned loading:1;		 /* Flag to indicate loading of a map. */
    Tcl_Obj *tagList;		 /* The tags associated with this map. */
    Tcl_Obj *storeList;		 /* The pattern list for NV storage. */
    struct TnmMapItem *itemList; /* The list of items managed by this map. */
    struct TnmMapBind *bindList; /* The event bindings for this map. */
    struct TnmMapEvent *eventList; /* The event history for this map. */
    struct TnmMapMsg *msgList;	 /* The message history for this map. */
    struct TnmMap *nextPtr;	 /* Next map in out list of maps. */
} TnmMap;

/*
 *----------------------------------------------------------------
 * The TnmMapItem structure defines a single item. Note, this 
 * strucure is included in item type specific structures which 
 * augment this structure with type specific information.
 *----------------------------------------------------------------
 */

typedef struct TnmMapItem {
    int x, y;			  /* The position of this item on the map. */
    Tcl_Obj *name;		  /* The (network) name of this item. */
    char *address;		  /* The (network) address of this item. */
    Tcl_Obj *path;		  /* The path where statistics are kept. */
    Tcl_Obj *color;		  /* The color of this item. */
    Tcl_Obj *font;		  /* The font used by this item. */
    Tcl_Obj *icon;		  /* The icon name used by this item. */
    struct TnmMapItem *parent;	  /* The parent item containing this item. */
    TnmVector memberItems;	  /* Vector of items contained in this item. */
    TnmVector linkedItems;	  /* Vector of items linked to this item. */
    struct TnmMapItem *srcPtr;	  /* The starting point of this link. */
    struct TnmMapItem *dstPtr;	  /* The end point of this link. */
    int expire;			  /* Time in secs used to expire events. */
    int health;			  /* The health history (0..100000). */
    short priority;		  /* The priority of this item (0..100). */
    unsigned dumped:1;		  /* Flag to keep track of dumped items. */
    Tcl_Command token;		  /* The command token used by Tcl. */
    Tcl_HashTable attributes;	  /* The table of item attributes. */
    Tcl_Time ctime;		  /* The creation time stamp. */
    Tcl_Time mtime;		  /* The last modification time stamp. */
    Tcl_Obj *tagList;		  /* The tags associated with this item. */
    Tcl_Obj *storeList;		  /* The pattern list for NV storage. */
    struct TnmMap *mapPtr;	  /* The map which manages this item. */
    struct TnmMapItemType *typePtr; /* The type for this item. */
    struct TnmMapBind *bindList;  /* The event bindings for this item. */
    struct TnmMapEvent *eventList; /* The event history for this item. */
    struct TnmMapMsg *msgList;	  /* The message history for this item. */
    struct TnmMapItem *nextPtr;	  /* The next item in the maps item list. */
} TnmMapItem;

/*
 *----------------------------------------------------------------
 * The TnmMapItemType structure defines all information needed
 * to implement a new item type.
 *----------------------------------------------------------------
 */

typedef int	
TnmMapItemCreateProc	_ANSI_ARGS_((Tcl_Interp *interp, TnmMap *mapPtr,
				     TnmMapItem *itemPtr));
typedef void
TnmMapItemDeleteProc	_ANSI_ARGS_((TnmMapItem *itemPtr));

typedef int
TnmMapItemDumpProc	_ANSI_ARGS_((Tcl_Interp *interp, TnmMapItem *itemPtr));

typedef int
TnmMapItemMoveProc	_ANSI_ARGS_((Tcl_Interp *interp, TnmMapItem *itemPtr, 
				     int x, int y));

typedef struct TnmMapItemType {
    char *name;			     /* The unique name of this item type. */
    size_t itemSize;		     /* The size of an item of this type. */
    unsigned nextId;		     /* The next id for a new map item. */
    int commonCmds;		     /* The common commands of this type. */
    TnmTable *configTable;	     /* The config options of this type. */
    struct TnmMapItemType *parentType;	/* Allowed parent type. */
    TnmMapItemCreateProc *createProc;	/* Procedure to create an item. */
    TnmMapItemDeleteProc *deleteProc;	/* Procedure to delete an item. */
    TnmMapItemDumpProc *dumpProc;	/* Procedure to dump an item. */
    TnmMapItemMoveProc *moveProc;	/* Procedure to move an item. */
    Tcl_ObjCmdProc *cmdProc;	     /* Procedure to process item commands. */
    struct TnmMapItemType *nextPtr;
} TnmMapItemType;

EXTERN void
TnmMapRegisterItemType		_ANSI_ARGS_((TnmMapItemType *itemType));

EXTERN TnmMapItemType tnmNodeType;
EXTERN TnmMapItemType tnmPortType;
EXTERN TnmMapItemType tnmNetworkType;
EXTERN TnmMapItemType tnmLinkType;
EXTERN TnmMapItemType tnmGroupType;

/*
 *----------------------------------------------------------------
 * Functions used to trace map internal changes. Used for debugging
 * purposes and to update clients that connect to this server.
 *----------------------------------------------------------------
 */

typedef struct TnmMapEvent {
    int type;			 /* The event type (see below). */
    TnmMap *mapPtr;		 /* The map that caused this event. */
    TnmMapItem *itemPtr;	 /* The item that caused this event. */
    char *eventName;		 /* The name of the current event. */
    Tcl_Time eventTime;		 /* The event time stamp. */
    char *eventData;		 /* Event type specific information. */
    Tcl_Interp *interp;          /* The interpreter which owns this event. */
    Tcl_Command token;		 /* The command token used by Tcl. */
    struct TnmMapEvent *nextPtr; /* Next even in event history queue. */
} TnmMapEvent;

EXTERN TnmMapEvent *tnmCurrentEvent;	/* The currenty active event. */

/*
 * Internal event types which are generated whenever the state of
 * a map changes. The TNM_MAP_USER_EVENT type is the event type
 * which is used to handle events visible from the Tcl event API.
 *
 * The TNM_MAP_EVENT_QUEUE bit is set if the event should be queued
 * in the event history queue. This bit is usually only set for
 * TNM_MAP_USER_EVENT types.
 */

#define TNM_MAP_CREATE_EVENT	0x0001
#define TNM_MAP_DELETE_EVENT	0x0002
#define TNM_MAP_CONFIGURE_EVENT	0x0003
#define TNM_MAP_NAME_EVENT	0x0004
#define TNM_MAP_ADDRESS_EVENT	0x0005
#define TNM_MAP_MOVE_EVENT	0x0006
#define TNM_MAP_COLLAPSE_EVENT	0x0007
#define TNM_MAP_EXPAND_EVENT	0x0008
#define TNM_MAP_ATTRIBUTE_EVENT	0x0009
#define TNM_MAP_LABEL_EVENT	0x000A
#define TNM_MAP_USER_EVENT	0x000B

#define TNM_MAP_EVENT_MASK	0xFFFF
#define TNM_MAP_EVENT_QUEUE	0x00010000

typedef void
TnmMapEventProc		_ANSI_ARGS_((TnmMap *mapPtr, TnmMapEvent *eventPtr,
				     char *eventData));
EXTERN void
TnmMapCreateEvent	_ANSI_ARGS_((int type, TnmMapItem *itemPtr, 
				     char *eventData));
EXTERN TnmMapEvent*
TnmMapCreateUserEvent	_ANSI_ARGS_((TnmMap *mapPtr, TnmMapItem *itemPtr,
				     char *name, char *args));
EXTERN void
TnmMapRaiseEvent	_ANSI_ARGS_((TnmMapEvent *eventPtr));

EXTERN void
TnmMapExpireEvents	_ANSI_ARGS_((TnmMapEvent **eventListPtr, 
				     long expireTime));

typedef struct TnmMapBind {
    int type;			/* The type of this binding. */
    TnmMap *mapPtr;		/* The map that owns this binding. */
    TnmMapItem *itemPtr;	/* The item that owns this binding. */
    TnmMapEventProc *proc;	/* The function implementing this binding. */
    char *pattern;		/* The glob pattern for this binding. */
    char *bindData;		/* The data describing binding details. */
    Tcl_Interp *interp;         /* The interpreter which owns this event. */
    Tcl_Command token;		/* The command token used by Tcl. */
    struct TnmMapBind *nextPtr;	/* The next binding in a list of bindings. */
} TnmMapBind;

EXTERN TnmMapBind*
TnmMapUserBinding	_ANSI_ARGS_((TnmMap *mapPtr, TnmMapItem *itemPtr,
				     char *pattern, char *script));
/*
 *----------------------------------------------------------------
 * Functions used to collect information about map or item 
 * changes as well as statistical data that is to be saved in 
 * stable storage.
 *----------------------------------------------------------------
 */

#define TNM_MSG_EXPIRED		0x01
#define TNM_MSG_SAVED		0x02
#define TNM_MSG_STORE		0x04

typedef struct TnmMapMsg {
    int flags;			/* ??? */
    unsigned interval;		/* Interval in which message was obtained. */
    int health;			/* Health change indicated by this message. */
    Tcl_Obj *tag;		/* The tag used to group messages. */
    Tcl_Obj *msg;		/* The reason for the value change. */
    Tcl_Time msgTime;		/* The message time stamp. */
    TnmMap *mapPtr;		/* The map that owns this message. */
    TnmMapItem *itemPtr;	/* The item that owns this message. */
    Tcl_Interp *interp;         /* The interpreter which owns this log. */
    Tcl_Command token;		/* The command token used by Tcl. */
    struct TnmMapMsg *nextPtr;	/* The next logging message. */
} TnmMapMsg;

EXTERN TnmMapMsg*
TnmMapCreateMsg		_ANSI_ARGS_((TnmMap *mapPtr, TnmMapItem *itemPtr,
				     Tcl_Obj *tag, Tcl_Obj *message));
EXTERN int
TnmMapMsgCmd		_ANSI_ARGS_((Tcl_Interp *interp, TnmMap *mapPtr,
				     TnmMapItem *itemPtr, 
				     int objc, Tcl_Obj *CONST objv[]));
EXTERN void
TnmMapExpireMsgs	_ANSI_ARGS_((TnmMapMsg **msgListPtr, 
				     long expireTime));

/*
 *----------------------------------------------------------------
 * Functions and definitions used by various item types.
 *----------------------------------------------------------------
 */

#define TNM_ITEM_CMD_MAP	0x0001
#define TNM_ITEM_CMD_MOVE	0x0002
#define TNM_ITEM_CMD_TYPE	0x0004
#define TNM_ITEM_CMD_ATTRIBUTE	0x0008
#define TNM_ITEM_CMD_DUMP	0x0010
#define TNM_ITEM_CMD_DESTROY	0x0020
#define TNM_ITEM_CMD_BIND	0x0040
#define TNM_ITEM_CMD_RAISE	0x0080
#define TNM_ITEM_CMD_HEALTH	0x0100
#define TNM_ITEM_CMD_INFO	0x0200
#define TNM_ITEM_CMD_MSG	0x0400
#define TNM_ITEM_CMD_CGET	0x0800
#define TNM_ITEM_CMD_CONFIG	0x1000

#define TNM_ITEM_OPT_COLOR	0x01
#define TNM_ITEM_OPT_FONT	0x02
#define TNM_ITEM_OPT_ICON	0x03
#define TNM_ITEM_OPT_PRIO	0x04
#define TNM_ITEM_OPT_SRC	0x05
#define TNM_ITEM_OPT_DST	0x06
#define TNM_ITEM_OPT_CTIME	0x07
#define TNM_ITEM_OPT_MTIME	0x08
#define TNM_ITEM_OPT_EXPIRE	0x09
#define TNM_ITEM_OPT_PATH	0x0A
#define TNM_ITEM_OPT_TAGS	0x0B
#define TNM_ITEM_OPT_STORE	0x0C
#define TNM_ITEM_OPT_PARENT	0x0D
#define TNM_ITEM_OPT_NAME	0x0E
#define TNM_ITEM_OPT_ADDRESS	0x0F

EXTERN int
TnmMapItemObjCmd	_ANSI_ARGS_((TnmMapItem *itemPtr, Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]));
EXTERN void
TnmMapItemCmdList	_ANSI_ARGS_((TnmMapItem *itemPtr, Tcl_Interp *interp));

EXTERN void
TnmMapItemDump		_ANSI_ARGS_((TnmMapItem *itemPtr, Tcl_Interp *interp));

EXTERN int
TnmMapItemConfigure	_ANSI_ARGS_((TnmMapItem *itemPtr, Tcl_Interp *interp,
				     int objc, Tcl_Obj *CONST objv[]));
EXTERN TnmMapItem*
TnmMapFindItem		_ANSI_ARGS_((Tcl_Interp *interp, TnmMap *mapPtr,
				     char *name));

#endif /* _TNMMAP */
