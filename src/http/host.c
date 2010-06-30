/*
 *  host.c -- Host class for all HTTP hosts
 *
 *  The Host class is used for the default HTTP server and for all virtual hosts (including SSL hosts).
 *  Many objects are controlled at the host level. Eg. URL handlers.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/****************************** Forward Declarations **************************/

#undef lock
#undef unlock
#define lock(host) mprLock(host->mutex)
#define unlock(host) mprUnlock(host->mutex)

static int  getRandomBytes(MaHost *host, char *buf, int bufsize);
static void hostTimer(MaHost *host, MprEvent *event);
static void updateCurrentDate(MaHost *host);

/*********************************** Code *************************************/
/*
 *  Create a host from scratch
 */
MaHost *maCreateHost(MaServer *server, cchar *ipAddrPort, MaLocation *location)
{
    MaHost      *host;

    host = mprAllocObjZeroed(server, MaHost);
    if (host == 0) {
        return 0;
    }

    host->aliases = mprCreateList(host);
    host->dirs = mprCreateList(host);
    host->connections = mprCreateList(host);
    host->locations = mprCreateList(host);

    if (ipAddrPort) {
        host->ipAddrPort = mprStrdup(server, ipAddrPort);
        host->name = mprStrdup(server, ipAddrPort);
    } else {
        host->ipAddrPort = 0;
        host->name = 0;
    }

    host->server = server;
    host->flags = MA_HOST_NO_TRACE;
    host->httpVersion = MPR_HTTP_1_1;
    host->timeout = MA_SERVER_TIMEOUT;
    host->limits = &server->http->limits;
    
    host->traceMask = MA_TRACE_REQUEST | MA_TRACE_RESPONSE | MA_TRACE_HEADERS;
    host->traceLevel = 3;
    host->traceMaxLength = INT_MAX;

    host->keepAliveTimeout = MA_KEEP_TIMEOUT;
    host->maxKeepAlive = MA_MAX_KEEP_ALIVE;
    host->keepAlive = 1;

    host->location = (location) ? location : maCreateBareLocation(host);
    maAddLocation(host, host->location);
    updateCurrentDate(host);

#if BLD_FEATURE_AUTH
    host->location->auth = maCreateAuth(host->location, host->location->auth);
#endif

#if BLD_FEATURE_MULTITHREAD
    host->mutex = mprCreateLock(host);
#endif
    return host;
}


/*
 *  Create a new virtual host and inherit settings from another host
 */
MaHost *maCreateVirtualHost(MaServer *server, cchar *ipAddrPort, MaHost *parent)
{
    MaHost      *host;

    host = mprAllocObjZeroed(server, MaHost);
    if (host == 0) {
        return 0;
    }

    host->parent = parent;
    host->connections = mprCreateList(host);

    if (ipAddrPort) {
        host->ipAddrPort = mprStrdup(server, ipAddrPort);
        host->name = mprStrdup(server, ipAddrPort);
    } else {
        host->ipAddrPort = 0;
        host->name = 0;
    }

    /*
     *  The aliases, dirs and locations are all copy-on-write
     */
    host->aliases = parent->aliases;
    host->dirs = parent->dirs;
    host->locations = parent->locations;
    host->server = parent->server;
    host->flags = parent->flags;
    host->httpVersion = parent->httpVersion;
    host->timeout = parent->timeout;
    host->limits = parent->limits;
    host->keepAliveTimeout = parent->keepAliveTimeout;
    host->maxKeepAlive = parent->maxKeepAlive;
    host->keepAlive = parent->keepAlive;
    host->accessLog = parent->accessLog;
    host->location = maCreateLocation(host, parent->location);

    host->traceMask = parent->traceMask;
    host->traceLevel = parent->traceLevel;
    host->traceMaxLength = parent->traceMaxLength;
    if (parent->traceInclude) {
        host->traceInclude = mprCopyHash(host, parent->traceInclude);
    }
    if (parent->traceExclude) {
        host->traceExclude = mprCopyHash(host, parent->traceExclude);
    }

    maAddLocation(host, host->location);
    updateCurrentDate(host);

#if BLD_FEATURE_MULTITHREAD
    host->mutex = mprCreateLock(host);
#endif

    return host;
}


int maStartHost(MaHost *host)
{
    char    ascii[MA_MAX_SECRET * 2 + 1];

    if (getRandomBytes(host, ascii, sizeof(ascii)) < 0) {
        //  Use the best we can get. getRandomBytes will report.
    }
    host->secret = mprStrdup(host, ascii);

#if BLD_FEATURE_ACCESS_LOG
    return maStartAccessLogging(host);
#else
    return 0;
#endif
}


int maStopHost(MaHost *host)
{
#if BLD_FEATURE_ACCESS_LOG
    return maStopAccessLogging(host);
#else
    return 0;
#endif
}


void maSetDocumentRoot(MaHost *host, cchar *dir)
{
    MaAlias     *alias;
    char        *doc;

    doc = host->documentRoot = maMakePath(host, dir);
    /*
     *  Create a catch-all alias
     */
    alias = maCreateAlias(host, "", doc, 0);
    maInsertAlias(host, alias);
}


/*
 *  Set the host name. Comes from the ServerName directive. Name should not contain "http://"
 */
void maSetHostName(MaHost *host, cchar *name)
{
    if (host->name) {
        mprFree(host->name);
    }
    host->name = mprStrdup(host, name);
}


void maSetHostIpAddrPort(MaHost *host, cchar *ipAddrPort)
{
    mprAssert(ipAddrPort);
    mprFree(host->ipAddrPort);

    if (*ipAddrPort == ':') {
        ++ipAddrPort;
    }
    if (isdigit((int) *ipAddrPort) && strchr(ipAddrPort, '.') == 0) {
        host->ipAddrPort = mprStrcat(host, -1, "127.0.0.1", ":", ipAddrPort, NULL);
    } else {
        host->ipAddrPort = mprStrdup(host, ipAddrPort);
    }
}


void maSetHttpVersion(MaHost *host, int version)
{
    host->httpVersion = version;
}


void maSetKeepAlive(MaHost *host, bool on)
{
    host->keepAlive = on;
}


void maSetKeepAliveTimeout(MaHost *host, int timeout)
{
    host->keepAliveTimeout = timeout;
}


void maSetMaxKeepAlive(MaHost *host, int timeout)
{
    host->maxKeepAlive = timeout;
}


void maSetNamedVirtualHost(MaHost *host)
{
    host->flags |= MA_HOST_NAMED_VHOST;
}


void maSecureHost(MaHost *host, struct MprSsl *ssl)
{
    MaListen    *lp;
    cchar       *hostIp;
    char        *ipAddr;
    int         port, next;

    host->secure = 1;

    hostIp = host->ipAddrPort;
    if (mprStrcmpAnyCase(hostIp, "_default_") == 0) {
        hostIp = (char*) "*:*";
    }

    mprParseIp(host, hostIp, &ipAddr, &port, -1);
   
    for (next = 0; (lp = mprGetNextItem(host->server->listens, &next)) != 0; ) {
        if (port > 0 && port != lp->port) {
            continue;
        }
        if (*lp->ipAddr && ipAddr && ipAddr[0] != '*' && strcmp(ipAddr, lp->ipAddr) != 0) {
            continue;
        }
#if BLD_FEATURE_SSL
        if (host->secure) {
            if (host->flags & MA_HOST_NAMED_VHOST) {
                mprError(host, "SSL does not support named virtual hosts");
                return;
            }
            lp->ssl = ssl;
        }
#endif
    }
}


void maSetHostTrace(MaHost *host, int level, int mask)
{
    host->traceMask = mask;
    host->traceLevel = level;
}


void maSetHostTraceFilter(MaHost *host, int len, cchar *include, cchar *exclude)
{
    char    *word, *tok, *line;

    host->traceMaxLength = len;

    if (include && strcmp(include, "*") != 0) {
        host->traceInclude = mprCreateHash(host, 0);
        line = mprStrdup(host, include);
        word = mprStrTok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddHash(host->traceInclude, word, host);
            word = mprStrTok(NULL, ", \t\r\n", &tok);
        }
        mprFree(line);
    }
    if (exclude) {
        host->traceExclude = mprCreateHash(host, 0);
        line = mprStrdup(host, exclude);
        word = mprStrTok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddHash(host->traceExclude, word, host);
            word = mprStrTok(NULL, ", \t\r\n", &tok);
        }
        mprFree(line);
    }
}


int maSetupTrace(MaHost *host, cchar *ext)
{
    if (host->traceLevel > mprGetLogLevel(host)) {
        return 0;
    }
    if (ext) {
        if (host->traceInclude && !mprLookupHash(host->traceInclude, ext)) {
            return 0;
        }
        if (host->traceExclude && mprLookupHash(host->traceExclude, ext)) {
            return 0;
        }
    }
    return host->traceMask;
}


void maSetTraceMethod(MaHost *host, bool on)
{
    if (on) {
        host->flags &= ~MA_HOST_NO_TRACE;
    } else {
        host->flags |= MA_HOST_NO_TRACE;
    }
}


void maSetVirtualHost(MaHost *host)
{
    host->flags |= MA_HOST_VHOST;
}


/*
 *  Set the default request timeout. This is the maximum time a request can run.
 *  No to be confused with the session timeout or the keep alive timeout.
 */
void maSetTimeout(MaHost *host, int timeout)
{
    host->timeout = timeout;
}


int maInsertAlias(MaHost *host, MaAlias *newAlias)
{
    MaAlias     *alias, *old;
    int         rc, next, index;

    if (mprGetParent(host->aliases) == host->parent) {
        host->aliases = mprDupList(host, host->parent->aliases);
    }

    /*
     *  Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc. But we sort redirects with
     *  status codes first.
     */
    for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
        rc = strcmp(newAlias->prefix, alias->prefix);
        if (rc == 0) {
            index = mprLookupItem(host->aliases, alias);
            old = (MaAlias*) mprGetItem(host->aliases, index);
            mprRemoveItem(host->aliases, alias);
            mprInsertItemAtPos(host->aliases, next - 1, newAlias);
            return 0;
            
        } else if (rc > 0) {
            if (newAlias->redirectCode >= alias->redirectCode) {
                mprInsertItemAtPos(host->aliases, next - 1, newAlias);
                return 0;
            }
        }
    }
    mprAddItem(host->aliases, newAlias);
    return 0;
}


int maInsertDir(MaHost *host, MaDir *newDir)
{
    MaDir       *dir;
    int         rc, next;

    mprAssert(newDir);
    mprAssert(newDir->path);
    
    if (mprGetParent(host->dirs) == host->parent) {
        host->dirs = mprDupList(host, host->parent->dirs);
    }

    /*
     *  Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc
     */
    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        mprAssert(dir->path);
        rc = strcmp(newDir->path, dir->path);
        if (rc == 0) {
            mprRemoveItem(host->dirs, dir);
            mprInsertItemAtPos(host->dirs, next - 1, newDir);
            return 0;

        } else if (rc > 0) {
            mprInsertItemAtPos(host->dirs, next - 1, newDir);
            return 0;
        }
    }

    mprAddItem(host->dirs, newDir);
    return 0;
}


int maAddLocation(MaHost *host, MaLocation *newLocation)
{
    MaLocation  *location;
    int         next, rc;

    mprAssert(newLocation);
    mprAssert(newLocation->prefix);
    
    if (mprGetParent(host->locations) == host->parent) {
        host->locations = mprDupList(host, host->parent->locations);
    }

    /*
     *  Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc
     */
    for (next = 0; (location = mprGetNextItem(host->locations, &next)) != 0; ) {
        rc = strcmp(newLocation->prefix, location->prefix);
        if (rc == 0) {
            mprRemoveItem(host->locations, location);
            mprInsertItemAtPos(host->locations, next - 1, newLocation);
            return 0;
        }
        if (strcmp(newLocation->prefix, location->prefix) > 0) {
            mprInsertItemAtPos(host->locations, next - 1, newLocation);
            return 0;
        }
    }
    mprAddItem(host->locations, newLocation);

    return 0;
}


MaAlias *maGetAlias(MaHost *host, cchar *uri)
{
    MaAlias     *alias;
    int         next;

    if (uri) {
        for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
            if (strncmp(alias->prefix, uri, alias->prefixLen) == 0) {
                if (uri[alias->prefixLen] == '\0' || uri[alias->prefixLen] == '/') {
                    return alias;
                }
            }
        }
    }
    /*
     *  Must always return an alias. The last is the catch-all.
     */
    return mprGetLastItem(host->aliases);
}


MaAlias *maLookupAlias(MaHost *host, cchar *prefix)
{
    MaAlias     *alias;
    int         next;

    for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
        if (strcmp(alias->prefix, prefix) == 0) {
            return alias;
        }
    }
    return 0;
}


/*
 *  Find an exact dir match
 */
MaDir *maLookupDir(MaHost *host, cchar *pathArg)
{
    MaDir       *dir;
    char        *path, *tmpPath;
    int         next, len;

    if (!mprIsAbsPath(host, pathArg)) {
        path = tmpPath = mprGetAbsPath(host, pathArg);
    } else {
        path = (char*) pathArg;
        tmpPath = 0;
    }
    len = (int) strlen(path);

    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        if (dir->path != 0) {
            if (mprSamePath(host, dir->path, path)) {
                mprFree(tmpPath);
                return dir;
            }
        }
    }
    mprFree(tmpPath);
    return 0;
}


/*
 *  Find the directory entry that this file (path) resides in. path is a physical file path. We find the most specific
 *  (longest) directory that matches. The directory must match or be a parent of path. Not called with raw files names.
 *  They will be lower case and only have forward slashes. For windows, the will be in cannonical format with drive
 *  specifiers.
 */
MaDir *maLookupBestDir(MaHost *host, cchar *path)
{
    MaDir   *dir;
    int     next, len, dlen;

    len = (int) strlen(path);

    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        dlen = dir->pathLen;
        if (mprSamePathCount(host, dir->path, path, dlen)) {
            if (dlen >= 0) {
                return dir;
            }
        }
    }
    return 0;
}


MaLocation *maLookupLocation(MaHost *host, cchar *prefix)
{
    MaLocation      *location;
    int             next;

    for (next = 0; (location = mprGetNextItem(host->locations, &next)) != 0; ) {
        if (strcmp(prefix, location->prefix) == 0) {
            return location;
        }
    }
    return 0;
}


MaLocation *maLookupBestLocation(MaHost *host, cchar *uri)
{
    MaLocation  *location;
    int         next, rc;

    if (uri) {
        for (next = 0; (location = mprGetNextItem(host->locations, &next)) != 0; ) {
            rc = strncmp(location->prefix, uri, location->prefixLen);
            if (rc == 0 && uri[location->prefixLen] == '/') {
                return location;
            }
        }
    }
    return mprGetLastItem(host->locations);
}


static int getRandomBytes(MaHost *host, char *buf, int bufsize)
{
    MprTime     now;
    char        *hex = "0123456789abcdef";
    char        *ap, *cp;
    char        bytes[MA_MAX_SECRET], *bp;
    int         i, pid;

    mprLog(host, 8, "Get random bytes");

    memset(bytes, 0, sizeof(bytes));

    /*
     *  Create a random secret for use in authentication. Don't block.
     */
    if (mprGetRandomBytes(host, bytes, sizeof(bytes), 0) < 0) {

        mprError(host, "Can't get sufficient random data for secure SSL operation. If SSL is used, it will not be secure.");

        now = mprGetTime(host); 
        pid = (int) getpid();
        cp = (char*) &now;
        bp = bytes;
        for (i = 0; i < sizeof(now) && bp < &bytes[MA_MAX_SECRET]; i++) {
            *bp++= *cp++;
        }
        cp = (char*) &now;
        for (i = 0; i < sizeof(pid) && bp < &bytes[MA_MAX_SECRET]; i++) {
            *bp++ = *cp++;
        }
    }

    for (i = 0, ap = buf; ap < &buf[bufsize - 1] && i < sizeof(bytes); i++) {
        *ap++ = hex[(bytes[i] >> 4) & 0xf];
        *ap++ = hex[bytes[i] & 0xf];
    }
    *ap = '\0';

    mprLog(host, 8, "Got %d random bytes", (int) sizeof(bytes));

    return 0;
}


/*
 *  Start the host timer. This may create multiple timers -- no worry. maAddConn does its best to only schedule one.
 */
static void startTimer(MaHost *host)
{
    host->timer = mprCreateTimerEvent(mprGetDispatcher(host), (MprEventProc) hostTimer, 
        MA_TIMER_PERIOD, MPR_NORMAL_PRIORITY, host, MPR_EVENT_CONTINUOUS);
}


/*
 *  The host timer does maintenance activities and will fire per second while there is active requests.
 *  When multi-threaded, the host timer runs as an event off the service thread. Because we lock the host here,
 *  connections cannot be deleted while we are modifying the list.
 */
static void hostTimer(MaHost *host, MprEvent *event)
{
    MaConn      *conn;
    int         next, connCount;

    mprAssert(event);
    
    /*
     *  Locking ensures connections won't be deleted
     */
    lock(host);
    updateCurrentDate(host);
    mprLog(host, 5, "Connection Count %d", mprGetListCount(host->connections));

    /*
     *  Check for any expired connections
     */
    for (connCount = 0, next = 0; (conn = mprGetNextItem(host->connections, &next)) != 0; connCount++) {
        /*
         *  Workaround for a GCC bug when comparing two 64bit numerics directly. Need a temporary.
         */
        int64 diff = conn->expire - host->now;
        if (diff < 0 && !mprGetDebugMode(host)) {
            conn->keepAliveCount = 0;
            if (conn->request) {
                mprLog(host, 4, "Request timed out %s", conn->request->url);
            } else {
                mprLog(host, 4, "Idle connection timed out");
            }
            if (!conn->disconnected) {
                mprDisconnectSocket(conn->sock);
            }
        }
    }
    if (connCount == 0) {
        mprFree(event);
        host->timer = 0;
    }
    unlock(host);
}


void maAddConn(MaHost *host, MaConn *conn)
{
    lock(host);
    mprAddItem(host->connections, conn);
    conn->started = mprGetTime(conn);
    conn->seqno = host->connCount++;

    if ((host->now + MPR_TICKS_PER_SEC) < conn->started) {
        updateCurrentDate(host);
    }
    if (host->timer == 0) {
        startTimer(host);
    }
    unlock(host);
}


static void updateCurrentDate(MaHost *host)
{
    struct tm   tm;
    char        *oldDate;

    oldDate = host->currentDate;
    host->now = mprGetTime(host);
    host->currentDate = maGetDateString(host, 0);
    mprFree(oldDate);

    mprDecodeUniversalTime(host, &tm, host->now + (86400 * 1000));
    oldDate = host->expiresDate;
    host->expiresDate = mprFormatTime(host, MPR_HTTP_DATE, &tm);
    mprFree(oldDate);
}


/*
 *  See locking note for maAddConn
 */
void maRemoveConn(MaHost *host, MaConn *conn)
{
    lock(host);
    mprRemoveItem(host->connections, conn);
    unlock(host);
}


MaHostAddress *maCreateHostAddress(MprCtx ctx, cchar *ipAddr, int port)
{
    MaHostAddress   *hostAddress;

    mprAssert(ipAddr && ipAddr);
    mprAssert(port >= 0);

    hostAddress = mprAllocObjZeroed(ctx, MaHostAddress);
    if (hostAddress == 0) {
        return 0;
    }

    hostAddress->flags = 0;
    hostAddress->ipAddr = mprStrdup(hostAddress, ipAddr);
    hostAddress->port = port;
    hostAddress->vhosts = mprCreateList(hostAddress);

    return hostAddress;
}


void maInsertVirtualHost(MaHostAddress *hostAddress, MaHost *vhost)
{
    mprAddItem(hostAddress->vhosts, vhost);
}


bool maIsNamedVirtualHostAddress(MaHostAddress *hostAddress)
{
    return hostAddress->flags & MA_IPADDR_VHOST;
}


void maSetNamedVirtualHostAddress(MaHostAddress *hostAddress)
{
    hostAddress->flags |= MA_IPADDR_VHOST;
}


/*
 *  Look for a host with the right host name (ServerName)
 */
MaHost *maLookupVirtualHost(MaHostAddress *hostAddress, cchar *hostStr)
{
    MaHost      *host;
    int         next;

    for (next = 0; (host = mprGetNextItem(hostAddress->vhosts, &next)) != 0; ) {
        if (hostStr == 0 || strcmp(hostStr, host->name) == 0) {
            return host;
        }
    }
    return 0;
}

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
 *  Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
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
 *  @end
 */
