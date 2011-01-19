/*
 *  queue.c -- Queue support routines. Queues are the bi-directional data flow channels for the request/response pipeline.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

#if BLD_DEBUG
static void checkQueueCount(MaQueue *q);
#else
#define checkQueueCount(q)
#endif

/************************************ Code ************************************/
/*
 *  Createa a new queue for the given stage. If prev is given, then link the new queue after the previous queue.
 */
MaQueue *maCreateQueue(MaConn *conn, MaStage *stage, int direction, MaQueue *prev)
{
    MaQueue     *q;
    MaResponse  *resp;
    MaLimits    *limits;

    resp = conn->response;
    limits = &conn->http->limits;

    q = mprAllocObjZeroed(resp, MaQueue);
    if (q == 0) {
        return 0;
    }
    
    maInitQueue(conn->http, q, stage->name);
    maInitSchedulerQueue(q);

    q->conn = conn;
    q->stage = stage;
    q->close = stage->close;
    q->open = stage->open;
    q->start = stage->start;
    q->direction = direction;

    q->max = limits->maxStageBuffer;
    q->packetSize = limits->maxStageBuffer;

    if (direction == MA_QUEUE_SEND) {
        q->put = stage->outgoingData;
        q->service = stage->outgoingService;
        
    } else {
        q->put = stage->incomingData;
        q->service = stage->incomingService;
    }
    if (prev) {
        maInsertQueue(prev, q);
    }
    return q;
}


/*
 *  Initialize a bare queue. Used for dummy heads.
 */
void maInitQueue(MaHttp *http, MaQueue *q, cchar *name)
{
    q->nextQ = q;
    q->prevQ = q;
    q->owner = name;
    q->max = http->limits.maxStageBuffer;
    q->low = q->max / 100 * 5;
}


/*
 *  Insert a queue after the previous element
 */
void maAppendQueue(MaQueue *head, MaQueue *q)
{
    q->nextQ = head;
    q->prevQ = head->prevQ;
    head->prevQ->nextQ = q;
    head->prevQ = q;
}


/*
 *  Insert a queue after the previous element
 */
void maInsertQueue(MaQueue *prev, MaQueue *q)
{
    q->nextQ = prev->nextQ;
    q->prevQ = prev;
    prev->nextQ->prevQ = q;
    prev->nextQ = q;
}


void maRemoveQueue(MaQueue *q)
{
    q->prevQ->nextQ = q->nextQ;
    q->nextQ->prevQ = q->prevQ;
    q->prevQ = q->nextQ = q;
}


MaQueue *findPreviousQueue(MaQueue *q)
{
    while (q->prevQ) {
        q = q->prevQ;
        if (q->service) {
            return q;
        }
    }
    return 0;
}


bool maIsQueueEmpty(MaQueue *q)
{
    return q->first == 0;
}


/*
 *  Get the next packet from the queue
 */
MaPacket *maGet(MaQueue *q)
{
    MaConn      *conn;
    MaQueue     *prev;
    MaPacket    *packet;

    checkQueueCount(q);

    conn = q->conn;
    while (q->first) {
        if ((packet = q->first) != 0) {
            if (packet->flags & MA_PACKET_DATA && conn->requestFailed) {
                q->first = packet->next;
                q->count -= maGetPacketLength(packet);
                maFreePacket(q, packet);
                continue;
            }
            q->first = packet->next;
            packet->next = 0;
            q->count -= maGetPacketLength(packet);
            mprAssert(q->count >= 0);
            if (packet == q->last) {
                q->last = 0;
                mprAssert(q->first == 0);
            }
        }
        checkQueueCount(q);
        if (q->flags & MA_QUEUE_FULL && q->count < q->low) {
            /*
             *  This queue was full and now is below the low water mark. Back-enable the previous queue.
             */
            q->flags &= ~MA_QUEUE_FULL;
            prev = findPreviousQueue(q);
            if (prev && prev->flags & MA_QUEUE_DISABLED) {
                maEnableQueue(prev);
            }
        }
        checkQueueCount(q);
        return packet;
    }
    return 0;
}


/*
 *  Create a new packet. If size is -1, then also create a default growable buffer -- used for incoming body content. If 
 *  size > 0, then create a non-growable buffer of the requested size.
 */
MaPacket *maCreatePacket(MprCtx ctx, int size)
{
    MaPacket    *packet;

    packet = mprAllocObjZeroed(ctx, MaPacket);
    if (packet == 0) {
        return 0;
    }
    if (size != 0) {
        packet->content = mprCreateBuf(packet, size < 0 ? MA_BUFSIZE: size, -1);
        if (packet->content == 0) {
            mprFree(packet);
            return 0;
        }
    }
    return packet;
}


/*
 *  Create a packet for the connection to read into. This may come from the connection packet free list
 */
MaPacket *maCreateConnPacket(MaConn *conn, int size)
{
    if (conn->state == MPR_HTTP_STATE_COMPLETE) {
        return maCreatePacket((MprCtx) conn, size);
    }
#if UNUSED
    MaPacket    *packet;
    MaRequest   *req;
    req = conn->request;
    if (req) {
        /* These packets are all owned by the request */
        if ((packet = req->freePackets) != NULL && size <= packet->content->buflen) {
            req->freePackets = packet->next; 
            packet->next = 0;
            return packet;
        }
    }
#endif
    return maCreatePacket(conn->request ? (MprCtx) conn->request: (MprCtx) conn, size);
}


void maFreePacket(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaRequest   *req;

    conn = q->conn;
    req = conn->request;

#if UNUSED
    if (req == 0 || packet->content == 0 || packet->content->buflen < MA_BUFSIZE || mprGetParent(packet) == conn) {
        /* 
         *  Don't bother recycling non-content, small packets or packets owned by the connection
         *  We only store packets owned by the request and not by the connection on the free list.
         */
        mprFree(packet);
        return;
    }

    /*
     *  Add to the packet free list for recycling
     */
    mprAssert(packet->content);
    mprFlushBuf(packet->content);
    mprFree(packet->prefix);
    packet->prefix = 0;
    mprFree(packet->suffix);
    packet->suffix = 0;
    packet->entityLength = 0;
    packet->flags = 0;
    packet->next = req->freePackets;
    req->freePackets = packet;
#else
    mprFree(packet);
#endif
} 


/*
 *  Create the response header packet
 */
MaPacket *maCreateHeaderPacket(MprCtx ctx)
{
    MaPacket    *packet;

    packet = maCreatePacket(ctx, MA_BUFSIZE);
    if (packet == 0) {
        return 0;
    }
    packet->flags = MA_PACKET_HEADER;
    return packet;
}


MaPacket *maCreateDataPacket(MprCtx ctx, int size)
{
    MaPacket    *packet;

    packet = maCreatePacket(ctx, size);
    if (packet == 0) {
        return 0;
    }
    packet->flags = MA_PACKET_DATA;
    return packet;
}


MaPacket *maCreateEndPacket(MprCtx ctx)
{
    MaPacket    *packet;

    packet = maCreatePacket(ctx, 0);
    if (packet == 0) {
        return 0;
    }
    packet->flags = MA_PACKET_END;
    return packet;
}


#if BLD_DEBUG
static void checkQueueCount(MaQueue *q)
{
    MaPacket    *packet;
    int         count;

    count = 0;
    for (packet = q->first; packet; packet = packet->next) {
        count += maGetPacketLength(packet);
    }
    mprAssert(count == q->count);
}
#endif


/*
 *  Put a packet on the service queue.
 */
void maPutForService(MaQueue *q, MaPacket *packet, bool serviceQ)
{
    mprAssert(packet);
   
    checkQueueCount(q);
    q->count += maGetPacketLength(packet);
    packet->next = 0;
    
    if (q->first) {
        q->last->next = packet;
        q->last = packet;
        
    } else {
        q->first = packet;
        q->last = packet;
    }
    checkQueueCount(q);
    if (serviceQ && !(q->flags & MA_QUEUE_DISABLED))  {
        maScheduleQueue(q);
    }
}


/*
 *  Join a packet onto the service queue
 */
void maJoinForService(MaQueue *q, MaPacket *packet, bool serviceQ)
{
    MaPacket    *old;

    if (q->first == 0) {
        /*
         *  Just use the service queue as a holding queue while we aggregate the post data.
         */
        maPutForService(q, packet, 0);

    } else {
        q->count += maGetPacketLength(packet);
        if (q->first && maGetPacketLength(q->first) == 0) {
            old = q->first;
            packet = q->first->next;
            q->first = packet;
            maFreePacket(q, old);

        } else {
            /*
             *  Aggregate all data into one packet and free the packet.
             */
            maJoinPacket(q->first, packet);
            maFreePacket(q, packet);
        }
    }
    checkQueueCount(q);
    if (serviceQ && !(q->flags & MA_QUEUE_DISABLED))  {
        maScheduleQueue(q);
    }
}


/*
 *  Pass to a queue
 */
void maPut(MaQueue *q, MaPacket *packet)
{
    mprAssert(packet);
    
    mprAssert(q->put);
    q->put(q, packet);
}


/*
 *  Pass to the next queue
 */
void maPutNext(MaQueue *q, MaPacket *packet)
{
    mprAssert(packet);
    
    mprAssert(q->nextQ->put);
    q->nextQ->put(q->nextQ, packet);
}


/*
 *  Put the packet back at the front of the queue
 */
void maPutBack(MaQueue *q, MaPacket *packet)
{
    mprAssert(packet);
    mprAssert(packet->next == 0);
    
    packet->next = q->first;

    if (q->first == 0) {
        q->last = packet;
    }
    q->first = packet;

    mprAssert(maGetPacketLength(packet) >= 0);
    q->count += maGetPacketLength(packet);
    mprAssert(q->count >= 0);
    checkQueueCount(q);
}


/*
 *  Return true if the next queue will accept this packet. If not, then disable the queue's service procedure.
 *  This may split the packet if it exceeds the downstreams maximum packet size.
 */
bool maWillNextQueueAccept(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaQueue     *next;
    int         size;

    conn = q->conn;
    next = q->nextQ;

    size = maGetPacketLength(packet);
    if (size <= next->packetSize && (size + next->count) <= next->max) {
        return 1;
    }
    if (maResizePacket(q, packet, 0) < 0) {
        return 0;
    }
    size = maGetPacketLength(packet);
    if (size <= next->packetSize && (size + next->count) <= next->max) {
        return 1;
    }

    /*
     *  The downstream queue is full, so disable the queue and mark the downstream queue as full and service immediately. 
     */
    maDisableQueue(q);
    next->flags |= MA_QUEUE_FULL;
    maScheduleQueue(next);
    return 0;
}


void maSendEndPacket(MaQueue *q)
{
    maPutNext(q, maCreateEndPacket(q));
    q->flags |= MA_QUEUE_EOF;
}


void maSendPackets(MaQueue *q)
{
    MaPacket    *packet;

    for (packet = maGet(q); packet; packet = maGet(q)) {
        maPutNext(q, packet);
    }
}


void maDisableQueue(MaQueue *q)
{
    mprLog(q, 7, "Disable queue %s", q->owner);
    q->flags |= MA_QUEUE_DISABLED;
}


void maScheduleQueue(MaQueue *q)
{
    MaQueue     *head;
    
    mprAssert(q->conn);
    head = &q->conn->serviceq;
    
    if (q->scheduleNext == q) {
        q->scheduleNext = head;
        q->schedulePrev = head->schedulePrev;
        head->schedulePrev->scheduleNext = q;
        head->schedulePrev = q;
    }
}


MaQueue *maGetNextQueueForService(MaQueue *q)
{
    MaQueue     *next;
    
    if (q->scheduleNext != q) {
        next = q->scheduleNext;
        next->schedulePrev->scheduleNext = next->scheduleNext;
        next->scheduleNext->schedulePrev = next->schedulePrev;
        next->schedulePrev = next->scheduleNext = next;
        return next;
    }
    return 0;
}


void maInitSchedulerQueue(MaQueue *q)
{
    q->scheduleNext = q;
    q->schedulePrev = q;
}


void maServiceQueue(MaQueue *q)
{
    /*
     *  Since we are servicing the queue, remove it from the service queue if it is at the front of the queue.
     */
    if (q->conn->serviceq.scheduleNext == q) {
        maGetNextQueueForService(&q->conn->serviceq);
    }
    if (!(q->flags & MA_QUEUE_DISABLED)) {
        q->service(q);
        q->flags |= MA_QUEUE_SERVICED;
    }
}


void maEnableQueue(MaQueue *q)
{
    mprLog(q, 7, "Enable q %s", q->owner);
    q->flags &= ~MA_QUEUE_DISABLED;
    maScheduleQueue(q);
}


/*
 *  Return the number of bytes the queue will accept. Always positive.
 */
int maGetQueueRoom(MaQueue *q)
{
    mprAssert(q->max > 0);
    mprAssert(q->count >= 0);
    
    if (q->count >= q->max) {
        return 0;
    }
    return q->max - q->count;
}


/*
 *  Return true if the packet is too large to be accepted by the downstream queue.
 */
bool maPacketTooBig(MaQueue *q, MaPacket *packet)
{
    int     size;
    
    size = mprGetBufLength(packet->content);
    return size > q->max || size > q->packetSize;
}


/*
 *  Split a packet if required so it fits in the downstream queue. Put back the 2nd portion of the split packet on the queue.
 *  Ensure that the packet is not larger than "size" if it is greater than zero.
 */
int maResizePacket(MaQueue *q, MaPacket *packet, int size)
{
    MaPacket    *tail;
    MaConn      *conn;
    MprCtx      ctx;
    int         len;
    
    if (size <= 0) {
        size = MAXINT;
    }

    /*
     *  Calculate the size that will fit
     */
    len = packet->content ? maGetPacketLength(packet) : packet->entityLength;
    size = min(size, len);
    size = min(size, q->nextQ->max);
    size = min(size, q->nextQ->packetSize);

    if (size == 0) {
        /* Can't fit anything downstream, no point splitting yet */
        return 0;
    }
    if (size == len) {
        return 0;
    }
    conn = q->conn;
    ctx = conn->request ? (MprCtx) conn->request : (MprCtx) conn;
    tail = maSplitPacket(ctx, packet, size);
    if (tail == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    maPutBack(q, tail);
    return 0;
}


/*
 *  Drain a service queue by scheduling the queue and servicing all queues. Return true if there is room for more data.
 */
static bool drain(MaQueue *q, bool block)
{
    MaConn      *conn;
    MaQueue     *next;
    int         oldMode;

    conn = q->conn;

    /*
     *  Queue is full. Need to drain the service queue if possible.
     */
    do {
        oldMode = mprSetSocketBlockingMode(conn->sock, block);
        maScheduleQueue(q);
        next = q->nextQ;
        if (next->count >= next->max) {
            maScheduleQueue(next);
        }
        maServiceQueues(conn);
        mprSetSocketBlockingMode(conn->sock, oldMode);
    } while (block && q->count >= q->max);
    
    return (q->count < q->max) ? 1 : 0;
}


/*
 *  Write a block of data. This is the lowest level write routine for dynamic data. If block is true, this routine will 
 *  block until all the block is written. If block is false, then it may return without having written all the data.
 */
int maWriteBlock(MaQueue *q, cchar *buf, int size, bool block)
{
    MaPacket    *packet;
    MaConn      *conn;
    MaResponse  *resp;
    int         bytes, written, packetSize;

    mprAssert(q->stage->flags & MA_STAGE_HANDLER);
               
    conn = q->conn;
    resp = conn->response;
    packetSize = (resp->chunkSize > 0) ? resp->chunkSize : q->max;
    packetSize = min(packetSize, size);
    
    if ((q->flags & MA_QUEUE_DISABLED) || (q->count > 0 && (q->count + size) >= q->max)) {
        if (!drain(q, block)) {
            return 0;
        }
    }
    for (written = 0; size > 0; ) {
        if (q->count >= q->max && !drain(q, block)) {
            checkQueueCount(q);
            break;
        }
        if (conn->disconnected) {
            return MPR_ERR_CANT_WRITE;
        }
        if ((packet = maCreateDataPacket(q, packetSize)) == 0) {
            return MPR_ERR_NO_MEMORY;
        }
        bytes = mprPutBlockToBuf(packet->content, buf, size);
        buf += bytes;
        size -= bytes;
        written += bytes;
        maPutForService(q, packet, 1);
        checkQueueCount(q);
    }
    checkQueueCount(q);
    return written;
}


int maWriteString(MaQueue *q, cchar *s)
{
    return maWriteBlock(q, s, (int) strlen(s), 1);
}


int maWrite(MaQueue *q, cchar *fmt, ...)
{
    va_list     vargs;
    char        *buf;
    int         rc;
    
    va_start(vargs, fmt);
    buf = mprVasprintf(q, -1, fmt, vargs);
    va_end(vargs);

    rc = maWriteString(q, buf);
    mprFree(buf);
    return rc;
}


/*
 *  Join two packets by pulling the content from the second into the first.
 */
int maJoinPacket(MaPacket *packet, MaPacket *p)
{
    if (mprPutBlockToBuf(packet->content, mprGetBufStart(p->content), maGetPacketLength(p)) < 0) {
        return MPR_ERR_NO_MEMORY;
    }
    return 0;
}


/*
 *  Split a packet at a given offset and return a new packet containing the data after the offset.
 *  The suffix data migrates to the new packet. 
 */
MaPacket *maSplitPacket(MprCtx ctx, MaPacket *orig, int offset)
{
    MaPacket    *packet;
    int         count, size;

    if (offset >= maGetPacketLength(orig)) {
        mprAssert(0);
        return 0;
    }
    count = maGetPacketLength(orig) - offset;
    size = max(count, MA_BUFSIZE);
    size = MA_PACKET_ALIGN(size);
    
    if ((packet = maCreateDataPacket(ctx, (orig->content == 0) ? 0: size)) == 0) {
        return 0;
    }
    packet->flags = orig->flags;

    if (orig->entityLength) {
        orig->entityLength = offset;
        packet->entityLength = count;
    }
    if (orig->content && maGetPacketLength(orig) > 0) {
        mprAdjustBufEnd(orig->content, -count);
        mprPutBlockToBuf(packet->content, mprGetBufEnd(orig->content), count);
#if BLD_DEBUG
        mprAddNullToBuf(orig->content);
#endif
    }
    return packet;
}


/*
 *  Remove packets from a queue which do not need to be processed.
 *  Remove data packets if no body is required (HEAD|TRACE|OPTIONS|PUT|DELETE method, not modifed content, or error)
 *  This actually removes and frees the data packets whereas maDiscardData will just flush the data packets.
 */
void maCleanQueue(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaPacket    *packet, *next, *prev;

    conn = q->conn;
    resp = conn->response;

    if (!(resp->flags & MA_RESP_NO_BODY)) {
        return;
    }

    for (prev = 0, packet = q->first; packet; packet = next) {
        next = packet->next;
        if (packet->flags & (MA_PACKET_RANGE | MA_PACKET_DATA)) {
            if (prev) {
                prev->next = next;
            } else {
                q->first = next;
            }
            q->count -= maGetPacketLength(packet);
            maFreePacket(q, packet);
            continue;
        }
        prev = packet;
    }
    checkQueueCount(q);
}


/*
 *  Remove all data from non-header packets in the queue. Don't worry about freeing. Will happen automatically at 
 *  the request end. See also maCleanQueue above.
 */
void maDiscardData(MaQueue *q, bool removePackets)
{
    MaPacket    *packet;
    int         len;

    if (q->first) {
        /*
         *  Skip the header packet
         */
        if (q->first->flags & MA_PACKET_HEADER) {
            packet = q->first->next;
        } else {
            packet = q->first;
        }

        /*
         *  Just flush each packet. Don't remove so the EOF packet is preserved
         */
        for (; packet; packet = packet->next) {
            if (packet->content) {
                len = maGetPacketLength(packet);
                q->conn->response->length -= len;
                q->count -= len;
                mprFlushBuf(packet->content);
            }
        }
        checkQueueCount(q);
    }
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
    vim: sw=4 ts=4 expandtab

    @end
 */
