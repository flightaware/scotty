/*
 * tnmMibTree.c --
 *
 *	Some utilities to build a tree that contains the data found 
 *	in MIB definitions.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMibTree.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * The following table is used to hash nodes by name before building
 * the MIB tree. This has the nice effect that nodes with the same
 * parent can be found in the list that saves the collisions.
 */

#define NODEHASHSIZE	127
static TnmMibNode	*nodehashtab[NODEHASHSIZE];

/*
 * Hashtable used to store textual conventions by name. This
 * allows fast lookups. The nodeHashTable is used to lookup
 * MIB names.
 */

static Tcl_HashTable *typeHashTable = NULL;
static Tcl_HashTable *nodeHashTable = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static TnmMibNode*
LookupOID		(TnmMibNode *root, char *label,
				     int *offset, int exact);
static TnmMibNode*
LookupLabelOID		(TnmMibNode *root, char *label,
				     int *offset, int exact);
static TnmMibNode*
LookupLabel		(TnmMibNode *root, char *start, 
				     char *label, char *moduleName,
				     int *offset, int exact, int fuzzy);
static void
HashNode		(TnmMibNode *node);

static TnmMibNode*
BuildTree		(TnmMibNode *nlist);

static void
BuildSubTree		(TnmMibNode *root);

static void
HashNodeList		(TnmMibNode *nlist);

static int
HashNodeLabel		(char *label);


/*
 *----------------------------------------------------------------------
 *
 * TnmMibNodeFromOid --
 *
 *	This procedure searches for a MIB node by a given TnmOid. The
 *	optional argument nodeOidPtr is modified to contain the TnmOid
 *	of the selected MIB node, which might be shorter than the search
 *	TnmOid.
 *
 * Results:
 *	The pointer to the node or NULL if the node was not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMibNode*
TnmMibNodeFromOid(oidPtr, nodeOidPtr)
    TnmOid *oidPtr;
    TnmOid *nodeOidPtr;
{
    int i;
    TnmMibNode *p, *q = NULL;

    if (nodeOidPtr) {
	TnmOidFree(nodeOidPtr);
    }

    for (p = tnmMibTree; p ; p = p->nextPtr) {
	if (TnmOidGet(oidPtr, 0) == p->subid) break;
    }
    if (!p) {
	return NULL;
    }

    if (nodeOidPtr) {
	TnmOidAppend(nodeOidPtr, TnmOidGet(oidPtr, 0));
    }

    for (q = p, i = 1; i < TnmOidGetLength(oidPtr); p = q, i++) {
	for (q = p->childPtr; q; q = q->nextPtr) {
	    if (q->subid == TnmOidGet(oidPtr, i)) {
		if (nodeOidPtr) {
		    TnmOidAppend(nodeOidPtr, TnmOidGet(oidPtr, i));
		}
		break;
	    }
	}
	if (!q) {
	    return p;
	}
    }

    return q;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupOID --
 *
 *	This procedure searches for the node given by soid. It converts 
 *	the oid from string to binary representation and uses to subids
 *	to go from the root to the requested node. 
 *
 * Results:
 *	The pointer to the node or NULL if the node was not found.
 *	The offset to any trailing subidentifiers is written to offset, 
 *	if offset is not a NULL pointer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmMibNode*
LookupOID(root, label, offset, exact)
    TnmMibNode *root;
    char *label;
    int *offset;
    int exact;
{
    TnmOid oid;
    int i;
    TnmMibNode *p, *q = NULL;
    char *s = label;

    if (offset) *offset = -1;

    /*
     * We handle some special cases here because the usual OID
     * conversion does not allow OIDs with a single sub-identifier.
     */

    TnmOidInit(&oid);
    if (strcmp(label, "0") == 0) {
	TnmOidSetLength(&oid, 1);
	TnmOidSet(&oid, 0, 0);
    } else if (strcmp(label, "1") == 0) {
	TnmOidSetLength(&oid, 1);
	TnmOidSet(&oid, 0, 1);
    } else if (strcmp(label, "2") == 0) {
	TnmOidSetLength(&oid, 1);
	TnmOidSet(&oid, 0, 2);
    } else if (TnmOidFromString(&oid, label) != TCL_OK) {
	return NULL;
    }

    for (p = root; p ; p = p->nextPtr) {
	if (TnmOidGet(&oid, 0) == p->subid) break;
    }
    if (!p) {
	TnmOidFree(&oid);
	return NULL;
    }

    while (offset && s && ispunct(*s)) s++;
    while (offset && s && isdigit(*s)) s++;

    for (q = p, i = 1; i < TnmOidGetLength(&oid); p = q, i++) {
	for (q = p->childPtr; q; q = q->nextPtr) {
	    if (q->subid == TnmOidGet(&oid, i)) break;
	}
	if (!q) {
	    if (! exact) {
		if (offset) {
		    *offset = s - label;
		}
		TnmOidFree(&oid);
		return p;
	    } else {
		TnmOidFree(&oid);
		return NULL; 
	    }
	}
	while (offset && s && ispunct(*s)) s++;
	while (offset && s && isdigit(*s)) s++;
    }

    TnmOidFree(&oid);
    return q;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupLabelOID --
 *
 *	This procedure searches for the a MIB node given the very 
 *	common format label.oid where label is a simple name of a MIB 
 *	node and oid an object identifier in dotted notation. If this 
 *	is true and the lookup is not exact, search for the label in 
 *	the hash table and append the object identifier part to the 
 *	oid of the label. (This a special case for frequently used 
 *	names like snmpInTooBigs.0 which tend otherwise to be slow.)
 *
 * Results:
 *	The pointer to the node or NULL if the node was not found.
 *	The offset to any trailing subidentifiers is written to offset, 
 *	if offset is not a NULL pointer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmMibNode*
LookupLabelOID(root, label, offset, exact)
    TnmMibNode *root;
    char *label;
    int *offset;
    int exact;
{
    Tcl_HashEntry *entryPtr = NULL;
    TnmMibNode *nodePtr = NULL;

    if (exact) {
	return NULL;
    }

    if (nodeHashTable) {
	char *name = ckstrdup(label);
	char *oid = name;
	while (*oid && *oid != '.') {
	    oid++;
	}
	if (*oid && TnmIsOid(oid)) {
	    *oid++ = '\0';
	    entryPtr = Tcl_FindHashEntry(nodeHashTable, name);
	    if (entryPtr) {
		nodePtr = (TnmMibNode *) Tcl_GetHashValue(entryPtr);
	    }
	    if (nodePtr) {
		if (offset) {
		    *offset = oid - name - 1;
		    if (*offset) {
			int i;
			TnmMibNode *nPtr;
			TnmOid o;

			/*
			 * Check is we can resolve some more
			 * subidentifier from the node we have found.  
			 */

			TnmOidInit(&o);
			TnmOidFromString(&o, label+*offset);
			for (i = 0; i < TnmOidGetLength(&o); i++) {
			    for (nPtr = nodePtr->childPtr; 
				 nPtr; nPtr = nPtr->nextPtr) {
				if (nPtr->subid == TnmOidGet(&o, i)) {
				    nodePtr = nPtr;
				    break;
				}
			    }
			    if (! nPtr) break;
			}
			TnmOidFree(&o);

			/*
			 * Adjust the offset if we were successful.
			 */

			for (; i > 0; i--) {
			    char *p = label + *offset;
			    if (*p && *p == '.') p++, (*offset)++;
			    while (*p && *p != '.') p++, (*offset)++;
			}
		    }
		}
		ckfree(name);
		return nodePtr;
	    }
	}
	ckfree(name);
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupLabel --
 *
 *	This procedure extracts the first element (head) of the label
 *	argument and recursively searches for a tree node named head.
 *	If found, it checks whether the remaining label elements (tail)
 *	can be resolved from this node.
 *
 * Results:
 *	The pointer to the node or NULL if the node was not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmMibNode*
LookupLabel(root, start, label, moduleName, offset, exact, fuzzy)
    TnmMibNode *root;
    char *start;
    char *label;
    char *moduleName;
    int *offset;
    int exact;
    int fuzzy;
{
    char head[TNM_OID_MAX_SIZE * 8];
    char *tail = label, *p = head;
    TnmMibNode *tp = NULL, *brother;
    int num = 1;

    if (!root) return NULL;

    if (offset) *offset = -1;

    while (*tail && *tail != '.') {
	num = isdigit(*tail) ? num : 0;
	*p++ = *tail++;
    }
    *p = '\0';
    if (*tail == '.') tail++;
    num = num ? atoi(head) : -1;

    for (brother = root; brother; brother = brother->nextPtr) {
        if (((strcmp (head, brother->label) == 0)
	     && (*moduleName == '\0'
		 || strcmp(moduleName, brother->moduleName) == 0))
	    || ((u_int) num == brother->subid)) {
	    if (! *tail) {
		tp = brother;
	    } else if (brother->childPtr) {
		tp = LookupLabel(brother->childPtr, start, tail, 
				 moduleName, offset, exact, 0);
	    } else if (! exact) {
		tp = brother;
	    }
	    if (tp) {
		if (offset && (*offset < tail-start-1) && *offset != -2) {
		    *offset = *tail ? tail-start-1 : -2;
		}
		return tp;
	    }
	}
	if (fuzzy && brother->childPtr) {
	    tp = LookupLabel(brother->childPtr, start, label, 
			     moduleName, offset, exact, 1);
	    if (tp) {
		if (offset && (*offset < tail-start-1) && *offset != -2) {
		    *offset = *tail ? tail-start-1 : -2;
		}
		return tp;
	    }
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibFindNode --
 *
 *	This procedure takes the name of a MIB node and searches for
 *	the corresponding node in the MIB tree. The object identifier
 *	of the node is stored at soid if soid is not NULL. soid must
 *	be large enough to hold the complete path. TnmMibFindNode()
 *	calls LookupOID() if the name is an object identier for fast
 *	MIB searches. Otherwise we call slow LookupLabel() to compare
 *	the tree node labels.
 *
 * Results:
 *	The pointer to the node or NULL if the node was not found.
 *	The offset is set to the subidentifier offset in the name
 *	or -1 if there is no subidentifier left.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMibNode*
TnmMibFindNode(name, offset, exact)
    char *name;
    int *offset;
    int exact;
{
    TnmMibNode *nodePtr = NULL;
    char *expanded;
    char moduleName[255];
    char *p = NULL;
    int dummy, moduleNameLen = 0;
    int extra = 0;

    if (! offset) {
	offset = &dummy;
    }
    *offset = -1;

    *moduleName = '\0';
    p = strchr(name, '!');
    if (!p) {
	p = strchr(name, ':');
	if (p && p[1] != ':') {
	   p = NULL;
	}
    }
    if (!p && isupper(name[0])) {
	p = strchr(name, '.');
    }
    if (p) {
	moduleNameLen = p - name;
	if (moduleNameLen < 255) {
	    strncpy(moduleName, name, (size_t) moduleNameLen);
	    moduleName[moduleNameLen] = '\0';
	} else {
	    strcpy(moduleName, "********");
	}
	name += moduleNameLen + 1;
	if (*name == ':') { name++; extra = 1; }
    }

    expanded = TnmHexToOid(name);
    if (expanded) name = expanded;

    if (TnmIsOid(name)) {
	nodePtr = LookupOID(tnmMibTree, name, offset, exact);
    } else {
	Tcl_HashEntry *entryPtr = NULL;
	if (nodeHashTable) {
	    entryPtr = Tcl_FindHashEntry(nodeHashTable, name);
	}
	if (entryPtr) {
	    nodePtr = (TnmMibNode *) Tcl_GetHashValue(entryPtr);
	}
	if (! nodePtr) {
	    nodePtr = LookupLabelOID(tnmMibTree, name, offset, exact);
	}
	if (! nodePtr) {
	    nodePtr = LookupLabel(tnmMibTree, name, name, moduleName, 
				  offset, exact, 1);
	}
    }

    /*
     * If we have a module name prefix, we must ensure that the node
     * is really the node we were looking for.
     */

    if (nodePtr && nodePtr->moduleName && *moduleName) {
	if (strcmp(moduleName, nodePtr->moduleName) == 0) {
	    if (offset && *offset > 0) {
		*offset += moduleNameLen + 1 + extra;
	    }
	} else {
	    nodePtr = NULL;
	}
    }

    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibAddType --
 *
 *	This procedure adds a TnmMibType structure to the set of
 *	known MIB types which are saved in a hash table. The
 *	TnmMibType structure is also linked into the tnmMibTypeList.
 *
 * Results:
 *	The pointer to the Tnm_MibTC structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMibType*
TnmMibAddType(typePtr)
    TnmMibType *typePtr;
{
    Tcl_HashEntry *entryPtr;
    int isnew;

    if (! typeHashTable) {
	typeHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(typeHashTable, TCL_STRING_KEYS);
    }

    if (! typePtr->moduleName) {
	/* This should not happen! */
	return NULL;
    }
    
    entryPtr = Tcl_CreateHashEntry(typeHashTable, typePtr->name, &isnew);
    
    if (! isnew) {
	return (TnmMibType *) Tcl_GetHashValue(entryPtr);
    }

    typePtr->nextPtr = tnmMibTypeList;
    tnmMibTypeList = typePtr;
    
    Tcl_SetHashValue(entryPtr, (ClientData) typePtr);

    /*
     * Create another entry with a full name. The previous entry
     * is actually not correct because the label itself must not
     * be globally unique.
     */

    {
	Tcl_DString dst;
	Tcl_DStringInit(&dst);
	Tcl_DStringAppend(&dst, typePtr->moduleName, -1);
	Tcl_DStringAppend(&dst, "!", 1);
	Tcl_DStringAppend(&dst, typePtr->name, -1);
	entryPtr = Tcl_CreateHashEntry(typeHashTable, 
				       Tcl_DStringValue(&dst), &isnew);
	if (isnew) {
	    Tcl_SetHashValue(entryPtr, (ClientData) typePtr);
	}
	Tcl_DStringFree(&dst);
    }

    return typePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibFindType --
 *
 *	This procedure searches for a MIB type by name. This procedure
 *	also accepts primitive types and returns a pointer to a static
 *	structure for a primitive type.
 *
 * Results:
 *	The pointer to the TnmMibType structure or NULL if the name
 *	can not be resolved.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */

TnmMibType*
TnmMibFindType(name)
    char *name;
{
    static TnmMibType snmpType;
    Tcl_HashEntry *entryPtr = NULL;
    int syntax;
    
    if (! typeHashTable) {
	return NULL;
    }

    entryPtr = Tcl_FindHashEntry(typeHashTable, name);
    if (entryPtr == NULL) {
	char *label = strchr(name, '!');
	if (label) {
	    entryPtr = Tcl_FindHashEntry(typeHashTable, ++label);
	}
    }

    if (entryPtr) {
	return (TnmMibType *) Tcl_GetHashValue(entryPtr);
    }

    /*
     * Check if we have a primitive type. We fake a TnmMibType
     * structure for primitive types. This should be changed so
     * that primitive types are always known in the type list
     * and the hash tables.
     */

    syntax = TnmGetTableKey(tnmSnmpTypeTable, name);
    if (syntax != -1) {
	memset((char *) &snmpType, 0, sizeof(snmpType));
	snmpType.syntax = syntax;
	snmpType.name = name;
	return &snmpType;
    }

    /*
     * Finally, check whether we have the pseudo type BITS.
     * Fake a type entry for BITS which refers to the underlying
     * OCTET STRING type.
     */

    if (strcmp(name, "BITS") == 0) {
	memset((char *) &snmpType, 0, sizeof(snmpType));
        snmpType.syntax = ASN1_OCTET_STRING;
        snmpType.name = name;
        return &snmpType;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibListTypes --
 *
 *	This procedure assembles a list of all known MIB types.
 *	The pattern, if not NULL, is matched agains the type names.
 *	Only types which match the pattern are listed.
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
TnmMibListTypes(pattern, listPtr)
    char *pattern;
    Tcl_Obj *listPtr;
{
    TnmTable *tablePtr;
    Tcl_HashEntry *entryPtr = NULL;
    Tcl_HashSearch search;

    for (tablePtr = tnmSnmpTypeTable; tablePtr->value; tablePtr++) {
	if (pattern && !Tcl_StringMatch(tablePtr->value, pattern)) {
	    continue;
	}
	Tcl_ListObjAppendElement(NULL, listPtr,
				 Tcl_NewStringObj(tablePtr->value, -1));
    }

    if (typeHashTable) {
	entryPtr = Tcl_FirstHashEntry(typeHashTable, &search);
	while (entryPtr != NULL) {
	    char *typeName = Tcl_GetHashKey(typeHashTable, entryPtr);
	    if (strchr(typeName, '!')) {
		if (!pattern 
		    || (pattern && Tcl_StringMatch(typeName, pattern))) {
		    Tcl_ListObjAppendElement(NULL, listPtr,
					     Tcl_NewStringObj(typeName, -1));
		}
	    }
	    entryPtr = Tcl_NextHashEntry(&search);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibNewNode --
 *
 *	This procedure allocates a new tree element and does some
 *	basic initializations.
 *
 * Results:
 *	The pointer the new TnmMibNode structure is returned.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

TnmMibNode*
TnmMibNewNode(label)
    char *label;
{
    TnmMibNode *nodePtr = (TnmMibNode *) ckalloc(sizeof(TnmMibNode));
    memset((char *) nodePtr, 0, sizeof(TnmMibNode));
    if (label) {
	nodePtr->label = ckstrdup(label);
    }
    nodePtr->syntax = ASN1_OTHER;
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * HashNode --
 *
 *	This procedure maintans a hash table which is used to lookup
 *	MIB nodes by names. This works well as long as the label of
 *	the node is unique. Otherwise, we have to recurse on the tree.
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
HashNode(nodePtr)
    TnmMibNode *nodePtr;
{
    char *name = nodePtr->label;
    Tcl_HashEntry *entryPtr;
    int isnew;
    
    if (! nodeHashTable) {
	nodeHashTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(nodeHashTable, TCL_STRING_KEYS);
    }
    
    entryPtr = Tcl_CreateHashEntry(nodeHashTable, name, &isnew);
    
    if (! isnew) {
	if (nodePtr != (TnmMibNode *) Tcl_GetHashValue(entryPtr)) {

	    /*
	     * leave in hashtable, but mark with NULL ptr:
	     */

	    Tcl_SetHashValue(entryPtr, (ClientData) 0);
	}
	return;
    }

    Tcl_SetHashValue(entryPtr, (ClientData) nodePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * BuildTree --
 *
 *	This procedure builds a MIB tree for the nodes given in the
 *	nodeList parameter.
 *
 * Results:
 *	Returns a pointer to root if successful and NULL otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmMibNode*
BuildTree(nodeList)
    TnmMibNode *nodeList;
{
    TnmMibNode *ccitt, *iso, *joint;
    
    HashNodeList(nodeList);

    /*
     * Initialize the MIB tree with three default nodes: 
     * ccitt(0), iso(1) and joint-iso-ccitt (2).
     */

    ccitt = TnmMibNewNode("ccitt");
    ccitt->parentName = ckstrdup("(unknown)");
    ccitt->syntax = ASN1_OBJECT_IDENTIFIER;
    
    iso = TnmMibNewNode("iso");
    iso->parentName = ckstrdup("(unknown)");
    iso->subid = 1;
    iso->syntax = ASN1_OBJECT_IDENTIFIER;
    ccitt->nextPtr = iso;

    joint = TnmMibNewNode("joint-iso-ccitt");
    joint->parentName = ckstrdup("(unknown)");
    joint->subid = 2;
    joint->syntax = ASN1_OBJECT_IDENTIFIER;
    iso->nextPtr = joint;
    
    /* 
     * Build the tree out of these three nodes.
     */

    BuildSubTree(ccitt);
    BuildSubTree(iso);
    BuildSubTree(joint);

    /*
     * Return the first node in the tree (perhaps "iso" would be
     * better, but this way is consistent)
     */
    
    return ccitt;
}

/*
 *----------------------------------------------------------------------
 *
 * BuildSubTree --
 *
 *	This procedure finds all the children of root in the list of
 *	nodes and links them into the tree and out of the node list.
 *	nodeList parameter.
 *
 * Results:
 *	Returns a pointer to root if successful and NULL otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
BuildSubTree(root)
    TnmMibNode *root;
{
    TnmMibNode **np, **ptr;
    int	hash = HashNodeLabel(root->label);

    /*
     * Loop through all nodes whose parent is root. They are all
     * members of the list which starts at the same hash key.
     */

    for (np = &nodehashtab[hash]; *np != NULL; ) {
	
	if (root->label[0] == ((*np)->parentName)[0]
	    && (strcmp(root->label, (*np)->parentName) == 0)) {

	    TnmMibNode *thisNode = *np;
	    *np = (*np)->nextPtr;

	    thisNode->fileName = tnmMibFileName;

/*** XXX: This should be freed if the data came from the parser but
          it may not be freed if the data came from a frozen file.
          Same below.
	    if (thisNode->parentName) {
		ckfree(thisNode->parentName);
		thisNode->parentName = NULL;
	    }
***/
	    thisNode->parentPtr = root;
	    thisNode->childPtr = NULL;
	    thisNode->nextPtr  = NULL;
	    
	    /* 
	     * Link node in the tree. First skip over all nodes with a
	     * subid less than the new subid. Insert the node if the
	     * node does not already exist. Otherwise free this node.
	     */

	    ptr = &root->childPtr;
	    while (*ptr && (*ptr)->subid < thisNode->subid) {
		ptr = &(*ptr)->nextPtr;
	    }
	    if (*ptr && (*ptr)->subid == thisNode->subid) {
/*** XXX	if (thisNode->label) ckfree((char *) thisNode->label);
		ckfree((char *) thisNode);		***/
	    } else {
		thisNode->nextPtr = *ptr;
                *ptr = thisNode;
		HashNode(thisNode);
	    }

	    BuildSubTree(*ptr);			/* recurse on child */

	} else {
	    np = &(*np)->nextPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibAddNode --
 *
 *	This procedure adds all nodes in nodeList to the MIB tree
 *	given by rootPtr. It first initializes the hash table and
 *	calls BuildSubTree() to move nodes from the hash table into 
 *	the MIB tree.
 *
 * Results:
 *	Returns 0 on success and -1 in case of an error.
 *
 * Side effects:
 *	The nodes are moved from the nodeList into the correct 
 *	position in the MIB tree.
 *
 *----------------------------------------------------------------------
 */

int
TnmMibAddNode(rootPtr, nodeList)
    TnmMibNode **rootPtr;
    TnmMibNode *nodeList;
{
    TnmMibNode *nodePtr;
    TnmMibNode *tree;
    TnmMibNode *root = *rootPtr;
    int i, result = 0;

    if (! nodeList) {
	return 0;
    }

    if (! root) {
	*rootPtr = BuildTree(nodeList);
    }

   /*
    * Test if the parent of the first node exists. We expect that we
    * load individual subtrees where the parent of the first node
    * defines the anchor. This must already exist. Note, the first 
    * node is the last node in our nodeList.
    */

    for (nodePtr = nodeList; nodePtr->nextPtr; nodePtr = nodePtr->nextPtr) ;
    tree = TnmMibFindNode(nodePtr->parentName, NULL, 1);
    HashNodeList(nodeList);
    if (tree) {
	BuildSubTree(tree);
    }
    
   /*
    * Now the slow part: If there are any nodes left in the hash table,
    * we either have two isolated subtrees or the last node in nodeList
    * was not the root of our tree. We scan for nodes which have known
    * parents in the existing tree and call BuildSubTree for those
    * parents.
    *
    * Perhaps we should check for candidates in the nodelist, as this
    * would allow some local scope. XXX
    */

  repeat:
    for (i = 0; i < NODEHASHSIZE; i++) {
	for (nodePtr = nodehashtab[i]; nodePtr; nodePtr = nodePtr->nextPtr) {
	    tree = TnmMibFindNode(nodePtr->parentName, NULL, 1);
	    if (tree) {
		BuildSubTree(tree);
		goto repeat;
	    }
	}
    }

    /* 
     * Finally print out all the node labels that are still in the
     * hashtable. This should allow people to fix the ordering of
     * MIB load commands.
     */
    
    for (i = 0; i < NODEHASHSIZE; i++) {
	for (nodePtr = nodehashtab[i]; nodePtr; nodePtr = nodePtr->nextPtr) {
	    fprintf(stderr, "%s: no parent %s for node %s\n", 
		    tnmMibFileName, nodePtr->parentName, nodePtr->label);
	    result = -1;
	}
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * HashNodeList --
 *
 *	This procedure moves the nodes from the nodeList parameter
 *	into the node hash table. The hash function is computed
 *	upon the parent names so that all nodes with the same parent 
 *	are in one collision list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The nodes are moved from the nodeList into lists sorted by
 *	the name of the parent node.
 *
 *----------------------------------------------------------------------
 */

static void
HashNodeList(nodeList)
    TnmMibNode *nodeList;
{
    int	hash;
    TnmMibNode *nodePtr, *nextp;

    memset((char *) nodehashtab, 0, sizeof(nodehashtab));

    for (nodePtr = nodeList; nodePtr != NULL; ) {
	if (! nodePtr->parentName) {
	    fprintf(stderr, "%s: %s has no parent in the MIB tree!\n",
		    tnmMibFileName, nodePtr->label);
	    return;
	}

	hash = HashNodeLabel(nodePtr->parentName);

	nextp = nodePtr->nextPtr;
	nodePtr->nextPtr = nodehashtab[hash];
	nodehashtab[hash] = nodePtr;
	nodePtr = nextp;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * HashNodeLabel --
 *
 *	This procedure computes the hash function used to hash
 *	MIB nodes by parent name.
 *
 * Results:
 *	The hash value of the given label.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
HashNodeLabel(label)
    char *label;
{
    int hash = 0;
    char *cp;
    
    for (cp = label; *cp; cp++) {
	hash += *cp;
    }

    return (hash % NODEHASHSIZE);
}

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * End:
 */
