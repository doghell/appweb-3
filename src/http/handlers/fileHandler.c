/*
 *  fileHandler.c -- Static file content handler
 *
 *  This handler manages static file based content such as HTML, GIF /or JPEG pages. It supports all methods including:
 *  GET, PUT, DELETE, OPTIONS and TRACE.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_FILE

/***************************** Forward Declarations ***************************/

static void handleDeleteRequest(MaQueue *q);
static int  readFileData(MaQueue *q, MaPacket *packet, MprOff pos, int size);
static void handlePutRequest(MaQueue *q);

/*********************************** Code *************************************/
/*
 *  Initialize a handler instance for the file handler.
 */
static void openFile(MaQueue *q)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaLocation      *location;
    MaConn          *conn;
    char            *date;

    conn = q->conn;
    resp = conn->response;
    req = conn->request;
    location = req->location;

    switch (req->method) {
    case MA_REQ_GET:
    case MA_REQ_HEAD:
    case MA_REQ_POST:
        if (resp->fileInfo.valid && resp->fileInfo.mtime) {
            date = maGetDateString(conn->arena, &resp->fileInfo);
            maSetHeader(conn, 0, "Last-Modified", date);
            mprFree(date);
        }
        if (maContentNotModified(conn)) {
            maSetResponseCode(conn, MPR_HTTP_CODE_NOT_MODIFIED);
            maOmitResponseBody(conn);
        } else {
            maSetEntityLength(conn, resp->fileInfo.size);
        }
        
        if (!resp->fileInfo.isReg && !resp->fileInfo.isLink) {
            maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't locate document: %s", req->url);
            
        } else if (!(resp->connector == conn->http->sendConnector)) {
            /*
             *  Open the file if a body must be sent with the response. The file will be automatically closed when 
             *  the response is freed. Cool eh?
             */
            resp->file = mprOpen(resp, resp->filename, O_RDONLY | O_BINARY, 0);
            if (resp->file == 0) {
                maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't open document: %s", resp->filename);
            }
        }
        break;
                
    case MA_REQ_PUT:
    case MA_REQ_DELETE:
        if (location->flags & MA_LOC_PUT_DELETE) {
            maOmitResponseBody(conn);
            break;
        }
        /* Method not supported - fall through */
            
    default:
        maFailRequest(q->conn, MPR_HTTP_CODE_BAD_METHOD, 
            "Method %s not supported by file handler at this location %s", req->methodName, location->prefix);
        break;
    }
}


/*
    Called after auth has run
 */
static void startFile(MaQueue *q)
{
    MaRequest       *req;
    MaLocation      *location;
    MaConn          *conn;

    conn = q->conn;
    req = conn->request;
    location = req->location;

    if (conn->requestFailed) {
        return;
    }
    switch (req->method) {
    case MA_REQ_PUT:
    case MA_REQ_DELETE:
        if (location->flags & MA_LOC_PUT_DELETE) {
            if (req->method == MA_REQ_PUT) {
                handlePutRequest(q);
            } else {
                handleDeleteRequest(q);
            }
            break;
        }
    }
}


static void runFile(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;
    MaResponse      *resp;
    MaPacket        *packet;

    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    
    maPutForService(q, maCreateHeaderPacket(q), 0);
   
    if (!(resp->flags & MA_RESP_NO_BODY) || req->method & MA_REQ_HEAD) {
        /*
         *  Create a single data packet based on the entity length.
         */
        packet = maCreateEntityPacket(q, 0, resp->entityLength, readFileData);
        if (!req->ranges) {
            resp->length = resp->entityLength;
        }
        maPutForService(q, packet, 0);
    }

    /*
     *  Append end-of-data packet. Signifies end of stream.
     */
    maPutForService(q, maCreateEndPacket(q), 1);
}


/*  
    Return true if the next queue will accept this packet. If not, then disable the queue's service procedure.
    This may split the packet if it exceeds the downstreams maximum packet size.
 */
static int prepPacket(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaQueue     *nextQ;
    int         size, nbytes;

    conn = q->conn;
    resp = conn->response;
    nextQ = q->nextQ;

    if (packet->esize > nextQ->packetSize) {
        maPutBack(q, maSplitPacket(resp, packet, nextQ->packetSize));
        size = nextQ->packetSize;
    } else {
        size = (int) packet->esize;
    }
    if ((size + nextQ->count) > nextQ->max) {
        /*  
            The downstream queue is full, so disable the queue and mark the downstream queue as full and service 
            Will re-enable via a writable event on the connection.
         */
        mprLog(q, 7, "Disable queue %s", q->owner);
        maDisableQueue(q);
        nextQ->flags |= MA_QUEUE_FULL;
        if (!(nextQ->flags & MA_QUEUE_DISABLED)) {
            maScheduleQueue(nextQ);
        }
        return 0;
    }
    nbytes = readFileData(q, packet, q->ioPos, size);
    if (nbytes > 0) {
        q->ioPos += nbytes;
    }
    return nbytes;
}


/*
 *  The service routine will be called when all incoming data has been received. This routine may flow control if the 
 *  downstream stage cannot accept all the file data. It will then be re-called as required to send more data.
 */
static void outgoingFileService(MaQueue *q)
{
    MaConn      *conn;
    MaRequest   *req;
    MaResponse  *resp;
    MaPacket    *packet;
    bool        usingSend;
    int         len;

    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    usingSend = resp->connector == conn->http->sendConnector;

    mprLog(q, 7, "\noutgoingFileService");

    for (packet = maGet(q); packet; packet = maGet(q)) {
        if (!req->ranges && !usingSend && packet->flags & MA_PACKET_DATA) {
            if ((len = prepPacket(q, packet)) <= 0) {
                if (len < 0) {
                    return;
                }
                maPutBack(q, packet);
                return;
            }
        }
        maPutNext(q, packet);
    }
    mprLog(q, 7, "outgoingFileService complete");
}


static void incomingFileData(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaRequest   *req;
    MaRange     *range;
    MprBuf      *buf;
    MprFile     *file;
    int         len;

    conn = q->conn;
    resp = conn->response;
    req = conn->request;

    file = (MprFile*) q->queueData;
    if (file == 0) {
        /* 
         *  Not a PUT so just ignore the incoming data.
         */
        maFreePacket(q, packet);
        return;
    }
    if (maGetPacketLength(packet) == 0) {
        /*
         *  End of input
         */
        mprFree(file);
        q->queueData = 0;
        return;
    }
    buf = packet->content;
    len = mprGetBufLength(buf);
    mprAssert(len > 0);

    range = req->inputRange;
    if (range && mprSeek(file, SEEK_SET, range->start) != range->start) {
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't seek to range start to %d", range->start);

    } else if (mprWrite(file, mprGetBufStart(buf), len) != len) {
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't PUT to %s", resp->filename);
    }
    maFreePacket(q, packet);
}


/*
 *  Populate a packet with file data
 */
static int readFileData(MaQueue *q, MaPacket *packet, MprOff pos, int size)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaRequest   *req;
    int         nbytes;

    conn = q->conn;
    resp = conn->response;
    req = conn->request;
    
    if (packet->content == 0 && (packet->content = mprCreateBuf(packet, size, size)) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    mprLog(q, 7, "readFileData size %Ld, pos %Ld", size, pos);
    
    if (pos >= 0) {
        mprSeek(resp->file, SEEK_SET, pos);
    }
    if ((nbytes = mprRead(resp->file, mprGetBufStart(packet->content), size)) != size) {
        /*
         *  As we may have sent some data already to the client, the only thing we can do is abort and hope the client 
         *  notices the short data.
         */
        maFailRequest(conn, MPR_HTTP_CODE_SERVICE_UNAVAILABLE, "Can't read file %s", resp->filename);
        return MPR_ERR_CANT_READ;
    }
    mprAdjustBufEnd(packet->content, nbytes);
    packet->esize -= nbytes;
    mprAssert(packet->esize == 0);
    return nbytes;
}


/*
 *  This is called to setup for a HTTP PUT request. It is called before receiving the post data via incomingFileData
 */
static void handlePutRequest(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;
    MaResponse      *resp;
    MprFile         *file;
    char            *path;

    mprAssert(q->pair->queueData == 0);

    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    path = resp->filename;

    if (req->ranges) {
        /*
         *  Open an existing file with fall-back to create
         */
        file = mprOpen(q, path, O_BINARY | O_WRONLY, 0644);
        if (file == 0) {
            file = mprOpen(q, path, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0644);
            if (file == 0) {
                maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't create the put URI");
                return;
            }
        } else {
            mprSeek(file, SEEK_SET, 0);
        }
    } else {
        file = mprOpen(q, path, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0644);
        if (file == 0) {
            maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't create the put URI");
            return;
        }
    }
    maSetResponseCode(conn, resp->fileInfo.isReg ? MPR_HTTP_CODE_NO_CONTENT : MPR_HTTP_CODE_CREATED);
    q->pair->queueData = (void*) file;
}


/*
 *  Immediate processing of delete requests
 */
static void handleDeleteRequest(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;
    char            *path;

    conn = q->conn;
    req = conn->request;
    path = conn->response->filename;

    if (!conn->response->fileInfo.isReg) {
        maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "URI not found");
        return;
    }
    if (mprDeletePath(q, path) < 0) {
        maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't remove URI");
        return;
    }
    maSetResponseCode(conn, MPR_HTTP_CODE_NO_CONTENT);
}


/*
 *  Dynamic module initialization
 */
MprModule *maFileHandlerInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *handler;

    module = mprCreateModule(http, "fileHandler", BLD_VERSION, NULL, NULL, NULL);
    if (module == 0) {
        return 0;
    }
    handler = maCreateHandler(http, "fileHandler", 
        MA_STAGE_GET | MA_STAGE_HEAD | MA_STAGE_POST | MA_STAGE_PUT | MA_STAGE_DELETE | MA_STAGE_VERIFY_ENTITY);
    if (handler == 0) {
        mprFree(module);
        return 0;
    }
    handler->open = openFile;
    handler->start = startFile;
    handler->run = runFile;
    handler->outgoingService = outgoingFileService;
    handler->incomingData = incomingFileData;
    http->fileHandler = handler;
    return module;
}


#else

MprModule *maFileHandlerInit(MaHttp *http, cchar *path)
{
    return 0;
}
#endif /* BLD_FEATURE_FILE */

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
