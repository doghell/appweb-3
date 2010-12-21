/*
 *  listen.c -- Listen for client connections.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/
/*
 *  Listen on an ipAddress:port. NOTE: ipAddr may be empty which means bind to all addresses.
 */
MaListen *maCreateListen(MaServer *server, cchar *ipAddr, int port, int flags)
{
    MaListen    *listen;

    mprAssert(ipAddr);
    mprAssert(port > 0);

    listen = mprAllocObjZeroed(server, MaListen);
    if (listen == 0) {
        return 0;
    }
    listen->server = server;
    listen->flags = flags;
    listen->port = port;
    listen->ipAddr = mprStrdup(listen, ipAddr);
    listen->flags = flags;
    return listen;
}


int maStartListening(MaListen *listen)
{
    MaHttp      *http;
    cchar       *proto;
    char        *ipAddr;
    int         rc;

    listen->sock = mprCreateSocket(listen, listen->ssl);
    if (mprOpenServerSocket(listen->sock, listen->ipAddr, listen->port, (MprSocketAcceptProc) maAcceptConn, listen->server,
            MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD) < 0) {
        mprError(listen, "Can't open a socket on %s, port %d", listen->ipAddr, listen->port);
        return MPR_ERR_CANT_OPEN;
    }
    proto = mprIsSocketSecure(listen->sock) ? "HTTPS" : "HTTP";
    ipAddr = listen->ipAddr;
    if (ipAddr == 0 || *ipAddr == '\0') {
        ipAddr = "*";
    }
    mprLog(listen, MPR_CONFIG, "Listening for %s on %s:%d", proto, ipAddr, listen->port);

    http = listen->server->http;
    if (http->listenCallback) {
        if ((rc = (http->listenCallback)(http, listen)) < 0) {
            return rc;
        }
    }
    return 0;
}


int maStopListening(MaListen *listen)
{
    if (listen->sock) {
        mprFree(listen->sock);
        listen->sock = 0;
    }
    return 0;
}


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
    vim: sw=8 ts=8 expandtab

    @end
 */
