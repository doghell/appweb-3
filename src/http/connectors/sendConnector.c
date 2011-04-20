/*
 *  sendConnector.c -- Send file connector. 
 *
 *  The Sendfile connector supports the optimized transmission of whole static files. It uses operating system 
 *  sendfile APIs to eliminate reading the document into user space and multiple socket writes. The send connector 
 *  is not a general purpose connector. It cannot handle dynamic data or ranged requests. It does support chunked requests.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_SEND

/**************************** Forward Declarations ****************************/

static void addPacketForSend(MaQueue *q, MaPacket *packet);
static void adjustSendVec(MaQueue *q, int64 written);
static int64  buildSendVec(MaQueue *q);
static void freeSentPackets(MaQueue *q, int64 written);

/*********************************** Code *************************************/
/*
 *  Invoked to initialize the send connector for a request
 */
static void sendOpen(MaQueue *q)
{
    MaConn          *conn;
    MaResponse      *resp;

    conn = q->conn;
    resp = conn->response;

    if (!conn->requestFailed && !(resp->flags & MA_RESP_NO_BODY)) {
        resp->file = mprOpen(q, resp->filename, O_RDONLY | O_BINARY, 0);
        if (resp->file == 0) {
            maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't open document: %s", resp->filename);
        }
    }
}


/*
 *  Outgoing data service routine. May be called multiple times.
 */
static void sendOutgoingService(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    int         written;
    int         errCode;

    conn = q->conn;
    resp = conn->response;

    if (conn->sock == 0) {
        return;
    }

    /*
     *  Loop doing non-blocking I/O until blocked or all the packets received are written.
     */
    while (1) {
        /*
         *  Rebuild the iovector only when the past vector has been completely written. Simplifies the logic quite a bit.
         */
        written = 0;
        if (q->ioIndex == 0 && buildSendVec(q) <= 0) {
            break;
        }
        /*
         *  Write the vector and file data. Exclude the file entry in the io vector.
         */
        written = (int) mprSendFileToSocket(conn->sock, resp->file, q->ioPos, q->ioCount, q->iovec, q->ioIndex, NULL, 0);
        mprLog(q, 5, "Send connector written %d", written);
        if (written < 0) {
            errCode = mprGetError();
            if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
                maRequestWriteBlocked(conn);
                break;
            }
            if (errCode == EPIPE || errCode == ECONNRESET) {
                maDisconnectConn(conn);
            } else {
                mprLog(conn, 7, "mprSendFileToSocket failed, errCode %d", errCode);
                maDisconnectConn(conn);
            }
            freeSentPackets(q, MAXINT);
            break;

        } else if (written == 0) {
            /* Socket is full. Wait for an I/O event */
            maRequestWriteBlocked(conn);
            break;

        } else if (written > 0) {
            resp->bytesWritten += written;
            freeSentPackets(q, written);
            adjustSendVec(q, written);
        }
    }
    if (q->ioCount == 0 && q->flags & MA_QUEUE_EOF) {
        maCompleteRequest(conn);
    }
}


/*
 *  Build the IO vector. This connector uses the send file API which permits multiple IO blocks to be written with 
 *  file data. This is used to write transfer the headers and chunk encoding boundaries. Return the count of bytes to 
 *  be written.
 */
static int64 buildSendVec(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaPacket    *packet;

    conn = q->conn;
    resp = conn->response;

    mprAssert(q->ioIndex == 0);
    q->ioCount = 0;
    q->ioFile = 0;

    /*
     *  Examine each packet and accumulate as many packets into the I/O vector as possible. Can only have one data packet at
     *  a time due to the limitations of the sendfile API (on Linux). And the data packet must be after all the 
     *  vector entries. Leave the packets on the queue for now, they are removed after the IO is complete for the 
     *  entire packet.
     */
    for (packet = q->first; packet; packet = packet->next) {
        if (packet->flags & MA_PACKET_HEADER) {
            maFillHeaders(conn, packet);
            q->count += maGetPacketLength(packet);

        } else if (maGetPacketLength(packet) == 0 && packet->esize == 0) {
            q->flags |= MA_QUEUE_EOF;
            if (packet->prefix == NULL) {
                break;
            }
        } else if (resp->flags & MA_RESP_NO_BODY) {
            maDiscardData(q, 0);
            continue;
        }
        if (q->ioFile || q->ioIndex >= (MA_MAX_IOVEC - 2)) {
            break;
        }
        addPacketForSend(q, packet);
    }
    return q->ioCount;
}


/*
 *  Add one entry to the io vector
 */
static void addToSendVector(MaQueue *q, char *ptr, int bytes)
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
static void addPacketForSend(MaQueue *q, MaPacket *packet)
{
    MaResponse  *resp;
    MaConn      *conn;
    MprIOVec    *iovec;
    int         mask;

    conn = q->conn;
    resp = conn->response;
    iovec = q->iovec;
    
    mprAssert(q->count >= 0);
    mprAssert(q->ioIndex < (MA_MAX_IOVEC - 2));

    if (packet->prefix) {
        addToSendVector(q, mprGetBufStart(packet->prefix), mprGetBufLength(packet->prefix));
    }
    if (packet->esize > 0) {
        mprAssert(q->ioFile == 0);
        q->ioFile = 1;
        q->ioCount += packet->esize;

    } else if (maGetPacketLength(packet) > 0) {
        /*
         *  Header packets have actual content. File data packets are virtual and only have a count.
         */
        addToSendVector(q, mprGetBufStart(packet->content), mprGetBufLength(packet->content));
        mask = (packet->flags & MA_PACKET_HEADER) ? MA_TRACE_HEADERS : MA_TRACE_BODY;
        if (maShouldTrace(conn, mask)) {
            maTraceContent(conn, packet, 0, resp->bytesWritten, mask);
        }
    }
}


/*
 *  Clear entries from the IO vector that have actually been transmitted. This supports partial writes due to the socket
 *  being full. Don't come here if we've seen all the packets and all the data has been completely written. ie. small files
 *  don't come here.
 */
static void freeSentPackets(MaQueue *q, int64 bytes)
{
    MaPacket    *packet;
    int64       len;

    mprAssert(q->first);
    mprAssert(q->count >= 0);
    mprAssert(bytes >= 0);

    while ((packet = q->first) != 0) {
        if (packet->prefix) {
            len = mprGetBufLength(packet->prefix);
            len = min(len, bytes);
            mprAdjustBufStart(packet->prefix, (int) len);
            bytes -= len;
            /* Prefixes don't count in the q->count. No need to adjust */
            if (mprGetBufLength(packet->prefix) == 0) {
                mprFree(packet->prefix);
                packet->prefix = 0;
            }
        }
        if (packet->esize) {
            len = min(packet->esize, bytes);
            packet->esize -= len;
            packet->epos += len;
            bytes -= len;
            mprAssert(packet->esize >= 0);
            mprAssert(bytes == 0);
            if (packet->esize > 0) {
                break;
            }
        } else if ((len = maGetPacketLength(packet)) > 0) {
            len = min(len, bytes);
            mprAdjustBufStart(packet->content, (int) len);
            bytes -= len;
            q->count -= (int) len;
            mprAssert(q->count >= 0);
        }
        if (maGetPacketLength(packet) == 0) {
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
 *  Clear entries from the IO vector that have actually been transmitted. This supports partial writes due to the socket
 *  being full. Don't come here if we've seen all the packets and all the data has been completely written. ie. small files
 *  don't come here.
 */
static void adjustSendVec(MaQueue *q, int64 written)
{
    MprIOVec    *iovec;
    MaResponse  *resp;
    size_t      len;
    int         i, j;

    resp = q->conn->response;
    iovec = q->iovec;
    for (i = 0; i < q->ioIndex; i++) {
        len = iovec[i].len;
        if (written < len) {
            iovec[i].start += (size_t) written;
            iovec[i].len -= (size_t) written;
            return;
        }
        written -= len;
        q->ioCount -= len;
        for (j = i + 1; i < q->ioIndex; ) {
            iovec[j++] = iovec[i++];
        }
        q->ioIndex--;
        i--;
    }
    if (written > 0 && q->ioFile) {
        /* All remaining data came from the file */
        q->ioPos += written;
    }
    q->ioIndex = 0;
    q->ioCount = 0;
    q->ioFile = 0;
}


int maOpenSendConnector(MaHttp *http)
{
    MaStage     *stage;

    stage = maCreateConnector(http, "sendConnector", MA_STAGE_ALL);
    if (stage == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    stage->open = sendOpen;
    stage->outgoingService = sendOutgoingService; 
    http->sendConnector = stage;
    return 0;
}


#else
void __dummySendConnector() {}
#endif /* BLD_FEATURE_SEND */

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
