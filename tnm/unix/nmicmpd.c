/*
 * nmicmpd.c --
 *
 *	This is external ICMP echo/mask/timestamp/traceroute server
 *	for the UNIX version of the Tnm Tcl extension. This server can
 *	be used by other programs as well as long as they use the
 *	protocol defined in the nmicmpd(n) man page. This program
 *	allows processing of parallel probes which makes ICMP requests
 *	to multiple hosts very efficient.
 *
 * 	This code is derived from ntping, but now uses a binary
 *	protocol between the Tnm Tcl extension and nmicmpd. It is
 *	integrated into the scotty package as nmicmpd.
 *
 * Copyright (c) 1996 Juergen Schoenwaelder and Erik Schoenfelder
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#define PROGNAME	"nmicmpd"			/* my name */
static char *version =  "nmicmpd  v0.99i  Dec 1996";	/* my version */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef HAVE_SOCKLEN_T
typedef size_t socklen_t;
#endif

#include <sys/socket.h>
#ifdef linux
/* at least in linux 2.1.3 we need this for ntohl(),... */
#define __KERNEL__
#endif
#include <netinet/in.h>
#ifdef linux
#undef __KERNEL__
#endif

#include <syslog.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

/* malloc.h is deprecated, stdlib.h declares the malloc function  */
#ifndef HAVE_STDLIB_H
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#endif
#ifdef DB_MALLOC
# include <dbmalloc.h>
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE 32
#endif

#include <errno.h>
#if defined(pyr) || (defined(MACH) && defined (MTXINU))
/* should make into the configure script... */
extern int errno;
#endif

#ifdef HAVE__RES
# include <arpa/nameser.h>
# include <resolv.h>
#endif

/*
 * AIX failes to use IP_TTL correct, so we shouldn't 
 * define USE_DLPI:
 */

#if !defined(_AIX)   /** XXX: && !defined(linux) **/
# if defined(IP_TTL) && ! defined(USE_DLPI)
/*
 * with USE_DLPI we will not send our own handmade ip (udp) packets.
 * instead we open a ordinariy udp socket and set the ttl via
 * setsockopt with IP_TTL. 
 * This is at least needed for HP-UX and SVR4-boxes.
 */
#  define USE_DLPI
# endif /* IP_TTL && ! USE_DLPI */
#endif /* ! _AIX */

#if defined(linux)
/*
 * this is for linux around 0.99.15 and above:
 * (well, and above 2.0.20 and 2.1.3 too)
 */

#ifdef WORDS_BIGENDIAN
struct ip { u_char ip_v:4, ip_hl:4; u_char ip_tos; short ip_len; 
	    u_short ip_id; short ip_off; u_char ip_ttl; u_char ip_p; 
	    u_short ip_sum; struct in_addr ip_src,ip_dst;
#else
struct ip { u_char ip_hl:4, ip_v:4; u_char ip_tos; short ip_len; 
	    u_short ip_id; short ip_off; u_char ip_ttl; u_char ip_p; 
	    u_short ip_sum; struct in_addr ip_src,ip_dst;
#endif
};
struct icmp { u_char  icmp_type; u_char icmp_code; u_short icmp_cksum;
	      u_short icmp_id, icmp_seq; char icmp_data [1];
};
struct udphdr { u_short uh_sport, uh_dport; short uh_ulen; u_short uh_sum; };

# define ICMP_MINLEN     	8
# define ICMP_ECHO       	8
# define ICMP_ECHOREPLY       	0
# define ICMP_UNREACH    	3
# define ICMP_SOURCEQUENCH	4
# define ICMP_TIMXCEED   	11
# define ICMP_TSTAMP		13
# define ICMP_TSTAMPREPLY	14
# define ICMP_MASKREQ    	17
# define ICMP_MASKREPLY  	18
# define ICMP_UNREACH_PORT	3
# define ICMP_TIMXCEED_INTRANS	0
#else
/*
 * Headers for our sun (and mostly bsd 4.2 lookalike - i guess):
 */
# include <netinet/in_systm.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <netinet/udp.h>
#endif

#ifdef linux
# include <linux/wait.h>
#endif

#include <netdb.h>
#include <arpa/inet.h>

/* 
 * some simple macros for time handling.
 * time is handled internally in milliseconds.
 */
#define timediff2(t1,t2)  (((t2).tv_sec - (t1).tv_sec) * 1000 \
	+ ((t2).tv_usec - (t1).tv_usec) / 1000)
#define time_diff(t1,t2)  (timediff2(t1,t2) <= 0 ? (- timediff2(t1,t2)) \
			   : timediff2(t1,t2))

#define timediff2usec(t1,t2)  (((t2).tv_sec - (t1).tv_sec) * 1000000 \
	+ ((t2).tv_usec - (t1).tv_usec))
#define time_diff_usec(t1,t2)  (timediff2usec(t1,t2) <= 0 ? (- timediff2usec(t1,t2)) : timediff2usec(t1,t2))

/* fetch gettimofday: */
#define gettime(tv,dofail)	\
	if (gettimeofday (tv, (struct timezone *) 0) < 0)	\
	  { PosixError("gettimeofday failed"); dofail; }


/* 
 * simple debug help:
 */
#define dsyslog		if (do_debug) syslog

/* 
 * min/max data size we are accepting and willing to send:
 */
#define MIN_DATALEN			44
#define MAX_POSSIBLE_DATALEN		65536
static int max_data_len = MAX_POSSIBLE_DATALEN;

/* emit debug messages (don't try this at home): */
static int do_debug = 0;

/* the icmp and ip-communication sockets: */
static int icsock = -1;				/* icmp */
static int ipsock = -1;				/* ip/udp */

#ifdef USE_DLPI
/* port # our udp-socket is bound to: */
static unsigned short src_port = 0;

/* second ip-socket (for switched byte-sex problems): */
static int ipsock2 = -1;
#endif

/*
 * sigh - check for bytesex in unreach/exceed icmp's too. 
 * some synoptics are dumb enough to reply wrong packets: 
 */
static int checkComplementPort = 1;

/*
 * Communication is done via stdin/stdout. The message format is
 * aligned with the the beginning of this structure. Numbers larger 
 * than a char are in network-byteorder.
 */

typedef struct _jobElem {
    unsigned char version;
    unsigned char type;
    unsigned char status;
    unsigned char flags;
    uint32_t tid;
    struct in_addr addr;
    union {
	struct {
	    unsigned char ttl;
	    unsigned char timeout;
	    unsigned char retries;
	    unsigned char delay;
	} c;					/* receive parameter */
	unsigned int data;			/* reply parameter */
    } u;
    uint16_t size;				/* packet size requested */
    uint16_t window;				/* comm. window size */

    /*
     * private section of a job:
     */
    union {
	struct {
	    unsigned short port;	/* dest port for traceroute */
	    struct timeval tv;		/* time ttl probe sent. */
	} trace;
    } p;

    int probe_cnt;			/* # of probes still sent */
    struct timeval time_sent;
    unsigned retry_ival;
    int id;
    int done;
    int inServe;			/* are we processing it -- window ok */
    struct _jobElem *next;
} jobElem;


#define ICMP_PROTO_VERSION	0		/* protocol version */
#define ICMP_PROTO_CMD_LEN	20		/* length of a command */
#define ICMP_PROTO_REPLY_LEN	16		/* length of a reply */

#define ICMP_TYPE_ECHO		1		/* icmp echo request */
#define ICMP_TYPE_MASK		2		/* icmp mask request */
#define ICMP_TYPE_TSTAMP	3		/* icmp timestamp request */
#define ICMP_TYPE_TRACE		4		/* udp/icmp trace-packet */

#define ICMP_STATUS_NOERROR	0x00
#define ICMP_STATUS_TIMEOUT	0x01
#define ICMP_STATUS_GENERROR	0x02

#define ICMP_FLAG_FINALHOP	0x01

/* root of the job queue: */
static jobElem *job_list = 0;

/* forward: */
static void ReceivePacket();

#include <sys/resource.h>

static void
SetCpuLimit(int secs)
{
    struct rlimit rlimit;

    if (getrlimit (RLIMIT_CPU, &rlimit) < 0) {
	perror ("cpu rlimit get failed");
	exit(5);
    }

    rlimit.rlim_cur = secs;

    if (setrlimit (RLIMIT_CPU, &rlimit) < 0) {
	perror ("cpu rlimit set failed");
	exit(6);
    }

    return;
}


/*
 *----------------------------------------------------------------------
 *
 * PosixError --
 *
 *	This procedure logs a posix error message to the system
 *	logging facility.
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
PosixError(char *msg)
{
#ifdef HAVE_STRERROR
    syslog(LOG_ERR, "%s: %s", msg, strerror(errno));
#else
    syslog(LOG_ERR, "%s: %d", msg, errno);
#endif
}
/*
 *----------------------------------------------------------------------
 *
 * SwapShort --
 *
 *	This procedure exchanges the bytes in the supplied short value.
 *
 * Results:
 *	Returns the byte swapped argument.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SwapShort(unsigned x)
{
    return ((x & 0xff) << 8) | ((x & 0xff00) >> 8);
}

/*
 *----------------------------------------------------------------------
 *
 * GetFreeUdpPort --
 *
 *	This procedure returns a udp port number useable for traceing
 *	purposes. The port returned is checked not to be byte-swapped
 *	or in use.
 *
 * Results:
 *	Returns the next possible port to use.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned short
GetFreeUdpPort()
{  
    /* default base port for ttl probes: */
#define BASE_PORT		50000
#define MAX_BASE_PORT		60000

    static int probe_port = 0;		/* base port for ttl probes */
    int swapped_port;
    jobElem *job;

    /* 
     * Take the next port between BASE_PORT and MAX_BASE_PORT.
     * Make sure we wrap if we reach MAX_BASE_PORT. (This limits
     * the number of active ICMP_TYPE_TRACE jobs but the interval
     * should be large enough.)
     */

    probe_port++;
    if (probe_port < BASE_PORT || probe_port > MAX_BASE_PORT) {
	probe_port = BASE_PORT;
    }
    swapped_port = SwapShort(probe_port);
    
    /* 
     * Check, if this port is already in use, or a bad candidate
     * (because it matches a byte-swapped port that is in use).
     * Recurse if required (this is computer scientists code :-)
     */

    for (job = job_list; job; job = job->next) {
	if (job->type == ICMP_TYPE_TRACE) {
	    if (job->p.trace.port == probe_port || 
		job->p.trace.port == swapped_port) {
		return GetFreeUdpPort();
	    }
	}
    }

    return probe_port;
}

/*
 *----------------------------------------------------------------------
 *
 * GetWindow --
 *
 *	This procedure returns the window counter after adjusting it
 *	by the argument.
 *
 * Results:
 *	Returns window counter.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned short
GetWindow(int val)
{  
    static int counter = 0;

    return counter += val;
}

/*
 *----------------------------------------------------------------------
 *
 * ReceivePending --
 *
 *	This procedure checks about pending icmp messages.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	Pending messages are received, processed and answered.
 *	At least some time given by the parameter is spent.
 *
 *----------------------------------------------------------------------
 */

static void
ReceivePending(int wtim)
{
    fd_set fds;
    struct timeval tv, time_mark, now;
    long tdiff;
    int rc;

    /* we should not wait longer than time_mark + wtim: */
    if (wtim > 0) {
	gettime(&time_mark, /* ignore */ );
    }
    
    do {
#if 0
        /* XXX: seems to break delay option: */
	/* don't spend time, if we are done: */
	if (wtim > 0 && ! job_list) {
	    return;
	}
#endif
	
	/* wait for a ready icmp socket, or timeout: */
	do {
	    FD_ZERO (&fds);
	    FD_SET (icsock, &fds);
	    
	    if (wtim > 0) {
		tv.tv_usec = (wtim * 1000) % 1000000;
		tv.tv_sec = (wtim * 1000) / 1000000;
	    } else {
		tv.tv_sec = tv.tv_usec = 0;
	    }

	    rc = select(icsock + 1, &fds, (fd_set *) 0, (fd_set *) 0, &tv);
	    
	    if (rc < 0 && errno != EINTR && errno != EAGAIN) {
		PosixError("select failed");
		exit(1);
	    }
	} while (rc < 0);
	
	/* if timeout we are done: */
	if (! rc) {
	    return;
	} else {
	    /* icmp socket is ready: */
	    ReceivePacket();
	}
	
	/* at least poll, until the queue is empty: */
	if (wtim <= 0) {
	    wtim = 0;
	    continue;
	}

	/* recalculate the remaining waittime we have to spend: */
	gettime(&now, return);
	tdiff = time_diff(time_mark, now);
	dsyslog(LOG_DEBUG, "** timediff to spend: %ld", tdiff);
	/* time to leave ? */
	if (tdiff > wtim) {
	    return;
	}
	  
	time_mark = now;
	wtim -= tdiff;
	
    } while (wtim >= 0);
}

/*
 *----------------------------------------------------------------------
 *
 * FindJobByPort --
 *
 *	This procedure looks for a job by a port number or a 
 *	complementary portnumber if checkComplementPort is set.
 *
 * Results:
 *	Returns the job or 0 on error.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static jobElem *
FindJobByPort(struct ip *ip, struct udphdr *udph)
{
    unsigned short id = ntohs(udph->uh_sport);
    unsigned short port = ntohs(udph->uh_dport);
    jobElem *job;
    
    dsyslog(LOG_DEBUG, "* looking for src %u (0x%lx)  dest %u (0x%x) ...", 
	    (unsigned) id, (long) id, (unsigned) port, (int) port);

    for (job = job_list; job; job = job->next) {

	unsigned short src;
	int got_it = 0;

#ifndef USE_DLPI
	src = job->id;
#else
	src = src_port;
#endif

#ifndef USE_DLPI
	dsyslog(LOG_DEBUG, " %u", src);
#else
	dsyslog(LOG_DEBUG, "  %u (0x%lx)", (unsigned) job->p.trace.port, 
		(long) job->p.trace.port);
#endif
	  
	if (job->p.trace.port == port && src == id) {
	    got_it = 1;
	}
	  
	if (! got_it && checkComplementPort) {
	    if ((job->p.trace.port == SwapShort(port) && src == SwapShort(id))
		|| (job->p.trace.port == port && src == SwapShort(id))
		|| (job->p.trace.port == SwapShort(port) && src == id)) {

		got_it = 1;
		dsyslog(LOG_DEBUG,
			"icmp-reply from 0x%08lx with byte-swapped port", 
			(unsigned long) ntohl(ip->ip_src.s_addr));
	    }
	}
	  
	if (got_it) {
	    dsyslog(LOG_DEBUG, "job %d: received icmp reply", job->tid);
	    return job;
	}
    }

    dsyslog(LOG_DEBUG, "nope");
    
    return (jobElem *) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FindJobById --
 *
 *	This procedure looks about a job by the given id.
 *
 * Results:
 *	Returns the job or 0 on error.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static jobElem *
FindJobById(unsigned short id)
{
    jobElem *job;

    for (job = job_list; job; job = job->next) {
	if (job->id == id) {
	    dsyslog(LOG_DEBUG, "looking for job id %u ... got it",
		    (unsigned) id);
	    return job;
	}
    }

    dsyslog(LOG_DEBUG, "looking for job id %u ... not found", (unsigned) id);
    return (jobElem *) 0;
}

/*
 *----------------------------------------------------------------------
 *
 * CleanupJobs --
 *
 *	This procedure walks along the job queue and checks about
 *	finished jobs. The jobs are answered to stdout and removed 
 *	from the queue.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	The global job_list may be changed.
 *      May decrement the window-counter.
 *
 *----------------------------------------------------------------------
 */

static void
CleanupJobs()
{
    jobElem **j = &job_list;

    while (*j) {

	jobElem *job = *j;
	int rc;

	if (job->done) {

	    rc = write(fileno(stdout), (char *) job, ICMP_PROTO_REPLY_LEN);
	    if (rc < 0) {
#if defined(EWOULDBLOCK)
		if (errno == EWOULDBLOCK) {
		    j = &(*j)->next;
		    continue;
		}
#endif
		PosixError("write failed");
	    }
	    if (rc != ICMP_PROTO_REPLY_LEN) {
		syslog(LOG_ERR, "write returned %d instead of %d bytes",
			   rc, ICMP_PROTO_REPLY_LEN);
	    }

	    GetWindow(-1);

	    /*
	     * Remove the job from our queue and free memory:
	     */

	    *j = (*j)->next;
	    free((char *) job);

	} else {
	    j = &(*j)->next;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ReceivePacket --
 *
 *	This procedure receives and processes a icmp-message. 
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	If a job is completed, cleanup is called and the global
 *	job_list may be changed.
 *
 *----------------------------------------------------------------------
 */

static void
ReceivePacket()
{
    char packet[MAX_POSSIBLE_DATALEN + 128];
    size_t len = sizeof(packet);
    struct ip *ip = (struct ip *) packet;
    struct icmp *icp;			/* for pings */
    struct udphdr *udph;		/* for ttl's */
    struct timeval tp1, tp2;
    struct sockaddr_in sfrom;
    socklen_t fromlen = sizeof(sfrom);
    int hlen = 0, cc, ttl_is_done = 0;
    int type = -1;
    jobElem *job = 0;
    
    cc = recvfrom(icsock, (char *) packet, len, 0,
		  (struct sockaddr *) &sfrom, &fromlen);

    if (cc < 0 && errno != EINTR && errno != EAGAIN) {
	PosixError("recvfrom failed:");
	return;
    }

    gettime(&tp2, return);
    
    dsyslog(LOG_DEBUG, "recvfrom got rc = %d", cc);

    /*
     * raw-socket with icmp-protocol (send and) receive
     * ip packets with complete header:
     */
    hlen = ip->ip_hl << 2;
    
    if (cc < hlen + ICMP_MINLEN) {
	dsyslog(LOG_DEBUG, "short packet (%d < %d) - ignored",
		cc, hlen + ICMP_MINLEN);
	return;
    }
    
    icp = (struct icmp *) (packet + hlen);
    udph = (struct udphdr *) (icp->icmp_data + sizeof(struct ip));
    
    dsyslog(LOG_DEBUG,
	    "got icmp type %d  code %d  id %u (0x%x)\n",
	    icp->icmp_type, icp->icmp_code,
	    (unsigned) icp->icmp_id,
	    (unsigned) icp->icmp_id);
    
    switch (icp->icmp_type) {
    case ICMP_ECHOREPLY:
	type = ICMP_TYPE_ECHO;
	break;
    case ICMP_MASKREPLY:
	type = ICMP_TYPE_MASK;
	break;
    case ICMP_TSTAMPREPLY:
	type = ICMP_TYPE_TSTAMP;
	break;
    case ICMP_UNREACH:
	if (icp->icmp_code != ICMP_UNREACH_PORT) {
	    dsyslog(LOG_DEBUG, "bad icmp code - discarded");
	    return;
	}
	type = ICMP_TYPE_TRACE;
	ttl_is_done = 1;
	break;
    case ICMP_TIMXCEED:
	if (icp->icmp_code != ICMP_TIMXCEED_INTRANS) {
	    dsyslog(LOG_DEBUG, "bad icmp code - discarded");
	    return;
	}
	type = ICMP_TYPE_TRACE;
	break;
    default:
	dsyslog(LOG_DEBUG, "unknown icmp type - discarded");
	return;
    }
    
    if ((type != ICMP_TYPE_TRACE && ! (job = FindJobById(icp->icmp_id)))
	|| (type == ICMP_TYPE_TRACE && ! (job = FindJobByPort(ip, udph))))
      {
	  dsyslog(LOG_DEBUG, "unknown packet id - discarded");
	  return;
      }
    
    /* this one is still in progress: */
    if (job->done) {
	dsyslog(LOG_DEBUG, "already done - discarded");
	return;
    }

    if ((type == ICMP_TYPE_ECHO
	 || type == ICMP_TYPE_MASK
	 || type == ICMP_TYPE_TSTAMP)
	&& sfrom.sin_addr.s_addr != job->addr.s_addr) {
	dsyslog(LOG_DEBUG, "unexpected packet from %s - discarded", 
		inet_ntoa(sfrom.sin_addr));
	return;
    }

    switch (type) {
    case ICMP_TYPE_ECHO:
	tp1 = job->p.trace.tv;		/* time sent */
	memcpy((char *) &tp1, (char *) icp->icmp_data, 
	       sizeof(struct timeval)); 
	{ 
	    int tdiff = time_diff_usec(tp1, tp2);
	    memcpy((char *) &job->u.data, (char *) &tdiff, 
		   sizeof(job->u.data));
	    job->u.data = htonl(job->u.data);
	}
	job->done = 1;
	job->addr = sfrom.sin_addr;
	dsyslog(LOG_DEBUG, "job %d: echo from %s with rtt %d", job->tid,
		inet_ntoa(job->addr), ntohl(job->u.data));
	break;

    case ICMP_TYPE_MASK:
	memcpy((char *) &job->u.data, icp->icmp_data, sizeof(job->u.data));
	job->done = 1;
	job->addr = sfrom.sin_addr;
	dsyslog(LOG_DEBUG, "job %d: mask 0x%lx\n", job->tid,
		(unsigned long) ntohl((long) job->u.data));
	break;
	
    case ICMP_TYPE_TSTAMP:
	{ 
	    int val = ntohl(((int32_t *) icp->icmp_data) [1]) -
		ntohl(((int32_t *) icp->icmp_data) [0]);
	    memcpy((char *) &job->u.data, (char *) &val, sizeof(job->u.data));
	    job->u.data = htonl(job->u.data);
	}
	job->done = 1;
	job->addr = sfrom.sin_addr;
	dsyslog(LOG_DEBUG, "job %d: timestamp diff %ld", 
		job->tid, (long) job->u.data);
	break;

    case ICMP_TYPE_TRACE:
	tp1 = job->p.trace.tv;		/* time sent */
	{
	    int tdiff = time_diff_usec(tp1, tp2);
	    memcpy((char *) &job->u.data, (char *) &tdiff, 
		   sizeof(job->u.data));
	    job->u.data = htonl(job->u.data);
	}
	job->addr = sfrom.sin_addr;
	if (ttl_is_done) {
	    job->flags |= ICMP_FLAG_FINALHOP;
	}
	dsyslog(LOG_DEBUG, "job %d: trace hop %s flags=%d",
		job->tid, inet_ntoa(sfrom.sin_addr), ttl_is_done);
	job->done = 1;
	break;
	
    default:
	dsyslog(LOG_DEBUG, "unknown type - discarded");
	return;
    }

    /* fine: */
    if (job->done) {
	CleanupJobs();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CalcIcmpCksum --
 *
 *	This procedure calculates a icmp checksum.
 *
 * Results:
 *	Returns the icmp-checksum.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int 
CalcIcmpCksum(unsigned short *buf, int n)
{
    int sum = 0, nleft = n;
    unsigned short *ptr = buf;
#if 0
    unsigned short odd_byte = 0;
#endif
    
    while (nleft > 1) {
	sum += *ptr++, nleft -= 2;
    }

    if (nleft == 1) {
#if 1
	sum += ntohs(*(unsigned char *)ptr << 8);
#else
	*(unsigned char *) (&odd_byte) = *(unsigned char *) ptr,
	sum += odd_byte;
#endif
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    
    return ~sum;
}

/*
 *----------------------------------------------------------------------
 *
 * SendPacket --
 *
 *	This procedure sends a packet to the specified address.
 *
 * Results:
 *	0 on success, -1 on error.
 * 
 * Side effects:
 *	On error marks this job as done.
 *
 *----------------------------------------------------------------------
 */

static int
SendPacket(jobElem *job, int sock, char *buf, size_t size,
           struct sockaddr *addr, socklen_t addrLen)
{
    int rc;

resend:
    rc = sendto(sock, buf, size, 0, addr, addrLen);
	
    /* 
     * At least linux has the irritating behavior, to return an error
     * on the next sento call, if the previous one got a port
     * unreachable, with returning connection refused. So we will 
     * work around it... 
     */
    if (rc < 0 	&& (errno == EINTR
#ifdef ECONNREFUSED
		    || errno == ECONNREFUSED
#endif
		    || errno == EAGAIN)) {
        PosixError("sendto failed (ignored)");
	goto resend;
    }
    
#ifdef EHOSTDOWN
    /*
     * Some BSD machines return an EHOSTUNREACH error when the
     * pending ARP entry has timed out for the host. Just ignore
     * the error, and attempt a normal retry in this case.
     * Reported by Louis A. Mamakos <louie@transsys.com>.
     */
    if (rc < 0 && errno == EHOSTDOWN) {
	return 0;
    }
#endif
    
    if (rc != size) {
	if (rc < 0) {
	    PosixError("sendto failed");
	}
	job->status = ICMP_STATUS_GENERROR;
	job->u.data = 0;
	job->done = 1;
	return 1;
    } else {
        return 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SendTrace --
 *
 *	This procedure prepares and sends a udp-trace packet.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	Increments the probe counter.
 *	On error marks this job as done.
 *
 *----------------------------------------------------------------------
 */

static void
SendTrace(jobElem *job)
{
    char *datap, outpack [MAX_POSSIBLE_DATALEN + 128];
#ifndef USE_DLPI
    struct ip *ip = (struct ip *) outpack;
    struct udphdr *udph = (struct udphdr *) (outpack + sizeof(struct ip));
#endif /* ! USE_DLPI */
    struct sockaddr_in *sto, staddr;
    int i, j;
	
    sto = &staddr;
    sto->sin_addr = job->addr;
    sto->sin_family = AF_INET;

    syslog(LOG_DEBUG, "SendTrace()");

    if (job->done) {
	return;
    }

#ifndef USE_DLPI
    ip->ip_hl = sizeof(struct ip) / sizeof(int32_t);
    ip->ip_v = 4;			/* take this - live and in color */
    ip->ip_tos = 0;
    ip->ip_id = job->id;		/* ??? */
    ip->ip_sum = 0;
    /* fixed by Jan L. Peterson (jlp@math.byu.edu): */
    ip->ip_src.s_addr = 0;		/* jlp */
    
    ip->ip_off = 0;
    ip->ip_p = IPPROTO_UDP;
    ip->ip_len = job->size;
    ip->ip_ttl = job->u.c.ttl;
    ip->ip_dst = sto->sin_addr;	       /* needed for linux (no bind) */
    
    udph->uh_sport = htons(job->id);
    udph->uh_dport = htons(job->p.trace.port);
    udph->uh_ulen = htons((u_short) (job->size - sizeof(struct ip)));
    udph->uh_sum = 0;
    
    datap = outpack + sizeof(struct ip) + sizeof(struct udphdr);

#else /* USE_DLPI */
    sto->sin_port = htons(job->p.trace.port);
    datap = outpack;
#endif /* USE_DLPI */

    /* save time this probe ws sent: */
    gettime(&job->p.trace.tv, return);
    
    for (i = sizeof(struct timeval) + 2, j = 'A'; i < job->size; i++, j++) {
	datap [i] = j;
    }

    /*
     * may be simply: #ifdef IP_TTL  (eventually wrapped around a USE_DLPI) ?
     */
#ifdef USE_DLPI
    { 
	int opt_ttl = job->u.c.ttl;
	socklen_t opt_ttl_len = sizeof(opt_ttl);
	
	if (setsockopt(ipsock, IPPROTO_IP, IP_TTL, 
		       (char *) &opt_ttl, opt_ttl_len) < 0)
	    PosixError("can not set ttl (setsockopt)");
	
	if (getsockopt(ipsock, IPPROTO_IP, IP_TTL,
		       (char *) &opt_ttl, &opt_ttl_len) < 0)
	    PosixError("can not set ttl (getsockopt)");
	else if (job->u.c.ttl != opt_ttl)
	    syslog(LOG_ERR, "can not set ttl (dlpi)");
    }
#endif

    job->probe_cnt++;
    gettime(&job->time_sent, );

    if (! SendPacket(job, ipsock, (char *) outpack, job->size,
		     (struct sockaddr *) sto, sizeof (struct sockaddr_in))) {
        dsyslog(LOG_DEBUG, "job %d: ttl %d sent to %s port %u (0x%x)",
		job->tid, job->u.c.ttl, inet_ntoa(job->addr),
		(unsigned) job->p.trace.port,
		(unsigned) job->p.trace.port);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SendIcmp --
 *
 *	This procedure prepares and sends a icmp probe.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	Increments the probe counter.
 *	On error marks this job as done.
 *
 *----------------------------------------------------------------------
 */

static void
SendIcmp(jobElem *job)
{
    char outpack [MAX_POSSIBLE_DATALEN + 128];
    struct icmp *icp = (struct icmp *) outpack;
    char *datap = icp->icmp_data;
    int i, data_offset;
    struct sockaddr_in staddr, *sto;
    struct timeval tv;

    memset (outpack, 0, sizeof(outpack));

    sto = &staddr;
    sto->sin_addr = job->addr;
    sto->sin_family = AF_INET;
    sto->sin_port = 0;

    if (job->done) {
        dsyslog(LOG_DEBUG, "job %d: job is already done (ignored)", job->tid);
	return;
    }
    
    icp->icmp_type = job->type == ICMP_TYPE_MASK ? ICMP_MASKREQ : 
	(job->type == ICMP_TYPE_TSTAMP ? ICMP_TSTAMP : ICMP_ECHO);
    icp->icmp_code = 0;
    icp->icmp_cksum = 0;
    icp->icmp_seq = job->probe_cnt;
    icp->icmp_id = job->id;
    
    gettime(&tv, return);
	
    if (job->type == ICMP_TYPE_TSTAMP) {
	* (int32_t *) datap = htonl((tv.tv_sec % 86400) * 1000
				  + (tv.tv_usec / 1000));
	data_offset = sizeof(int32_t);
    } else if (job->type == ICMP_TYPE_MASK) {
	data_offset = 0;
    } else {
        /* ping: */
	* (struct timeval *) datap = tv;
	data_offset = sizeof(struct timeval);
    }

    for (i = data_offset; i < job->size; i++) {
	datap[i] = i;
    }

    /* icmp checksum: */
    icp->icmp_cksum = CalcIcmpCksum((unsigned short *) icp, job->size);

    /* last chance to update time stamp of this job: */
    job->time_sent = tv;
    job->probe_cnt++;


    if (! SendPacket(job, icsock, (char *) outpack, job->size,
		     (struct sockaddr *) sto, sizeof (struct sockaddr_in))) {
	dsyslog(LOG_DEBUG, "job %d: %s ping %d sent to %s",
		job->tid, job->type == ICMP_TYPE_MASK ? "mask" : 
		(job->type == ICMP_TYPE_TSTAMP ? "tstamp" : "regular"),
		job->probe_cnt - 1, inet_ntoa(job->addr));
    }

    dsyslog(LOG_DEBUG, "send_icmp return");
}

/*
 *----------------------------------------------------------------------
 *
 * InitSockets --
 *
 *	This procedure initializes the communication ports.
 *
 * Results:
 *	Returns 0 on error and 1 on success.
 * 
 * Side effects:
 *	The global socket-fd's are initialized.
 *
 *----------------------------------------------------------------------
 */

static int
InitSockets()
{
    struct protoent *proto;
    int icmp_proto = 1;			/* Capt'n Default */
    struct sockaddr_in maddr;
#if !defined(nec_ews) && !defined(linux) && !defined(USE_DLPI)
#ifdef IP_HDRINCL
    int on = 1;				/* karl */
#endif
#endif
#ifdef USE_DLPI
    struct sockaddr_in tmp_addr;
    socklen_t ta_len = sizeof(tmp_addr);
#endif
    
    /* 
     * Sanity check: Don't complain about being unable to open the 
     * socket, if no root permissions are available. Check the uid 
     * before trying to open the ICMP socket.
     */

    if (geteuid ()) {
	syslog(LOG_WARNING, 
	       "running with euid %d - not with root permissions", geteuid());
	syslog(LOG_WARNING, 
	       "expect missing permissions getting the icmp socket");
    }

    if ((proto = getprotobyname("icmp")) == NULL) {
	/* PosixError("icmp protocol unknown"); */ 
	/* fall through */
    } else {
	icmp_proto = proto->p_proto;
    }
    
    if ((icsock = socket(AF_INET, SOCK_RAW, icmp_proto)) == -1) {
	PosixError("can not get icmp socket");
	return 0;
    }

#ifdef USE_DLPI
    /*
     * My mom told me: if it hurts don't do it ...
     * but this looks quite useable.
     */
    if ((ipsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	PosixError("can not get udp socket");
	return 0;
    }
#else /* ! USE_DLPI */
    if ((ipsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
	PosixError("can not get raw ip socket");
	return 0;
    }
#endif /* ! USE_DLPI */

    /*
     * SO_SNDBUF and IP_HDRINCL fix from karl@sugar.NeoSoft.COM:
     * seems to be neccesary for 386bsd and others.
     */
#if defined(SO_SNDBUF)
    /* try possible sizes: */
    while (max_data_len > 128) {
	if (setsockopt(ipsock, SOL_SOCKET, SO_SNDBUF, (char *) &max_data_len,
		       sizeof(max_data_len)) != -1) {
	    break;
	}
	max_data_len = max_data_len / 2;
    }
    if (max_data_len <= 128) {
	PosixError("SO_SNDBUF error");
#if !defined(__minix)
	return 0;
#else
	/* On old Minix3 network stack SO_SNDBUF is not implemented but
	 * it is defined.  IP_HDRINCLUDE the same.  The latter would be
	 * achieved only with a Minix specific ioctl. Source irc #minix,
	 * Saturn.
	 *
	 * The max_data_len is set to a conservative MAX_POSSIBLE_DATALEN
	 * minus IP header size.
	 *
	 * Note that MAX_POSSIBLE_DATALEN is +1 off.
	 */
	max_data_len = 65535-20;
#endif /* minix */
    }
#endif /* SO_SNDBUF */

    dsyslog(LOG_DEBUG, "using max_data_len of %d", max_data_len);

    /*
     * if the SO_BROADCAST option is avail, try to set this.
     * if it fails, this should cause no extra trouble.
     * (note: at least needed for linux 2.2.x)
     */
#if defined(linux) && defined(SO_BROADCAST)
    {
	int flag = 1;
	socklen_t flag_len = sizeof(flag);
	if (setsockopt(ipsock, SOL_SOCKET,SO_BROADCAST, 
		       (char *) &flag, flag_len) < 0) {
	    dsyslog(LOG_DEBUG, "note: cannot set broadcast options for ipsock");
	}
    }
#endif
    
#if !defined(nec_ews) && !defined(linux) && !defined(USE_DLPI)
#ifdef IP_HDRINCL
    if (setsockopt(ipsock, IPPROTO_IP, IP_HDRINCL, (char *) &on,
		   sizeof(on)) < 0) {
	PosixError("IP_HDRINCL error - trying to continue");
    }
#endif /* IP_HDRINCL */
#endif /* ! nec_ews && ! linux && ! USE_DLPI */
    
    memset((char *) &maddr, 0, sizeof (maddr));
    maddr.sin_family = AF_INET;
    maddr.sin_addr.s_addr = INADDR_ANY;
    maddr.sin_port = 0;

#if defined(linux) && ! defined(USE_DLPI)
    /* linux pukes on bind, but seems to do the tracing stuff.
     * try it on your own risk.... */
#else
    if (bind(ipsock, (struct sockaddr *) &maddr, sizeof(maddr)) < 0) {
	PosixError("can not bind socket");
	return 0;
    }
#endif
    
#ifdef USE_DLPI
    /*
     * We cannot send a datagramm with a src-port of our own 
     * choice - too bad.
     * Therefore we cannot identify the icmp-port-unreachable 
     * with our homebrewed id. save the original port and check about:
     */
    
    if (getsockname(ipsock, (struct sockaddr *) &tmp_addr, &ta_len) < 0) {
	PosixError("can not get socket name");
	return 0;
    }

    src_port = ntohs(tmp_addr.sin_port);
    dsyslog(LOG_DEBUG, "dlpi: my port is %d (0x%lx)",
	    (int) src_port, (long) src_port);
    
    /*
     * Let's try to work around a funny bug: Open the complement
     * port, to allow reception of icmp-answers with byte-swapped
     * src-port field. If we cannot get this port, silently
     * ignore and continue normal.
     */
    
    if ((ipsock2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	PosixError("can not get udp socket #2 (ignored)");
	ipsock2 = -1;
    } else {
	struct sockaddr_in maddr2;
	  
	memset((char *) &maddr2, 0, sizeof (maddr2));
	maddr2.sin_family = AF_INET;
	maddr2.sin_addr.s_addr = INADDR_ANY;
	maddr2.sin_port = htons(SwapShort(src_port));
	
	if (bind(ipsock2, (struct sockaddr *) &maddr2, 
		 sizeof(maddr2)) < 0) {
	    PosixError("can not bind socket #2 (ignored)");
	    ipsock2 = -1;
	}
    }
    
    if (ipsock2 != -1) {
	dsyslog(LOG_DEBUG, "got the second port # %d (0x%x)\n", 
		SwapShort(src_port), SwapShort(src_port));
    }
    
#endif /* USE_DLPI */
    
    /* everything is fine: */
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * SetUnblock --
 *
 * 	This procedure sets a file descriptor to non-blocking mode.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	May set the write channel to non-blocking.
 *
 *----------------------------------------------------------------------
 */

static void
SetUnblock(int fd)
{
#if defined(EWOULDBLOCK) && defined(FNDELAY)
    if (fcntl(fd, F_SETFL, FNDELAY) < 0) {
	dsyslog(LOG_DEBUG, "can not set fd %d to non blocking - ignored", fd);
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * ReadJob --
 *
 *	This procedure reads a command from stdin.
 *
 * Results:
 *	Returns 0 on success and -1 on EOF or Error.
 * 
 * Side effects:
 *	May add a job to the global command queue.
 *      May increment the window-counter.
 *
 *----------------------------------------------------------------------
 */

static int
ReadJob()
{
    jobElem newJob, *job;
    int rc;
    static unsigned short ident_cnt = 0;

    job = &newJob;

    if (! ident_cnt) {
	/* init the ident count somewhat process dependent: */
	ident_cnt = (getpid() & 0xff) << 8;
    }

    rc = read(fileno(stdin), (char *) job, ICMP_PROTO_CMD_LEN);
    if (rc < 0) {
	PosixError("read failed");
	return -1;
    }
    if (rc == 0) {
	return -1;
    }
    if (rc != ICMP_PROTO_CMD_LEN) {
	syslog(LOG_ERR, "read returned %d instead of %d bytes", 
	       rc, ICMP_PROTO_CMD_LEN);
	return 0;
    }
    
    /* convert network-byteorder parameter fields: */
    job->size = ntohs(job->size);
    job->window = ntohs(job->window);
    
    /* init reply fields: */
    job->status = ICMP_STATUS_NOERROR;
    job->flags = 0;

    /* init internal values: */
    job->retry_ival = (1000 * job->u.c.timeout) / (job->u.c.retries + 1);

    job->probe_cnt = 0;
    job->time_sent.tv_sec = job->time_sent.tv_usec = 0;
    job->id = ident_cnt++;
    job->done = 0;
    if ((job->inServe = (job->window == 0))) {
        GetWindow(1);
    }
    if (job->type == ICMP_TYPE_TRACE) {
	job->p.trace.port = GetFreeUdpPort();
    }

    /* 
     * Alloc a new job struct; on error return a generror.
     */

    if (! (job = (jobElem *) malloc(sizeof(jobElem)))) {
	syslog(LOG_ERR, "out of memory - job rejected");
	newJob.status = ICMP_STATUS_GENERROR;
	if (write(fileno(stdout), (char *) &newJob, ICMP_PROTO_REPLY_LEN) 
	    != ICMP_PROTO_REPLY_LEN) {
	    PosixError("write failed");
	}
	return 0;
    } else {
	*job = newJob;
    }

    /* add to our job list: */
    job->next = job_list;
    job_list = job;

    dsyslog(LOG_DEBUG,
       "job %d: type=%d id=%u status=%d dest=%s size=%d retries=%d timeout=%d",
	    job->tid, job->type, job->id, job->status, inet_ntoa(job->addr),
	    job->size, job->u.c.retries, job->u.c.timeout);

    /* 
     * sanity checks: 
     */

    if (job->version != ICMP_PROTO_VERSION || job->type > 4) {
	syslog(LOG_ERR, "job %d: bad version %d or type %d",
	       job->tid, job->version, job->type);
	job->status = ICMP_STATUS_GENERROR;
	job->u.data = 0;
	job->done = 1;
    }

    if (job->size > max_data_len || job->size < MIN_DATALEN) {
	syslog(LOG_ERR, "job %d: bad size %d", job->tid, job->size);
	job->status = ICMP_STATUS_GENERROR;
	job->u.data = 0;
	job->done = 1;
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupNextRetryTimeout --
 *
 *	This procedure walks along the job queue and checks the
 *	remaining retry time intervals.
 *
 * Results:
 *	Returns in the argument the smallest retry timeout in ms.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
LookupNextRetryTimeout(struct timeval *tv)
{
    jobElem *job;
    struct timeval now;
    int min_diff = -1;

    gettime(&now, return);

    for (job = job_list; job; job = job->next) {
        int tdiff;

	if (! job->inServe) {
	    continue;
        }

	tdiff = time_diff(job->time_sent, now);
	if (tdiff > job->retry_ival) {
	    tdiff = 0;
	} else {
	    tdiff = job->retry_ival - tdiff;
	}
	
	dsyslog(LOG_DEBUG, "job %d: timeout is %d", job->tid, tdiff);
	if (min_diff < 0) {
	    min_diff = tdiff;
	} else if (tdiff < min_diff) {
	    min_diff = tdiff;
	}
    }

    if (min_diff < 0) {
	min_diff = 0;
    }

    tv->tv_sec = min_diff / 1000;
    tv->tv_usec = (min_diff % 1000) * 1000;

    dsyslog(LOG_DEBUG, "next timeout in %d ms", min_diff);
}

/*
 *----------------------------------------------------------------------
 *
 * SendPending --
 *
 *	This procedure resends all pending icmp and trace packets.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	Collects replies and may remove jobs from the global job_list.
 *
 *----------------------------------------------------------------------
 */

static void
SendPending()
{
    jobElem *job;
    struct timeval now;
    int window;

    gettime(&now, return);

retry:

    window = GetWindow(0);

    for (job = job_list; job; job = job->next) {
 
	/* 
	 * Check if we have space in the window to process this job:
	 */

        if (! job->inServe && job->window > window) {
	    job->inServe = 1;
	    window = GetWindow(1);
	} 

	/* 
	 * If this job is not in service, don't try to send:
	 */

        if (! job->inServe) {
	    dsyslog(LOG_DEBUG, "job %d: delayed (not in service)", job->tid);
	    continue;
	}

	dsyslog(LOG_DEBUG, 
		"job %d: done %d, probe_cnt %d, retries %d, tdiff %ld, ival %d",
		job->tid, job->done, job->probe_cnt, job->u.c.retries,
		time_diff(job->time_sent,now), job->retry_ival);

	/*
	 * Send a packet if the we have a try left and a timeout reached:
	 */

	if (! job->done && job->probe_cnt <= (job->u.c.retries + 1)
	      && time_diff(job->time_sent,now) > job->retry_ival) {

	      dsyslog(LOG_DEBUG, 
		      "job %d: sending probe # %d (%ld > %d) with win %d >= %d",
		      job->tid, job->probe_cnt, time_diff(job->time_sent,now), 
		      job->retry_ival, job->window, window);
	      
	      if (job->type == ICMP_TYPE_TRACE) {
		  SendTrace(job);
	      } else {
		  SendIcmp(job);
	      }

	      /*
	       * Process answers and wait delay time:
	       */

	      ReceivePending(job->u.c.delay);
	      goto retry;

	} else if (! job->done && job->probe_cnt > job->u.c.retries + 1) {

	    dsyslog(LOG_DEBUG, "job %d: failed after %d tries", 
		    job->tid, job->probe_cnt);
	    job->done = 1;
	    job->u.data = 0;
	    job->status |= ICMP_STATUS_TIMEOUT;
	    goto retry;
	}
    }

    /*
     * Respond for all jobs (success or timeout) that are done and
     * remove them from the job list.
     */

    CleanupJobs();
}

/*
 *----------------------------------------------------------------------
 *
 * DoOneEvent --
 *
 *	This procedure checks for new jobs, received ICMP answers, 
 *	and timeouts that trigger retries.
 *
 * Results:
 *	Returns 0 on success and -1 if EOF seen and all jobs are 
 *	processed.
 * 
 * Side effects:
 *	May change global job_list.
 *
 *----------------------------------------------------------------------
 */

static int
DoOneEvent()
{
    fd_set fds;
    struct timeval tv, *tvp;
    int rc;
    static int eof_seen = 0; 

    FD_ZERO(&fds);
    FD_SET(fileno(stdin), &fds);
    FD_SET(icsock, &fds);

    if (eof_seen && ! job_list) {
	dsyslog(LOG_DEBUG, "exiting on EOF");
	return -1;
    }

    /*
     * Calculate the timeout value for the select call.
     * We may block forever if the job list is empty.
     */
    
    if (! job_list) {
	tvp = 0;
    } else {
	LookupNextRetryTimeout(tvp = &tv);
    }

    /*
     * Wait for an event and process incoming messages.
     */

    rc = select(32, &fds, (fd_set *) 0, (fd_set *) 0, tvp);
    if (rc < 0) {
	if (errno != EINTR && errno != EAGAIN) {
	    PosixError("select failed");
	}
    } else {
	if (FD_ISSET(icsock, &fds)) {
	    ReceivePending(0);
	} 
	if (FD_ISSET(fileno(stdin), &fds)) {
	    if (ReadJob() < 0) {
		eof_seen = 1;
	    }
	}
    }

    /*
     * Finally, send all pending requests.
     */

    SendPending();
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 * 	This procedure initialize and manages processing of icmp/trace
 *	commands received from stdin and writes the results to stdout.
 *
 * Results:
 *	Returns 0 on EOF for the stdin channel or 1 on error.
 * 
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    int i;

    while (++argv, --argc > 0) {
	if (! strcmp (argv[0], "-D")) {
	    do_debug++;
	} else {
	    break;
	}
    }

    if (argc > 0) {
	fprintf(stderr, "use: nmicmpd [-D]\nnmicmpd version %s\n", version);
	fprintf(stderr, "  this demon is started and used by scotty(1)\n");
	fprintf(stderr, "  and its related icmp(n) command.\n");
	exit(-1);
    }

    /*
     * Clean up file descriptors that might have been inherited.
     */

    for (i = 3; i < 32; i++) {
	(void) close(i);
    }

    /*
     * Open the connection to the system logger. Beware: some old BSD
     * systems have an old system interface.
     */

#ifdef ultrix
    openlog("nmicmpd", LOG_PID);
#else
    openlog("nmicmpd", LOG_PID, LOG_USER);
#endif

    /* 
     * Fetch and initialize the icmp and the ip sockets: 
     */

    if (! InitSockets()) {
	return 1;
    }

    /* 
     * Switch back to normal user rights: 
     */

    setuid(getuid ());

    SetCpuLimit (10);

    /* 
     * If possible avoid to block while sending responses:
     */

    SetUnblock(fileno(stdout));

    /* NB set a resource limit to guard against an undiagnosed infinite
     * loop. */

    /* 
     * This is the main loop. Process incoming jobs until nothing
     * is left.
     */

    while (! DoOneEvent()) {
	continue;
    }

    return 0;
}


