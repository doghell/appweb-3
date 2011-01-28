/*
 *  server.c -- Server Class to manage a single server
 *
 *  An instance of the MaServer Class may be created for each http.conf file. Each server can manage multiple hosts
 *  (standard, virtual or SSL). This file parses the http.conf file and creates all the necessary MaHost, MaDir and
 *  MaLocation objects to manage the server's operation.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

#if BLD_UNIX_LIKE
static int allDigits(cchar *s);
#endif
static void initLimits(MaHttp *http);
static int  httpDestructor(MaHttp *http);

/************************************ Code ************************************/
/*
 *  Create a web server described by a config file. 
 */
MaHttp *maCreateWebServer(cchar *configFile)
{
    Mpr         *mpr;
    MaHttp      *http;
    MaServer    *server;

    /*
     *  Initialize and start the portable runtime services.
     */
    if ((mpr = mprCreate(0, NULL, NULL)) == 0) {
        mprError(mpr, "Can't create the web server runtime");
        return 0;
    }
    if (mprStart(mpr, 0) < 0) {
        mprError(mpr, "Can't start the web server runtime");
        return 0;
    }
    http = maCreateHttp(mpr);
    if ((server = maCreateServer(http, configFile, NULL, NULL, -1)) == 0) {
        mprError(mpr, "Can't create the web server");
        return 0;
    }
    if (maParseConfig(server, configFile) < 0) {
        mprError(mpr, "Can't parse the config file %s", configFile);
        return 0;
    }
    return http;
}


/*
 *  Service requests for a web server.
 */
int maServiceWebServer(MaHttp *http)
{
    if (maStartHttp(http) < 0) {
        mprError(http, "Can't start the web server");
        return MPR_ERR_CANT_CREATE;
    }
    mprServiceEvents(mprGetDispatcher(http), -1, MPR_SERVICE_EVENTS | MPR_SERVICE_IO);
    maStopHttp(http);
    return 0;
}


/*
 *  Run a web server using a config file. 
 */
int maRunWebServer(cchar *configFile)
{
    MaHttp      *http;

    if ((http = maCreateWebServer(configFile)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    return maServiceWebServer(http);
}


int maRunSimpleWebServer(cchar *ipAddr, int port, cchar *docRoot)
{
    Mpr         *mpr;
    MaHttp      *http;
    MaServer    *server;

    /*
     *  Initialize and start the portable runtime services.
     */
    if ((mpr = mprCreate(0, NULL, NULL)) == 0) {
        mprError(mpr, "Can't create the web server runtime");
        return MPR_ERR_CANT_CREATE;
    }
    if (mprStart(mpr, 0) < 0) {
        mprError(mpr, "Can't start the web server runtime");
        return MPR_ERR_CANT_INITIALIZE;
    }

    /*
     *  Create the HTTP object.
     */
    if ((http = maCreateHttp(mpr)) == 0) {
        mprError(mpr, "Can't create the web server http services");
        return MPR_ERR_CANT_INITIALIZE;
    }

    /*
     *  Create and start the HTTP server. Give the server a name of "default" and define "." as 
     *  the default serverRoot, ie. the directory with the server configuration files.
     */
    server = maCreateServer(http, ipAddr, ".", ipAddr, port);
    if (server == 0) {
        mprError(mpr, "Can't create the web server");
        return MPR_ERR_CANT_CREATE;
    }
    maSetDocumentRoot(server->defaultHost, docRoot);
    
    if (maStartHttp(http) < 0) {
        mprError(mpr, "Can't start the web server");
        return MPR_ERR_CANT_CREATE;
    }
    mprServiceEvents(mprGetDispatcher(mpr), -1, MPR_SERVICE_EVENTS | MPR_SERVICE_IO);
    maStopHttp(http);
    mprFree(mpr);
    return 0;
}


MaHttp *maCreateHttp(MprCtx ctx)
{
    MaHttp      *http;

    http = mprAllocObjWithDestructorZeroed(ctx, MaHttp, httpDestructor);
    if (http == 0) {
        return 0;
    }
    mprGetMpr(ctx)->appwebHttpService = http;
    http->servers = mprCreateList(http);
    http->stages = mprCreateHash(http, 0);

#if BLD_FEATURE_MULTITHREAD
    http->mutex = mprCreateLock(http);
#endif

    initLimits(http);

#if BLD_UNIX_LIKE
{
    struct passwd   *pp;
    struct group    *gp;

    http->uid = getuid();
    if ((pp = getpwuid(http->uid)) == 0) {
        mprError(http, "Can't read user credentials: %d. Check your /etc/passwd file.", http->uid);
    } else {
        http->username = mprStrdup(http, pp->pw_name);
    }

    http->gid = getgid();
    if ((gp = getgrgid(http->gid)) == 0) {
        mprError(http, "Can't read group credentials: %d. Check your /etc/group file", http->gid);
    } else {
        http->groupname = mprStrdup(http, gp->gr_name);
    }
}
#else
    http->uid = http->gid = -1;
#endif

#if BLD_FEATURE_SEND
    maOpenSendConnector(http);
#endif
#if BLD_FEATURE_NET
    maOpenNetConnector(http);
#endif
    maOpenPassHandler(http);
    return http;
}


static int httpDestructor(MaHttp *http)
{
    maUnloadStaticModules(http);
    return 0;
}


void maAddServer(MaHttp *http, MaServer *server)
{
    mprAddItem(http->servers, server);
}


void maSetDefaultServer(MaHttp *http, MaServer *server)
{
    http->defaultServer = server;
}


MaServer *maLookupServer(MaHttp *http, cchar *name)
{
    MaServer    *server;
    int         next;

    for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (strcmp(server->name, name) == 0) {
            return server;
        }
    }
    return 0;
}


void maRegisterStage(MaHttp *http, MaStage *stage)
{
    mprAddHash(http->stages, stage->name, stage);
}


#if UNUSED
int maRemoveStage(MaHttp *http, cchar *name)
{
    return mprRemoveHash(http->stages, name);
}
#endif


MaStage *maLookupStage(MaHttp *http, cchar *name)
{
    return (MaStage*) mprLookupHash(http->stages, name);
}


void *maLookupStageData(MaHttp *http, cchar *name)
{
    MaStage     *stage;

    stage = (MaStage*) mprLookupHash(http->stages, name);
    return stage->stageData;
}


void maSetForkCallback(MaHttp *http, MprForkCallback callback, void *data)
{
    http->forkCallback = callback;
    http->forkData = data;
}


int maStartHttp(MaHttp *http)
{
    MaServer    *server;
    int         next;

    /*
     *  Start servers (and hosts)
     */
    for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (maStartServer(server) < 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }

    return 0;
}


int maStopHttp(MaHttp *http)
{
    MaServer    *server;
    int         next;

    for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        maStopServer(server);
    }
    return 0;
}


int maSetHttpUser(MaHttp *http, cchar *newUser)
{
#if BLD_UNIX_LIKE
    struct passwd   *pp;

    if (allDigits(newUser)) {
        http->uid = atoi(newUser);
        if ((pp = getpwuid(http->uid)) == 0) {
            mprError(http, "Bad user id: %d", http->uid);
            return MPR_ERR_CANT_ACCESS;
        }
        newUser = pp->pw_name;

    } else {
        if ((pp = getpwnam(newUser)) == 0) {
            mprError(http, "Bad user name: %s", newUser);
            return MPR_ERR_CANT_ACCESS;
        }
        http->uid = pp->pw_uid;
    }
#endif

    mprFree(http->username);
    http->username = mprStrdup(http, newUser);

    return 0;
}


int maSetHttpGroup(MaHttp *http, cchar *newGroup)
{
#if BLD_UNIX_LIKE
    struct group    *gp;

    if (allDigits(newGroup)) {
        http->gid = atoi(newGroup);
        if ((gp = getgrgid(http->gid)) == 0) {
            mprError(http, "Bad group id: %d", http->gid);
            return MPR_ERR_CANT_ACCESS;
        }
        newGroup = gp->gr_name;

    } else {
        if ((gp = getgrnam(newGroup)) == 0) {
            mprError(http, "Bad group name: %s", newGroup);
            return MPR_ERR_CANT_ACCESS;
        }
        http->gid = gp->gr_gid;
    }
#endif

    mprFree(http->groupname);
    http->groupname = mprStrdup(http, newGroup);

    return 0;
}


int maApplyChangedUser(MaHttp *http)
{
#if BLD_UNIX_LIKE
    if (http->uid >= 0) {
        if ((setuid(http->uid)) != 0) {
            mprError(http, "Can't change user to: %s: %d\n"
                "WARNING: This is a major security exposure", http->username, http->uid);
#if LINUX && PR_SET_DUMPABLE
        } else {
            prctl(PR_SET_DUMPABLE, 1);
#endif
        }
    }
#endif
    return 0;
}


int maApplyChangedGroup(MaHttp *http)
{
#if BLD_UNIX_LIKE
    if (http->gid >= 0) {
        if (setgid(http->gid) != 0) {
            mprError(http, "Can't change group to %s: %d\n"
                "WARNING: This is a major security exposure", http->groupname, http->gid);
#if LINUX && PR_SET_DUMPABLE
        } else {
            prctl(PR_SET_DUMPABLE, 1);
#endif
        }
    }
#endif
    return 0;
}


void maSetListenCallback(MaHttp *http, MaListenCallback fn)
{
    http->listenCallback = fn;
}


/*
 *  Load a module. Returns 0 if the modules is successfully loaded either statically or dynamically.
 */
int maLoadModule(MaHttp *http, cchar *name, cchar *libname)
{
    MprModule   *module;
    char        entryPoint[MPR_MAX_FNAME];
    char        *path;

    path = 0;

    module = mprLookupModule(http, name);
    if (module) {
        mprLog(http, MPR_CONFIG, "Activating module (Builtin) %s", name);
        return 0;
    }
    mprSprintf(entryPoint, sizeof(entryPoint), "ma%sInit", name);
    entryPoint[2] = toupper((int) entryPoint[2]);

    if (libname == 0) {
        path = mprStrcat(http, -1, "mod_", name, BLD_SHOBJ, NULL);
    }
    if (mprLoadModule(http, (libname) ? libname: path, entryPoint) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    mprLog(http, MPR_CONFIG, "Activating module (Loadable) %s", name);
    return 0;
}


int maUnloadModule(MaHttp *http, cchar *name)
{
    MprModule   *module;
    MaStage     *stage;

    if ((module = mprLookupModule(http, name)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    if (module->timeout) {
        //  MOB - stop all requests 
        if ((stage = maLookupStage(http, name)) != 0) {
            stage->flags |= MA_STAGE_UNLOADED;
        }
        mprUnloadModule(module);
    }
    return 0;
}


static void initLimits(MaHttp *http)
{
    MaLimits    *limits;

    limits = &http->limits;

    limits->maxBody = MA_MAX_BODY;
    limits->maxChunkSize = MA_MAX_CHUNK_SIZE;
    limits->maxResponseBody = MA_MAX_RESPONSE_BODY;
    limits->maxStageBuffer = MA_MAX_STAGE_BUFFER;
    limits->maxNumHeaders = MA_MAX_NUM_HEADERS;
    limits->maxHeader = MA_MAX_HEADERS;
    limits->maxUrl = MPR_MAX_URL;
    limits->maxUploadSize = MA_MAX_UPLOAD_SIZE;
    limits->maxThreads = MA_DEFAULT_MAX_THREADS;
    limits->minThreads = 0;

    /*
     *  Zero means use O/S defaults
     */
    limits->threadStackSize = 0;
}


/*
 *  Create a new server. There is typically only one server with one or more (virtual) hosts.
 */
MaServer *maCreateServer(MaHttp *http, cchar *name, cchar *root, cchar *ipAddr, int port)
{
    MaServer        *server;
    MaHostAddress   *hostAddress;
    MaListen        *listen;
    static int      staticModulesLoaded = 0;

    mprAssert(http);
    mprAssert(name && *name);

    server = mprAllocObjZeroed(http, MaServer);
    if (server == 0) {
        return 0;
    }

    server->hosts = mprCreateList(server);
    server->listens = mprCreateList(server);
    server->hostAddresses = mprCreateList(server);
    server->name = mprStrdup(server, name);
    server->http = http;

    maAddServer(http, server);
    maSetServerRoot(server, root);

    if (ipAddr && port > 0) {
        listen = maCreateListen(server, ipAddr, port, 0);
        maAddListen(server, listen);

        hostAddress = maCreateHostAddress(server, ipAddr, port);
        mprAddItem(server->hostAddresses, hostAddress);
    }
    maSetDefaultServer(http, server);
    if (!staticModulesLoaded) {
        staticModulesLoaded = 1;
        maLoadStaticModules(http);
    }
    return server;
}


int maStartServer(MaServer *server)
{
    MaHost      *host;
    MaListen    *listen;
    int         next, count, warned;

    /*
     *  Start the hosts
     */
    for (next = 0; (host = mprGetNextItem(server->hosts, &next)) != 0; ) {
        mprLog(server, 1, "Starting host named: \"%s\"", host->name);
        if (maStartHost(host) < 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }

    /*
     *  Listen to all required ipAddr:ports
     */
    warned = 0;
    count = 0;
    for (next = 0; (listen = mprGetNextItem(server->listens, &next)) != 0; ) {
        if (maStartListening(listen) < 0) {
            mprError(server, "Can't listen for HTTP on %s:%d", listen->ipAddr, listen->port);
            warned++;
            break;

        } else {
            count++;
        }
    }

    if (count == 0) {
        if (! warned) {
            mprError(server, "Server is not listening on any addresses");
        }
        return MPR_ERR_CANT_OPEN;
    }

    /*
     *  Now change user and group to the desired identities (user must be last)
     */
    if (maApplyChangedGroup(server->http) < 0 || maApplyChangedUser(server->http) < 0) {
        return MPR_ERR_CANT_COMPLETE;
    }

    return 0;
}


/*
 *  Stop the server and stop listening on all ports
 */
int maStopServer(MaServer *server)
{
    MaHost      *host;
    MaListen    *listen;
    int         next;

    for (next = 0; (listen = mprGetNextItem(server->listens, &next)) != 0; ) {
        maStopListening(listen);
    }
    for (next = 0; (host = mprGetNextItem(server->hosts, &next)) != 0; ) {
        maStopHost(host);
    }
    return 0;
}


void maAddHost(MaServer *server, MaHost *host)
{
    mprAddItem(server->hosts, host);
}


void maAddListen(MaServer *server, MaListen *listen)
{
    mprAddItem(server->listens, listen);
}


MaHost *maLookupHost(MaServer *server, cchar *name)
{
    MaHost  *host;
    int     next;

    for (next = 0; (host = mprGetNextItem(server->hosts, &next)) != 0; ) {
        if (strcmp(host->name, name) == 0) {
            return host;
        }
    }
    return 0;
}


/*
 *  Lookup a host address. If ipAddr is null or port is -1, then those elements are wild.
 */
MaHostAddress *maLookupHostAddress(MaServer *server, cchar *ipAddr, int port)
{
    MaHostAddress   *address;
    int             next;

    for (next = 0; (address = mprGetNextItem(server->hostAddresses, &next)) != 0; ) {
        if (address->port < 0 || port < 0 || address->port == port) {
            if (ipAddr == 0 || address->ipAddr == 0 || strcmp(address->ipAddr, ipAddr) == 0) {
                return address;
            }
        }
    }
    return 0;
}


/*
 *  Create the host addresses for a host. Called for hosts or for NameVirtualHost directives (host == 0).
 */
int maCreateHostAddresses(MaServer *server, MaHost *host, cchar *configValue)
{
    MaListen        *listen;
    MaHostAddress   *address;
    char            *ipAddrPort, *ipAddr, *value, *tok;
    char            addrBuf[MPR_MAX_IP_ADDR_PORT];
    int             next, port;

    address = 0;
    value = mprStrdup(server, configValue);
    ipAddrPort = mprStrTok(value, " \t", &tok);

    while (ipAddrPort) {
        if (mprStrcmpAnyCase(ipAddrPort, "_default_") == 0) {
            mprAssert(0);
            ipAddrPort = "*:*";
        }

        if (mprParseIp(server, ipAddrPort, &ipAddr, &port, -1) < 0) {
            mprError(server, "Can't parse ipAddr %s", ipAddrPort);
            continue;
        }
        mprAssert(ipAddr && *ipAddr);
        if (ipAddr[0] == '*') {
            ipAddr = mprStrdup(server, "");
        }

        /*
         *  For each listening endpiont,
         */
        for (next = 0; (listen = mprGetNextItem(server->listens, &next)) != 0; ) {
            if (port > 0 && port != listen->port) {
                continue;
            }
            if (listen->ipAddr[0] != '\0' && ipAddr[0] != '\0' && strcmp(ipAddr, listen->ipAddr) != 0) {
                continue;
            }

            /*
             *  Find the matching host address or create a new one
             */
            if ((address = maLookupHostAddress(server, listen->ipAddr, listen->port)) == 0) {
                address = maCreateHostAddress(server, listen->ipAddr, listen->port);
                mprAddItem(server->hostAddresses, address);
            }

            /*
             *  If a host is specified
             */
            if (host == 0) {
                maSetNamedVirtualHostAddress(address);

            } else {
                maInsertVirtualHost(address, host);
                if (listen->ipAddr[0] != '\0') {
                    mprSprintf(addrBuf, sizeof(addrBuf), "%s:%d", listen->ipAddr, listen->port);
                } else {
                    mprSprintf(addrBuf, sizeof(addrBuf), "%s:%d", ipAddr, listen->port);
                }
                maSetHostName(host, addrBuf);
            }
        }
        mprFree(ipAddr);
        ipAddrPort = mprStrTok(0, " \t", &tok);
    }

    if (host) {
        if (address == 0) {
            mprError(server, "No valid IP address for host %s", host->name);
            mprFree(value);
            return MPR_ERR_CANT_INITIALIZE;
        }
        if (maIsNamedVirtualHostAddress(address)) {
            maSetNamedVirtualHost(host);
        }
    }

    mprFree(value);

    return 0;
}


/*
 *  Set the Server Root directory. We convert path into an absolute path.
 */
void maSetServerRoot(MaServer *server, cchar *path)
{
    if (path == 0 || BLD_FEATURE_ROMFS) {
        path = ".";
    }
#if !VXWORKS
    /*
     *  VxWorks stat() is broken if using a network FTP server.
     */
    if (! mprPathExists(server, path, R_OK)) {
        mprError(server, "Can't access ServerRoot directory %s", path);
        return;
    }
#endif
    mprFree(server->serverRoot);
    server->serverRoot = mprGetAbsPath(server, path);
}


void maSetDefaultHost(MaServer *server, MaHost *host)
{
    server->defaultHost = host;
}


int maSplitConfigValue(MprCtx ctx, char **s1, char **s2, char *buf, int quotes)
{
    char    *next;

    if (maGetConfigValue(ctx, s1, buf, &next, quotes) < 0 || maGetConfigValue(ctx, s2, next, &next, quotes) < 0) {
        return MPR_ERR_BAD_SYNTAX;
    }
    if (*s1 == 0 || *s2 == 0) {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


int maGetConfigValue(MprCtx ctx, char **arg, char *buf, char **nextToken, int quotes)
{
    char    *endp;

    if (buf == 0) {
        return -1;
    }
    while (isspace((int) *buf)) {
        buf++;
    }

    if (quotes && *buf == '\"') {
        *arg = ++buf;
        if ((endp = strchr(buf, '\"')) != 0) {
            *endp++ = '\0';
        } else {
            return MPR_ERR_BAD_SYNTAX;
        }
        while ((int) isspace((int) *endp)) {
            endp++;
        }
        *nextToken = endp;
    } else {
        *arg = mprStrTok(buf, " \t\n", nextToken);
    }
    return 0;
}


/*
 *  Convenience function to create a new default host
 */
MaHost *maCreateDefaultHost(MaServer *server, cchar *docRoot, cchar *ipAddr, int port)
{
    MaHost          *host;
    MaListen        *listen;
    MaHostAddress   *address;

    if (ipAddr == 0) {
        /*
         *  If no IP:PORT specified, find the first listening endpoint. In this case, we expect the caller to
         *  have setup the lisenting endponts and to have added them to the host address hash.
         */
        listen = mprGetFirstItem(server->listens);
        if (listen) {
            ipAddr = listen->ipAddr;
            port = listen->port;

        } else {
            listen = maCreateListen(server, "localhost", MA_SERVER_DEFAULT_PORT_NUM, 0);
            maAddListen(server, listen);
            ipAddr = "localhost";
            port = MA_SERVER_DEFAULT_PORT_NUM;
        }
        host = maCreateHost(server, ipAddr, NULL);

    } else {
        /*
         *  Create a new listening endpoint
         */
        listen = maCreateListen(server, ipAddr, port, 0);
        maAddListen(server, listen);
        host = maCreateHost(server, ipAddr, NULL);
    }

    if (maOpenMimeTypes(host, "mime.types") < 0) {
        maAddStandardMimeTypes(host);
    }

    /*
     *  Insert the host and create a directory object for the docRoot
     */
    maAddHost(server, host);
    maInsertDir(host, maCreateBareDir(host, docRoot));
    maSetDocumentRoot(host, docRoot);

    /*
     *  Ensure we are in the hash lookup of all the addresses to listen to acceptWrapper uses this hash to find
     *  the host to serve the request.
     */
    address = maLookupHostAddress(server, ipAddr, port);
    if (address == 0) {
        address = maCreateHostAddress(server, ipAddr, port);
        mprAddItem(server->hostAddresses, address);
    }
    maInsertVirtualHost(address, host);

    if (server->defaultHost == 0) {
        server->defaultHost = host;
    }
    return host;
}



#if BLD_UNIX_LIKE
static int allDigits(cchar *s)
{
    return strspn(s, "1234567890") == strlen(s);
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
