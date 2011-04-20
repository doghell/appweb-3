/*
 *  chunkFilter.c - Transfer chunk endociding filter.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_CHUNK
/********************************** Forwards **********************************/

static void setChunkPrefix(MaQueue *q, MaPacket *packet);

/*********************************** Code *************************************/

static void openChunk(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;

    conn = q->conn;
    req = conn->request;

    q->packetSize = (int) min(conn->http->limits.maxChunkSize, q->max);
    req->chunkState = MA_CHUNK_START;
}


static bool matchChunk(MaConn *conn, MaStage *handler, cchar *uri)
{
    return (conn->response->length <= 0) ? 1: 0;
}


/*
 *  Get the next chunk size. Chunked data format is:
 *      Chunk spec <CRLF>
 *      Data <CRLF>
 *      Chunk spec (size == 0) <CRLF>
 *      <CRLF>
 *  Chunk spec is: "HEX_COUNT; chunk length DECIMAL_COUNT\r\n". The "; chunk length DECIMAL_COUNT is optional.
 *  As an optimization, use "\r\nSIZE ...\r\n" as the delimiter so that the CRLF after data does not special consideration.
 *  Achive this by parseHeaders reversing the input start by 2.
 */
static void incomingChunkData(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaRequest   *req;
    MprBuf      *buf;
    char        *start, *cp;
    int         bad;

    conn = q->conn;
    req = conn->request;
    buf = packet->content;

    mprAssert(req->flags & MA_REQ_CHUNKED);

    if (packet->content == 0) {
        if (req->chunkState == MA_CHUNK_DATA) {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad chunk state");
            return;
        }
        req->chunkState = MA_CHUNK_EOF;
    }
    
    /*
     *  NOTE: the request head ensures that packets are correctly sized by packet inspection. The packet will never
     *  have more data than the chunk state expects.
     */
    switch (req->chunkState) {
    case MA_CHUNK_START:
        /*
         *  Validate:  "\r\nSIZE.*\r\n"
         */
        if (mprGetBufLength(buf) < 5) {
            break;
        }
        start = mprGetBufStart(buf);
        bad = (start[0] != '\r' || start[1] != '\n');
        for (cp = &start[2]; cp < buf->end && *cp != '\n'; cp++) {}
        if (*cp != '\n' && (cp - start) < 80) {
            break;
        }
        bad += (cp[-1] != '\r' || cp[0] != '\n');
        if (bad) {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return;
        }
        req->chunkSize = (int) mprAtoi(&start[2], 16);
        if (!isxdigit((int) start[2]) || req->chunkSize < 0) {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return;
        }
        mprAdjustBufStart(buf, (int) (cp - start + 1));
        req->remainingContent = req->chunkSize;
        if (req->chunkSize == 0) {
            req->chunkState = MA_CHUNK_EOF;
            /*
             *  We are lenient if the request does not have a trailing "\r\n" after the last chunk
             */
            cp = mprGetBufStart(buf);
            if (mprGetBufLength(buf) == 2 && *cp == '\r' && cp[1] == '\n') {
                mprAdjustBufStart(buf, 2);
            }
        } else {
            req->chunkState = MA_CHUNK_DATA;
        }
        mprAssert(mprGetBufLength(buf) == 0);
        maFreePacket(q, packet);
        mprLog(q, 5, "chunkFilter: start incoming chunk of %d bytes", req->chunkSize);
        break;

    case MA_CHUNK_DATA:
        mprAssert(maGetPacketLength(packet) <= req->chunkSize);
        mprLog(q, 5, "chunkFilter: data %d bytes, req->remainingContent %d", maGetPacketLength(packet), 
            req->remainingContent);
        maPutNext(q, packet);
        if (req->remainingContent == 0) {
            req->chunkState = MA_CHUNK_START;
            req->remainingContent = MA_BUFSIZE;
        }
        break;

    case MA_CHUNK_EOF:
        mprAssert(maGetPacketLength(packet) == 0);
        maPutNext(q, packet);
        mprLog(q, 5, "chunkFilter: last chunk");
        break;    

    default:
        mprAssert(0);
    }
}


/*
 *  Apply chunks to dynamic outgoing data. 
 */
static void outgoingChunkService(MaQueue *q)
{
    MaConn      *conn;
    MaPacket    *packet;
    MaResponse  *resp;

    conn = q->conn;
    resp = conn->response;

    if (!(q->flags & MA_QUEUE_SERVICED)) {
        /*
         *  If the last packet is the end packet, we have all the data. Thus we know the actual content length 
         *  and can bypass the chunk handler.
         */
        if (q->last->flags & MA_PACKET_END) {
            if (resp->chunkSize < 0 && resp->length <= 0) {
                /*  
                 *  Set the response content length and thus disable chunking -- not needed as we know the entity length.
                 */
                resp->length = q->count;
            }
        } else {
            if (resp->chunkSize < 0) {
                resp->chunkSize = (int) min(conn->http->limits.maxChunkSize, q->max);
            }
        }
    }

    if (resp->chunkSize <= 0) {
        maDefaultOutgoingServiceStage(q);
    } else {
        for (packet = maGet(q); packet; packet = maGet(q)) {
            if (!(packet->flags & MA_PACKET_HEADER)) {
                if (maGetPacketLength(packet) > resp->chunkSize) {
                    maResizePacket(q, packet, resp->chunkSize);
                }
            }
            if (!maWillNextQueueAccept(q, packet)) {
                maPutBack(q, packet);
                return;
            }
            if (!(packet->flags & MA_PACKET_HEADER)) {
                setChunkPrefix(q, packet);
            }
            maPutNext(q, packet);
        }
    }
}


static void setChunkPrefix(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;

    conn = q->conn;

    if (packet->prefix) {
        return;
    }
    packet->prefix = mprCreateBuf(packet, 32, 32);
    /*
     *  NOTE: prefixes don't count in the queue length. No need to adjust q->count
     */
    if (maGetPacketLength(packet)) {
        mprPutFmtToBuf(packet->prefix, "\r\n%x\r\n", maGetPacketLength(packet));
    } else {
        mprPutStringToBuf(packet->prefix, "\r\n0\r\n\r\n");
    }
}


/*
 *  Loadable module initialization
 */
MprModule *maChunkFilterInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *filter;

    module = mprCreateModule(http, "chunkFilter", BLD_VERSION, NULL, NULL, NULL);
    if (module == 0) {
        return 0;
    }
    filter = maCreateFilter(http, "chunkFilter", MA_STAGE_ALL);
    if (filter == 0) {
        mprFree(module);
        return 0;
    }
    http->chunkFilter = filter;
    filter->open = openChunk; 
    filter->match = matchChunk; 
    filter->outgoingService = outgoingChunkService; 
    filter->incomingData = incomingChunkData; 
    return module;
}


#else

MprModule *maChunkFilterInit(MaHttp *http, cchar *path)
{
    return 0;
}
#endif /* BLD_FEATURE_CHUNK */

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
