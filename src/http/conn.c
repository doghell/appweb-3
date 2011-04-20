/*
 *  conn.c -- Connection module to handle individual HTTP connections.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static int  connectionDestructor(MaConn *conn);
static inline MaPacket *getPacket(MaConn *conn, int *bytesToRead);
static void readEvent(MaConn *conn);
static int  ioEvent(MaConn *conn, int mask);
#if SHOW_REQUEST
static void showRequest(MprBuf *content, int nbytes, int len);
#else
#define showRequest(content, nbytes, len)
#endif

/*
 *  The connection lock is the master per connection/request lock. This lock is held when multithreaded 
 *  throughout an incoming I/O event. Handlers with callbacks (CGI) will assert this lock to prevent reentrant modification
 *  of the connection or request.
 */
#undef lock
#undef unlock
#define lock(conn) mprLock(conn->mutex)
#define unlock(conn) mprUnlock(conn->mutex)

/*********************************** Code *************************************/
/*
 *  Create a new connection object.
 */
static MaConn *createConn(MprCtx ctx, MaHost *host, MprSocket *sock, cchar *ipAddr, int port, MaHostAddress *address)
{
    MaConn      *conn;

    conn = mprAllocObjWithDestructorZeroed(ctx, MaConn, connectionDestructor);
    if (conn == 0) {
        return 0;
    }
    if (host->keepAlive) {
        conn->keepAliveCount = host->maxKeepAlive;
    }
    conn->http = host->server->http;
    conn->sock = sock;
    mprStealBlock(conn, sock);

    conn->state = MPR_HTTP_STATE_BEGIN;
    conn->timeout = host->timeout;
    conn->remotePort = port;
    conn->remoteIpAddr = mprStrdup(conn, ipAddr);
    conn->address = address;
    conn->host = host;
    conn->originalHost = host;
    conn->expire = mprGetTime(conn) + host->timeout;
    conn->eventMask = -1;

    maInitSchedulerQueue(&conn->serviceq);

#if BLD_FEATURE_MULTITHREAD
    conn->mutex = mprCreateLock(conn);
#endif
    return conn;
}


/*
 *  Cleanup a connection. Invoked automatically whenever the connection is freed.
 */
static int connectionDestructor(MaConn *conn)
{
    mprAssert(conn);
    mprAssert(conn->host);
    mprAssert(conn->sock);

    /*
     *  Must remove from the connection list first. This ensures that the host timer will not find the connection 
     *  nor mess with it anymore.
     */
    maRemoveConn(conn->host, conn);

    if (conn->sock) {
        mprLog(conn, 4, "Closing connection fd %d", conn->sock->fd);
        mprCloseSocket(conn->sock, conn->connectionFailed ? 0 : MPR_SOCKET_GRACEFUL);
        mprFree(conn->sock);
    }
    return 0;
}


/*
 *  Set the connection for disconnection when the I/O event returns. Setting keepAliveCount to zero will cause a 
 *  server-led disconnection.
 */
void maDisconnectConn(MaConn *conn)
{
    conn->canProceed = 0;
    conn->disconnected = 1;
    conn->keepAliveCount = 0;
    conn->requestFailed = 1;

    if (conn->response) {
        mprLog(conn, 4, "Disconnect conn fd %d", conn->sock ? conn->sock->fd : 0);
        maCompleteRequest(conn);
        maDiscardPipeData(conn);
    }
}


/*
 *  Prepare a connection after completing a request for a new request.
 */
void maPrepConnection(MaConn *conn)
{
    mprAssert(conn);

    conn->requestFailed = 0;
    conn->request = 0;
    conn->response = 0;
    conn->trace = 0;
    conn->state =  MPR_HTTP_STATE_BEGIN;
    conn->flags &= ~MA_CONN_CLEAN_MASK;
    conn->expire = conn->time + conn->host->keepAliveTimeout;
    conn->dedicated = 0;
    if (conn->sock) {
        mprSetSocketBlockingMode(conn->sock, 0);
    }
}


/*
 *  Accept a new client connection. If multithreaded, this will come in on a worker thread dedicated to this connection.
 *  This is called from the listen wait handler.
 */
int maAcceptConn(MprSocket *sock, MaServer *server, cchar *ip, int port)
{
    MaHostAddress   *address;
    MaHost          *host;
    MaConn          *conn;
    MprSocket       *listenSock;
    MprHeap         *arena;
    int             rc;

    mprAssert(server);
    mprAssert(sock);
    mprAssert(ip);
    mprAssert(port > 0);

    rc = 0;
    listenSock = sock->listenSock;

    mprLog(server, 4, "New connection from %s:%d for %s:%d %s",
        ip, port, listenSock->ipAddr, listenSock->port, listenSock->sslSocket ? "(secure)" : "");

    /*
     *  Map the address onto a suitable host to initially serve the request initially until we can parse the Host header.
     */
    address = (MaHostAddress*) maLookupHostAddress(server, listenSock->ipAddr, listenSock->port);

    if (address == 0 || (host = mprGetFirstItem(address->vhosts)) == 0) {
        mprError(server, "No host configured for request %s:%d", listenSock->ipAddr, listenSock->port);
        mprFree(sock);
        return 1;
    }
    arena = mprAllocHeap(host, "conn", 1, 0, NULL);
    if (arena == 0) {
        mprError(server, "Can't create connect arena object. Insufficient memory.");
        mprFree(sock);
        return 1;
    }

    conn = createConn(arena, host, sock, ip, port, address);
    if (conn == 0) {
        mprError(server, "Can't create connect object. Insufficient memory.");
        mprFree(sock);
        return 1;
    }
    conn->arena = arena;
    maAddConn(host, conn);

    mprSetSocketCallback(conn->sock, (MprSocketProc) ioEvent, conn, MPR_READABLE, MPR_NORMAL_PRIORITY);
 
#if BLD_FEATURE_MULTITHREAD
    mprEnableSocketEvents(listenSock);
#endif
    return rc;
}


/*
 *  IO event handler. If multithreaded, this will be run by a worker thread. NOTE: a request is not typically permanently 
 *  assigned to a worker thread. Each io event may be serviced by a different worker thread. The exception is CGI
 *  requests which block to wait for the child to complete (Needed on some platforms that don't permit cross thread
 *  waiting).
 */
static int ioEvent(MaConn *conn, int mask)
{
    mprAssert(conn);

    lock(conn);
    conn->time = mprGetTime(conn);

    mprLog(conn, 7, "ioEvent for fd %d, mask %d\n", conn->sock->fd);
    if (mask & MPR_WRITABLE) {
        maProcessWriteEvent(conn);
    }
    if (mask & MPR_READABLE) {
        readEvent(conn);
    }
    if (mprIsSocketEof(conn->sock) || conn->disconnected || conn->connectionFailed || 
            (conn->request == 0 && conn->keepAliveCount < 0)) {
        /*
         *  This will close the connection and free all connection resources. NOTE: we compare keepAliveCount with "< 0" 
         *  so that the client can have one more keep alive request. It should respond to the "Connection: close" and 
         *  thus initiate a client-led close. This reduces TIME_WAIT states on the server. Must unlock the connection 
         *  to allow pending callbacks to run and complete.
         */
        unlock(conn);
        maDestroyPipeline(conn);
        mprFree(conn->arena);
        return 1;
    }

    /*
     *  We allow read events even if the current request is not complete and does not have body data. The pipelined
     *  request will be buffered and be ready for when the current request completes.
     */
    maEnableConnEvents(conn, MPR_READABLE);
    unlock(conn);
    return 0;
}


/*
 *  Enable the connection's I/O events
 */
void maEnableConnEvents(MaConn *conn, int eventMask)
{
    if (conn->request) {
        if (conn->response->queue[MA_QUEUE_SEND].prevQ->first) {
            eventMask |= MPR_WRITABLE;
        }
    }
    mprLog(conn, 7, "Enable conn events mask %x", eventMask);
    conn->expire = conn->time;
    conn->expire += (conn->state == MPR_HTTP_STATE_BEGIN) ? conn->host->keepAliveTimeout : conn->host->timeout;
    eventMask &= conn->eventMask;
    mprSetSocketCallback(conn->sock, (MprSocketProc) ioEvent, conn, eventMask, MPR_NORMAL_PRIORITY);
}


/*
 *  Process a socket readable event
 */
static void readEvent(MaConn *conn)
{
    MaPacket    *packet;
    MprBuf      *content;
    int         nbytes, len;

    do {
        if ((packet = getPacket(conn, &len)) == 0) {
            break;
        }
        mprAssert(len > 0);
        content = packet->content;
        nbytes = mprReadSocket(conn->sock, mprGetBufEnd(content), len);
        showRequest(content, nbytes, len);
       
        if (nbytes > 0) {
            mprAdjustBufEnd(content, nbytes);
            maProcessReadEvent(conn, packet);
        } else {
            if (mprIsSocketEof(conn->sock)) {
                conn->dedicated = 0;
                if (conn->request) {
                    maProcessReadEvent(conn, packet);
                }
            } else if (nbytes < 0) {
                maFailConnection(conn, MPR_HTTP_CODE_COMMS_ERROR, "Communications read error");
            }
        }
    } while (!conn->disconnected && conn->dedicated);
}


/*
 *  Get the packet into which to read data. This may be owned by the connection or if mid-request, may be owned by the
 *  request. Also return in *bytesToRead the length of data to attempt to read.
 */
static inline MaPacket *getPacket(MaConn *conn, int *bytesToRead)
{
    MaPacket    *packet;
    MaRequest   *req;
    MprBuf      *content;
    MprOff      remaining;
    int         len;

    req = conn->request;
    len = MA_BUFSIZE;

    /*
     *  The input packet may have pipelined headers and data left from the prior request. It may also have incomplete
     *  chunk boundary data.
     */
    if ((packet = conn->input) == NULL) {
        conn->input = packet = maCreateConnPacket(conn, len);

    } else {
        content = packet->content;
        mprResetBufIfEmpty(content);
        if (req) {
            /*
             *  Don't read more than the remainingContent unless chunked. We do this to minimize requests split 
             *  across packets.
             */
            if (req->remainingContent) {
                remaining = req->remainingContent;
                if (req->flags & MA_REQ_CHUNKED) {
                    remaining = max(remaining, MA_BUFSIZE);
                }
                len = (int) min(remaining, MAXINT);
            }
            len = min(len, MA_BUFSIZE);
            mprAssert(len > 0);
            if (mprGetBufSpace(content) < len) {
                mprGrowBuf(content, len);
            }

        } else {
            /*
             *  Still reading the request headers
             */
            if (mprGetBufLength(content) >= conn->http->limits.maxHeader) {
                maFailConnection(conn, MPR_HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
                return 0;
            }
            if (mprGetBufSpace(content) < MA_BUFSIZE) {
                mprGrowBuf(content, MA_BUFSIZE);
            }
            len = mprGetBufSpace(content);
        }
    }
    mprAssert(len > 0);
    *bytesToRead = len;
    return packet;
}


void maDedicateThreadToConn(MaConn *conn)
{
    mprAssert(conn);

    conn->dedicated = 1;
    if (conn->sock) {
        mprSetSocketBlockingMode(conn->sock, 1);
    }
}


void *maGetHandlerQueueData(MaConn *conn)
{
    MaQueue     *q;

    q = &conn->response->queue[MA_QUEUE_SEND];
    return q->nextQ->queueData;
}


#if SHOW_REQUEST
static void showRequest(MprBuf *content, int nbytes, int len);
{
    mprLog(content, 5, "readEvent: read nbytes %d, bufsize %d", nbytes, len);
    char *buf = mprGetBufStart(content);
    buf[nbytes] = '\0';
    print("\n>>>>>>>>>>>>>>>>>>>>>>>>>");
    write(1, buf, nbytes);
    print("<<<<<<<<<<<<<<<<<<<<<<<<<\n");
}
#endif


/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
 *
 *  This software is distributed under commercial and open source licenses.
 *  You may use the GPL open source license described below or you may acquire
 *  a commercial license from Embedthis Software. You agree to be fully bound
 *  by the terms of either license. Consult the LICENSE.TXT distributed with
 *  this software for full details.
 *
 *  This software is open source; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version. See the GNU General Public License for more
 *  details at: http://www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http://www.embedthis.com
 *
 *  Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
