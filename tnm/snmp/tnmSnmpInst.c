/*
 * tnmSnmpInst.c --
 *
 *	Utilities to organize the tree of SNMP nodes maintained by snmp
 *	session to keep track of session bindings and agent instances.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpInst.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * The root of the tree containing all MIB instances.
 */

static TnmSnmpNode *instTree = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
DumpTree                _ANSI_ARGS_((TnmSnmpNode *instPtr));

static void
FreeNode		_ANSI_ARGS_((TnmSnmpNode *inst));

static TnmSnmpNode*
AddNode			_ANSI_ARGS_((char *id, int offset, int syntax,
				     int access, char *tclVarName));
static void
RemoveNode		_ANSI_ARGS_((TnmSnmpNode *root, char *varname));

static TnmSnmpNode*
FindNode		_ANSI_ARGS_((TnmSnmpNode *root, TnmOid *oidPtr));

static TnmSnmpNode*
FindNextNode		_ANSI_ARGS_((TnmSnmpNode *root, u_int *oid, int len));

static char*
DeleteNodeProc		_ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
				     char *name1, char *name2, int flags));


/*
 *----------------------------------------------------------------------
 *
 * DumpTree --
 *
 *      This procedure dump the whole instance tree structure. It is
 *      only used to debug the instance tree processing code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
DumpTree(instPtr)
    TnmSnmpNode *instPtr;
{
    if (instPtr) {
        fprintf(stderr, "** %s (%s)\n",
                instPtr->label ? instPtr->label : "(none)",
                TnmGetTableValue(tnmMibAccessTable,
				 (unsigned) instPtr->access));
        if (instPtr->childPtr) {
            DumpTree(instPtr->childPtr);
        }
        if (instPtr->nextPtr) {
            DumpTree(instPtr->nextPtr);
        }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FreeNode --
 *
 *	This procedure frees an instance node and all associated
 *	resources.
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
FreeNode(instPtr)
    TnmSnmpNode *instPtr;
{

    if (instPtr->label) {
	ckfree(instPtr->label);
    }
    if (instPtr->tclVarName) {
	ckfree(instPtr->tclVarName);
    }
    while (instPtr->bindings) {
	TnmSnmpBinding *bindPtr = instPtr->bindings;
	instPtr->bindings = instPtr->bindings->nextPtr;
	if (bindPtr->command) {
	    ckfree(bindPtr->command);
	}
	ckfree((char *) bindPtr);
    }
    ckfree((char *) instPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AddNode --
 *
 *	This procedure adds a new instance node to the tree 
 *	of instances.
 *
 * Results:
 *	A pointer to the new node or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpNode*
AddNode(soid, offset, syntax, access, tclVarName)
    char *soid;
    int offset;
    int syntax;
    int access;
    char *tclVarName;
{
    Tnm_Oid *oid;
    int i, oidlen;
    TnmSnmpNode *p, *q = NULL;

    if (instTree == NULL) {
	instTree = (TnmSnmpNode *) ckalloc(sizeof(TnmSnmpNode));
	memset((char *) instTree, 0, sizeof(TnmSnmpNode));
	instTree->label = "1";
	instTree->subid = 1;
    }

    oid = TnmStrToOid(soid, &oidlen);
    if (oid[0] != 1 || oidlen < 1) {
	return NULL;
    }
    if (oidlen == 1 && oid[0] == 1) {
        return instTree;
    }

    for (p = instTree, i = 1; i < oidlen; p = q, i++) {
	for (q = p->childPtr; q; q = q->nextPtr) {
	    if (q->subid == oid[i]) break;
	}
	if (! q) {

	    /*
	     * Create new intermediate nodes.
	     */

	    TnmSnmpNode *n;
	    char *s = TnmOidToStr(oid, i+1);
	    
	    n = (TnmSnmpNode *) ckalloc(sizeof(TnmSnmpNode));
	    memset((char *) n, 0, sizeof(TnmSnmpNode));
	    n->label = ckstrdup(s);
	    n->subid = oid[i];
	    n->offset = offset;

	    if (! p->childPtr) {			/* first node  */
		p->childPtr = n;

	    } else if (p->childPtr->subid > oid[i]) {	/* insert head */
		n->nextPtr = p->childPtr;
		p->childPtr = n;

	    } else {					/* somewhere else */
		for (q = p->childPtr; q->nextPtr && q->nextPtr->subid < oid[i]; 
		     q = q->nextPtr) ;
		if (q->nextPtr && q->nextPtr->subid == oid[i]) {
		    continue;
		}
		n->nextPtr = q->nextPtr;
		q->nextPtr = n;
	    }

	    q = n;
	}
    }

    if (q) {
	if (q->label) ckfree(q->label);
	if (q->tclVarName && q->tclVarName != tclVarName) {
	    ckfree(q->tclVarName);
	}
	
	q->label  = soid;
	q->offset = offset;
	q->syntax = syntax;
	q->access = access;
	q->tclVarName = tclVarName;
    }
  
    return q;
}

/*
 *----------------------------------------------------------------------
 *
 * FindNextNode --
 *
 *	This procedure locates the lexikographic next instance
 *	node in the instance tree.
 *
 * Results:
 *	A pointer to the node or NULL if there is no next node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpNode*
FindNextNode(root, oid, len)
    TnmSnmpNode *root;
    u_int *oid;
    int len;
{
    TnmSnmpNode *p, *inst;
    static int force = 0;

    /*
     * Reset the force flag if we start a new search from the root of
     * the instance tree. The flag will be set whenever we decide that
     * the next instance we find will be a good candidate.
     */

    if (root == instTree) {
	force = 0;
    }

    /*
     * Skip over all subidentifier that are lower than the
     * subidentifier of interest.
     */

    p = root;
    if (len) {
	while (p && p->subid < oid[0]) p = p->nextPtr;
    }

    /*
     * Loop over all neighbours at this tree level. Descend if possible.
     */

    while (p) {
	if (p->childPtr) {
	    if (len > 0 && p->subid == oid[0]) {
		/* descend - positive match */
		inst = FindNextNode(p->childPtr, oid + 1, len - 1);
	    } else if (p->syntax) {
		/* no match - use this node */
		return p;
	    } else {
		/* descend - force next node */
		force = 1;
		inst = FindNextNode(p->childPtr, NULL, 0);
	    }
	    if (inst) return inst;
	} else {
	    if (len == 0 && p->syntax) {
		/* found - node has larger oid */
		return p;
	    } else if (((len && p->subid != oid[0]) || force) && p->syntax) {
		/* no match - forced to use this node */
		return p;
	    } else {
		/* force next node */
		force = 1;
	    }
	}
	p = p->nextPtr;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FindNode --
 *
 *	This procedure locates an instance node in the instance
 *	node tree.
 *
 * Results:
 *	A pointer to the node or NULL if there is no next node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpNode*
FindNode(root, oidPtr)
    TnmSnmpNode *root;
    TnmOid *oidPtr;
{
    TnmSnmpNode *p, *q = NULL;
    int i;
    
    if (TnmOidGet(oidPtr, 0) != 1) return NULL;
    for (p = root, i = 1; p && i < TnmOidGetLength(oidPtr); p = q, i++) {
	for (q = p->childPtr; q; q = q->nextPtr) {
	    if (q->subid == TnmOidGet(oidPtr, i)) break;
	}
	if (!q) {
	    return NULL; 
	}
    }
    return q;
}

/*
 *----------------------------------------------------------------------
 *
 * RemoveNode --
 *
 *	This procedure removes all nodes from the tree that are 
 *	associated with a given Tcl variable.
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
RemoveNode(root, varName)
    TnmSnmpNode *root;
    char *varName;
{
    TnmSnmpNode *p, *q;

    if (!root) return;

    for (p = root; p; p = p->nextPtr) {
	if (p->childPtr) {
	    q = p->childPtr;
	    RemoveNode(q, varName);
	    if (q->tclVarName && (strcmp(q->tclVarName, varName) == 0)) {
		p->childPtr = q->nextPtr;
		FreeNode(q);
	    }
	}
	if (p->nextPtr) {
	    q = p->nextPtr;
	    if (q->tclVarName && (strcmp(q->tclVarName, varName) == 0)) {
		p->nextPtr = q->nextPtr;
		FreeNode(q);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteNodeProc --
 *
 *	This procedure is a variable trace callback which is called
 *	by the Tcl interpreter whenever a MIB variable is removed.
 *	We have to run through the whole tree to discard these 
 *	variables.
 *
 * Results:
 *	Always NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char*
DeleteNodeProc(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
    size_t len = strlen(name1);
    char *varName;
			 
    if (name2) {
	len += strlen(name2);
    }
    varName = ckalloc(len + 3);
    strcpy(varName, name1);
    if (name2) {
	strcat(varName,"(");
	strcat(varName, name2);
	strcat(varName,")");
    }

    RemoveNode(instTree, varName);
    ckfree(varName);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpCreateNode --
 *
 *	This procedure creates a new node in the instance tree 
 *	and a Tcl array variable that will be used to access and 
 *	modify the instance from within Tcl.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
 
int
TnmSnmpCreateNode(interp, label, tclVarName, defval)
    Tcl_Interp *interp;
    char *label;
    char *tclVarName;
    char *defval;
{
    char *soid = NULL;
    TnmMibNode *nodePtr = TnmMibFindNode(label, NULL, 0);
    int access, offset = 0, syntax = 0;
    char *varName = NULL;

    if (!nodePtr || nodePtr->childPtr) {
	Tcl_AppendResult(interp, "unknown object type \"", label, "\"", 
			 (char *) NULL);
	return TCL_ERROR;
    }

    soid = ckstrdup(TnmMibGetOid(label));

    if (! TnmIsOid(soid)) {
	Tcl_AppendResult(interp, "illegal instance identifier \"",
			 soid, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Calculate the instance identifier. Return an error if we
     * have no real instance. Otherwise save a pointer to the
     * instance identifier so that we can access it later.
     */

    {
	int oidLen;
	Tnm_Oid *oid;
	TnmMibNode *basePtr = NULL;
	char *freeme = NULL;

	for (oid = TnmStrToOid(soid, &oidLen); oidLen; oidLen--) {
	    freeme = TnmOidToStr(oid, oidLen);
	    basePtr = TnmMibFindNode(freeme, NULL, 1);
	    if (basePtr) break;
	}

	if (! basePtr || strlen(soid) <= strlen(freeme)) {
	    Tcl_AppendResult(interp, "instance identifier missing in \"",
			     label, "\"", (char *) NULL);
	    return TCL_ERROR;
	}

	if (freeme) {
	    offset = strlen(freeme)+1;
	}
    }

    syntax = TnmMibGetBaseSyntax(label);

    /*
     * Check whether the instance is accessible.
     */

    access = nodePtr->access;
    if (access == TNM_MIB_NOACCESS) {
	Tcl_AppendResult(interp, "object \"", label, "\" is not accessible",
			 (char *) NULL);
	goto errorExit;
    }

    /*
     * Check if the instance identifier is "0" for scalars. We have
     * a scalar if the parent node is not of ASN.1 syntax SEQUENCE.
     *
     * XXX We should also check if the instance identifier is valid
     * for table entries.
     */

    if (! nodePtr->parentPtr) {
	Tcl_AppendResult(interp, "instance \"", label, "\" not allowed",
			 (char *) NULL);
	goto errorExit;
    }

    if (nodePtr->parentPtr && nodePtr->parentPtr->syntax != ASN1_SEQUENCE) {
	if (strcmp(soid + offset, "0") != 0) {
	    Tcl_AppendResult(interp, "illegal instance identifier \"",
			     soid + offset, "\" for instance \"", label, "\"",
			     (char *) NULL);
	    goto errorExit;
	}
    }

    /*
     * Now create the Tcl variable and the instance tree node.
     * Do not use tclVarName directly because it might be a string
     * which is not writable and Tcl likes to modify this string
     * internally.
     */

    varName = ckstrdup(tclVarName);

    if (defval) {
	if (Tcl_SetVar(interp, varName, defval, 
		       TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
	    goto errorExit;
	}
    }

    AddNode(soid, offset, syntax, access, varName);
    Tcl_TraceVar(interp, varName, TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY, 
		 DeleteNodeProc, (ClientData) NULL);
    Tcl_ResetResult(interp);
    return TCL_OK;

  errorExit:
    if (soid) ckfree(soid);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpFindNode --
 *
 *	This procedure locates the instance node for the given oid
 *	in the instance node tree.
 *
 * Results:
 *	A pointer to the node or NULL if there is no next node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmSnmpNode*
TnmSnmpFindNode(session, oidPtr)
    TnmSnmp *session;
    TnmOid *oidPtr;
{
    return FindNode(instTree, oidPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpFindNextNode --
 *
 *	This procedure locates the next instance for the given oid
 *	in the instance tree.
 *
 * Results:
 *	A pointer to the node or NULL if there is no next node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmSnmpNode*
TnmSnmpFindNextNode(session, oidPtr)
    TnmSnmp *session;
    TnmOid *oidPtr;
{
#if 0
    DumpTree(instTree);
#endif
    return FindNextNode(instTree, TnmOidGetElements(oidPtr), 
			TnmOidGetLength(oidPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpSetNodeBinding --
 *
 *	This procedure creates and modifies SNMP event bindings
 *	for a specific MIB node.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpSetNodeBinding(session, oidPtr, event, command)
    TnmSnmp *session;
    TnmOid *oidPtr;
    int event;
    char *command;
{
    TnmSnmpNode *node = NULL;
    TnmSnmpBinding *bindPtr = NULL;

    /*
     * Create an anonymous node if there is no instance known yet.
     */
	
    node = FindNode(instTree, oidPtr);
    if (!node) {
	node = AddNode(ckstrdup(TnmOidToString(oidPtr)), 0, 0, 0, NULL);
	if (! node) {
	    return TCL_ERROR;
	}
    }

    /*
     * Check if we already have a binding for this event type.
     */

    for (bindPtr = node->bindings; bindPtr; bindPtr = bindPtr->nextPtr) {
	if (bindPtr->event == event) break;
    }

    /*
     * Create a new binding if necessary. Overwrite already
     * existing bindings.
     */

    if (command) {
	if (!bindPtr) {
	    bindPtr = (TnmSnmpBinding *) ckalloc(sizeof(TnmSnmpBinding));
	    memset((char *) bindPtr, 0, sizeof(TnmSnmpBinding));
	    bindPtr->event = event;
	    bindPtr->nextPtr = node->bindings;
	    node->bindings = bindPtr;
	}
	if (bindPtr->command) {
	    ckfree(bindPtr->command);
	    bindPtr->command = NULL;
	}
	if (*command != '\0') {
	    bindPtr->command = ckstrdup(command);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpGetNodeBinding --
 *
 *	This procedure retrieves the current binding for the given
 *	MIB node and event type.
 *
 * Results:
 *	A pointer to the command bound to this event or NULL if 
 *	there is either no node of if there is no binding for 
 *	the node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmSnmpGetNodeBinding(session, oidPtr, event)
    TnmSnmp *session;
    TnmOid *oidPtr;
    int event;
{
    TnmSnmpNode *node = NULL;
    TnmSnmpBinding *bindPtr = NULL;

    node = FindNode(instTree, oidPtr);
    if (! node) {
	return NULL;
    }

    for (bindPtr = node->bindings; bindPtr; bindPtr = bindPtr->nextPtr) {
	if (bindPtr->event == event) break;
    }
    if (! bindPtr) {
	return NULL;
    }

    return bindPtr->command;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpEvalNodeBinding --
 *
 *	This procedure evaluates a binding for a given node. We
 *	start at the given node and follow the path to the top of 
 *	the instance tree. We evaluate all bindings during our walk
 *	up the tree. A break return code can be used to stop this 
 *	process.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpEvalNodeBinding(session, pdu, inst, event, value, oldValue)
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmSnmpNode *inst;
    int event;
    char *value;
    char *oldValue;
{
    TnmOid oid;
    int code = TCL_OK, len;
    char *instOid;

    TnmOidInit(&oid);
    TnmOidFromString(&oid, inst->label);

    /*
     * Make a copy the string from the instance variable because the
     * commit binding might delete the instance which will cause a
     * dangling pointer.
     */

    instOid = ckstrdup(inst->label+inst->offset);

    for (len = TnmOidGetLength(&oid); len > 0; len--) {
	TnmSnmpBinding *bindPtr;

	TnmOidSetLength(&oid, len);
	inst = FindNode(instTree, &oid);
	if (!inst) continue;

	for (bindPtr = inst->bindings; bindPtr; bindPtr = bindPtr->nextPtr) {
	    if (bindPtr->event == event) break;
	}

	if (bindPtr && bindPtr->command) {

	    /*
	     * Evaluate the binding and check if the instance is still
	     * there after we complete the binding. It may have been
	     * deleted in the callback. Also make sure that we have an
	     * error status of TNM_SNMP_NOERROR and error index 0 during the
	     * callback.
	     */

	    int errorStatus = pdu->errorStatus;
	    int errorIndex  = pdu->errorIndex;
	    pdu->errorStatus = TNM_SNMP_NOERROR;
	    pdu->errorIndex  = 0;
	    code = TnmSnmpEvalCallback(session->interp, session,
				       pdu, bindPtr->command,
				       inst->label, instOid, 
				       value, oldValue);
	    pdu->errorStatus = errorStatus;
	    pdu->errorIndex  = errorIndex;

	    if (code == TCL_OK && !FindNode(instTree, &oid)) {
	        code = TCL_ERROR;
	    }
	    if (code == TCL_BREAK || code == TCL_ERROR) break;
	}
    }
    ckfree(instOid);
    TnmOidFree(&oid);

    return code;
}

