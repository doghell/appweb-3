/*
 *  rangeFilter.c - Ranged request filter.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_RANGE
/********************************** Forwards **********************************/

static MaPacket *createRangePacket(MaConn *conn, MaRange *range);
static MaPacket *createFinalRangePacket(MaConn *conn);

/*********************************** Code *************************************/
/*
 *  Apply ranges to outgoing data. 
 */
static void rangeService(MaQueue *q, MaRangeFillProc fill)
{
    MaPacket    *packet;
    MaRange     *range;
    MaConn      *conn;
    MaRequest   *req;
    MaResponse  *resp;
    int         bytes, count, endpos;

    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    range = resp->currentRange;

    if (!(q->flags & MA_QUEUE_SERVICED)) {
        if (resp->entityLength < 0 && q->last->flags & MA_PACKET_END) {
           /*
            *   Have all the data, so compute an entity length. This allows negative ranges computed from the 
            *   end of the data.
            */
           resp->entityLength = q->count;
        }
        if (resp->code != MPR_HTTP_CODE_OK || !maFixRangeLength(conn)) {
            maSendPackets(q);
            maRemoveQueue(q);
            return;
        }
        if (req->ranges->next) {
            maCreateRangeBoundary(conn);
        }
        resp->code = MPR_HTTP_CODE_PARTIAL;
    }

    for (packet = maGet(q); packet; packet = maGet(q)) {
        if (!(packet->flags & MA_PACKET_DATA)) {
            if (packet->flags & MA_PACKET_END && resp->rangeBoundary) {
                maPutNext(q, createFinalRangePacket(conn));
            }
            if (!maWillNextQueueAccept(q, packet)) {
                maPutBack(q, packet);
                return;
            }
            maPutNext(q, packet);
            continue;
        }

        /*
         *  Process the current packet over multiple ranges ranges until all the data is processed or discarded.
         */
        bytes = packet->content ? mprGetBufLength(packet->content) : packet->entityLength;
        while (range && bytes > 0) {

            endpos = resp->pos + bytes;
            if (endpos < range->start) {
                /* Packet is before the next range, so discard the entire packet */
                resp->pos += bytes;
                maFreePacket(q, packet);
                break;

            } else if (resp->pos > range->end) {
                /* Missing some output - should not happen */
                mprAssert(0);

            } else if (resp->pos < range->start) {
                /*  Packets starts before range with some data in range so skip some data */
                count = range->start - resp->pos;
                bytes -= count;
                resp->pos += count;
                if (packet->content == 0) {
                    packet->entityLength -= count;
                }
                if (packet->content) {
                    mprAdjustBufStart(packet->content, count);
                }
                continue;

            } else {
                /* In range */
                mprAssert(range->start <= resp->pos && resp->pos < range->end);
                count = min(bytes, range->end - resp->pos);
                count = min(count, q->nextQ->packetSize);
                mprAssert(count > 0);
                if (count < bytes) {
                    maResizePacket(q, packet, count);
                }
                if (!maWillNextQueueAccept(q, packet)) {
                    maPutBack(q, packet);
                    return;
                }
                if (fill) {
                    if ((*fill)(q, packet) < 0) {
                        return;
                    }
                }
                bytes -= count;
                resp->pos += count;
                if (resp->rangeBoundary) {
                    maPutNext(q, createRangePacket(conn, range));
                }
                maPutNext(q, packet);
                if (resp->pos >= range->end) {
                    range = range->next;
                }
                break;
            }
        }
    }
    resp->currentRange = range;
}


static void outgoingRangeService(MaQueue *q)
{
    rangeService(q, NULL);
}


/*
 *  Create a range boundary packet
 */
static MaPacket *createRangePacket(MaConn *conn, MaRange *range)
{
    MaPacket        *packet;
    MaResponse      *resp;
    char            lenBuf[16];

    resp = conn->response;

    if (resp->entityLength >= 0) {
        mprItoa(lenBuf, sizeof(lenBuf), resp->entityLength, 10);
    } else {
        lenBuf[0] = '*';
        lenBuf[1] = '\0';
    }
    packet = maCreatePacket(resp, MA_RANGE_BUFSIZE);
    packet->flags |= MA_PACKET_RANGE;
    mprPutFmtToBuf(packet->content, 
        "\r\n--%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Range: bytes %d-%d/%s\r\n\r\n",
        resp->rangeBoundary, resp->mimeType, range->start, range->end - 1, lenBuf);
    return packet;
}


/*
 *  Create a final range packet that follows all the data
 */
static MaPacket *createFinalRangePacket(MaConn *conn)
{
    MaPacket        *packet;
    MaResponse      *resp;

    resp = conn->response;

    packet = maCreatePacket(resp, MA_RANGE_BUFSIZE);
    packet->flags |= MA_PACKET_RANGE;
    mprPutFmtToBuf(packet->content, "\r\n--%s--\r\n", resp->rangeBoundary);
    return packet;
}


/*
 *  Create a range boundary. This is required if more than one range is requested.
 */
void maCreateRangeBoundary(MaConn *conn)
{
    MaResponse      *resp;

    resp = conn->response;
    mprAssert(resp->rangeBoundary == 0);
    resp->rangeBoundary = mprAsprintf(resp, -1, "%08X%08X", PTOI(resp) + PTOI(conn) * (int) conn->time, (int) conn->time);
}


/*
 *  Ensure all the range limits are within the entity size limits. Fixup negative ranges.
 */
bool maFixRangeLength(MaConn *conn)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaRange     *range;
    int         length;

    req = conn->request;
    resp = conn->response;
    length = resp->entityLength;

    for (range = req->ranges; range; range = range->next) {
        /*
         *      Range: 0-49             first 50 bytes
         *      Range: 50-99,200-249    Two 50 byte ranges from 50 and 200
         *      Range: -50              Last 50 bytes
         *      Range: 1-               Skip first byte then emit the rest
         */
        if (length) {
            if (range->end > length) {
                range->end = length;
            }
            if (range->start > length) {
                range->start = length;
            }
        }
        if (range->start < 0) {
            if (length <= 0) {
                /*
                 *  Can't compute an offset from the end as we don't know the entity length
                 */
                return 0;
            }
            /* select last -range-end bytes */
            range->start = length - range->end + 1;
            range->end = length;
        }
        if (range->end < 0) {
            if (length <= 0) {
                return 0;
            }
            range->end = length - range->end - 1;
        }
        range->len = range->end - range->start;
    }
    return 1;
}


/*
 *  Loadable module initialization
 */
MprModule *maRangeFilterInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *filter;

    module = mprCreateModule(http, "rangeFilter", BLD_VERSION, NULL, NULL, NULL);
    if (module == 0) {
        return 0;
    }

    filter = maCreateFilter(http, "rangeFilter", MA_STAGE_ALL);
    if (filter == 0) {
        mprFree(module);
        return 0;
    }
    http->rangeFilter = filter;
    http->rangeService = rangeService;
    filter->outgoingService = outgoingRangeService; 
    return module;
}


#else

MprModule *maRangeFilterInit(MaHttp *http, cchar *path)
{
    return 0;
}
#endif /* BLD_FEATURE_RANGE */

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
