/*
 * tnmMap.c --
 *
 *	This file contains all functions that manipulate the state
 *	of a network map. A network map object keeps track of all
 *	objects (and processes) associated with a network map.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMap.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

static TnmMapItemType *itemTypes = NULL;
static int sortMode;

#define TNM_SORT_NONE	0x00
#define TNM_SORT_MTIME	0x01
#define TNM_SORT_CTIME	0x02
#define TNM_SORT_HEALTH	0x04
#define TNM_SORT_DECR	0x80000000

/*
 * Every Tcl interpreter has an associated MapControl record. It
 * keeps track of the default settings for this interpreter.
 */

static char tnmMapControl[] = "tnmMapControl";

typedef struct MapControl {
    TnmMap *mapList;		/* The list of maps for this interpreter. */
} MapControl;

/*
 * The dynamic string defined below is used to implement the
 * shared clipboard buffer.
 */

static Tcl_DString *clip = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static void
TickProc	(ClientData clientData);

static TnmMapItemType*
GetItemType	(Tcl_Interp *interp, char *name);

static int
CreateItem	(Tcl_Interp *interp, TnmMap *mapPtr,
			     int objc, Tcl_Obj *CONST objv[]);
static void
ItemDeleteProc	(ClientData clientData);

static int
SortProc	(CONST VOID *first, CONST VOID *second);

static int
FindItems	(Tcl_Interp *interp, TnmMap *mapPtr,
			     int objc, Tcl_Obj *CONST objv[]);
static int
CreateMap	(Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]);
static void
MapDeleteProc	(ClientData clientData);

static void
MapDestroyProc	(char *memPtr);

static Tcl_Obj*
GetOption	(Tcl_Interp *interp, ClientData object, 
			     int option);
static int
SetOption	(Tcl_Interp *interp, ClientData object,
			     int option, Tcl_Obj *objPtr);
static void
DumpMapProc	(Tcl_Interp *interp, TnmMap *mapPtr, 
			     TnmMapItem *itemPtr, Tcl_DString *dsPtr);
static int
DumpMap		(Tcl_Interp *interp, TnmMap *mapPtr);

static void
ClearMap	(Tcl_Interp *interp, TnmMap *mapPtr);

static int
LoadMap		(Tcl_Interp *interp, TnmMap *mapPtr,
			     char *channelName);
static int
SaveMap		(Tcl_Interp *interp, TnmMap *mapPtr,
			     char *channelName);
static int
CopyMap		(Tcl_Interp *interp, TnmMap *mapPtr,
			     int objc, Tcl_Obj *CONST objv[]);
static int
PasteMap	(Tcl_Interp *interp, TnmMap *mapPtr,
			     Tcl_DString *script);
static int
MapObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int
FindMaps	(Tcl_Interp *interp, MapControl *control,
			     int objc, Tcl_Obj *CONST objv[]);

/*
 * The options used to configure map objects.
 */

enum options {
    optExpire, optHeight, optName, optPath, optStore, 
    optTags, optTick, optWidth
};

static TnmTable optionTable[] = {
    { optExpire,	"-expire" },
    { optHeight,	"-height" },
    { optName,		"-name" },
    { optPath,		"-path" },
    { optStore,		"-store" },
    { optTags,		"-tags" },
    { optTick,		"-tick" },
    { optWidth,		"-width" },
    { 0, NULL }
};

static TnmConfig configTable = {
    optionTable,
    SetOption,
    GetOption
};

/*
 *----------------------------------------------------------------------
 *
 * AssocDeleteProc --
 *
 *	This procedure is called when a Tcl interpreter gets 
 *	destroyed so that we can clean up the data associated with
 *	this interpreter.
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
    MapControl *control = (MapControl *) clientData;

    /*
     * Note, we do not care about map objects since Tcl first
     * removes all commands before calling this delete procedure.
     */

    if (control) {
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TickProc --
 *
 *	This procedure is invoked by the timer associated with a
 *	map. It is used to recompute the health of all map items
 *	and to expire entries from the event history.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The internal state of some map items gets updated.
 *
 *----------------------------------------------------------------------
 */

static void
TickProc(ClientData clientData)
{
    TnmMap *mapPtr = (TnmMap *) clientData;
    TnmMapItem *itemPtr;
    TnmMapMsg *msgPtr;
    Tcl_Time currentTime;
    int timeout = mapPtr->interval / 1000;

    Tcl_GetTime(&currentTime);

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {

	int min = 0, max = 0, cnt = 0;
	int score = 100 * 1000;

	/*
	 * Calculate the mininum and the maximum health value in the
	 * last tick interval.
	 */

	for (msgPtr = itemPtr->msgList; msgPtr; msgPtr = msgPtr->nextPtr) {

	    if (currentTime.sec - msgPtr->msgTime.sec > timeout) break;

	    if (msgPtr->health > max) {
		max = msgPtr->health;
	    }
	    if (msgPtr->health < min) {
		min = msgPtr->health;
	    }
	    cnt++;
	}

	if (min >= 0) {
	    score += max;
	} else if (max <= 0) {
	    score += min;
	} else {
	    score += (max + min) / 2;
	}
	
	if (score > 100 * 1000) {
	    score = 100 * 1000;
	}
	if (score < 0) {
	    score = 0;
	}
	    
	itemPtr->health = (int) (0.6 * score + 0.4 * itemPtr->health);
	    
	if (itemPtr && itemPtr->expire) {
	    int expireTime = currentTime.sec - itemPtr->expire;
	    TnmMapExpireEvents(&itemPtr->eventList, expireTime);
	    TnmMapExpireMsgs(&itemPtr->msgList, expireTime);
	}
    }
	
    if (mapPtr->expire) {
	int expireTime = currentTime.sec - mapPtr->expire;
	TnmMapExpireEvents(&mapPtr->eventList, expireTime);
	TnmMapExpireMsgs(&mapPtr->msgList, expireTime);
    }

    mapPtr->timer = Tcl_CreateTimerHandler(mapPtr->interval, 
					   TickProc, (ClientData) mapPtr);
    mapPtr->lastTick = currentTime;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapItemType --
 *
 *	This procedure registers a new item type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmMapRegisterItemType(TnmMapItemType *itemType)
{
    itemType->nextPtr = itemTypes;
    itemTypes = itemType;
}

/*
 *----------------------------------------------------------------------
 *
 * GetItemType --
 *
 *	This procedure converts a type name into an item type pointer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmMapItemType*
GetItemType(Tcl_Interp *interp, char *name)
{
    TnmMapItemType *typePtr;

    for (typePtr = itemTypes; typePtr; typePtr = typePtr->nextPtr) {
	if (strcmp(typePtr->name, name) == 0) break;
    }
    if (! typePtr) {
	Tcl_AppendResult(interp, "unknown item type \"", name, "\"",
			 (char *) NULL);
    }
    return typePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateItem --
 *
 *	This procedure is invoked to process the "create" command
 *	option of the map object command.
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
CreateItem(Tcl_Interp *interp, TnmMap *mapPtr, int objc, Tcl_Obj *CONST objv[])
{
    TnmMapItemType *typePtr;
    TnmMapItem *itemPtr;
    char *name = NULL;
    int result;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, 
			 "type ?option value? ?option value? ...");
	return TCL_ERROR;
    }

    typePtr = GetItemType(interp, Tcl_GetStringFromObj(objv[2], NULL));
    if (! typePtr) {
	return TCL_ERROR;
    }

    itemPtr = (TnmMapItem *) ckalloc(typePtr->itemSize);
    memset((char *) itemPtr, 0, typePtr->itemSize);
    itemPtr->name = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(itemPtr->name);
    itemPtr->path = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(itemPtr->path);
    TnmVectorInit(&(itemPtr->linkedItems));
    TnmVectorInit(&(itemPtr->memberItems));
    itemPtr->mapPtr = mapPtr;
    itemPtr->typePtr = typePtr;
    itemPtr->expire = 3600;
    itemPtr->health = 100 * 1000;
    itemPtr->tagList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(itemPtr->tagList);
    itemPtr->storeList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(itemPtr->storeList);
    Tcl_InitHashTable(&itemPtr->attributes, TCL_STRING_KEYS);

    result = TnmMapItemConfigure(itemPtr, interp, objc - 1, objv + 1);
    if (result != TCL_OK) {
	ckfree((char *) itemPtr);
	return TCL_ERROR;
    }

    if (typePtr->createProc) {
	result = (typePtr->createProc) (interp, mapPtr, itemPtr);
	if (result != TCL_OK) {
	    ckfree((char *) itemPtr);
	    return TCL_ERROR;
	}
    }

    if (itemPtr->ctime.sec == 0 && itemPtr->ctime.usec == 0) {
	Tcl_GetTime(&itemPtr->ctime);
	itemPtr->mtime = itemPtr->ctime;
    }

    name = TnmGetHandle(interp, typePtr->name, &typePtr->nextId);
    if (typePtr->cmdProc) {
	itemPtr->token = Tcl_CreateObjCommand(interp, name, typePtr->cmdProc,
				   (ClientData) itemPtr, ItemDeleteProc);
    }

    Tcl_SetResult(interp, name, TCL_STATIC);
    itemPtr->nextPtr = mapPtr->itemList;
    mapPtr->itemList = itemPtr;
    mapPtr->numItems++;
    TnmMapCreateEvent(TNM_MAP_CREATE_EVENT, itemPtr, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ItemDeleteProc --
 *
 *	This procedure is called to delete an item. It cleans up
 *	all the resources associated with the item.
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
ItemDeleteProc(ClientData clientData)
{
    TnmMapItem **itemPtrPtr;
    TnmMapItem *itemPtr = (TnmMapItem *) clientData;
    TnmMap *mapPtr = itemPtr->mapPtr;
    int i;

    TnmMapCreateEvent(TNM_MAP_DELETE_EVENT, itemPtr, NULL);

    /*
     * First, update the list of all known items.
     */

    itemPtrPtr = &mapPtr->itemList;
    while (*itemPtrPtr && (*itemPtrPtr) != itemPtr) {
	itemPtrPtr = &(*itemPtrPtr)->nextPtr;
    }
    if (*itemPtrPtr) {
	(*itemPtrPtr) = itemPtr->nextPtr;
    }
    mapPtr->numItems--;

    /*
     * Call the item type specifc delete proc if available.
     */

    if (itemPtr->typePtr->deleteProc) {
	(itemPtr->typePtr->deleteProc) (itemPtr);
    }

    /*
     * Update any linked items pointing back to this item.
     */

    if (itemPtr->srcPtr) {
	TnmVectorDelete(&(itemPtr->srcPtr->linkedItems), (ClientData) itemPtr);
    }
    if (itemPtr->dstPtr) {
	TnmVectorDelete(&(itemPtr->dstPtr->linkedItems), (ClientData) itemPtr);
    }

    /*
     * Delete all tags associated with this particular item.
     */

    if (itemPtr->tagList) {
	Tcl_DecrRefCount(itemPtr->tagList);
    }

    /*
     * Update any objects containing this object.
     */
    
    if (itemPtr->parent) {
        TnmVectorDelete(&itemPtr->parent->memberItems, (ClientData) itemPtr);
    }

    /*
     * Delete all links that include this item as a source or a
     * destination. Make sure that we first eliminate any duplicates
     * so that we do not fall down if someone links an item with
     * itself. Finally, free the vector itself.
     */

repeat:
    for (i = 0; i < TnmVectorSize(&itemPtr->linkedItems); i++) {
	TnmMapItem *iPtr;
	ClientData *elementPtr = TnmVectorElements(&itemPtr->linkedItems);
	iPtr = (TnmMapItem *) elementPtr[0];
	if (iPtr && iPtr->mapPtr && iPtr->mapPtr->interp) {
	    Tcl_DeleteCommandFromToken(iPtr->mapPtr->interp, iPtr->token);
	    goto repeat;
	}
    }

    /*
     * Handle references to members. We simply set the parent pointer
     * to NULL and we are done. Note, special semantics must be
     * implemented in the item type specific code. See the node item
     * as an example.
     */

    {
	ClientData *elementPtr = TnmVectorElements(&itemPtr->memberItems);
	for (i = 0; elementPtr[i]; i++) {
	    ((TnmMapItem *) elementPtr[i])->parent = NULL;
	}
    }

    TnmVectorFree(&itemPtr->linkedItems);
    TnmVectorFree(&itemPtr->memberItems);

    /*
     * Clear the attribute hash table and finally free the 
     * item structure itself.
     */

    TnmAttrClear(&itemPtr->attributes);
    Tcl_DeleteHashTable(&itemPtr->attributes);

    ckfree((char *) itemPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SortProc --
 *
 *	This procedure is used to compare two items. Its behaviour is
 *	controlled by the sortMode variable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SortProc(CONST VOID *first, CONST VOID *second)
{
    TnmMapItem *firstItem = *((TnmMapItem **) first);
    TnmMapItem *secondItem = *((TnmMapItem **) second);
    int order = 0;

    switch (sortMode & 0xFF) {
    case TNM_SORT_MTIME:
	if (firstItem->mtime.sec < secondItem->mtime.sec) {
	    order = -1;
	} else if (firstItem->mtime.sec > secondItem->mtime.sec) {
	    order = 1;
	}
	break;
    case TNM_SORT_CTIME:
	if (firstItem->ctime.sec < secondItem->ctime.sec) {
	    order = -1;
	} else if (firstItem->ctime.sec > secondItem->ctime.sec) {
	    order = 1;
	}
	break;
    }

    if (sortMode & TNM_SORT_DECR) {
	order = -order;
    }

    /*
     * Sort by name if both items are otherwise equal.
     */

    if (order == 0 && firstItem->name && secondItem->name) {
	order = strcmp(Tcl_GetStringFromObj(firstItem->name, NULL),
		       Tcl_GetStringFromObj(secondItem->name, NULL));
    }
    
    return order;
}

/*
 *----------------------------------------------------------------------
 *
 * FindItems --
 *
 *	This procedure is invoked to process the "find" command
 *	option of the map object command.
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
FindItems(Tcl_Interp *interp, TnmMap *mapPtr, int objc, Tcl_Obj *CONST objv[])
{
    TnmMapItem *itemPtr, **itemVector;
    TnmMapItemType *typePtr = NULL;
    char *address = NULL, *name = NULL, *order;
    int i, result;
    size_t size, itemCnt = 0;
    Tcl_Obj *listPtr, *patList = NULL;

    enum options { 
	optAddress, optName, optOrder, optSort, optTags, optType
    } option;

    static CONST char *optionTable[] = {
	"-address", "-name", "-order", "-sort", "-tags", "-type",
	(char *) NULL
    };

    static TnmTable sortModeTable[] = {
	{ TNM_SORT_CTIME,	"ctime" },
	{ TNM_SORT_MTIME,	"mtime" },
	{ TNM_SORT_HEALTH,	"health" },
	{ 0, NULL }
    };

    if (objc % 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value? ?option value? ...");
	return TCL_ERROR;
    }

    sortMode = TNM_SORT_NONE;
    for (i = 2; i < objc; i++) {
	result = Tcl_GetIndexFromObj(interp, objv[i++], optionTable, 
				     "option", TCL_EXACT, (int *) &option);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (option) {
	case optAddress:
	    address = Tcl_GetStringFromObj(objv[i], NULL);
	    break;
	case optName:
	    name = Tcl_GetStringFromObj(objv[i], NULL);
	    break;
	case optSort:
	    sortMode = TnmGetTableKeyFromObj(interp, sortModeTable,
					     objv[i], "sort mode");
	    if (sortMode == -1) {
		return TCL_ERROR;
	    }
	    break;
	case optOrder:
	    order = Tcl_GetStringFromObj(objv[i], NULL);
	    if (strcmp(order, "increasing") == 0) {
		sortMode &= ~TNM_SORT_DECR;
	    } else if (strcmp(order, "decreasing") == 0) {
		sortMode |= TNM_SORT_DECR;
	    } else {
		Tcl_AppendResult(interp, "unknown sort order \"",
                                 order, "\": should be increasing, ",
				 "or decreasing", (char *) NULL);
                return TCL_ERROR;
	    }
	    break;
	case optTags:
	    patList = objv[i];
	    break;
	case optType:
	    typePtr = GetItemType(interp, Tcl_GetStringFromObj(objv[i], NULL));
	    if (! typePtr) {
		return TCL_ERROR;
	    }
	    break;
	}
    }

    size = mapPtr->numItems * sizeof(TnmMapItem *);
    if (size == 0) {
	return TCL_OK;
    }

    itemVector = (TnmMapItem **) ckalloc(size);
    memset((char *) itemVector, 0, size);

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {

	char *p;

	if (typePtr && itemPtr->typePtr != typePtr) continue;

	p = TnmGetTableValue(itemPtr->typePtr->configTable, 
			     TNM_ITEM_OPT_NAME);
	if (name && p && itemPtr->name) {
	    if (! Tcl_StringMatch(Tcl_GetStringFromObj(itemPtr->name, NULL),
				  name)) continue;
	}

	p = TnmGetTableValue(itemPtr->typePtr->configTable, 
			     TNM_ITEM_OPT_ADDRESS);
	if (address && p && itemPtr->address) {
	    if (! Tcl_StringMatch(itemPtr->address, address)) continue;
	}

	if (patList) {
	    int match = TnmMatchTags(interp, itemPtr->tagList, patList);
	    if (match < 0) {
		return TCL_ERROR;
	    }
	    if (! match) continue;
	}

	itemVector[itemCnt++] = itemPtr;
    }

    if (itemCnt && (sortMode & 0xFF) != TNM_SORT_NONE) {
	qsort(itemVector, itemCnt, sizeof(TnmMapItem *), SortProc);
    }

    listPtr = Tcl_GetObjResult(interp);
    for (i = 0; i < (int) itemCnt; i++) {
	CONST char *cmdName = Tcl_GetCommandName(interp, itemVector[i]->token);
	Tcl_Obj *elemObjPtr = Tcl_NewStringObj(cmdName, -1);
	Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
    }
    ckfree((char *) itemVector);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateMap --
 *
 *	This procedure is invoked to create a new map object.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new Tcl command to access the map object is created.
 *
 *----------------------------------------------------------------------
 */

static int
CreateMap(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    TnmMap *mapPtr, *p;
    static unsigned nextid = 0;
    char *name;
    int code;
    MapControl *control = (MapControl *) 
	Tcl_GetAssocData(interp, tnmMapControl, NULL);

    mapPtr = (TnmMap *) ckalloc(sizeof(TnmMap));
    memset((char *) mapPtr, 0, sizeof(TnmMap));
    mapPtr->name = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(mapPtr->name);
    mapPtr->path = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(mapPtr->path);
    mapPtr->width = 0;
    mapPtr->height = 0;
    mapPtr->expire = 3600;
    mapPtr->interp = interp;
    mapPtr->interval = 60 * 1000;
    mapPtr->tagList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(mapPtr->tagList);
    mapPtr->storeList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(mapPtr->storeList);
    mapPtr->timer = Tcl_CreateTimerHandler(mapPtr->interval, 
					   TickProc, (ClientData) mapPtr);
    Tcl_GetTime(&mapPtr->lastTick);
    Tcl_InitHashTable(&(mapPtr->attributes), TCL_STRING_KEYS);

    code = TnmSetConfig(interp, &configTable, (ClientData) mapPtr,
			objc, objv);
    if (code != TCL_OK) {
	Tcl_EventuallyFree((ClientData) mapPtr, MapDestroyProc);
        return TCL_ERROR;
    }

    /*
     * Put the new map in our map list. We add it at the end
     * to preserve the order in which the maps were created.
     */

    if (control->mapList == NULL) {
        control->mapList = mapPtr;
    } else {
        for (p = control->mapList; p->nextPtr != NULL; p = p->nextPtr) ;
	p->nextPtr = mapPtr;
    }

    /*
     * Create a new Tcl command for this map object.
     */

    name = TnmGetHandle(interp, "map", &nextid);
    mapPtr->token = Tcl_CreateObjCommand(interp, name, MapObjCmd, 
				   (ClientData) mapPtr, MapDeleteProc);
    Tcl_SetResult(interp, name, TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MapDeleteProc --
 *
 *	This procedure is invoked to destroy a map object. Free
 *	everything allocated before destroying the map structure.
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
MapDeleteProc(ClientData clientData)
{
    TnmMap **mapPtrPtr;
    TnmMap *mapPtr = (TnmMap *) clientData;
    MapControl *control = (MapControl *) 
	Tcl_GetAssocData(mapPtr->interp, tnmMapControl, NULL);

    /*
     * First, stop the timer associated with this map and remove all
     * items that are associated with this map.
     */

    if (mapPtr->timer) {
	Tcl_DeleteTimerHandler(mapPtr->timer);
	mapPtr->timer = 0;
	mapPtr->interval = 0;
    }
    ClearMap(mapPtr->interp, mapPtr);

    /*
     * Update the list of all known maps.
     */

    mapPtrPtr = &control->mapList;
    while (*mapPtrPtr && (*mapPtrPtr) != mapPtr) {
	mapPtrPtr = &(*mapPtrPtr)->nextPtr;
    }
    if (*mapPtrPtr) {
	(*mapPtrPtr) = mapPtr->nextPtr;
    }

    Tcl_EventuallyFree((ClientData) mapPtr, MapDestroyProc);
}

/*
 *----------------------------------------------------------------------
 *
 * MapDestroyProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a map at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the map is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
MapDestroyProc(char *memPtr)
{
    TnmMap *map = (TnmMap *) memPtr;

    if (map->name) Tcl_DecrRefCount(map->name);
    TnmAttrClear(&map->attributes);
    Tcl_DeleteHashTable(&map->attributes);

    ckfree((char*) map);
}

/*
 *----------------------------------------------------------------------
 *
 * GetOption --
 *
 *	This procedure retrieves the value of a map option.
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
    TnmMap *mapPtr = (TnmMap *) object;
    static char *freeme = NULL;

    if (freeme) {
	ckfree(freeme);
	freeme = NULL;
    }

    switch ((enum options) option) {
    case optExpire:
	return Tcl_NewIntObj(mapPtr->expire);
    case optHeight:
	return Tcl_NewIntObj(mapPtr->height);
    case optName:
	return mapPtr->name;
    case optPath:
	return mapPtr->path;
    case optStore:
	return mapPtr->storeList;
    case optTags:
	return mapPtr->tagList;
    case optTick:
	return Tcl_NewIntObj(mapPtr->interval / 1000);
    case optWidth:
	return Tcl_NewIntObj(mapPtr->width);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOption --
 *
 *	This procedure modifies a single option of a map object.
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
    TnmMap *mapPtr = (TnmMap *) object;
    int num;

    switch ((enum options) option) {
    case optExpire:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
            return TCL_ERROR;
	}
	mapPtr->expire = num;
	break;
    case optHeight:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	mapPtr->height = num;
	break;
    case optName:
	if (mapPtr->name) {
	    Tcl_DecrRefCount(mapPtr->name);
	}
	mapPtr->name = objPtr;
	Tcl_IncrRefCount(mapPtr->name);
	break;
    case optPath:
	if (mapPtr->path) {
	    Tcl_DecrRefCount(mapPtr->path);
	}
	mapPtr->path = objPtr;
	Tcl_IncrRefCount(mapPtr->path);
	break;
    case optStore:
	if (mapPtr->storeList) {
	    Tcl_DecrRefCount(mapPtr->storeList);
	}
	mapPtr->storeList = objPtr;
	Tcl_IncrRefCount(mapPtr->storeList);
	break;
    case optTags:
	Tcl_DecrRefCount(mapPtr->tagList);
	mapPtr->tagList = objPtr;
	Tcl_IncrRefCount(mapPtr->tagList);
	break;
    case optTick:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (mapPtr->timer) {
	    Tcl_DeleteTimerHandler(mapPtr->timer);
	    mapPtr->timer = 0;
	}
	mapPtr->interval = num * 1000;
	if (mapPtr->interval) {
	    mapPtr->timer = Tcl_CreateTimerHandler(mapPtr->interval, 
					   TickProc, (ClientData) mapPtr);
	    Tcl_GetTime(&mapPtr->lastTick);
	}
	break;
    case optWidth:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	mapPtr->width = num;
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DumpMapProc --
 *
 *	This procedure adds the dump of an item to the given dynamic 
 *	string. The dumped flag is set so that we can avoid multiple
 *	dumps for the same object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Item specific dumps are added to the dynamic string.
 *
 *----------------------------------------------------------------------
 */

static void
DumpMapProc(Tcl_Interp *interp, TnmMap *mapPtr, TnmMapItem *itemPtr, Tcl_DString *dsPtr)
{
    char *tok, *val;
    CONST char *str;

    if (! itemPtr || itemPtr->dumped) {
	return;
    }

    if (itemPtr->parent) {
	DumpMapProc(interp, mapPtr, itemPtr->parent, dsPtr);
    }
    if (itemPtr->srcPtr) {
	DumpMapProc(interp, mapPtr, itemPtr->srcPtr, dsPtr);
    }
    if (itemPtr->dstPtr) {
	DumpMapProc(interp, mapPtr, itemPtr->dstPtr, dsPtr);
    }

    if (itemPtr->typePtr->dumpProc) {
	(itemPtr->typePtr->dumpProc) (interp, itemPtr);
    } else {
	TnmMapItemDump(itemPtr, interp);
    }

    Tcl_DStringAppend(dsPtr, "\n", 1);

    /*
     * Some configuration options contain references to other
     * objects. We search for these options in the configuration
     * string and replace the item names with the corresponding
     * variable name. 
     */

    str = Tcl_GetStringResult(interp);
    val = TnmGetTableValue(itemPtr->typePtr->configTable, 
			   TNM_ITEM_OPT_SRC);
    if (val) {
	tok = strstr(str, " -src ");
	if (tok) {
	    Tcl_DStringAppend(dsPtr, str, tok - str + 6);
	    Tcl_DStringAppend(dsPtr, "$", 1);
	    str = tok + 6;
	}
    }

    val = TnmGetTableValue(itemPtr->typePtr->configTable, 
			   TNM_ITEM_OPT_DST);
    if (val) {
	tok = strstr(str, " -dst ");
	if (tok) {
	    Tcl_DStringAppend(dsPtr, str, tok - str + 6);
	    Tcl_DStringAppend(dsPtr, "$", 1);
	    str = tok + 6;
	}
    }

    Tcl_DStringAppend(dsPtr, str, -1);
    Tcl_ResetResult(interp);

    itemPtr->dumped = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DumpMap --
 *
 *	This procedure builds a string containing Tcl commands to
 *	rebuild the current map.
 *
 * Results:
 *	A standard Tcl result. The script describing the current
 *	map is returned in the interpreter result buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DumpMap(Tcl_Interp *interp, TnmMap *mapPtr)
{
    TnmMapItem *itemPtr;
    Tcl_DString ds;

    Tcl_DStringInit(&ds);

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {
	itemPtr->dumped = 0;
    }

    Tcl_DStringAppend(&ds, "$map configure ", -1);
    TnmSetConfig(interp, &configTable, (ClientData) mapPtr, 0, NULL);
    Tcl_DStringAppend(&ds, Tcl_GetStringFromObj(Tcl_GetObjResult(interp), 
						NULL), -1);
    Tcl_DStringAppend(&ds, "\n", 1);
    Tcl_ResetResult(interp);

    TnmAttrDump(&mapPtr->attributes, "$map", &ds);

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {
	if (! itemPtr->dumped) {
	    DumpMapProc(interp, mapPtr, itemPtr, &ds);
	}
    }
    Tcl_DStringResult(interp, &ds);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ClearMap --
 *
 *	This procedure is invoked to clear a map.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static void
ClearMap(Tcl_Interp *interp, TnmMap *mapPtr)
{
 repeat:
    if (mapPtr->itemList) {
	Tcl_DeleteCommandFromToken(interp, mapPtr->itemList->token);
	goto repeat;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * LoadMap --
 *
 *	This procedure is invoked to load a map from a channel. It scans
 *	the header to ensure we deal with a map file. We simple evaluate
 *	the body to create the map itself.
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
LoadMap(Tcl_Interp *interp, TnmMap *mapPtr, char *channelName)
{
    int mode, code, offset, valid;
    Tcl_Channel channel;
    Tcl_DString script;

    channel = Tcl_GetChannel(interp, channelName, &mode);
    if (! channel) {
	return TCL_ERROR;
    }
    if ((mode & TCL_READABLE) == 0) {
	Tcl_AppendResult(interp, "channel \"", channelName, "\" not readable",
			 (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Read the script line by line and check the header (all lines
     * starting with a # character in column zero) if they match a
     * magic signature. The offset is used to point to the beginning
     * of the last line read from the channel.
     */

    Tcl_DStringInit(&script);
    offset = 0;
    valid = 0;
    while (Tcl_Gets(channel, &script) >= 0) {
	char *line = Tcl_DStringValue(&script) + offset;
	if (*line != '#') break;
	if (Tcl_StringMatch(line, "#*Tnm map file*>> DO NOT EDIT <<")) {
	    valid++;
	}
	Tcl_DStringAppend(&script, "\n", 1);
	offset = Tcl_DStringLength(&script);
    }

    if (! valid) {
	Tcl_DStringFree(&script);
	Tcl_SetResult(interp, "invalid Tnm map file", TCL_STATIC);
	return TCL_ERROR;
    }

    if (! Tcl_Eof(channel)) {
	while (Tcl_Gets(channel, &script) >= 0) {
	    Tcl_DStringAppend(&script, "\n", 1);
	}
    }

    /*
     * Check if we have reached the normal end of file or if there
     * was an error while reading from the channel.
     */

    if (! Tcl_Eof(channel)) {
	Tcl_DStringFree(&script);
	Tcl_AppendResult(interp, "error reading \"", channelName, "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    mapPtr->loading = 1;
    code = PasteMap(interp, mapPtr, &script);
    mapPtr->loading = 0;
    Tcl_DStringFree(&script);
    return code;
}
/*
 *----------------------------------------------------------------------
 *
 * SaveMap --
 *
 *	This procedure is invoked to save a map on a channel. It creates
 *	a header to make sure we can later identify the version of the
 *	if we have to change the map format.
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
SaveMap(Tcl_Interp *interp, TnmMap *mapPtr, char *channelName)
{
    Tcl_Channel channel;
    Tcl_DString ds;
    CONST char *value;
    int mode, num;

    channel = Tcl_GetChannel(interp, channelName, &mode);
    if (! channel) {
	return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
	Tcl_AppendResult(interp, "channel \"", channelName, "\" not writable",
			 (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_DStringInit(&ds);

    Tcl_DStringAppend(&ds, "#!/bin/sh\n", -1);
    Tcl_DStringAppend(&ds, "# Machine generated Tnm map file.", -1);
    Tcl_DStringAppend(&ds, "\t-*- tcl -*-\t>> DO NOT EDIT <<\n#\n", -1);
    value = Tcl_GetVar2(interp, "tnm", "version", TCL_GLOBAL_ONLY);
    if (value) {
	Tcl_DStringAppend(&ds, "# TnmVersion: ", -1);
	Tcl_DStringAppend(&ds, value, -1);
	Tcl_DStringAppend(&ds, "\n", -1);
    }
    value = Tcl_GetVar2(interp, "tnm", "user", TCL_GLOBAL_ONLY);
    if (value) {
	Tcl_DStringAppend(&ds, "# TnmUser: ", -1);
	Tcl_DStringAppend(&ds, value, -1);
	Tcl_DStringAppend(&ds, "\n", -1);
    }
    value = Tcl_GetVar2(interp, "tnm", "arch", TCL_GLOBAL_ONLY);
    if (value) {
	Tcl_DStringAppend(&ds, "# TnmArch: ", -1);
	Tcl_DStringAppend(&ds, value, -1);
	Tcl_DStringAppend(&ds, "\n", -1);
    }
    Tcl_DStringAppend(&ds,  
		      "#\n# The map being loaded is expected to be", -1);
    Tcl_DStringAppend(&ds, 
		      " in the Tcl variable \"map\".\n#\\\n", -1);
    Tcl_DStringAppend(&ds, "exec scotty \"$0\" \"$*\"\n\n", -1);
    Tcl_DStringAppend(&ds, 
	      "if ![info exists map] { set map [Tnm::map create] }\n\n", -1);
    DumpMap(interp, mapPtr);
    Tcl_DStringAppend(&ds, 
	      Tcl_GetStringFromObj(Tcl_GetObjResult(interp), NULL), -1);
    Tcl_ResetResult(interp);

    num = Tcl_Write(channel, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
    Tcl_DStringFree(&ds);
    if (num < 0) {
	Tcl_AppendResult(interp, "error writing \"", channelName, "\": ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    if (Tcl_Flush(channel) != TCL_OK) {
	Tcl_AppendResult(interp, "error flushing \"", channelName, "\": ",
                         Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * CopyMap --
 *
 *	This procedure copies a string to re-create the list of items
 *	on the internal clipboard buffer.
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
CopyMap(Tcl_Interp *interp, TnmMap *mapPtr, int objc, Tcl_Obj *CONST objv[])
{
    TnmMapItem *itemPtr, **itemv;
    Tcl_CmdInfo info;
    Tcl_Obj **elemPtrs;
    int i, listLen, result;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "items");
	return TCL_ERROR;
    }

    /*
     * Convert the list of item names into a vector of pointer
     * to the item structures. Make sure we got a command name 
     * of an item that actually belongs to this map.
     */

    result = Tcl_ListObjGetElements(interp, objv[2], &listLen, &elemPtrs);
    if (result != TCL_OK) {
	return result;
    }

    itemv = (TnmMapItem **) ckalloc(listLen * sizeof(TnmMapItem *));
    for (i = 0; i < listLen; i++) {
	char *name = Tcl_GetStringFromObj(elemPtrs[i], NULL);
	int ok = Tcl_GetCommandInfo(interp, name, &info);
	for (itemPtr = mapPtr->itemList; 
	     ok && itemPtr; itemPtr = itemPtr->nextPtr) {
	    if (itemPtr == (TnmMapItem *) info.clientData) break;
	}
	if (!ok || !itemPtr) {
	    Tcl_AppendResult(interp, "unknown item \"", name, "\"", 
			     (char *) NULL);
	    ckfree((char *) itemv);
	    return TCL_ERROR;
	}
	itemv[i] = (TnmMapItem *) info.clientData;
    }

    /*
     * Mark all items as dumped which are not in the item vector so
     * that we can use the normal dump procedure to create a script
     * to re-build these objects.
     */

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {
	for (i = 0; i < listLen; i++) {
	    if (itemPtr == itemv[i]) break;
	}
	itemPtr->dumped = (i == listLen);
    }
    ckfree((char *) itemv);

    Tcl_DStringFree(clip);
    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {
	if (! itemPtr->dumped) {
	    Tcl_DStringAppend(clip, "\n", 1);
	    DumpMapProc(interp, mapPtr, itemPtr, clip);
	}
    }
    return TCL_OK;
}
/*
 *----------------------------------------------------------------------
 *
 * PasteMap --
 *
 *	This procedure evaluates the clipboard to re-create items
 *	that have been previously saved on the internal clipboard.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *---------------------------------------------------------------------- */

static int
PasteMap(Tcl_Interp *interp, TnmMap *mapPtr, Tcl_DString *script)
{
    int code;
    const char *value;
    CONST char *map = Tcl_GetCommandName(interp, mapPtr->token);

    /*
     * Map scripts expect the name of the map which is modified in
     * the global Tcl variable "map". We set this variable here.
     * We restore the current value of this variable if it already
     * exists.
     */

    value = Tcl_GetVar(interp, "map", 0);
    if (value) {
	value = ckstrdup(value);
    }
    Tcl_SetVar(interp, "map", map, 0);

    code = Tcl_Eval(interp, Tcl_DStringValue(script));

    if (value) {
	Tcl_SetVar(interp, "map", value, 0);
	ckfree(value);
    } else {
	Tcl_UnsetVar(interp, "map", 0);
    }

    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * MapObjCmd --
 *
 *	This procedure implements the map object command.
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
MapObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    TnmMap *mapPtr = (TnmMap *) clientData;
    int result;
    TnmMapEvent *eventPtr;
    TnmMapBind *bindPtr;
    TnmMapMsg *msgPtr;
    char *pattern = NULL;
    Tcl_Obj *listPtr;

    enum commands {
	cmdAttribute, cmdBind, cmdClear, cmdCget, cmdConfigure, cmdCopy, 
	cmdCreate, cmdDestroy, cmdDump, cmdFind, cmdInfo, cmdLoad,
	cmdMsg, cmdPaste, cmdRaise, cmdSave, cmdUpdate
    } cmd;

    static CONST char *cmdTable[] = {
	"attribute", "bind", "clear", "cget", "configure", "copy",
	"create", "destroy", "dump", "find", "info", "load",
	"message", "paste", "raise", "save", "update", (char *) NULL
    };

    enum infos { infoBindings, infoEvents, infoMsgs } info;

    static CONST char *infoTable[] = {
	"bindings", "events", "messages", (char *) NULL
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

    /*
     * Preserve the mapPtr so that it can't be deleted while executing
     * one of the map command options. This means that it is not legal
     * to simply 'return' from the switch below to keep the reference
     * count valid.
     */

    Tcl_Preserve((ClientData) mapPtr);

    switch ((enum commands) cmd) {

    case cmdAttribute:
	if (objc < 2 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?name ?value??");
	    result = TCL_ERROR;
	    break;
	}
	switch (objc) {
	case 2:
	    TnmAttrList(&mapPtr->attributes, interp);
	    break;
	case 3:
	    result = TnmAttrSet(&mapPtr->attributes, interp, 
				Tcl_GetStringFromObj(objv[2], NULL), NULL);
	    break;
	case 4:
	    TnmAttrSet(&mapPtr->attributes, interp, 
		       Tcl_GetStringFromObj(objv[2], NULL),
		       Tcl_GetStringFromObj(objv[3], NULL));
	    break;
	}
	break;

    case cmdBind:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "pattern script");
	    result = TCL_ERROR;
            break;
	}
	bindPtr = TnmMapUserBinding(mapPtr, NULL, 
				    Tcl_GetStringFromObj(objv[2], NULL),
				    Tcl_GetStringFromObj(objv[3], NULL));
	result = (bindPtr) ? TCL_OK : TCL_ERROR;
	break;

    case cmdClear:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
            break;
	}
	ClearMap(interp, mapPtr);
	break;

    case cmdCget:
	result = TnmGetConfig(interp, &configTable,
			      (ClientData) mapPtr, objc, objv);
	break;

    case cmdConfigure:
	result = TnmSetConfig(interp, &configTable,
			      (ClientData) mapPtr, objc, objv);
	break;

    case cmdCopy:
	result = CopyMap(interp, mapPtr, objc, objv);
	break;

    case cmdCreate:
	result = CreateItem(interp, mapPtr, objc, objv);
	break;

    case cmdDestroy:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	result = Tcl_DeleteCommandFromToken(interp, mapPtr->token);
	break;

    case cmdDump:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	result = DumpMap(interp, mapPtr);
	break;

    case cmdInfo:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "subject ?pattern?");
	    result = TCL_ERROR;
            break;
	}
	result = Tcl_GetIndexFromObj(interp, objv[2], infoTable, 
				     "option", TCL_EXACT, (int *) &info);
	if (result != TCL_OK) {
	    break;
	}
	pattern = (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL;
	listPtr = Tcl_GetObjResult(interp);
	switch (info) {
	case infoMsgs:
	    for (msgPtr = mapPtr->msgList; msgPtr; msgPtr = msgPtr->nextPtr) {
		if (pattern &&
		    !Tcl_StringMatch(Tcl_GetStringFromObj(msgPtr->tag, NULL),
				     pattern)) {
		    continue;
		}
		if (msgPtr->token) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, msgPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	case infoEvents:
	    for (eventPtr = mapPtr->eventList; 
		 eventPtr; eventPtr = eventPtr->nextPtr) {
		if (pattern && !Tcl_StringMatch(eventPtr->eventName, 
						pattern)) {
		    continue;
		}
		if (eventPtr->token) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, eventPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	case infoBindings:
	    for (bindPtr = mapPtr->bindList; 
		 bindPtr; bindPtr = bindPtr->nextPtr) {
		if (pattern && !Tcl_StringMatch(bindPtr->pattern, pattern)) {
		    continue;
		}
		if (bindPtr->type == TNM_MAP_USER_EVENT) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, bindPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	}	
	break;

    case cmdFind:
	result = FindItems(interp, mapPtr, objc, objv);
	break;

    case cmdLoad:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "channel");
	    result = TCL_ERROR;
	    break;
	}
	result = LoadMap(interp, mapPtr, Tcl_GetStringFromObj(objv[2], NULL));
	if (result == TCL_OK) {
	    Tcl_SetObjResult(interp, objv[0]);
	}
	break;

    case cmdMsg:
        result = TnmMapMsgCmd(interp, mapPtr, NULL, objc, objv);
	break;

    case cmdPaste:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
            break;
	}
	result = PasteMap(interp, mapPtr, clip);
	break;

    case cmdRaise:	
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?argument?");
	    result = TCL_ERROR;
	    break;
	}
	eventPtr = TnmMapCreateUserEvent(mapPtr, (TnmMapItem *) NULL, 
				 Tcl_GetStringFromObj(objv[2], NULL),
		   (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL);
	if (eventPtr) {
	    TnmMapRaiseEvent(eventPtr);
	}
	break;

    case cmdSave:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "channel");
	    result = TCL_ERROR;
	    break;
	}
	result = SaveMap(interp, mapPtr, Tcl_GetStringFromObj(objv[2], NULL));
	break;

    case cmdUpdate:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
	if (mapPtr->timer) {
	    Tcl_DeleteTimerHandler(mapPtr->timer);
            mapPtr->timer = 0;
	}
	TickProc((ClientData) mapPtr);
	break;
    }

    Tcl_Release((ClientData) mapPtr);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FindMaps --
 *
 *	This procedure is invoked to process the "find" command
 *	option of the map command.
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
FindMaps(Tcl_Interp *interp, MapControl *control, int objc, Tcl_Obj *CONST objv[])
{
    int i, result;
    TnmMap *mapPtr;
    Tcl_Obj *listPtr, *patList = NULL;

    enum options { optTags } option;

    static CONST char *optionTable[] = {
        "-tags", (char *) NULL
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
	case optTags:
	    patList = objv[i];
	    break;
	}
    }

    listPtr = Tcl_GetObjResult(interp);
    for (mapPtr = control->mapList; mapPtr; mapPtr = mapPtr->nextPtr) {
	if (patList) {
	    int match = TnmMatchTags(interp, mapPtr->tagList, patList);
	    if (match < 0) {
		return TCL_ERROR;
	    }
	    if (! match) continue;
	}
	{
	    CONST char *cmdName = Tcl_GetCommandName(interp, mapPtr->token);
	    Tcl_Obj *elemObjPtr = Tcl_NewStringObj(cmdName, -1);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_MapObjCmd --
 *
 *	This procedure is invoked to process the "map" command.
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
Tnm_MapObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    TnmMap *mapPtr;
    TnmMapItemType *typePtr;
    int result;
    char *pattern = NULL; 
    Tcl_Obj *listPtr;
    MapControl *control = (MapControl *) 
	Tcl_GetAssocData(interp, tnmMapControl, NULL);

    enum commands { cmdCreate, cmdFind, cmdInfo } cmd;

    static CONST char *cmdTable[] = {
	"create", "find", "info", (char *) NULL
    };

    enum infos { infoMaps, infoTypes } info;

    static CONST char *infoTable[] = {
	"maps", "types", (char *) NULL
    };

    if (! control) {
	control = (MapControl *) ckalloc(sizeof(MapControl));
	memset((char *) control, 0, sizeof(MapControl));
	Tcl_SetAssocData(interp, tnmMapControl, AssocDeleteProc, 
			 (ClientData) control);
	TnmMapRegisterItemType(&tnmNodeType);
	TnmMapRegisterItemType(&tnmPortType);
	TnmMapRegisterItemType(&tnmNetworkType);
	TnmMapRegisterItemType(&tnmLinkType);
	TnmMapRegisterItemType(&tnmGroupType);
    }

    if (! clip) {
	clip = (Tcl_DString *) ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(clip);
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
	result = CreateMap(interp, objc, objv);
	break;

    case cmdFind:
	result = FindMaps(interp, control, objc, objv);
	break;

    case cmdInfo:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "subject ?pattern?");
	    result = TCL_ERROR;
            break;
	}
	result = Tcl_GetIndexFromObj(interp, objv[2], infoTable, 
				     "option", TCL_EXACT, (int *) &info);
	if (result != TCL_OK) {
	    break;
	}
	pattern = (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL;
	listPtr = Tcl_GetObjResult(interp);
	switch (info) {
	case infoMaps:
	    if (control) {
		listPtr = Tcl_GetObjResult(interp);
		for (mapPtr = control->mapList; 
		     mapPtr; mapPtr = mapPtr->nextPtr) {
		    CONST char *mapName;
		    mapName = Tcl_GetCommandName(interp, mapPtr->token);
		    if (pattern && !Tcl_StringMatch(mapName, pattern)) {
			continue;
		    }
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(mapName, -1));
		}
	    }
	    break;
	case infoTypes:
	    listPtr = Tcl_GetObjResult(interp);
	    for (typePtr = itemTypes; typePtr; typePtr = typePtr->nextPtr) {
		if (pattern && !Tcl_StringMatch(typePtr->name, pattern)) {
		    continue;
		}
		Tcl_ListObjAppendElement(interp, listPtr, 
					 Tcl_NewStringObj(typePtr->name, -1));
	    }
	    break;
	}
	break;
    }

    return result;
}

