/*
 *  stages.c -- Stage module to processes HTTP requests.
 *
 *  Stages support the extensible and modular processing of HTTP requests. Handlers are a kind of stage that are the 
 *  first line processing of a request. Connectors are the last stage in a chain to send/receive data over a network.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

static void defaultOpen(MaQueue *q)
{
    MaResponse      *resp;

    resp = q->conn->response;
    q->packetSize = (resp->chunkSize > 0) ? min(q->max, resp->chunkSize): q->max;
}


static void defaultClose(MaQueue *q)
{
}


static int defaultParse(MaHttp *http, cchar *key, char *value, MaConfigState *state)
{
    mprAssert(http);
    mprAssert(key && *key);
    mprAssert(state);
    return 0;
}


/*
 *  The default put will put the packet on the service queue.
 */
static void outgoingData(MaQueue *q, MaPacket *packet)
{
    int     enableService;

    /*
     *  Handlers service routines must only be enabled if in the processing state.
     */
    enableService = !(q->stage->flags & MA_STAGE_HANDLER) || (q->conn->state & MPR_HTTP_STATE_PROCESSING) ? 1 : 0;
    maPutForService(q, packet, enableService);
}


/*
 *  Default incoming data routine. This is also used by the netConnector which is the default connector for incoming data.
 *  Simply transfer the data upstream to the next filter or handler.
 */
static void incomingData(MaQueue *q, MaPacket *packet)
{
    MaResponse  *resp;
    
    mprAssert(q);
    mprAssert(packet);
    
    resp = q->conn->response;

    if (q->nextQ->put) {
        maPutNext(q, packet);
    } else if (maGetPacketLength(packet)) { 
        maJoinForService(q, packet, 0);
    } else if ((q->stage->flags & MA_STAGE_HANDLER) && resp->handler->flags & MA_STAGE_VARS) {
        /*
         *  Do this just for handlers that don't want to define an incoming data handler but do want query/form vars (EGI)
         */
        maAddVarsFromQueue(q);
    }
}


/*
 *  The service routine runs when all input data has been received.
 */
void maDefaultOutgoingServiceStage(MaQueue *q)
{
    MaConn      *conn;
    MaPacket    *packet;

    conn = q->conn;

    for (packet = maGet(q); packet; packet = maGet(q)) {
        if (!maWillNextQueueAccept(q, packet)) {
            maPutBack(q, packet);
            return;
        }
        maPutNext(q, packet);
    }
}


static void incomingService(MaQueue *q)
{
}


MaStage *maCreateStage(MaHttp *http, cchar *name, int flags)
{
    MaStage     *stage;

    mprAssert(http);
    mprAssert(name && *name);

    stage = mprAllocObjZeroed(http, MaStage);
    if (stage == 0) {
        return 0;
    }
    stage->flags = flags;
    stage->name = mprStrdup(stage, name);

    /*
     *  Caller will selectively override
     */
    stage->open = defaultOpen;
    stage->close = defaultClose;
    stage->parse = defaultParse;
    
    stage->incomingData = incomingData;
    stage->incomingService = incomingService;
    
    stage->outgoingData = outgoingData;
    stage->outgoingService = maDefaultOutgoingServiceStage;

    maRegisterStage(http, stage);
    return stage;
}


MaStage *maCreateHandler(MaHttp *http, cchar *name, int flags)
{
    MaStage     *stage;
    
    stage = maCreateStage(http, name, flags);
    stage->flags |= MA_STAGE_HANDLER;
    return stage;
}


MaStage *maCreateFilter(MaHttp *http, cchar *name, int flags)
{
    MaStage     *stage;
    
    stage = maCreateStage(http, name, flags);
    stage->flags |= MA_STAGE_FILTER;
    return stage;
}


MaStage *maCreateConnector(MaHttp *http, cchar *name, int flags)
{
    MaStage     *stage;
    
    stage = maCreateStage(http, name, flags);
    stage->flags |= MA_STAGE_CONNECTOR;
    return stage;
}


bool maMatchFilterByExtension(MaFilter *filter, cchar *ext)
{
    return mprLookupHash(filter->extensions, ext) != 0;
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
