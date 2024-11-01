/*
 * tnmMibFrozen.c --
 *
 *	Save and load MIB-Definitions in/from a frozen-format file.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * Strings are collected in a hashtable for every parsed mib. This allows
 * us to write all strings in one big chunk so that we do not need to
 * allocate every single string. The offset (== size) of the string pool
 * is maintained in poolOffset.
 */

static Tcl_HashTable *poolHashTable = NULL;
static int poolOffset = 0;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
PoolInit		(void);

static void
PoolDelete		(void);

static void
PoolAddString		(char *string);

static int
PoolGetOffset		(char *string);

static void
PoolSave		(FILE *fp);

static void
SaveRest		(TnmMibRest *restPtr, int restKind, 
				     FILE *fp);
static void
SaveType		(TnmMibType *typePtr, int *i, FILE *fp);

static void
SaveNode		(TnmMibNode *nodePtr, int *i, FILE *fp);

static void
CollectData		(int *numRests, int *numTcs,
				     int *numNodes, TnmMibNode *nodePtr);
static void
SaveData		(FILE *fp, int numRests, int numTcs, 
				     int numNodes, TnmMibNode *nodePtr);


/*
 *----------------------------------------------------------------------
 *
 * PoolInit --
 *
 *	This procedure initializes the hash table that is used to
 *	create a string pool. The pool is used to eliminate 
 *	duplicated strings.
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
PoolInit()
{
    poolOffset = 0;
    if (poolHashTable == NULL) {
	poolHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
    }
    Tcl_InitHashTable(poolHashTable, TCL_STRING_KEYS);
}

/*
 *----------------------------------------------------------------------
 *
 * PoolDelete --
 *
 *	This procedure clears the memory used to create the string
 *	pool. Very simple and really not worth the comments. Thats
 *	why this one is getting a bit longer as usual. :-)
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
PoolDelete()
{
    if (poolHashTable) {
        Tcl_DeleteHashTable(poolHashTable);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PoolAddString --
 *
 *	This procedure adds a string to the pool if it is not yet 
 *	there. The value is initialized to mark this entry as used.
 *	The total offset is incremented to get total memory required 
 *	for the string pool. 
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
PoolAddString(char *s)
{
    Tcl_HashEntry *entryPtr;
    int isnew;

    if (! s) return;

    entryPtr = Tcl_CreateHashEntry(poolHashTable, s, &isnew);
    if (! isnew) {
	return;
    }
    Tcl_SetHashValue(entryPtr, 1);
    poolOffset += strlen(s) + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * PoolGetOffset --
 *
 *	This procedure returns the offset to the given string in the 
 *	string pool or 0 if the string is not in the pool.
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
PoolGetOffset(char *s)
{
    Tcl_HashEntry *entryPtr;

    if (! s) return 0;

    entryPtr = Tcl_FindHashEntry(poolHashTable, s);
    if (entryPtr) {
        return (int) Tcl_GetHashValue(entryPtr);
    } else {
	return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PoolSave --
 *
 *	This procedure writes the string pool to the given file pointer.
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
PoolSave(FILE *fp)
{
    Tcl_HashSearch searchPtr;
    Tcl_HashEntry *entryPtr;

    /*
     * Save magic and size:
     */

    poolOffset += strlen(TNM_VERSION) + 1;
    fwrite((char *) &poolOffset, sizeof(int), 1, fp);
    fwrite(TNM_VERSION, 1, strlen(TNM_VERSION) + 1, fp);

    /*
     * Save the strings in the pool:
     */

    poolOffset = strlen(TNM_VERSION) + 1;
    entryPtr = Tcl_FirstHashEntry(poolHashTable, &searchPtr);
    while (entryPtr) {
	char *s = Tcl_GetHashKey(poolHashTable, entryPtr);
	unsigned int len = strlen(s) + 1;
	Tcl_SetHashValue(entryPtr, poolOffset);
	fwrite(s, 1, len, fp);
	poolOffset += len;
	entryPtr = Tcl_NextHashEntry(&searchPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CollectData --
 *
 *	This procedure collects the strings (by adding them to the pool)
 *	and counts the number of enum/range restrictions, tcs and nodes
 *	to be saved.
 *
 * Results:
 *	Returns the number of enum/range restrictions, tcs and nodes.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
CollectData(int *numEnums, int *numTcs, int *numNodes, TnmMibNode *nodePtr)
{
    TnmMibNode *ptr;
    TnmMibRest *restPtr;
    TnmMibType *typePtr;

    *numEnums = *numTcs = *numNodes = 0;
    for (ptr = nodePtr; ptr; (*numNodes)++, ptr = ptr->nextPtr) {
	PoolAddString(ptr->label);
	PoolAddString(ptr->parentName);
	PoolAddString(ptr->fileName);
	PoolAddString(ptr->moduleName);
	PoolAddString(ptr->index);
	if (ptr->typePtr) {
	    (*numTcs)++;
	    PoolAddString(ptr->typePtr->name);
	    PoolAddString(ptr->typePtr->fileName);
	    PoolAddString(ptr->typePtr->moduleName);
	    PoolAddString(ptr->typePtr->displayHint);
	    if (ptr->typePtr->restKind == TNM_MIB_REST_ENUMS
		|| ptr->typePtr->restKind == TNM_MIB_REST_RANGE
		|| ptr->typePtr->restKind == TNM_MIB_REST_SIZE) {
		for (restPtr = ptr->typePtr->restList;
		     restPtr;
		     (*numEnums)++, restPtr = restPtr->nextPtr) {
		    if (ptr->typePtr->restKind == TNM_MIB_REST_ENUMS) {
			PoolAddString(restPtr->rest.intEnum.enumLabel);
		    }
		}
	    }
	}
    }
    for (typePtr = tnmMibTypeList; 
	 typePtr != tnmMibTypeSaveMark; typePtr = typePtr->nextPtr) {
        (*numTcs)++;
	PoolAddString(typePtr->name);
	PoolAddString(typePtr->fileName);
	PoolAddString(typePtr->moduleName);
	PoolAddString(typePtr->displayHint);
	if (typePtr->restKind == TNM_MIB_REST_ENUMS
	    || typePtr->restKind == TNM_MIB_REST_RANGE
	    || typePtr->restKind == TNM_MIB_REST_SIZE) {
	    for (restPtr = typePtr->restList;
		 restPtr;
		 (*numEnums)++, restPtr = restPtr->nextPtr) {
		if (typePtr->restKind == TNM_MIB_REST_ENUMS) {
		    PoolAddString(restPtr->rest.intEnum.enumLabel);
		}
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SaveRest --
 *
 *	This procedure writes a TnmMibRest structure. The char*
 *	pointers are used to store the offset into the string pool.
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
SaveRest(TnmMibRest *restPtr, int restKind, FILE *fp)
{
    TnmMibRest rest;

    if (restKind == TNM_MIB_REST_ENUMS) {
	memcpy((char *) &rest, (char *) restPtr, sizeof(TnmMibRest));
	rest.rest.intEnum.enumLabel = 
	    (char *) PoolGetOffset(restPtr->rest.intEnum.enumLabel);
	rest.nextPtr = (TnmMibRest *) (restPtr->nextPtr ? 1 : 0);
	restPtr = &rest;
    }

    fwrite((char *) restPtr, sizeof(TnmMibRest), 1, fp);
}

/*
 *----------------------------------------------------------------------
 *
 * SaveType --
 *
 *	This procedure writes a TnmMibType structure. The char*
 *	pointers are used to store the offset into the string pool.
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
SaveType(TnmMibType *typePtr, int *i, FILE *fp)
{
    TnmMibType tc;

    memcpy((char *) &tc, (char *) typePtr, sizeof(TnmMibType));
    tc.name = (char *) PoolGetOffset(typePtr->name);
    tc.fileName = (char *) PoolGetOffset(typePtr->fileName);
    tc.moduleName = (char *) PoolGetOffset(typePtr->moduleName);
    tc.displayHint = (char *) PoolGetOffset(typePtr->displayHint);
    if (typePtr->restList) {
	TnmMibRest *e;
	tc.restList = (TnmMibRest *) (*i + 1);
	for (e = typePtr->restList; e; (*i)++, e = e->nextPtr) ;
    }
    tc.nextPtr = (TnmMibType *) (typePtr->nextPtr ? 1 : 0);

    fwrite((char *) &tc, sizeof(TnmMibType), 1, fp);
}

/*
 *----------------------------------------------------------------------
 *
 * SaveNode --
 *
 *	This procedure writes a TnmMibNode structure. The char*
 *	pointers are used to store the offset into the string pool.
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
SaveNode(TnmMibNode *nodePtr, int *i, FILE *fp)
{
    struct TnmMibNode no;

    memcpy((char *) &no, (char *) nodePtr, sizeof(TnmMibNode));
    no.label = (char *) PoolGetOffset(nodePtr->label);
    no.parentName = (char *) PoolGetOffset(nodePtr->parentName);
    no.fileName = (char *) PoolGetOffset(nodePtr->fileName);
    no.moduleName = (char *) PoolGetOffset(nodePtr->moduleName);
    no.index = (char *) PoolGetOffset(nodePtr->index);
    no.childPtr = 0;
    if (nodePtr->typePtr) {
	no.typePtr = (TnmMibType *) ++(*i);
    }
    no.nextPtr = (TnmMibNode *) (nodePtr->nextPtr ? 1 : 0);
    
    fwrite((char *) &no, sizeof(TnmMibNode), 1, fp);
}

/*
 *----------------------------------------------------------------------
 *
 * SaveData --
 *
 *	This procedure writes the node list as an index file to fp.
 *
 *	int (#enum elements)
 *  		followed by #enum structs
 * 	int (#tc elements)
 *		followed by #tc structs
 *      int (#node elements)
 *		followed by n TnmMibNode structs.
 *
 *	String pointer are saved as offsets relative to the pool,
 *	enum pointer are saved as offset to the saved enums (+ 1)
 *	tc pointer dito.
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
SaveData(fp, numEnums, numTCs, numNodes, nodePtr)
    FILE *fp;
    int numEnums, numTCs, numNodes;
    TnmMibNode *nodePtr;
{
    TnmMibNode *nPtr;
    TnmMibRest *ePtr;
    TnmMibType *tPtr;
    int i;
    
    /*
     * Save numEnums TnmMibRest structures:
     */

    fwrite((char *) &numEnums, sizeof(int), 1, fp);
    for (nPtr = nodePtr; nPtr; nPtr = nPtr->nextPtr) {
	if (nPtr->typePtr) {
	    for (ePtr = nPtr->typePtr->restList; ePtr; ePtr = ePtr->nextPtr) {
		SaveRest(ePtr, nPtr->typePtr->restKind, fp);
	    }
	}
    }
    for (tPtr=tnmMibTypeList; tPtr != tnmMibTypeSaveMark; tPtr = tPtr->nextPtr) {
	for (ePtr = tPtr->restList; ePtr; ePtr = ePtr->nextPtr) {
	    SaveRest(ePtr, tPtr->restKind, fp);
	}
    }

    /*
     * Save numTCs Tnm_MibTC structures:
     */

    fwrite((char *) &numTCs, sizeof(int), 1, fp);
    for (i = 0, nPtr = nodePtr; nPtr; nPtr = nPtr->nextPtr) {
	if (nPtr->typePtr) {
	    SaveType(nPtr->typePtr, &i, fp);
	}
    }
    for (tPtr=tnmMibTypeList; tPtr != tnmMibTypeSaveMark; tPtr = tPtr->nextPtr) {
	SaveType(tPtr, &i, fp);
    }
    
    /*
     * Save numNodes TnmMibNode structures:
     */

    fwrite((char *) &numNodes, sizeof(int), 1, fp);
    for (i = 0, nPtr = nodePtr; nPtr; nPtr = nPtr->nextPtr) {
	SaveNode(nPtr, &i, fp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibWriteFrozen --
 *
 *	This procedure writes a frozen MIB file. See the description
 *	of TnmMibReadFrozen() for an explanation of the format.
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
TnmMibWriteFrozen(FILE *fp, TnmMibNode *nodePtr)
{
    int numEnums, numTcs, numNodes; 
    PoolInit();
    CollectData(&numEnums, &numTcs, &numNodes, nodePtr);
    PoolSave(fp);
    SaveData(fp, numEnums, numTcs, numNodes, nodePtr);
    PoolDelete();
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibReadFrozen --
 *
 *	This procedure reads a frozen MIB file that was written by 
 *	TnmMibWriteFrozen(). The expected format is:
 *
 *	int (stringpool size)
 *		pool_size bytes
 *	int (number of enum structs)
 *		#enum structs
 *	int (number of textual conventions)
 *		#tc structs
 *	int (number of nodes)
 *		#node structs
 *
 * Results:
 *	A pointer to the MIB root node or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMibNode*
TnmMibReadFrozen(FILE *fp)
{
    TnmMibNode *root = NULL;
    size_t poolSize;
    char *pool;
    size_t numEnums;
    TnmMibRest *enums = NULL;
    size_t numTcs;
    TnmMibType *tcs = NULL;
    size_t numNodes;
    TnmMibNode *nodes;

    /*
     * First, read the string space: 
     */

    if (1 != fread((char *) &poolSize, sizeof(int), 1, fp)) {
	TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "error reading string pool size...\n");
	return NULL;
    }

    pool = ckalloc(poolSize);
    if (poolSize != fread(pool, 1, poolSize, fp)) {
        TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "error reading string pool...\n");
	return NULL;
    }
    if (strcmp(pool, TNM_VERSION) != 0) {
       TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "wrong .idy file version...\n");
	return NULL;
    }

    /*
     * Next, read the restrictions:
     */

    if (1 != fread((char *) &numEnums, sizeof(numEnums), 1, fp)) {
        TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "error reading enum counter...\n");
	return NULL;
    }

    if (numEnums > 0) {
	TnmMibRest *restPtr;
	int i;
	enums = (TnmMibRest *) ckalloc(numEnums * sizeof(TnmMibRest));
	if (numEnums != fread(enums, sizeof(TnmMibRest), numEnums, fp)) {
	    TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			       "error reading enums...\n");
	    ckfree((char *) enums);
	    return NULL;
	}
    
	/* 
	 * Chain the pointers:
	 */
	
	for (i = 0, restPtr = enums; i < (int) numEnums; i++, restPtr++) {
	    restPtr->nextPtr = restPtr->nextPtr ? restPtr + 1 : 0;
	}
    }

    /*
     * Next, read the textual conventions: 
     */

    if (1 != fread((char *) &numTcs, sizeof(numTcs), 1, fp)) {
	TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "error reading tc counter...\n");
	return NULL;
    }

    if (numTcs > 0) {
	TnmMibType *typePtr;
	int i;

	tcs = (TnmMibType *) ckalloc(numTcs * sizeof(TnmMibType));
	if (numTcs != fread(tcs, sizeof(TnmMibType), numTcs, fp)) {
	    TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			       "error reading tcs...\n");
	    ckfree((char *) tcs);
	    return NULL;
	}
	
	/* 
	 * Adjust string and enum pointers: 
	 */

	for (i = 0, typePtr = tcs; i < (int) numTcs; i++, typePtr++) {
	    typePtr->name = (int) typePtr->name + pool;
	    if (typePtr->fileName) {
	        typePtr->fileName = (int) typePtr->fileName + pool;
	    }
	    if (typePtr->moduleName) {
	        typePtr->moduleName = (int) typePtr->moduleName + pool;
	    }
	    if (typePtr->displayHint) {
		typePtr->displayHint = (int) typePtr->displayHint + pool;
	    }
	    if (typePtr->restList) {
		typePtr->restList = (int) typePtr->restList + enums - 1;
		if (typePtr->restKind == TNM_MIB_REST_ENUMS) {
		    /* Adjust the string pool pointers. */
		    TnmMibRest *restPtr;
		    for (restPtr = typePtr->restList;
			 restPtr;
			 restPtr = restPtr->nextPtr) {
			restPtr->rest.intEnum.enumLabel = 
			    (int) restPtr->rest.intEnum.enumLabel + pool;
		    }
		}
	    }
	    if (typePtr->name[0] != '_') {
		TnmMibAddType(typePtr);
	    }
	}	
    }

    /*
     * Next, read the MIB nodes:
     */

    if (1 != fread((char *) &numNodes, sizeof(numNodes), 1, fp)) {
	TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			   "error reading node counter...\n");
	return NULL;
    }

    if (numNodes > 0) {
	TnmMibNode *ptr;
	int i;

	nodes = (TnmMibNode *) ckalloc(numNodes * sizeof(TnmMibNode));
	if (numNodes != fread(nodes, sizeof(TnmMibNode), numNodes, fp)) {
	    TnmWriteLogMessage(NULL, TNM_LOG_DEBUG, TNM_LOG_USER,
			       "error reading nodes...\n");
	    ckfree((char *) nodes);
	    return NULL;
	}
	
	/*
	 * Adjust string and tc pointers:
	 */

	for (i = 0, ptr = nodes; i < (int) numNodes; i++, ptr++) {
	    ptr->label = (int) ptr->label + pool;
	    ptr->parentName = (int) ptr->parentName + pool;
	    if (ptr->fileName) {
	        ptr->fileName = (int) ptr->fileName + pool;
	    }
	    if (ptr->moduleName) {
	        ptr->moduleName = (int) ptr->moduleName + pool;
	    }
	    if (ptr->index) {
	        ptr->index = (int) ptr->index + pool;
	    }
	    if (ptr->typePtr) {
	        ptr->typePtr = (int) ptr->typePtr + tcs - 1;
	    }
	    ptr->nextPtr = ptr->nextPtr ? ptr + 1 : 0;
	}
	root = nodes;
    }
    
    return root;
}

