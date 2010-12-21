/*
 *  netConnector.c -- General network connector. 
 *
 *  The Network connector handles output data (only) from upstream handlers and filters. It uses vectored writes to
 *  aggregate output packets into fewer actual I/O requests to the O/S. 
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_NET
/**************************** Forward Declarations ****************************/

static void addPacketForNet(MaQueue *q, MaPacket *packet);
static void adjustNetVec(MaQueue *q, int written);
static int  buildNetVec(MaQueue *q);
static void freeNetPackets(MaQueue *q, int written);

/*********************************** Code *************************************/

static void netOutgoingService(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    int         written, errCode;

    conn = q->conn;
    resp = conn->response;
    
    while (q->first || q->ioIndex) {
        written = 0;
        if (q->ioIndex == 0 && buildNetVec(q) <= 0) {
            break;
        }
        /*
         *  Issue a single I/O request to write all the blocks in the I/O vector
         */
        mprAssert(q->ioIndex > 0);
        written = mprWriteSocketVector(conn->sock, q->iovec, q->ioIndex);
        mprLog(q, 5, "Net connector written %d", written);
        if (written < 0) {
            errCode = mprGetError();
            if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
                break;
            }
            if (errCode == EPIPE || errCode == ECONNRESET) {
                maDisconnectConn(conn);
            } else {
                mprLog(conn, 7, "mprVectorWriteSocket failed, error %d", errCode);
                maDisconnectConn(conn);
            }
            freeNetPackets(q, MAXINT);
            break;

        } else if (written == 0) {
            /* 
             * Socket full. Wait for an I/O event. Conn.c will setup listening for write events if the queue is non-empty
             */
            maRequestWriteBlocked(conn);
            break;

        } else if (written > 0) {
            resp->bytesWritten += written;
            freeNetPackets(q, written);
            adjustNetVec(q, written);
        }
    }
    if (q->ioCount == 0 && q->flags & MA_QUEUE_EOF) {
        maCompleteRequest(conn);
    }
}


/*
 *  Build the IO vector. Return the count of bytes to be written. Return -1 for EOF.
 */
static int buildNetVec(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaPacket    *packet;

    conn = q->conn;
    resp = conn->response;

    /*
     *  Examine each packet and accumulate as many packets into the I/O vector as possible. Leave the packets on the queue 
     *  for now, they are removed after the IO is complete for the entire packet.
     */
    for (packet = q->first; packet; packet = packet->next) {
        if (packet->flags & MA_PACKET_HEADER) {
            if (resp->chunkSize <= 0 && q->count > 0 && resp->length < 0) {
                /* Incase no chunking filter and we've not seen all the data yet */
                conn->keepAliveCount = 0;
            }
            maFillHeaders(conn, packet);
            q->count += maGetPacketLength(packet);

        } else if (maGetPacketLength(packet) == 0) {
            q->flags |= MA_QUEUE_EOF;
            if (packet->prefix == NULL) {
                break;
            }
            
        } else if (resp->flags & MA_RESP_NO_BODY) {
            maCleanQueue(q);
            continue;
        }
        if (q->ioIndex >= (MA_MAX_IOVEC - 2)) {
            break;
        }
        addPacketForNet(q, packet);
    }
    return q->ioCount;
}


/*
 *  Add one entry to the io vector
 */
static void addToNetVector(MaQueue *q, char *ptr, int bytes)
{
    mprAssert(bytes > 0);

    q->iovec[q->ioIndex].start = ptr;
    q->iovec[q->ioIndex].len = bytes;
    q->ioCount += bytes;
    q->ioIndex++;
}


/*
 *  Add a packet to the io vector. Return the number of bytes added to the vector.
 */
static void addPacketForNet(MaQueue *q, MaPacket *packet)
{
    MaResponse  *resp;
    MaConn      *conn;
    MprIOVec    *iovec;
    int         index, mask;

    conn = q->conn;
    resp = conn->response;
    iovec = q->iovec;
    index = q->ioIndex;

    mprAssert(q->count >= 0);
    mprAssert(q->ioIndex < (MA_MAX_IOVEC - 2));

    if (packet->prefix) {
        addToNetVector(q, mprGetBufStart(packet->prefix), mprGetBufLength(packet->prefix));
    }
    if (maGetPacketLength(packet) > 0) {
        addToNetVector(q, mprGetBufStart(packet->content), mprGetBufLength(packet->content));
    }
    mask = MA_TRACE_RESPONSE | ((packet->flags & MA_PACKET_HEADER) ? MA_TRACE_HEADERS : MA_TRACE_BODY);
    if (maShouldTrace(conn, mask)) {
        maTraceContent(conn, packet, 0, resp->bytesWritten, mask);
    }
}


static void freeNetPackets(MaQueue *q, int bytes)
{
    MaPacket    *packet;
    int         len;

    mprAssert(q->first);
    mprAssert(q->count >= 0);
    mprAssert(bytes >= 0);

    while ((packet = q->first) != 0) {
        if (packet->prefix) {
            len = mprGetBufLength(packet->prefix);
            len = min(len, bytes);
            mprAdjustBufStart(packet->prefix, len);
            bytes -= len;
            /* Prefixes don't count in the q->count. No need to adjust */
            if (mprGetBufLength(packet->prefix) == 0) {
                packet->prefix = 0;
            }
        }

        if (packet->content) {
            len = mprGetBufLength(packet->content);
            len = min(len, bytes);
            mprAdjustBufStart(packet->content, len);
            bytes -= len;
            q->count -= len;
            mprAssert(q->count >= 0);
        }
        if (packet->content == 0 || mprGetBufLength(packet->content) == 0) {
            /*
             *  This will remove the packet from the queue and will re-enable upstream disabled queues.
             */
            if ((packet = maGet(q)) != 0) {
                maFreePacket(q, packet);
            }
        }
        mprAssert(bytes >= 0);
        if (bytes == 0) {
            break;
        }
    }
}


/*
 *  Clear entries from the IO vector that have actually been transmitted. Support partial writes.
 */
static void adjustNetVec(MaQueue *q, int written)
{
    MprIOVec    *iovec;
    MaResponse  *resp;
    int         i, j, len;

    resp = q->conn->response;

    /*
     *  Cleanup the IO vector
     */
    if (written == q->ioCount) {
        /*
         *  Entire vector written. Just reset.
         */
        q->ioIndex = 0;
        q->ioCount = 0;

    } else {
        /*
         *  Partial write of an vector entry. Need to copy down the unwritten vector entries.
         */
        q->ioCount -= written;
        mprAssert(q->ioCount >= 0);
        iovec = q->iovec;
        for (i = 0; i < q->ioIndex; i++) {
            len = (int) iovec[i].len;
            if (written < len) {
                iovec[i].start += written;
                iovec[i].len -= written;
                break;
            } else {
                written -= len;
            }
        }
        /*
         *  Compact
         */
        for (j = 0; i < q->ioIndex; ) {
            iovec[j++] = iovec[i++];
        }
        q->ioIndex = j;
    }
}


/*
 *  Initialize the net connector
 */
int maOpenNetConnector(MaHttp *http)
{
    MaStage     *stage;

    stage = maCreateConnector(http, "netConnector", MA_STAGE_ALL);
    if (stage == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    stage->outgoingService = netOutgoingService;
    http->netConnector = stage;
    return 0;
}


#else 
void __dummyNetConnector() {}
#endif /* BLD_FEATURE_NET */

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
