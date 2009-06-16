/*
 * tnmMibParser.c --
 *
 *	Functions to parse MIB files for creating a MIB tree. The 
 *	structure of MIB information must meet the definitions specified 
 *	in RFC 1155, RFC 1212, RFC 1215, RFC 2578, RFC 2579, RFC 2580.
 *	This code is partly derived from the MIB parser of the CMU
 *	SNMP implementation, but it now looks quite different as we
 *	have made many changes and rewritten large parts of it. Please
 *	read the Copyright notice of the CMU source.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMibParser.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

#define USE_RANGES 1

#define SYMBOL_MAXLEN 128	/* Maximum # characters in a symbol. */

/*
 * Token used by the MIB parser.
 */

#ifdef ERROR
#undef ERROR
#endif

#define ERROR		-2

#define DEFINITIONS	51
#define EQUALS		52
#define BEGIN		53
#define IMPORTS		54
#define EXPORTS		55
#define END		57
#define COMMENT		58
#define LABEL		59
#define	CONTINUE	60

#define SYNTAX		70

#define LEFTBRACKET	80
#define RIGHTBRACKET	81
#define	LEFTPAREN	82
#define RIGHTPAREN	83
#define COMMA		84
#define SEMICOLON	85
#define UPTO		86
#define OR		87

#define ACCESS		90
#define READONLY	91
#define READWRITE	92
#define READCREATE	93
#define	WRITEONLY	94
#define FORNOTIFY	95
#define NOACCESS	96

#define STATUS		100
#define MANDATORY	101
 
#ifdef OPTIONAL
#undef OPTIONAL
#endif
 
#define OPTIONAL	102
#define OBSOLETE	103
#define CURRENT		104
#define DEPRECATED	105

#define OBJGROUP	110
#define OBJECTS		111
#define OBJECTIDENTITY	112

#define MODULECOMP	120
#define REFERENCE	121

#define MODULEIDENTITY	130
#define LASTUPDATED	153
#define ORGANIZATION	154
#define CONTACTINFO	155

#define NOTIFYTYPE	156
#define NOTIFYGROUP	157
#define NOTIFICATIONS   188

#define TRAPTYPE	158

#define	CAPABILITIES	159
#define	FROM		159

#define TEXTUALCONV	160
#define DISPLAYHINT	161
#define ENTERPRISE	162
#define VARIABLES	163

#define OBJTYPE		186
#define PUNCT		127
#define NUMBER		129
#define DESCRIPTION	135
#define QUOTESTRING	136
#define INDEX		137
#define DEFVAL		138
#define IMPLIED		139
#define SIZE		140
#define BINVALUE	141
#define HEXVALUE	142
#define QUOTEVALUE	143
#define AUGMENTS	146
#define UNITS		149
#define NUM_ENTRIES	151
#define SIGNEDNUMBER	152

#define	HASHTAB_SIZE	17			/* size of hash table */
#define	HASH(x)		(x % HASHTAB_SIZE)	/* Hash function      */

#define NODEHASHSIZE	127			/* hashsize for nodes */

#define MAXTC		128

#define IN_MIB		1
#define OUT_OF_MIB	2

/*
 * Structure definition to hold the keyword table:
 */

typedef struct Keyword {
   char			*name;		/* keyword name			    */
   int			key;		/* value			    */
   int			hash;		/* hash of name			    */
   struct Keyword	*nextPtr;	/* pointer to next in hash table    */
} Keyword;

static Keyword keywords[] =
{
   { "DEFINITIONS",		DEFINITIONS,		0 },
   { "BEGIN",			BEGIN,			0 },
   { "END",			END,			0 },
   { "IMPORTS",			IMPORTS,		0 },
   { "FROM",			FROM,			0 },
   { "EXPORTS",			EXPORTS,		0 },
   { "::=",			EQUALS,			0 },

   { "{",			LEFTBRACKET,		0 },
   { "}",			RIGHTBRACKET,		0 },
   { "(",			LEFTPAREN,		0 },
   { ")",			RIGHTPAREN,		0 },
   { ",",			COMMA,			0 },
   { ";",			SEMICOLON,		0 },
   { ".",			CONTINUE,		0 },
   { "..",			UPTO,			0 },
   { "|",			OR,			0 },

   { "SYNTAX",			SYNTAX,			0 },

   { "INTEGER",			ASN1_INTEGER,		0 },
   { "OCTETSTRING",		ASN1_OCTET_STRING,	0 },
   { "OCTET",			CONTINUE,		0 },
   { "OBJECTIDENTIFIER",	ASN1_OBJECT_IDENTIFIER,	0 },
   { "OBJECT",			CONTINUE,		0 },
   { "BITS",			ASN1_OCTET_STRING,	0 },
   { "NULL",			ASN1_NULL,		0 },
   { "SEQUENCE",		ASN1_SEQUENCE,		0 },
   { "OF",			ASN1_SEQUENCE_OF,	0 },
   { "Integer32",		ASN1_INTEGER,		0 },
   { "IpAddress",		ASN1_IPADDRESS,		0 },
   { "NetworkAddress",		ASN1_IPADDRESS,		0 },
   { "Counter",			ASN1_COUNTER32,		0 },
   { "Counter32",		ASN1_COUNTER32,		0 },
   { "Gauge",			ASN1_GAUGE32,		0 },
   { "Gauge32",			ASN1_GAUGE32,		0 },
   { "TimeTicks",		ASN1_TIMETICKS,		0 },
   { "Opaque",			ASN1_OPAQUE,		0 },
   { "Counter64",		ASN1_COUNTER64,		0 },
   { "Unsigned32",		ASN1_GAUGE32,		0 },

   { "ACCESS",			ACCESS,			0 },
   { "read-only",		READONLY,		0 },
   { "read-write",		READWRITE,		0 },
   { "read-create",		READCREATE,		0 },
   { "write-only",		WRITEONLY,		0 },
   { "accessible-for-notify",   FORNOTIFY,		0 },
   { "not-accessible",		NOACCESS,		0 },

   { "STATUS",			STATUS,			0 },
   { "mandatory",		MANDATORY,		0 },
   { "optional",		OPTIONAL,		0 },
   { "obsolete",		OBSOLETE,		0 },
   { "current",			CURRENT,		0 },
   { "deprecated",		DEPRECATED,		0 },

   { "DESCRIPTION",		DESCRIPTION,		0 },

   { "OBJECT-GROUP",		OBJGROUP,		0 },
   { "OBJECTS",			OBJECTS,		0 },
   { "OBJECT-TYPE",		OBJTYPE,		0 },

   { "OBJECT-IDENTITY",		OBJECTIDENTITY,		0 },

   { "MODULE-COMPLIANCE",	MODULECOMP,		0 },
   { "REFERENCE",		REFERENCE,		0 },

   { "MODULE-IDENTITY",		MODULEIDENTITY,		0 },
   { "LAST-UPDATED",		LASTUPDATED,		0 },
   { "ORGANIZATION",		ORGANIZATION,		0 },
   { "CONTACT-INFO",		CONTACTINFO,		0 },

   { "NOTIFICATION-TYPE",	NOTIFYTYPE,		0 },
   { "NOTIFICATION-GROUP",	NOTIFYGROUP,		0 },
   { "NOTIFICATIONS",		NOTIFICATIONS,		0 },

   { "TRAP-TYPE",		TRAPTYPE,		0 },

   { "AGENT-CAPABILITIES",	CAPABILITIES,		0 },

   { "TEXTUAL-CONVENTION",	TEXTUALCONV,		0 },
   { "DISPLAY-HINT",		DISPLAYHINT,		0 },

   { "ENTERPRISE",		ENTERPRISE,		0 },
   { "VARIABLES",		VARIABLES,		0 },

   { "AUGMENTS",		AUGMENTS,		0 },
   { "UNITS",			UNITS,			0 },
   { "NUM-ENTRIES",		NUM_ENTRIES,		0 },
   { "INDEX",			INDEX,			0 },
   { "IMPLIED",			IMPLIED,		0 },
   { "DEFVAL",			DEFVAL,			0 },
   { "SIZE",			SIZE,			0 },
   { "MAX-ACCESS",		ACCESS,			0 },
   { NULL }
};

struct subid {			/* Element of an object identifier          */
   char		*parentName;	/* with an integer subidentifier or         */
   char		*label;		/* a textual string label, or both.         */
   int		subid;
   struct subid	*next;
};

/*
 * Some other useful module global variables:
 */

static int lastchar = ' ';	/* The last read character. */
static int line = 1;		/* The current line number. */

static Keyword *hashtab[HASHTAB_SIZE];

/*
 * A faster strcmp(). Note, some compiler optimize strcmp() et.al.
 * themself, so this is not always the best solution.
 */

#define fstrcmp(a,b)  ((a)[0] != (b)[0] || (a)[1] != (b)[1] || \
			strcmp((a), (b)))


/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AddNewNode		_ANSI_ARGS_((TnmMibNode **nodeList, char *label, 
				     char *parentName, u_int subid));
static TnmMibRest*
ScanIntEnums		_ANSI_ARGS_((char *str));

static TnmMibRest*
ScanRange		_ANSI_ARGS_((char *str));

static int
ReadIntEnums		_ANSI_ARGS_((FILE *fp, char **strPtr));

static int
ReadRange		_ANSI_ARGS_((FILE *fp, char **strPtr));

static char*
ReadNameList		_ANSI_ARGS_((FILE *fp));

static TnmMibType*
CreateType		_ANSI_ARGS_((char *name, int syntax, char *displayHint,
				     char *enums));
static TnmMibNode*
ParseFile		_ANSI_ARGS_((FILE *fp));

static int
ParseHeader		_ANSI_ARGS_((FILE *fp, char *keyword));

static int
ParseASN1Type		_ANSI_ARGS_((FILE *fp, char *keyword));

static TnmMibNode*
ParseModuleCompliance	_ANSI_ARGS_((FILE *fp, char *name, 
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseModuleIdentity	_ANSI_ARGS_((FILE *fp, char *name, 
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseNotificationType	_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseNotificationGroup	_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseCapabilitiesType	_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseTrapType		_ANSI_ARGS_((FILE *fp, char *name, 
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseObjectGroup	_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseObjectIdentity	_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseObjectID		_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static TnmMibNode*
ParseObjectType		_ANSI_ARGS_((FILE *fp, char *name,
				     TnmMibNode **nodeList));
static int
ParseNodeList		_ANSI_ARGS_((FILE *fp, TnmMibNode **nodeList,
				     TnmMibNode *nodePtr));
static void
HashKeywords		_ANSI_ARGS_((void));

static int
ReadKeyword		_ANSI_ARGS_((FILE *fp, char *keyword));

static struct subid *
ReadSubID		_ANSI_ARGS_((FILE *fp));


/*
 * Create a new node and link it into the list.
 */

static void
AddNewNode(nodeList, label, parentName, subid)
     TnmMibNode **nodeList;
     char *label;
     char *parentName;
     u_int subid;
{
    TnmMibNode *newPtr = TnmMibNewNode(label);
    newPtr->parentName = ckstrdup(parentName);
    newPtr->moduleName = tnmMibModuleName;
    newPtr->syntax = ASN1_OTHER;
    newPtr->subid = subid;
    newPtr->nextPtr = *nodeList;
    *nodeList = newPtr;
}

/*
 * TnmMibParse() opens a MIB file specified by file and returns a 
 * tree of objects in that MIB. The function returns a pointer to 
 * the "root" of the MIB, or NULL if an error occurred.
 */

char*
TnmMibParse(file, frozen)
    char *file;
    char *frozen;
{
    FILE *fp;		/* The current FILE pointer. */
    TnmMibNode *nodePtr = NULL;
    Tcl_StatBuf stbuf;
    time_t mib_mtime = 0, frozen_mtime = 0;
    Tcl_Obj *obj = NULL;

    tnmMibFileName = ckstrdup(file);
    obj = Tcl_NewStringObj(file, -1);
    Tcl_IncrRefCount(obj);
    if (Tcl_FSStat(obj, &stbuf) == 0) {
	mib_mtime = stbuf.st_mtime;
    }
    Tcl_DecrRefCount(obj);

    if (! frozen || mib_mtime == 0 || frozen_mtime == 0 || frozen_mtime < mib_mtime) {
	fp = fopen(file, "rb");
	if (fp == NULL) {
	    return NULL;
	}
	/* save pointer to still known tt's: */
	tnmMibTypeSaveMark = tnmMibTypeList;
	nodePtr = ParseFile(fp);
	fclose(fp);
	if (frozen) {
	    if (nodePtr == NULL && tnmMibTypeList == tnmMibTypeSaveMark) {
		unlink(frozen);
		return NULL;
	    } else {
		fp = fopen(frozen, "wb");
		if (fp != NULL) {
		    TnmMibWriteFrozen(fp, nodePtr);
				fclose(fp);
		}
	    }
	}
    } else {
	nodePtr = NULL;
	fp = fopen(frozen, "rb");
	if (fp) {
	    nodePtr = TnmMibReadFrozen(fp);
	    fclose(fp);
	}
    }

    if (TnmMibAddNode(&tnmMibTree, nodePtr) == -1) {
	unlink(frozen);
	return NULL;
    }

    if (nodePtr) {
	return nodePtr->moduleName;
    } else if (tnmMibTypeList != tnmMibTypeSaveMark) {
	return tnmMibTypeList->moduleName;
    }
    
    return NULL;
}


/*
 * ScanIntEnums() converts a string containing pairs of labels and integer
 * values into a chained list of MIB_IntEnum's.
 * NOTE: this puts \0 into the passed string.
 */

static TnmMibRest*
ScanIntEnums(str)
    char *str;
{
    int done = 0;
    TnmMibRest *enumList = NULL, **enumPtr = &enumList;

    if (! str) return NULL;

    if (strncmp (str, "D ", 2) != 0) return NULL;

    str += 2;

    while (*str) {

	char *val, *desc = str;

	while (*str && isspace (*str))
	  str++;
	while (*str && ! isspace (*str))
	  str++;
	if (! *str) break;
	*str++ = 0;
	val = str;
	while (*str && ! isspace (*str))
	  str++;
	if (! *str)
	  done = 1;
	else
	  *str++ = 0;

	*enumPtr = (TnmMibRest *) ckalloc(sizeof(TnmMibRest));
	(*enumPtr)->rest.intEnum.enumValue = atoi(val);
	(*enumPtr)->rest.intEnum.enumLabel = desc;
 	(*enumPtr)->nextPtr = 0;
	enumPtr = &(*enumPtr)->nextPtr;

	if (done) break;
    }

    return enumList;
}


/*
 * ScanRange() converts a string containing range
 * values into a chained list of MIB_Range's.
 * NOTE: this puts \0 into the passed string.
 */

static TnmMibRest*
ScanRange (str)
    char *str;
{
    TnmMibRest *rangeList = NULL;
    int base = 0;
    
    if (! str) return NULL;

    if (strncmp (str, "R ", 2) != 0) return NULL;

    str += 2;
    
    while (*str) {
	char *end, *start = str;
	TnmMibRest *newSubRange = NULL,
	    **currRangePtr = &rangeList,
	    **prevRangePtr = &rangeList;

	end = start;

	while (*str && isspace(*str))
	    str++;
	start = str;
	
	while (*str && (*str != '.' ) && !isspace(*str))
	    str++;
	if (*str)
	    *str++ = 0;
	
	if ( *str == '.' ) {
	    str++;
	    end = str;
	    while (*str && !isspace(*str))
		str++;
	    if (*str)
		*str++ = 0;
	}

	newSubRange = (TnmMibRest *) ckalloc(sizeof(TnmMibRest));
	/* Should this code be put into Parse?? */
	if ( toupper( *start ) == 'B' ) {
	    base = 2;
	    start++;
	} else
	    base = 0;
	newSubRange->rest.intRange.min = strtol(start, NULL, base);
	if ( toupper( *end ) == 'B' ) {
	    base = 2;
	    end++;
	} else
	    base = 0;
	newSubRange->rest.intRange.max = strtol(end, NULL, base);
 	newSubRange->nextPtr = NULL;

	while (((*currRangePtr) != NULL) &&
	       (newSubRange->rest.intRange.max 
		> (*currRangePtr)->rest.intRange.min)) {
	    prevRangePtr = currRangePtr;
	    currRangePtr = &(*currRangePtr)->nextPtr;
	}
	newSubRange->nextPtr = *currRangePtr;
	*currRangePtr = newSubRange;
    }

    return rangeList;
}


static TnmMibType*
CreateType(name, syntax, displayHint, enums)
    char *name;
    int syntax;
    char *displayHint;
    char *enums;
{
    TnmMibType *typePtr = TnmMibFindType(name);

    if (typePtr) {
	return typePtr;
    }

    typePtr = (TnmMibType *) ckalloc(sizeof(TnmMibType));
    memset((char *) typePtr, 0, sizeof(TnmMibType));
    
    if (name) {
	typePtr->name = ckstrdup(name);
    }
    typePtr->fileName = tnmMibFileName;
    typePtr->moduleName = tnmMibModuleName;
    typePtr->syntax = syntax;
    typePtr->macro = TNM_MIB_TEXTUALCONVENTION;
    if (displayHint) {
	typePtr->displayHint = ckstrdup(displayHint);
    }
    if (enums) {
	if (strncmp(enums, "D ", 2) == 0) {
	    typePtr->restKind = TNM_MIB_REST_ENUMS;
	    typePtr->restList = ScanIntEnums(ckstrdup(enums));
	} else if (strncmp(enums, "R ", 2) == 0) {
	    if (syntax == ASN1_OCTET_STRING
		|| syntax == ASN1_OBJECT_IDENTIFIER) {
		typePtr->restKind = TNM_MIB_REST_SIZE;
	    } else {
		typePtr->restKind = TNM_MIB_REST_RANGE;
	    }
	    typePtr->restList = ScanRange(ckstrdup(enums));
	} else {
	    typePtr->restKind = TNM_MIB_REST_NONE;
	}
    }
    
    return TnmMibAddType(typePtr);
}


/*
 * ReadIntEnums() scans a enum description, like 
 * ``INTEGER { foo(1), bar(2) }'', with the next token to be read 
 * the one after the ``{''; upon return the closing ``}'' is read.
 * the scanned enums are return in the passed dstring, in the form:
 * ``D foo 3 bar 4''
 */

static int
ReadIntEnums(fp, strPtr)
    FILE *fp;
    char **strPtr;
{
    Tcl_DString result;
    int syntax;
    int fail = 0;		/* parser thinks anything as alright */

    Tcl_DStringInit(&result);
    
    /* mark we append Descriptions: */
    Tcl_DStringAppend(&result, "D", 1);
    
    /* got LEFTBRACKET:  ``{'' */
    do {
	char str [SYMBOL_MAXLEN];
	char num [SYMBOL_MAXLEN];
	char keyword [SYMBOL_MAXLEN];

	syntax = ReadKeyword(fp, str);
#if 0
/** XXX: dont check - all we need is a string */
	if (syntax != LABEL) { fail = 1; break; }
#endif
	/* got the string:  ``{ foo'' */
	syntax = ReadKeyword(fp, keyword);
	if (syntax != LEFTPAREN) { fail = 1; break; }
	/* ``{ foo ('' */
	syntax = ReadKeyword(fp, num);
	if (syntax != NUMBER && syntax != SIGNEDNUMBER) { fail = 1; break; }
	/* append to the collecting string: */
	Tcl_DStringAppend(&result, " ", 1);
	Tcl_DStringAppend(&result, str, -1);
	Tcl_DStringAppend(&result, " ", 1);
	Tcl_DStringAppend(&result, num, -1);
	/* ``{ foo (99'' */
        syntax = ReadKeyword(fp, keyword);
	if (syntax != RIGHTPAREN) { fail = 1; break; }
	/* ``{ foo (99)'' */
	/* now there must follow either a ``,'' or a ``}'' */
        syntax = ReadKeyword(fp, keyword);

    } while (syntax == COMMA);
    
    /* the list is scanned, now there must be a ``}'' */
    
    if (fail || syntax != RIGHTBRACKET) {
	fprintf(stderr, "%s:%d: Warning: can not scan enums - ignored\n",
		tnmMibFileName, line);
    }

    *strPtr = ckstrdup(Tcl_DStringValue(&result));
    Tcl_DStringFree(&result);

    return syntax;
}


/*
 * ReadRange() scans a sub-range description, like 
 * ``INTEGER (1..10|15|20..30)'', with the next token to be read 
 * the one after the ``(''; upon return the closing ``)'' is read.
 * the scanned sub-ranges are return in the passed dstring, in the form:
 * ``R 1..10 15 20..30''
 */

static int
ReadRange(fp, strPtr)
    FILE *fp;
    char **strPtr;
{
    Tcl_DString result;
    int syntax;
    int fail = 0;		/* parser thinks anything as alright */

    char value [SYMBOL_MAXLEN];
    char start [SYMBOL_MAXLEN];
    char end [SYMBOL_MAXLEN];
    char keyword [SYMBOL_MAXLEN];

    Tcl_DStringInit(&result);
    
    /* mark we append Ranges: */
    Tcl_DStringAppend(&result, "R", 1);
    
    /* got LEFTPAREN:  ``('' */
    do {
	syntax = ReadKeyword(fp, value);

	switch (syntax) {
	case NUMBER:
	case SIGNEDNUMBER:
	    strcpy(start, value);
	    break;
	case HEXVALUE:
	    strcpy(start, "0x");
	    strcat(start, value);
	    break;
	case BINVALUE:
	    strcpy( start, "B" );
	    strcat(start, value);
	    break;
	default:
	    fail = 1;
	    goto out;
	}
	/* got the string:  ``( 1'' */
	/* now there must follow either a ``|'', a ``..'', or a ``)'' */
	syntax = ReadKeyword(fp, keyword);
	if (syntax == UPTO) {
	    /* ``( 1..'' */
	    syntax = ReadKeyword(fp, value);
	    
	    switch (syntax) {
	    case NUMBER:
	    case SIGNEDNUMBER:
		strcpy(end, value);
		break;
	    case HEXVALUE:
		strcpy(end, "0x");
		strcat(end, value);
		break;
	    case BINVALUE:
		strcpy(start, "B");
		strcat(start, value);
		break;
	    default:
		fail = 1;
		goto out;
	    }
	    /* got the string:  ``( 1..10'' */
	    /* now there must follow either a ``|'' or a ``)'' */
	    syntax = ReadKeyword(fp, keyword);
	} else {
	    /* just a single number, not a range like ``1..10'' */
	    *end = 0;
	}
	/* append to the collecting string: */
	Tcl_DStringAppend(&result, " ", 1);
	Tcl_DStringAppend(&result, start, -1);
	if ( *end != 0 ) {
	    Tcl_DStringAppend(&result, "..", 2);
	    Tcl_DStringAppend(&result, end, -1);
	}
    out:
	; /* syntactic sugar */
    } while (syntax == OR);
    
    /* the list is scanned, now there must be a ``)'' */
    
    if (fail || syntax != RIGHTPAREN) {
	fprintf(stderr, "%s:%d: Warning: can not scan range - ignored\n",
		tnmMibFileName, line);
    }
    
    *strPtr = ckstrdup(Tcl_DStringValue(&result));
    Tcl_DStringFree(&result);

    return syntax;
}


/*
 */

static char*
ReadNameList(fp)
    FILE *fp;
{
    int syntax;
    Tcl_DString dst;
    char keyword[SYMBOL_MAXLEN];
    char *result;
    
    if ((syntax = ReadKeyword(fp, keyword)) != LEFTBRACKET) {
	return NULL;
    }

    Tcl_DStringInit(&dst);
    while ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET) {
	switch (syntax) {
	case COMMA:
	    continue;
	case LABEL:
	    Tcl_DStringAppendElement(&dst, keyword);
	    break;
	default:
	    Tcl_DStringFree(&dst);
	    return NULL;
	}
    }
    
    result = ckstrdup(Tcl_DStringValue(&dst));
    Tcl_DStringFree(&dst);
    
    return result;
}


/*
 * ParseFile() reads a MIB file specified by *fp and return a
 * linked list of objects in that MIB.
 */

static TnmMibNode*
ParseFile (fp)
     FILE *fp;
{
    char name[SYMBOL_MAXLEN];
    char keyword[SYMBOL_MAXLEN];

    int	state  = OUT_OF_MIB;
    int	syntax = 0;

    TnmMibNode *nodePtr = NULL;
    TnmMibNode *nodeList = NULL;
    TnmMibNode *lastOTPtr = NULL;

    /* 
     * this is really a klude: try to fetch type-definitions
     * like: ``Boolean ::= INTEGER { true(1), false(2) }''
     * and: ``KBytes ::= INTEGER''.
     */

    char tt_name[SYMBOL_MAXLEN];
    TnmMibType *typePtr = NULL;
    /*
     * init hashtable and parse mib 
     */

    HashKeywords();
    line = 1;

    while ((syntax = ReadKeyword(fp, keyword)) != EOF) {
	
	if (state == OUT_OF_MIB) {

	    /*
	     * we are at the beginning of a mib
	     */
	
	    switch (syntax) {
	      case DEFINITIONS:
		state = IN_MIB;
		syntax = ParseHeader(fp, name);
		if (syntax == EOF || syntax == ERROR) {
		    fprintf(stderr, "%s:%d: bad format in MIB header\n",
			    tnmMibFileName, line);
		    return NULL;
		}

                /* maybe a type: */
                strncpy (tt_name, name, SYMBOL_MAXLEN);

		break;
	      case END:
		fprintf(stderr, "%s: end before start of MIB.\n", 
			tnmMibFileName);
		return NULL;
	      case ERROR:
		fprintf(stderr, "%s:%d: error in MIB\n", 
			tnmMibFileName, line);
		return NULL;
	      case LABEL:
		strncpy (name, keyword, SYMBOL_MAXLEN);
		break;
	      case LEFTBRACKET:
		{ int cnt = 1;
		  for (;;) {
		      char buf [SYMBOL_MAXLEN];
		      int syntax;
		      if ((syntax = ReadKeyword(fp, buf)) == LEFTBRACKET)
			cnt ++;
		      else if (syntax == RIGHTBRACKET)
			cnt--;
		      if (! cnt)
			break;
		  }
	        }
		break;
	      default:
		fprintf(stderr, "%s:%d: %s is a reserved word\n", 
			tnmMibFileName, line, keyword);
		return NULL;
	    }

	} else {

	    /*
	     * we are in a mib
	     */
	
	    switch (syntax) {
	      case DEFINITIONS:
		fprintf(stderr, "%s: Fatal: nested MIBS\n", tnmMibFileName);
		return NULL;
	      case END:
		tnmMibModuleName = NULL;
		state = OUT_OF_MIB;
		break;
	      case EQUALS:
		syntax = ParseASN1Type (fp, name);
		if (syntax == END) {
		    tnmMibModuleName = NULL;
		    state = OUT_OF_MIB;
		}

		/*
		 * Accept more primitive types than required by the
		 * SNMPv2 specs because there are a great number of
		 * private MIBs out there that use these types.
		 * Suggested by David Keeney <keeney@zk3.dec.com>.
		 */
	
		if (syntax == ASN1_INTEGER
		    || syntax == ASN1_OCTET_STRING 
		    || syntax == ASN1_OBJECT_IDENTIFIER
		    || syntax == ASN1_IPADDRESS
		    || syntax == ASN1_COUNTER32
		    || syntax == ASN1_GAUGE32
		    || syntax == ASN1_TIMETICKS
		    || syntax == ASN1_OPAQUE
		    || syntax == ASN1_COUNTER64)
		  {
		    /* sure a type; if one with the same name already
		     * exists (eg. DisplayString in rfc1903.tc
		     * (wanted) and then in rfc1213.mib (unwanted))
		     * ignore and use the existing one (but this may
		     * hurt -- you have been warned) */
		      
		      typePtr = CreateType(tt_name, syntax, 0, 0);
		      typePtr->macro = TNM_MIB_TYPE_ASSIGNMENT;
		  } else if (syntax == ASN1_SEQUENCE 
			     && lastOTPtr && lastOTPtr->syntax == LABEL) {
		      lastOTPtr->syntax = syntax;
		  }
		break;
	      case ERROR:
		fprintf(stderr, "%s:%d: error in MIB\n", 
			tnmMibFileName, line);
		return NULL;
	      case LABEL:
		strncpy (name, keyword, SYMBOL_MAXLEN);
		/* maybe a type: */
		strncpy (tt_name, keyword, SYMBOL_MAXLEN);
		break;
	      case MODULECOMP:
		nodePtr = ParseModuleCompliance(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in MODULE-COMPLIANCE\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_COMPLIANCE;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case MODULEIDENTITY:
		nodePtr = ParseModuleIdentity(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in MODULE-IDENTIY\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_MODULEIDENTITY;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case NOTIFYTYPE:
		nodePtr = ParseNotificationType(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in NOTIFICATION-TYPE\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_NOTIFICATIONTYPE;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case NOTIFYGROUP:
		nodePtr = ParseNotificationGroup(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in NOTIFICATION-GROUP\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_NOTIFICATIONGROUP;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case CAPABILITIES:
		nodePtr = ParseCapabilitiesType(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr, 
			    "%s:%d: bad format in AGENT-CAPABILITIES\n",
			    tnmMibFileName, line);
                    return NULL;
		}
		nodePtr->macro = TNM_MIB_CAPABILITIES;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case TRAPTYPE:
		nodePtr = ParseTrapType(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in TRAP-TYPE\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_TRAPTYPE;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case OBJGROUP:
		nodePtr = ParseObjectGroup(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in OBJECT-GROUP\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_OBJECTGROUP;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case OBJECTIDENTITY:
		nodePtr = ParseObjectIdentity(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in OBJECT-IDENTITY\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_OBJECTIDENTITY;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		break;
	      case ASN1_OBJECT_IDENTIFIER:
		nodePtr = ParseObjectID(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in OBJECT-IDENTIFIER\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->nextPtr = nodeList;
		nodePtr->macro = TNM_MIB_VALUE_ASSIGNEMENT;
		nodePtr->moduleName = tnmMibModuleName;
		nodeList = nodePtr;
		break;
	      case OBJTYPE:
		nodePtr = ParseObjectType(fp, name, &nodeList);
		if (nodePtr == NULL) {
		    fprintf(stderr,
			    "%s:%d: bad format in OBJECT-TYPE\n",
			    tnmMibFileName, line);
		    return NULL;
		}
		nodePtr->macro = TNM_MIB_OBJECTTYPE;
		nodePtr->moduleName = tnmMibModuleName;
		nodePtr->nextPtr = nodeList;
		nodeList = nodePtr;
		/* save for SEQUENCE hack: */
		lastOTPtr = nodePtr;
		break;
		
		/* 
		 * expect a leftparen; eg. for ``MacAddress ::= OCTET
		 * STRING (SIZE (6))''; action is to skip over. 
		 */
	      case LEFTPAREN:
#ifdef USE_RANGES
		if (typePtr &&
		    ((typePtr->syntax != ASN1_OCTET_STRING) ||
		     ((typePtr->syntax == ASN1_OCTET_STRING) &&
		      ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		      (syntax == SIZE) &&
		      ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		      (syntax == LEFTPAREN)))) {
		   char *ranges;

		   if ((ReadRange(fp, &ranges) != RIGHTPAREN) ||
		       (typePtr &&
		        ((typePtr->syntax == ASN1_OCTET_STRING) &&
		         (((syntax = ReadKeyword(fp, keyword)) == EOF) ||
		          (syntax != RIGHTPAREN))))) {
			ckfree (ranges);
		   } else if (typePtr) {
			typePtr->restKind = TNM_MIB_REST_RANGE;
			typePtr->restList = ScanRange(ranges);
			if (! typePtr->restList) {
			    fprintf(stderr, "%s:%d: bad range definition1\n",
				    tnmMibFileName, line);
			}
		   }
		} else {
		    fprintf(stderr, "%s:%d: bad range definition\n",
			    tnmMibFileName, line);
		}
 		break;
#else
		{ int cnt = 1;
		  for (;;) {
		      char buf [SYMBOL_MAXLEN];
		      int syntax;
		      if ((syntax = ReadKeyword(fp, buf)) == LEFTPAREN)
			cnt ++;
		      else if (syntax == RIGHTPAREN)
			cnt--;
		      if (! cnt)
			break;
		  }
	        }
		break;
#endif

	      case LEFTBRACKET:
		{ 
		    char *enums;
		    if (ReadIntEnums(fp, &enums) != RIGHTBRACKET) {
			fprintf(stderr, "%s:%d: bad mib format\n",
				tnmMibFileName, line);
			ckfree (enums);
		    } else if (typePtr) {
			typePtr->restKind = TNM_MIB_REST_ENUMS;
			typePtr->restList = ScanIntEnums(enums);
		    }
	        }
		break;

	      default:
		fprintf(stderr, "%s:%d: bad mib format\n", 
			tnmMibFileName, line);
		return NULL;
	    }
	}
    }
    
    /*
     * check if we finished correctly
     */

    if (state == OUT_OF_MIB) {
	return nodeList;
    }

    fprintf(stderr, "%s: Fatal: incomplete MIB module\n", tnmMibFileName);
    return NULL;
}


/*
 * ParseHeader() parses a MIB header and places the LABEL that
 * follows in the string pointed to by keyword. Returns EOF if no end
 * of MIB header found, ERROR if no LABEL found and LABEL if MIB
 * header is passed and a label placed into keyword.
 */

static int
ParseHeader (fp, keyword)
    FILE *fp;
    char *keyword;
{
    int syntax;

    tnmMibModuleName = ckstrdup(keyword);
   
    if ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	return ERROR;
    }

    if ((syntax = ReadKeyword(fp, keyword)) != BEGIN) {
	return ERROR;
    }

    syntax = ReadKeyword(fp, keyword);

    /*
     * if it's EXPORTS clause, read the next keyword after SEMICOLON
     */

    if (syntax == EXPORTS) {
	while ((syntax = ReadKeyword(fp, keyword)) != SEMICOLON) {
	    if (syntax == EOF) return EOF;
	}
	syntax = ReadKeyword(fp, keyword);
    }

    
    /*
     * if it's IMPORTS clause, read the next keyword after SEMICOLON
     */

    if (syntax == IMPORTS) {
	while ((syntax = ReadKeyword(fp, keyword)) != SEMICOLON) {
	    switch (syntax) {
	    case FROM:
		syntax = ReadKeyword(fp, keyword);
		if (syntax == EOF) return EOF;
		if (syntax != LABEL) return ERROR;
		/* fprintf(stderr, " module %s\n", keyword); */
#if 0
		{
		    int i, objc, code;
		    Tcl_Obj **objv;
		    
		    code = Tcl_ListObjGetElements(NULL, tnmMibModulesLoaded,
						  &objc, &objv);
		    if (code != TCL_OK) {
			Tcl_Panic("currupted internal list tnmMibModulesLoaded");
		    }
    
		    for (i = 0; i < objc; i++) {
			char *s = Tcl_GetStringFromObj(objv[i], NULL);
			if (strcmp(keyword, s) == 0) {
			    i = -1;
			    break;
			}	
		    }
		    if (i != -1) {
			fprintf(stderr, "unknown module %s imported from %s\n",
				keyword, tnmMibModuleName);
		    }
		}
#endif
		break;
	    case COMMA:
		break;
	    case LABEL:
		/* fprintf(stderr, " %s", keyword); */
		break;
	    case EOF:
		return EOF;
	    default:
		break;
	    }
	}
	syntax = ReadKeyword(fp, keyword);
    }

    /*
     * return syntax (should be label or defined label (e.g. DisplayString))
     */

    return syntax;
}


/*
 * Parses an ASN1 syntax and places the LABEL that follows in the
 * string pointed to by keyword. Returns 0 on error, LABEL on success.  
 */

static int
ParseASN1Type (fp, keyword)
     FILE *fp;
     char *keyword;
{
#ifndef USE_RANGES
    int level = 0;
#endif
    int syntax = 0, merk, osyntax, offset = 0, status = 0;
    char name [SYMBOL_MAXLEN];
    char otype [SYMBOL_MAXLEN];
    char convention [SYMBOL_MAXLEN];
    char *displayHint = NULL;
    char *enums = NULL;
    
    /* save passed name: */
    strcpy (name, keyword);
    
    syntax = ReadKeyword(fp, keyword);

    /*
     * Accept more primitive types than required by the
     * SNMPv2 specs because there are a great number of
     * private MIBs out there that use these types.
     * Suggested by David Keeney <keeney@zk3.dec.com>.
     */

    switch (syntax) {
    case ASN1_INTEGER:
    case ASN1_OCTET_STRING:
    case ASN1_OBJECT_IDENTIFIER:
    case ASN1_IPADDRESS:    /* KEENEY - 11/7/95 */
    case ASN1_COUNTER32:    /* KEENEY - 11/7/95 */
    case ASN1_GAUGE32:      /* KEENEY - 11/7/95 */
    case ASN1_TIMETICKS:    /* KEENEY - 11/7/95 */
    case ASN1_OPAQUE:       /* KEENEY - 11/7/95 */
    case ASN1_COUNTER64:    /* KEENEY - 11/7/95 */
	
	break;
    case ASN1_SEQUENCE:
	while ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET)
	    if (syntax == EOF) return 0;
	syntax = ASN1_SEQUENCE;
	break;
    case TEXTUALCONV:
	
	/* default: no convention/enums seen: */
	convention [0] = 0;
	
	while ((syntax = ReadKeyword(fp, keyword)) != SYNTAX
	       && syntax != DISPLAYHINT) {
	    switch (syntax) {
	    case STATUS:
		syntax = ReadKeyword(fp, keyword);
		if (syntax != CURRENT
		    && syntax != OBSOLETE && syntax != DEPRECATED) {
		    fprintf(stderr, "%s:%d: scan error near `%s'\n", 
			    tnmMibFileName, line, keyword);
		    return 0;
		}
		status = TnmGetTableKey(tnmMibStatusTable, keyword);
		break;
	    case DESCRIPTION:
		offset = ftell(fp);
		if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		    return 0;
		}
		break;
	    case EOF:
		return 0;
	    }
	}
	
	/*
	 * read the keyword following SYNTAX or DISPLAYHINT
	 */
	
	merk = ReadKeyword(fp, keyword);
	/* ugh. and yet another ugly hack to this ugly parser... */
	if (syntax == SYNTAX && merk == LABEL)
	{
	    TnmMibType *newTypePtr, *typePtr = TnmMibFindType(keyword);
	    if (typePtr) {
		newTypePtr = CreateType(name, typePtr->syntax, 0, 0);
		newTypePtr->displayHint = typePtr->displayHint;
		newTypePtr->restKind = typePtr->restKind;
		newTypePtr->restList = typePtr->restList;
		newTypePtr->fileOffset = offset;
		newTypePtr->status = status;
		return typePtr->syntax;
	    }
	    /* alas, we are (hopefully) done. */
	    return 0;
	}
	
	if (syntax == DISPLAYHINT)
	{
	    /* save convention */
	    strcpy (convention, keyword);
	    
	    /* skip to SYNTAX: */
	    while ((syntax = ReadKeyword(fp, keyword)) != SYNTAX) {
		switch (syntax) {
		case STATUS:
		    syntax = ReadKeyword(fp, keyword);
		    if (syntax != CURRENT
			&& syntax != OBSOLETE && syntax != DEPRECATED) {
			fprintf(stderr, "%s:%d: scan error near `%s'\n", 
				tnmMibFileName, line, keyword);
			return 0;
		    }
		    status = TnmGetTableKey(tnmMibStatusTable, keyword);
		    break;
		case DESCRIPTION:
		    offset = ftell(fp);
		    if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
			return 0;
		    }
		    break;
		case EOF:
		    return 0;
		}
	    }
	    
	    if ((merk = ReadKeyword(fp, keyword)) == LABEL)
		return 0;
	}
	
	/* XXX */
	/* save object type: */
	strcpy (otype, keyword);
 	osyntax = merk;
	
	/*
	 * if next keyword is a bracket, we have to continue
	 */
	
	if ((syntax = ReadKeyword(fp, keyword)) == LEFTPAREN) {
#ifdef USE_RANGES
	    if ((osyntax != ASN1_OCTET_STRING) ||
		((osyntax == ASN1_OCTET_STRING) &&
		 ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		 (syntax == SIZE) &&
		 ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		 (syntax == LEFTPAREN))) {
		if ((ReadRange(fp, &enums) != RIGHTPAREN) ||
		    ((osyntax == ASN1_OCTET_STRING) &&
		     (((syntax = ReadKeyword(fp, keyword)) == EOF) ||
		      (syntax != RIGHTPAREN)))) {
		    fprintf(stderr, "%s:%d: bad range definition\n",
		            tnmMibFileName, line);
		    ckfree(enums);
		}
	    } else {
		fprintf(stderr, "%s:%d: bad range definition\n",
			tnmMibFileName, line);
	    }
#else
	    level = 1;
	    while (level != 0) {
		if ((syntax = ReadKeyword(fp, keyword)) == EOF)
		    return 0;
		if (syntax == LEFTPAREN)
		    ++level;
		if (syntax == RIGHTPAREN)
		    --level;
	    }
	    syntax = ReadKeyword(fp, keyword);
#endif
	}
	
	if (syntax == LEFTBRACKET) {
	    syntax = ReadIntEnums(fp, &enums);
	}
	
	/* found MIB_TextConv: */
	
	if (convention && *convention) {
	    displayHint = convention;
	}
	if (enums && *enums == '\0') {
	    ckfree (enums);
	    enums = NULL;
	}

	{
	    TnmMibType *typePtr = CreateType(name, osyntax, 
					     displayHint, enums);
	    typePtr->fileOffset = offset;
	    typePtr->status = status;
	}

	if (enums) {
	    ckfree (enums);
	    enums = NULL;
	}
	
	break;
      default:
	{ TnmMibType *typePtr = TnmMibFindType(keyword);
	  if (typePtr) {
	      TnmMibType *newTypePtr;
	      newTypePtr = CreateType(name, typePtr->syntax, 0, 0);
	      newTypePtr->displayHint = typePtr->displayHint;
	      newTypePtr->restKind = typePtr->restKind;
	      newTypePtr->restList = typePtr->restList;
	      newTypePtr->fileOffset = offset;
	      newTypePtr->status = status;
	      return typePtr->syntax;
	  } else {
	      fprintf(stderr, "%s:%d: Warning: unknown syntax \"%s\"\n",
		      tnmMibFileName, line, keyword);
	      return 0;
	  }
        }
    }
    
    return syntax;
}


/*
 * ParseModuleCompliance() parses a MODULE-COMPLIANCE macro.
 * Returns NULL if an error was detected.
 */

static TnmMibNode*
ParseModuleCompliance (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int syntax;
    TnmMibNode *nodePtr;

    nodePtr = TnmMibNewNode(name);
    
    /*
     * read keywords until syntax EQUALS is found
     */
    
    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	case DESCRIPTION:
	    if (nodePtr->fileOffset <= 0) {
		nodePtr->fileOffset = ftell (fp);
		if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		    fprintf(stderr, "%d --> %s\n", syntax, keyword);
		    return NULL;
		}
	    }
	    break;
	case EOF:
	    return NULL;
	default:
	    break;
	}
    }

    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return nodePtr;
}


/*
 * ParseModuleIdentity() parses a MODULE-IDENTITY macro.
 */

static TnmMibNode*
ParseModuleIdentity (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;
    
    nodePtr = TnmMibNewNode(name);

    /*
     * read keywords until syntax EQUALS is found
     */

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case DESCRIPTION:
	      if (nodePtr->fileOffset <= 0) {
		  nodePtr->fileOffset = ftell(fp);
		  if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		      fprintf(stderr, "%d --> %s\n", syntax, keyword);
		      return NULL;
		  }
	      }
            break;
	  case EOF:
            return NULL;
	  default:
            break;
	}
    }

    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }

    return nodePtr;
}


/*
 * ParseNotificationType() parses a NOTIFICATION-TYPE macro.
 */

static TnmMibNode*
ParseNotificationType (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;

    nodePtr = TnmMibNewNode(name);

    /*
     * read keywords until syntax EQUALS is found
     */

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case STATUS:
	    syntax = ReadKeyword(fp, keyword);
	    if (syntax != CURRENT
		&& syntax != OBSOLETE && syntax != DEPRECATED) {
		fprintf(stderr, "%s:%d: scan error near `%s'\n", 
			tnmMibFileName, line, keyword);
		return NULL;
	    }
	    nodePtr->status = TnmGetTableKey(tnmMibStatusTable, keyword);
	    break;
	case OBJECTS:
	    nodePtr->index = ReadNameList(fp);
	    if (! nodePtr->index) {
		return NULL;
	    }
	    break;
	  case DESCRIPTION:
            nodePtr->fileOffset = ftell (fp);
            if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
            }
            break;
	  case EOF:
            return NULL;
	  default:
            break;
	}
    }
    
    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return nodePtr;
}


/*
 * ParseCapabilitiesType() parses or better ignores agent 
 * capabilities macros.
 */

static TnmMibNode*
ParseCapabilitiesType (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;

    nodePtr = TnmMibNewNode(name);

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
          case DESCRIPTION:
            nodePtr->fileOffset = ftell (fp);
            if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
            }
            break;
	  case EOF:
            return 0;
	  default:
            break;
	}
    }

    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return nodePtr;
}


/*
 * ParseTrapType() parses a TRAP-TYPE macro.
 */

static TnmMibNode*
ParseTrapType (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int  syntax, bracket = 0;
    TnmMibNode *nodePtr;
    char *enterprise = NULL;

    nodePtr = TnmMibNewNode(name);

    /*
     * read keywords until syntax EQUALS is found
     */

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	case DESCRIPTION:
	    nodePtr->fileOffset = ftell (fp);
	    if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
	    }
            break;
	case VARIABLES:
	    nodePtr->index = ReadNameList(fp);
	    if (! nodePtr->index) {
		return NULL;
	    }
	    break;
	case ENTERPRISE:
	    syntax = ReadKeyword(fp, keyword);
	    if (syntax == LEFTBRACKET) {
		bracket = 1;
		syntax = ReadKeyword(fp, keyword);
	    }
	    if (syntax != LABEL) {
		fprintf(stderr, "%s:%d: unable to parse ENTERPRISE %s\n",
			tnmMibFileName, line, keyword);
		return NULL;
	    }
	    enterprise = ckstrdup(keyword);
#if 1
	    /*
	     * This is a special hack. It is known to be buggy, but it
	     * works for most cases. (The rest will be done if people
	     * complain about it or when I have to rewrite this
	     * parser.) We check if we already have a node below the
	     * enterprise node which has the sub/identifier value 0.
	     * If not, we assume that we are converting a SNMPv1 MIB
	     * into a SNMPv2 MIB and that we have to create a new
	     * dummy MIB node for the subidentifier 0.
	     */

	    {
		TnmMibNode *n;
		for (n = *nodeList; n; n = n->nextPtr) {
		    if (n->subid == 0 
			&& strcmp(n->parentName, keyword) == 0) break;
		}
		if (n) {
		    nodePtr->parentName = ckstrdup(n->label);
		} else {
		    nodePtr->parentName = ckalloc(strlen(enterprise) + 8);
		    strcpy(nodePtr->parentName, enterprise);
		    strcat(nodePtr->parentName, "Traps");
		}
	    }
#endif
	    if (bracket) {
		syntax = ReadKeyword(fp, keyword);
		if (syntax != RIGHTBRACKET) {
		    fprintf(stderr, "%s:%d: expected bracket but got %s\n",
			    tnmMibFileName, line, keyword);
		    return NULL;
		}
	    }
	    break;
	case EOF:
	    return NULL;
	default:
	    break;
	}
    }

    /*
     * parse a number defining the trap number */

    syntax = ReadKeyword(fp, keyword);
    if (syntax != NUMBER || enterprise == NULL) {
	return NULL;
    }

    /*
     * TRAP-TYPE macro is complete. Add a new node with subidentifier 0
     * and a node for the trap type itself (see RFC 1908).
     */

    AddNewNode (nodeList, nodePtr->parentName, enterprise, 0);
    nodePtr->subid = atoi(keyword);
    return nodePtr;
}


/*
 * ParseObjectGroup() parses an OBJECT-GROUP macro.
 */

static TnmMibNode*
ParseObjectGroup (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;

    /*
     * next keyword must be OBJECTS
     */
    
    if ((syntax = ReadKeyword(fp, keyword)) != OBJECTS)
	return NULL;
    
    nodePtr = TnmMibNewNode(name);
    
    nodePtr->index = ReadNameList(fp);
    if (! nodePtr->index) {
	return NULL;
    }

    /*
     * read keywords until EQUALS are found
     */
    
    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case STATUS:
	    syntax = ReadKeyword(fp, keyword);
	    if (syntax != CURRENT
		&& syntax != OBSOLETE && syntax != DEPRECATED) {
		fprintf(stderr, "%s:%d: scan error near `%s'\n", 
			tnmMibFileName, line, keyword);
		return NULL;
	    }
	    nodePtr->status = TnmGetTableKey(tnmMibStatusTable, keyword);
	    break;
	  case DESCRIPTION:
	    nodePtr->fileOffset = ftell (fp);
	    if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
	    }
	    break;
	  case EOF:
	    return NULL;
	  default:
	    break;
	}
    }
    
    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }

    return nodePtr;
}




/*
 * ParseNotificationGroup() parses an NOTIFICATION-GROUP macro.
 */

static TnmMibNode*
ParseNotificationGroup (fp, name, nodeList)
    FILE *fp;
    char *name;
    TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;

    /*
     * next keyword must be NOTIFICATIONS
     */
    
    if ((syntax = ReadKeyword(fp, keyword)) != NOTIFICATIONS) {
	return NULL;
    }

    nodePtr = TnmMibNewNode(name);

    nodePtr->index = ReadNameList(fp);
    if (! nodePtr->index) {
	return NULL;
    }

    /*
     * read keywords until EQUALS are found
     */
    
    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case STATUS:
	    syntax = ReadKeyword(fp, keyword);
	    if (syntax != CURRENT
		&& syntax != OBSOLETE && syntax != DEPRECATED) {
		fprintf(stderr, "%s:%d: scan error near `%s'\n", 
			tnmMibFileName, line, keyword);
		return NULL;
	    }
	    nodePtr->status = TnmGetTableKey(tnmMibStatusTable, keyword);
	    break;
	  case DESCRIPTION:
	    nodePtr->fileOffset = ftell (fp);
	    if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
	    }
	    break;
	  case EOF:
	    return NULL;
	  default:
	    break;
	}
    }
    
    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return nodePtr;
}




/*
 * ParseObjectIdentity() parses an OBJECT-IDENTITY macro.
 */

static TnmMibNode*
ParseObjectIdentity (fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;
    
    nodePtr = TnmMibNewNode(name);

    /*
     * read keywords until EQUALS are found
     */

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case STATUS:
            syntax = ReadKeyword(fp, keyword);
            if (syntax != CURRENT
		&& syntax != OBSOLETE && syntax != DEPRECATED) {
		fprintf(stderr, "%s:%d: scan error near `%s'\n", 
			tnmMibFileName, line, keyword);
		return NULL;
            }
	    nodePtr->status = TnmGetTableKey(tnmMibStatusTable, keyword);
            break;
          case DESCRIPTION:
            nodePtr->fileOffset = ftell (fp);
            if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		fprintf(stderr, "%d --> %s\n", syntax, keyword);
		return NULL;
            }
            break;
	  case EOF:
            return NULL;
	  default:
            break;
	}
    }
    
    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }

    return nodePtr;
}




/*
 * ParseObjectID() parses an OBJECT-IDENTIFIER entry.
 */

static TnmMibNode*
ParseObjectID(fp, name, nodeList)
     FILE *fp;
     char *name;
     TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    TnmMibNode *nodePtr;

    /*
     * next keyword must be EQUALS
     */

    if ((syntax = ReadKeyword(fp, keyword)) != EQUALS)
      return NULL;
    
    nodePtr = TnmMibNewNode(name);
    nodePtr->syntax = ASN1_OTHER;
    
    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return (nodePtr);
}



/*
 * ParseObjectType() parses an OBJECT-TYPE macro.
 */

static TnmMibNode*
ParseObjectType (fp, name, nodeList)
    FILE *fp;
    char *name;
    TnmMibNode **nodeList;
{
    char keyword[SYMBOL_MAXLEN];
    int	syntax;
    size_t len;
    TnmMibNode *nodePtr;
    Tcl_DString dst;

    /*
     * next keyword must be SYNTAX
     */

    if ((syntax = ReadKeyword(fp, keyword)) != SYNTAX)
      return NULL;
    
    nodePtr = TnmMibNewNode(name);
    nodePtr->fileOffset = -1;

    /*
     * next keyword defines OBECT-TYPE syntax
     */

    if ((syntax = ReadKeyword(fp, keyword)) == ACCESS)
      return NULL;
    
    nodePtr->syntax = syntax;
    
    /*
     * no read keywords until ACCESS is found
     */

    /*
     * if the syntax found is LABEL, we should lookup the typedef:
     */
    
    if (syntax == LABEL) {

        nodePtr->typePtr = TnmMibFindType(keyword);
	if (nodePtr->typePtr) {
	    nodePtr->syntax = nodePtr->typePtr->syntax;
	} else {
	    nodePtr->syntax = ASN1_SEQUENCE;
#if 0
	    fprintf(stderr, "%s:%d: Warning: unknown syntax \"%s\"\n",
		    tnmMibFileName, line, keyword);
#endif
	}

	/* 
	 * old eat-it-up code: skip anything to the ACCESS keyword: 
	 */
	
	while ((syntax = ReadKeyword(fp, keyword)) != ACCESS)
	  if (syntax == EOF)
	    return NULL;

    } else if (syntax == ASN1_INTEGER || syntax == ASN1_OCTET_STRING) {

	int baseType = syntax;
    
	/*
	 * if the syntax found is INTEGER, there may follow the enums for a
	 * textual description of the integer value; extract - don't skip.
	 */

	char *restrictions = NULL;
	
	/* 
	 * We may get something like ``{ foo ( 99), ... }'' or
	 * ``(0..99)'' or nothing.
	 */ 

	syntax = ReadKeyword(fp, keyword);
	if (syntax == LEFTBRACKET) {
	    syntax = ReadIntEnums(fp, &restrictions);
	} else if (syntax == LEFTPAREN) {

#ifdef USE_RANGES
	    if ((baseType != ASN1_OCTET_STRING) ||
		((baseType == ASN1_OCTET_STRING) &&
		 ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		 (syntax == SIZE) &&
		 ((syntax = ReadKeyword(fp, keyword)) != EOF) &&
		 (syntax == LEFTPAREN))) {
		if ((ReadRange(fp, &restrictions) != RIGHTPAREN) ||
		    ((baseType == ASN1_OCTET_STRING) &&
		     (((syntax = ReadKeyword(fp, keyword)) == EOF) ||
		      (syntax != RIGHTPAREN)))) {
		    fprintf(stderr, "%s:%d: bad range definition\n",
		            tnmMibFileName, line);
		    ckfree(restrictions);
		    return NULL;
		}
	    } else {
		fprintf(stderr, "%s:%d: bad range definition\n",
			tnmMibFileName, line);
	    }
#else
	    /* got LEFTPAREN: ``('' */
	    /* XXX: fetch here ranges... -- we simply skip */
	    int level = 1;
	    
	    while ((syntax = ReadKeyword(fp, keyword)) != RIGHTPAREN
		   && level > 0) 
	      {
		  if (syntax == EOF)
		    return NULL;
		  else if (syntax == RIGHTPAREN)
		    level--;
	      }
#endif
	}
	
	/* 
	 * We simply skip up to the ACCESS key, like the old version 
	 * did: wrong, but effective... 
	 */
	
	while (syntax != ACCESS) {
	    syntax = ReadKeyword(fp, keyword);
	    if (syntax == EOF) {
		return NULL;
	    }
	}

	if (restrictions && *restrictions) {

	    /* we fake a name, to allow saving to the frozen-index: */

	    char c = nodePtr->label[0];
	    if (islower(nodePtr->label[0])) {
		nodePtr->label[0] =  toupper(nodePtr->label[0]);
	    }
	    nodePtr->typePtr = CreateType(nodePtr->label, 
					  baseType, 0, restrictions);
	    nodePtr->typePtr->macro = TNM_MIB_OBJECTTYPE;
	    nodePtr->label[0] = c;
	    ckfree(restrictions);
	    restrictions = NULL;
	}

    } else if (syntax == ASN1_SEQUENCE) {
	if ((syntax = ReadKeyword(fp, keyword)) == ASN1_SEQUENCE_OF) {
	    nodePtr->syntax = syntax;
	}
	while (syntax != ACCESS) {
            syntax = ReadKeyword(fp, keyword);
            if (syntax == EOF) {
                return NULL;
            }
        }
	/* we are fine. now continue below with the ACCESS mode: */ 
    } else {
	
	/* 
	* old eat-it-up code: skip anything to the ACCESS keyword: 
	*/
	
	while ((syntax = ReadKeyword(fp, keyword)) != ACCESS)
	  if (syntax == EOF)
	    return NULL;
    }
    
    /*
     * next keyword defines ACCESS mode for object
     */

    syntax = ReadKeyword(fp, keyword);
    if (syntax < READONLY || syntax > NOACCESS) {
	fprintf(stderr, "%s:%d: scan error near `%s'\n", 
		tnmMibFileName, line, keyword);
	return NULL;
    }

    switch (syntax) {
      case READONLY:   
	  nodePtr->access = TNM_MIB_READONLY; 
	  break;
      case READCREATE: 
	  nodePtr->access = TNM_MIB_READCREATE; 
	  break;
      case READWRITE:  
	  nodePtr->access = TNM_MIB_READWRITE; 
	  break;
      case WRITEONLY:		/* RFC 1908 section 2.1 point (9) */
	  nodePtr->access = TNM_MIB_READWRITE; 
	  break;
      case FORNOTIFY:  
	  nodePtr->access = TNM_MIB_FORNOTIFY; 
	  break;
      default:         
	  nodePtr->access = TNM_MIB_NOACCESS;
    }
    
    /*
     * next keyword must be STATUS
     */

    if ((syntax = ReadKeyword(fp, keyword)) != STATUS)
	return NULL;
    
    /*
     * next keyword defines status of object
     */

    syntax = ReadKeyword(fp, keyword);
    if (syntax < MANDATORY || syntax > DEPRECATED) {
	fprintf(stderr, "%s:%d: scan error near `%s'\n", 
		tnmMibFileName, line, keyword);
	return NULL;
    }
    switch (syntax) {
    case MANDATORY:
	syntax = CURRENT;
	strcpy(keyword, "current");
	break;
    case OPTIONAL:
	syntax = OBSOLETE;
	strcpy(keyword, "obsolete");
	break;
    }
    nodePtr->status = TnmGetTableKey(tnmMibStatusTable, keyword);
    if (nodePtr->typePtr && nodePtr->typePtr->macro == TNM_MIB_OBJECTTYPE) {
	nodePtr->typePtr->status = nodePtr->status;
    }

    /*
     * now determine optional parts of OBJECT-TYPE macro
     */

    while ((syntax = ReadKeyword(fp, keyword)) != EQUALS) {
	switch (syntax) {
	  case DESCRIPTION:
            nodePtr->fileOffset = ftell (fp);
            if ((syntax = ReadKeyword(fp, keyword)) != QUOTESTRING) {
		return NULL;
            }
            break;
	  case AUGMENTS:
	    if ((syntax = ReadKeyword(fp, keyword)) != LEFTBRACKET) {
		return NULL;
	    }
	    if ((syntax = ReadKeyword(fp, keyword)) != LABEL) {
		return NULL;
	    }
	    nodePtr->index = ckstrdup(keyword);
	    if ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET) {
		ckfree(nodePtr->index);
		nodePtr->index = NULL;
		return NULL;
	    }
	    nodePtr->augment = 1;
	    break;
	  case INDEX:
	    Tcl_DStringInit(&dst);
	    if ((syntax = ReadKeyword(fp, keyword)) != LEFTBRACKET) {
	        return NULL;
	    }
	    while ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET) {
		switch (syntax) {
		case COMMA:
		    break;
		case IMPLIED:
		    if (! nodePtr->implied) {
			nodePtr->implied = 1;
		    } else {
			fprintf(stderr, "%s:%d: multiple uses of IMPLIED\n",
				tnmMibFileName, line);
			return NULL;
		    }
		    break;
		case LABEL:
		    Tcl_DStringAppendElement(&dst, keyword);
		    break;
		default:
		    Tcl_DStringFree(&dst);
		    return NULL;
		}
	    }
	    nodePtr->index = ckstrdup(Tcl_DStringValue(&dst));
	    Tcl_DStringFree(&dst);
	    break;
	  case DEFVAL:
	    if ((syntax = ReadKeyword(fp, keyword)) != LEFTBRACKET) {
                return NULL;
            }
            while ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET) {
		if (syntax == EOF) {
		    return NULL;
		}
		nodePtr->index = ckstrdup(keyword);
	    }
	    len = nodePtr->index ? strlen(nodePtr->index) : 0;
	    if (len > 2) {
		if (nodePtr->index[0] == '\'' 
		    && nodePtr->index[len-2] == '\''
		    && (nodePtr->index[len-1] == 'h' 
			|| nodePtr->index[len-1] == 'H')) {
		    if (len == 3) {
			nodePtr->index[0] = '\0';
		    } else {
			char *r, *p, *n = ckalloc(len*3);
			r = n;
			p = nodePtr->index;
			if ((len - 3) % 2) {
			    *p = '0';
			} else {
			    p++;
			}
			while (*p && isxdigit(*p)) {
			    *r++ = *p++;
			    *r++ = *p++;
			    if (*p && isxdigit(*p)) {
				*r++ = ':';
			    }
			}
			*r = '\0';
			ckfree(nodePtr->index);
			nodePtr->index = n;
		    }
		}
	    }
	    break;
	  case EOF:
            return NULL;
	  default:
            break;
	}
    }

    if (ParseNodeList(fp, nodeList, nodePtr) < 0) {
	return NULL;
    }
    
    return nodePtr;
}

/*
 * ParseNodeList() parses an object identifier definition in the form
 * { label label* subid } and creates MIB nodes for all intermediate
 * identifiers. ParseNodeList() return 0 on success and -1 on error.
 */

static int
ParseNodeList(fp, nodeList, nodePtr)
    FILE *fp;
    TnmMibNode **nodeList;
    TnmMibNode *nodePtr;
{
    struct subid *subidList, *freePtr;

    subidList = ReadSubID(fp);
    if (subidList == NULL) {
	return -1;
    }

    while (subidList != NULL && subidList->subid != -1) {

	/*
	 * Check if it is the id for this OBJECT-IDENTIFIER. Otherwise,
	 * create a new node and link it to the node list.
	 */
	
	if (subidList->label == NULL) {
	    nodePtr->parentName = ckstrdup(subidList->parentName);
	    nodePtr->subid = subidList->subid;
	} else {
	    AddNewNode(nodeList, subidList->label, 
		       subidList->parentName,
		       (unsigned) subidList->subid);
	}
	
	freePtr  = subidList;
	subidList = subidList->next;
	ckfree ((char *) freePtr);
    }
    
    return 0;
}


/*
 * HashKeywords() builds up a hash table, including the defined keywords
 * as records. This enables the other functions to directly reference
 * records by doing arithmetic transformations on the keywords name.
 */

static void
HashKeywords()
{
    char *cp = NULL;
    int	hash_val = 0;
    int	hash_index = 0;
    Keyword *tp = NULL;

    memset((char *) hashtab, 0, sizeof (hashtab));
    
    for (tp = keywords; tp->name; tp++) {
	hash_val = 0;
	for (cp = tp->name; *cp; cp++) 
	  hash_val += *cp;
	hash_index = HASH (hash_val);
	tp->hash   = hash_val;

	if (hashtab[hash_index] != NULL) {
	    tp->nextPtr = hashtab[hash_index];
	}
	hashtab[hash_index] = tp;
    }
}

/*
 * ReadKeyword() parses a keyword from the MIB file and places it in
 * the string pointed to by keyword. Returns the syntax of keyword or
 * EOF if any error.
 */

static int
ReadKeyword(fp, keyword)
     FILE *fp;
     char *keyword;
{
    char *cp = keyword;
    int	ch = lastchar;
    int	hash_val = 0;
    char quoteChar = '\0';

    Keyword *tp;
    
    *keyword = '\0';

    /*
     * skip spaces
     */

    while (isspace (ch) && ch != EOF) {
	if (ch == '\n') line++;
	ch = getc (fp);
    }

    if (ch == EOF) return EOF;

    /*
     * skip textual descriptions enclosed in " characters
     * or hex/bin values enclosed in ' characters
     */

    if ((ch == '"') || (ch == '\'')) {
	quoteChar = ch;
    }

    if (ch == quoteChar) {
	int len = 0;
	*keyword = '\0';
	while ((ch = getc (fp)) != EOF) {
	    if (ch == '\n') {
		line++;
	    } else if (ch == quoteChar) {
		lastchar = ' ';
		if (quoteChar == '"') {
		    return QUOTESTRING;
		} else {
		    if ((ch = getc (fp)) != EOF) {
			switch (toupper(ch)) {
			case 'B':
			    return BINVALUE;
			case 'H':
			    return HEXVALUE;
			default:
			    ungetc( ch, fp );
			    break;
			}
		    }
		    return QUOTEVALUE;
		}
	    } else if (len < SYMBOL_MAXLEN - 2) {
		keyword [len++] = ch, keyword [len] = 0;
	    }
	}
	return EOF;
    }

    /*
     * skip comments
     */

    if (ch == '-') {
	hash_val += ch;
	*cp++ = ch;
	
	if ((ch = getc (fp)) == '-') {
	    *keyword = '\0';
	    while ((ch = getc (fp)) != EOF) {
		if (ch == '\n') {
		    line++;
		    break;
		}
	    }
	    if (ch == EOF) return EOF;
	    
	    lastchar = ' ';
	    return ReadKeyword(fp, keyword);
	}
    }
   
    /*
     * Read characters until end of keyword is found
     */

    do {
	if (ch == '\n') line++;
	
	if (isspace (ch) || ch == '(' || ch == ')' || ch =='{' ||
	    ch == '}' || ch == ',' || ch == ';' || ch == '.' ||
	    ch == '|') {

	    if ((ch == '.') && (lastchar == '.')) {
		*cp++ = lastchar;
		*cp++ = ch;
		*cp = 0;
		ch = getc(fp);
		lastchar = ' ';
		return UPTO;
	    }

	    /*
             * check for keyword of length 1
	     */

	    if (!isspace (ch) && *keyword == '\0') {
		hash_val += ch;
		*cp++ = ch;
		lastchar = ' ';
	    } else if (ch == '\n') {
		lastchar = ' ';
	    } else {
		lastchar = ch;
	    }
	       
	    *cp = '\0';

	    /*
	     * is this a defined keyword ?
	     */

	    for (tp = hashtab[HASH(hash_val)]; tp != NULL; tp = tp->nextPtr) {
		if ((tp->hash == hash_val) && !fstrcmp (tp->name, keyword))
		  break;
	    }
	    
	    if (tp != NULL) {

		/*
		 * if keyword is not complete, continue; otherwise return
		 */
		
		if (tp->key == CONTINUE) {
		    lastchar = ch;
		    continue;
		}
		return tp->key;
	    }
	    
	    /*
	     * is it a LABEL ?
	     */
	    
	    for (cp = keyword; *cp; cp++) {
		if (cp == keyword && (*cp == '-' || *cp == '+')) continue;
		if (! isdigit (*cp)) return LABEL;
	    }
	    
	    /*
	     * keywords consists of digits only
	     */
	    
	    return (keyword[0] == '-' || keyword[0] == '+') 
		? SIGNEDNUMBER : NUMBER;

	} else {
	
	    /*
	     * build keyword and hashvalue
	     */

	    hash_val += ch;
	    *cp++ = ch;
	}
    } while ((ch = getc (fp)) != EOF);
    
    return EOF;
}




/*
 * ReadSubID() parses a list of the form { iso org(3) dod(6) 1 }
 * and creates a parent-child entry for each node. Returns NULL
 * if we found an error.
 */

static struct subid*
ReadSubID (fp)
    FILE *fp;
{
   char	name[SYMBOL_MAXLEN]; 
   char	keyword[SYMBOL_MAXLEN]; 
   int is_child	= 0;
   int syntax = 0;
   struct subid	*np = NULL;
   struct subid *subidList = NULL;

   /*
    * EQUALS are passed, so first keyword must be LEFTBRACKET
    */

   if ((syntax = ReadKeyword(fp, keyword)) != LEFTBRACKET) return NULL;

   /*
    * now read keywords until RIGHTBRACKET is passed
    */

   while ((syntax = ReadKeyword(fp, keyword)) != RIGHTBRACKET) {
       switch (syntax) {
	 case EOF:
	   return NULL;
         case LABEL:

	 do_label:

	   /* allocate memory for new element */
	   
	   np = (struct subid *) ckalloc (sizeof(struct subid));
	   memset((char *) np, 0, sizeof (struct subid));
	   np->subid  = -1;
	   
	   /* if this label followed another one, it's the child */
	   
	   if (is_child) {
	       np->parentName = ckstrdup (name);
               np->label = ckstrdup (keyword);
	   } else {
	       np->parentName = ckstrdup (keyword);
               is_child = 1;		/* next labels are children   */
	   }
	   
	   np->next = subidList;
	   subidList = np;
	   strcpy (name, keyword);
	   break;
         case LEFTPAREN:
	   if ((syntax = ReadKeyword(fp, keyword)) != NUMBER) return NULL;
	   np->subid = atoi (keyword);
	   if ((syntax = ReadKeyword(fp, keyword)) != RIGHTPAREN)
	     return NULL;
	   break;            
         case NUMBER:
	   if (! np)
	     {
		 /* something like:   { 0 1 } */
		 char *label = TnmMibGetName(keyword, 1);
		 if (! label)
		   return NULL;
		 strcpy (keyword, label);
		 goto do_label;
	     }

            if (np->subid != -1) {
		np = (struct subid *) ckalloc (sizeof (struct subid));
		memset((char *) np, 0, sizeof (struct subid));
		np->parentName = ckstrdup (name);
		np->subid = -1;
		np->next = subidList;
		subidList = np;
	    }
	   np->subid = atoi (keyword);
	   break;
         default:
	   return NULL;
       }
   }
   return subidList;
}

