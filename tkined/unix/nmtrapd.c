/*
 * nmtrapd.c --
 *
 * A simple SNMP trap-sink. Mainly for the Tnm SNMP code, but also
 * usable by other clients. The nmtrapd demon listens to the snmp-trap
 * port 162/udp and forwards the received event to connected clients
 * (like Tnm). Because the port 162 needs root access and the port
 * can be opened only once, the use of a simple forwarding daemon is
 * a good choice.
 *
 * The client can connect to the AF_UNIX domain stream socket
 * /tmp/.nmtrapd-<port> and will get the trap-packets in raw binary 
 * format. See the documentation for a description of the format.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * TODO: This code should buffer received traps internally so that
 *       we do not loose traps due to socket receive buffer overruns.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <syslog.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef FD_SETSIZE
#define FD_SETSIZE 32
#endif

/*
 * Default values for the SNMP trap port number, the name of 
 * the UNIX domain socket and the IP multicast address.
 */

#define SNMP_TRAP_PORT		162
#define SNMP_TRAP_NAME		"snmp-trap"
#define SNMP_TRAP_VERSION	0
#define SNMP_TRAP_MCIP		"244.0.0.1"
#define SNMP_FRWD_PORT		1702


/*
 *----------------------------------------------------------------------
 *
 * IgnorePipe --
 *
 *	This procedure is a dummy signal handler to catch SIGPIPE
 *	signals. Restart signalhandler for all these bozo's outside.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef SIGPIPE
static void
IgnorePipe(int dummy)
{
    signal(SIGPIPE, IgnorePipe);
}
#endif

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
 * InternalError --
 *
 *	This procedure logs an error message to the system
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
InternalError(char *msg)
{
    syslog(LOG_ERR, "%s", msg);
}

/*
 *----------------------------------------------------------------------
 *
 * ForwardTrap --
 *
 *	This procedure forwards a trap message to a client. See the
 *	documentation for a description of the message format.
 *
 * Results:
 *	Returns 0 on success and a value != 0 on error.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ForwardTrap(int fd, struct sockaddr_in *addr, char *buf, size_t len)
{
    struct {
	u_char version;
	u_char unused;
	short port;
	int addr;
	int length;
    } msg;
    
    msg.version = SNMP_TRAP_VERSION;
    msg.unused = 0;
    msg.port = addr->sin_port;
    msg.addr = addr->sin_addr.s_addr;
    msg.length = htonl(len);

    if (write(fd, (char *) &msg, sizeof(msg)) != sizeof(msg)
	|| write(fd, buf, len) != len) {
	return 0;
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This procedure forwards a trap to a client.
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
main(int argc, char *argv[])
{
    struct servent *se;
    struct sockaddr_in taddr, laddr;
    struct sockaddr_in saddr, daddr;
    int trap_s, serv_s, dlen, llen, rc, i;
    fd_set fds;
    static int cl_addr[FD_SETSIZE];
    char buf[8192];
    int go_on;
    int mcast_s = -1;
    char *name;
    unsigned short port;
    const int on = 1;

    /* 
     * Check the number of arguments. We accept an optional argument
     * which specifies the port number we are listening on.
     */

    if (argc > 2) {
	fprintf(stderr, "usage: nmtrapd [port]\n");
	exit(1);
    }

    if (argc == 2) {
	name = argv[1];
	port = atoi(argv[1]);
    } else {
	name = SNMP_TRAP_NAME;
	port = SNMP_TRAP_PORT;
    }
    
    /*
     * Get the port that we are going to listen to. Check that
     * we do not try to open a priviledged port number, with
     * the exception of the SNMP trap port.
     */

    if ((se = getservbyname(name, "udp"))) {
	port = ntohs(se->s_port);
    }

    if (port != SNMP_TRAP_PORT && port < 1024) {
	fprintf(stderr, "nmtrapd: access to port %d denied\n", port);
	exit(1);
    }

    /* 
     * Fork a new process so that the calling process returns
     * immediately. This makes sure that Tcl is not waiting for
     * this process to terminate when it exits.
     */

    switch (fork()) {
    case -1:
	PosixError("unable to fork daemon (ignored)");
	break;
    case 0:
	break;
    default:
	exit(0);
    }

    /*
     * Close any "leftover" FDs from the parent. There is a relatively
     * high probability that the parent will be scotty, and that the
     * client side of the scotty-nmtrapd connection is among the open
     * FDs. This is bad news if the parent scotty goes away, since
     * this will eventually cause nmtrapd to "block against itself" in
     * the "forward data to client" write() calls below, since nmtrapd
     * itself is not consuming data from the client side of the
     * leftover open socket.
     */

    for (i = 0; i < FD_SETSIZE; i++) {
	(void) close(i);
    }
    setsid();

    /*
     * Open the connection to the system logger. Beware: some old BSD
     * systems have an old syslog interface.
     */

#ifdef ultrix
    openlog("nmtrapd", LOG_PID);
#else
    openlog("nmtrapd", LOG_PID, LOG_USER);
#endif

    /*
     * Open and bind the normal trap socket: 
     */

    if ((trap_s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	PosixError("unable to open trap socket");
	exit(1);
    }
    
    taddr.sin_family = AF_INET;
    taddr.sin_port = htons(port);
    taddr.sin_addr.s_addr = INADDR_ANY;

#ifdef SO_REUSEADDR
    setsockopt(trap_s, SOL_SOCKET, SO_REUSEADDR, 
	       (char *) &on, sizeof(on));
#endif

    if (bind(trap_s, (struct sockaddr *) &taddr, sizeof(taddr)) < 0) {
	PosixError("unable to bind trap socket");
	exit(1);
    }

#ifdef HAVE_MULTICAST
    if ((mcast_s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	PosixError("unable to open multicast trap socket");
    }

    if (mcast_s > 0) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(SNMP_TRAP_MCIP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(mcast_s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq,
		       sizeof(mreq)) == -1) {
	    PosixError("unable to join multicast group");
	    close(mcast_s);
	    mcast_s = -1;
	}
    }

#ifdef SO_REUSEADDR
    if (mcast_s > 0) {
	setsockopt(mcast_s, SOL_SOCKET, SO_REUSEADDR, 
		   (char *) &on, sizeof(on));
    }
#endif

    if (mcast_s > 0) {
        struct sockaddr_in maddr;
        maddr.sin_family = AF_INET;
	maddr.sin_port = htons(port);
	maddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(mcast_s, (struct sockaddr*) &maddr, sizeof(maddr)) == -1) {
	    PosixError("unable to bind multicast trap socket");
	    close(mcast_s);
	    mcast_s = -1;
	}
    }
#endif

    /* 
     * Switch back to normal user rights: 
     */

    setuid(getuid ());

    if ((serv_s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	PosixError("unable to open server socket");
	exit(1);
    }

    memset((char *) &saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SNMP_FRWD_PORT);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(serv_s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
	PosixError("unable to bind server socket");
	exit(1);
    }

    if (listen(serv_s, 5) < 0) {
	PosixError("unable to listen on server socket");
	exit(1);
    }

#ifdef SIGPIPE
    signal(SIGPIPE, IgnorePipe);
#endif
    
    /*
     * If there is a steady stream of traps bound for this host, we
     * need to allow some time for the client (scotty) to connect to
     * us. Otherwise, nmtrapd will just exit when the first trap
     * message arrives. The client does 5 retries with 1 second
     * in-between, so sleeping for 3 should be enough to let the
     * client connect. There really ought to be a better way to do
     * this.
     */

    sleep(3);

    /*
     * Fine everything is ready; lets listen for events: 
     * the for(;;) loop aborts, if the last client went away.
     */

    for (go_on = 1; go_on; ) {

	FD_ZERO(&fds);
	FD_SET(trap_s, &fds);
	FD_SET(serv_s, &fds);
	if (mcast_s > 0) {
	    FD_SET(mcast_s, &fds);
	}
	
	/* fd's connected from clients. listen for EOF's: */
	for (i = 0; i < FD_SETSIZE; i++) {
	    if (cl_addr[i] > 0) {
		FD_SET(i, &fds);
	    }
	}
	
	rc = select(FD_SETSIZE, &fds, (fd_set *) 0, (fd_set *) 0, 
		    (struct timeval *) 0);
	if (rc < 0) {
	    if (errno == EINTR || errno == EAGAIN) continue;
	    PosixError("select failed");
	}
	
	if (FD_ISSET(serv_s, &fds)) {
	    memset((char *) &daddr, 0, sizeof(daddr));
	    dlen = sizeof(daddr);
	    rc = accept(serv_s, (struct sockaddr *) &daddr, &dlen);
	    if (rc < 0) {
		PosixError("accept failed");
		continue;
	    }

	    /*
	     * Check for a potential buffer overflow if the accept()
	     * call returns a file descriptor larger than FD_SETSIZE.
	     */
	    
	    if (rc >= FD_SETSIZE) {
		InternalError("too many clients");
		close(rc);
		continue;
	    }
	    
	    cl_addr[rc] = 1;
	    
	} else if (FD_ISSET(trap_s, &fds)) {
	    llen = sizeof(laddr);
	    if ((rc = recvfrom(trap_s, buf, sizeof(buf), 0, 
			       (struct sockaddr *) &laddr, &llen)) < 0) {
		PosixError("unable to receive trap");
		continue;
	    }
	    
	    for (i = 0; i < FD_SETSIZE; i++) {
		if (cl_addr[i] > 0) {
		    if (! ForwardTrap(i, &laddr, buf, rc)) {
			cl_addr[i] = 0;
			close(i);
		    }
		}
	    }
	    
	    /* should we go on ? */
	    for (go_on = 0, i = 0; i < FD_SETSIZE; i++) {
		go_on += cl_addr[i] > 0;
	    }
	    
	} else if (mcast_s > 0 && FD_ISSET(mcast_s, &fds)) {
	    llen = sizeof(laddr);
	    if ((rc = recvfrom(mcast_s, buf, sizeof(buf), 0, 
			       (struct sockaddr *) &laddr, &llen)) < 0) {
		PosixError("unable to receive trap");
		continue;
	    }
	    
	    for (i = 0; i < FD_SETSIZE; i++) {
		if (cl_addr[i] > 0) {
		    if (! ForwardTrap(i, &laddr, buf, rc)) {
			cl_addr[i] = 0;
			close(i);
		    }
		}
	    }
	    
	    /* should we go on ? */
	    for (go_on = 0, i = 0; i < FD_SETSIZE; i++) {
		go_on += cl_addr[i] > 0;
	    }
	    
	} else {
	    /* fd's connected from clients. (XXX: should check for EOF): */
	    for (i = 0; i < FD_SETSIZE; i++) {
		if (cl_addr[i] > 0 && FD_ISSET(i, &fds)) {
		    cl_addr[i] = 0;
		    close(i);
		}
	    }
	    
	    /* should we go on ? */
	    for (go_on = 0, i = 0; i < FD_SETSIZE; i++) {
		go_on += cl_addr[i] > 0;
	    }
	}
	
    } /* end for (;;) */

    closelog();

    return 0;
}


