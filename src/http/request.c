/*
 *  request.c -- Request class to handle individual HTTP requests.
 *
 *  The Request class is the real work-horse in managing HTTP requests. An instance is created per HTTP request. During
 *  keep-alive it is preserved to process further requests. Requests run in a single thread and do not need multi-thread
 *  locking except for the timeout code which may run on another thread.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void addMatchEtag(MaConn *conn, char *etag);
static int  destroyRequest(MaRequest *req);
static char *getToken(MaConn *conn, cchar *delim);
static bool parseFirstLine(MaConn *conn, MaPacket *packet);
static bool parseHeaders(MaConn *conn, MaPacket *packet);
static bool parseRange(MaConn *conn, char *value);
static bool parseRequest(MaConn *conn, MaPacket *packet);
static bool processContent(MaConn *conn, MaPacket *packet);
static void setIfModifiedDate(MaConn *conn, MprTime when, bool ifMod);

/*********************************** Code *************************************/

MaRequest *maCreateRequest(MaConn *conn)
{
    MaRequest   *req;
    MprHeap     *arena;

    arena  = mprAllocHeap(conn->arena, "request", MA_REQ_MEM, 0, NULL);
    if (arena == 0) {
        return 0;
    }
    req = mprAllocObjWithDestructorZeroed(arena, MaRequest, destroyRequest);
    if (req == 0) {
        return 0;
    }
    req->conn = conn;
    req->arena = arena;
    req->length = -1;
    req->ifMatch = 1;
    req->ifModified = 1;
    req->host = conn->host;
    req->remainingContent = 0;
    req->method = 0;
    req->headers = mprCreateHash(req, MA_VAR_HASH_SIZE);
    req->formVars = mprCreateHash(req, MA_VAR_HASH_SIZE);
    req->httpProtocol = "HTTP/1.1";
    return req;
}


int destroyRequest(MaRequest *req)
{
    MaConn  *conn;

    conn = req->conn;
    maPrepConnection(conn);
    if (conn->input) {
        /* Left over packet */
        if (mprGetParent(conn->input) != conn) {
            conn->input = maSplitPacket(conn, conn->input, 0);
        }
    }
    return 0;
}


/*
 *  Process a write event. These occur when a request could not be completed when it was first received.
 */
void maProcessWriteEvent(MaConn *conn)
{
    mprLog(conn, 6, "maProcessWriteEvent, state %d", conn->state);

    if (unlikely(conn->expire <= conn->time)) {
        /*
         *  Ignore the event if we have expired.
         */
        return;
    }
    if (conn->response) {
        /*
         *  Enable the queue upstream from the connector
         */
        maEnableQueue(conn->response->queue[MA_QUEUE_SEND].prevQ);
        maServiceQueues(conn);
        if (conn->state == MPR_HTTP_STATE_COMPLETE) {
            maProcessCompletion(conn);
        }
    }
}


/*
 *  Process incoming requests. This will process as many requests as possible before returning. All socket I/O is 
 *  non-blocking, and this routine must not block. 
 */
void maProcessReadEvent(MaConn *conn, MaPacket *packet)
{
    mprAssert(conn);

    conn->canProceed = 1;
    
    while (conn->canProceed) {
        mprLog(conn, 7, "maProcessReadEvent, state %d", conn->state);
        
        switch (conn->state) {
        case MPR_HTTP_STATE_BEGIN:
            conn->canProceed = parseRequest(conn, packet);
            break;

        case MPR_HTTP_STATE_CONTENT:
            conn->canProceed = processContent(conn, packet);
            packet = conn->input;
            break;

        case MPR_HTTP_STATE_PROCESSING:
            conn->canProceed = maServiceQueues(conn);
            break;

        case MPR_HTTP_STATE_COMPLETE:
            conn->canProceed = maProcessCompletion(conn);
            packet = conn->input;
            break;

        default:
            conn->keepAliveCount = 0;
            mprAssert(0);
            return;
        }
    }
}


/*
 *  Parse a new request. Return true to keep going with this or subsequent request, zero means insufficient data to proceed.
 */
static bool parseRequest(MaConn *conn, MaPacket *packet) 
{
    MaRequest   *req;
    char        *start, *end;
    int         len;

    /*
     *  Must wait until we have the complete set of headers.
     */
    if ((len = mprGetBufLength(packet->content)) == 0) {
        return 0;
    }

    start = mprGetBufStart(packet->content);
    if ((end = mprStrnstr(start, "\r\n\r\n", len)) == 0) {
        return 0;
    }
    len = end - start;
    if (len >= conn->host->limits->maxHeader) {
        maFailConnection(conn, MPR_HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
        return 0;
    }
#if UNUSED
    *end = '\0'; mprLog(conn, 3, "\n@@@ Request =>\n%s\n", start); *end = '\r';
#endif

    if (parseFirstLine(conn, packet)) {
        parseHeaders(conn, packet);
    } else {
        return 0;
    }

    maMatchHandler(conn);
    
    /*
     *  Have read the headers. Create the request pipeline. This calls the open() stage entry routines.
     */
    maCreatePipeline(conn);

    req = conn->request;
    if (conn->connectionFailed) {
        /* Discard input data */
        mprAssert(conn->keepAliveCount == 0);
        conn->state = MPR_HTTP_STATE_PROCESSING;
        maRunPipeline(conn);
    } else if (req->remainingContent > 0) {
        conn->state = MPR_HTTP_STATE_CONTENT;
    } else if (req->length == -1 && (req->method == MA_REQ_POST || req->method == MA_REQ_PUT)) {
        conn->state = MPR_HTTP_STATE_CONTENT;
    } else {
        /*
         *  Can run the request now if there is no incoming data.
         */
        conn->state = MPR_HTTP_STATE_PROCESSING;
        maRunPipeline(conn);
    }
    return !conn->disconnected;
}


/*
 *  Parse the first line of a http request. Return true if the first line parsed. This is only called once all the headers
 *  have been read and buffered.
 */
static bool parseFirstLine(MaConn *conn, MaPacket *packet)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaHost      *host;
    MprBuf      *content;
    cchar       *endp;
    char        *methodName, *uri, *httpProtocol;
    int         method, len;

    req = conn->request = maCreateRequest(conn);
    resp = conn->response = maCreateResponse(conn);
    host = conn->host;

    methodName = getToken(conn, " ");
    if (*methodName == '\0') {
        maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad request method name");
        return 0;
    }

    method = 0;
    switch (methodName[0]) {
    case 'D':
        if (strcmp(methodName, "DELETE") == 0) {
            method = MA_REQ_DELETE;
        }
        break;

    case 'G':
        if (strcmp(methodName, "GET") == 0) {
            method = MA_REQ_GET;
        }
        break;

    case 'P':
        if (strcmp(methodName, "POST") == 0) {
            method = MA_REQ_POST;

        } else if (strcmp(methodName, "PUT") == 0) {
            method = MA_REQ_PUT;
        }
        break;

    case 'H':
        if (strcmp(methodName, "HEAD") == 0) {
            method = MA_REQ_HEAD;
            resp->flags |= MA_RESP_NO_BODY;
        }
        break;

    case 'O':
        if (strcmp(methodName, "OPTIONS") == 0) {
            method = MA_REQ_OPTIONS;
            resp->flags |= MA_RESP_NO_BODY;
        }
        break;

    case 'T':
        if (strcmp(methodName, "TRACE") == 0) {
            method = MA_REQ_TRACE;
            resp->flags |= MA_RESP_NO_BODY;
        }
        break;
    }

    if (method == 0) {
        maFailConnection(conn, MPR_HTTP_CODE_BAD_METHOD, "Bad method");
        return 0;
    }
    uri = getToken(conn, " ");
    if (*uri == '\0') {
        maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Bad URI.");
        return 0;
    }
    if ((int) strlen(uri) >= conn->http->limits.maxUrl) {
        maFailRequest(conn, MPR_HTTP_CODE_REQUEST_URL_TOO_LARGE, "Bad request. URI too long.");
        return 0;
    }
    httpProtocol = getToken(conn, "\r\n");
    if (strcmp(httpProtocol, "HTTP/1.1") == 0) {
        conn->protocol = 1;

    } else if (strcmp(httpProtocol, "HTTP/1.0") == 0) {
        conn->protocol = 0;
        if (method == MA_REQ_POST || method == MA_REQ_PUT) {
            req->remainingContent = MAXINT;
        }

    } else {
        maFailConnection(conn, MPR_HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
        return 0;
    }
    req->method = method;
    req->methodName = methodName;
    req->httpProtocol = httpProtocol;
    
    if (maSetRequestUri(conn, uri) < 0) {
        maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad URL format");
        return 0;
    }
    if ((conn->trace = maSetupTrace(host, conn->response->extension)) != 0) {
        if (maShouldTrace(conn, MA_TRACE_REQUEST | MA_TRACE_HEADERS)) {
            mprLog(req, host->traceLevel, "\n@@@ New request from %s:%d to %s:%d\n%s %s %s", 
                conn->remoteIpAddr, conn->remotePort, conn->sock->ipAddr, conn->sock->port,
                methodName, uri, httpProtocol);
            content = packet->content;
            endp = strstr((char*) content->start, "\r\n\r\n");
            len = (endp) ? (endp - mprGetBufStart(content) + 4) : 0;
            maTraceContent(conn, packet, len, 0, MA_TRACE_REQUEST | MA_TRACE_HEADERS);
        }
    } else {
        mprLog(conn, 2, "%s %s %s", methodName, uri, httpProtocol);
    }
    return 1;
}


/*
 *  Parse the request headers. Return true if the header parsed.
 */
static bool parseHeaders(MaConn *conn, MaPacket *packet)
{
    MaHostAddress   *address;
    MaRequest       *req;
    MaResponse      *resp;
    MaHost          *host, *hp;
    MaLimits        *limits;
    MprBuf          *content;
    char            keyBuf[MPR_MAX_STRING];
    char            *key, *value, *cp, *tok;
    int             count, keepAlive;

    req = conn->request;
    resp = conn->response;
    host = req->host;
    content = packet->content;
    conn->request->headerPacket = packet;
    limits = &conn->http->limits;
    keepAlive = 0;

    strcpy(keyBuf, "HTTP_");
    mprAssert(strstr((char*) content->start, "\r\n"));

    for (count = 0; content->start[0] != '\r'; count++) {

        if (count >= limits->maxNumHeaders) {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Too many headers");
            return 0;
        }
        if ((key = getToken(conn, ":")) == 0 || *key == '\0') {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad header format");
            return 0;
        }

        value = getToken(conn, "\r\n");
        while (isspace((int) *value)) {
            value++;
        }
        if (conn->requestFailed) {
            continue;
        }
        mprStrUpper(key);
        for (cp = key; *cp; cp++) {
            if (*cp == '-') {
                *cp = '_';
            }
        }
        mprLog(req, 8, "Key %s, value %s", key, value);
        if (strspn(key, "%<>/\\") > 0) {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad header key value");
            continue;
        }

        /*
         *  Define the header with a "HTTP_" prefix
         */
        mprStrcpy(&keyBuf[5], sizeof(keyBuf) - 5, key);
        mprAddDuplicateHash(req->headers, keyBuf, value);

        switch (key[0]) {
        case 'A':
            if (strcmp(key, "AUTHORIZATION") == 0) {
                value = mprStrdup(req, value);
                req->authType = mprStrTok(value, " \t", &tok);
                req->authDetails = tok;

            } else if (strcmp(key, "ACCEPT_CHARSET") == 0) {
                req->acceptCharset = value;

            } else if (strcmp(key, "ACCEPT") == 0) {
                req->accept = value;

            } else if (strcmp(key, "ACCEPT_ENCODING") == 0) {
                req->acceptEncoding = value;
            }
            break;

        case 'C':
            if (strcmp(key, "CONTENT_LENGTH") == 0) {
                if (req->length >= 0) {
                    maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Mulitple content length headers");
                    continue;
                }
                req->length = atoi(value);
                if (req->length < 0) {
                    maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad content length");
                    continue;
                }
                if (req->length >= host->limits->maxBody) {
                    maFailConnection(conn, MPR_HTTP_CODE_REQUEST_TOO_LARGE, 
                        "Request content length %d is too big. Limit %d", req->length, host->limits->maxBody);
                    continue;
                }
                mprAssert(req->length >= 0);
                req->remainingContent = req->length;
                req->contentLengthStr = value;

            } else if (strcmp(key, "CONTENT_RANGE") == 0) {
                /*
                 *  This headers specifies the range of any posted body data
                 *  Format is:  Content-Range: bytes n1-n2/length
                 *  Where n1 is first byte pos and n2 is last byte pos
                 */
                char    *sp;
                int     start, end, size;

                start = end = size = -1;
                sp = value;
                while (*sp && !isdigit((int) *sp)) {
                    sp++;
                }
                if (*sp) {
                    start = (int) mprAtoi(sp, 10);

                    if ((sp = strchr(sp, '-')) != 0) {
                        end = (int) mprAtoi(++sp, 10);
                    }
                    if ((sp = strchr(sp, '/')) != 0) {
                        /*
                         *  Note this is not the content length transmitted, but the original size of the input of which 
                         *  the client is transmitting only a portion.
                         */
                        size = (int) mprAtoi(++sp, 10);
                    }
                }
                if (start < 0 || end < 0 || size < 0 || end <= start) {
                    maFailRequest(conn, MPR_HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                    continue;
                }
                req->inputRange = maCreateRange(conn, start, end);

            } else if (strcmp(key, "CONTENT_TYPE") == 0) {
                req->mimeType = value;

            } else if (strcmp(key, "COOKIE") == 0) {
                if (req->cookie && *req->cookie) {
                    req->cookie = mprStrcat(req, -1, req->cookie, "; ", value, NULL);
                } else {
                    req->cookie = value;
                }

            } else if (strcmp(key, "CONNECTION") == 0) {
                req->connection = value;
                if (mprStrcmpAnyCase(value, "KEEP-ALIVE") == 0) {
                    keepAlive++;

                } else if (mprStrcmpAnyCase(value, "CLOSE") == 0) {
                    conn->keepAliveCount = 0;
                }
                if (!host->keepAlive) {
                    conn->keepAliveCount = 0;
                }
            }
            break;

        case 'F':
            req->forwarded = value;
            break;

        case 'H':
            if (strcmp(key, "HOST") == 0) {
                req->hostName = value;
                address = conn->address;
                if (maIsNamedVirtualHostAddress(address)) {
                    hp = maLookupVirtualHost(address, value);
                    if (hp == 0) {
                        maFailRequest(conn, 404, "No host to serve request. Searching for %s", value);
                        mprLog(conn, 1, "Can't find virtual host %s", value);
                        continue;
                    }
                    req->host = hp;
                    /*
                     *  Reassign this request to a new host
                     */
                    maRemoveConn(host, conn);
                    host = hp;
                    conn->host = hp;
                    maAddConn(hp, conn);
                }
            }
            break;

        case 'I':
            if ((strcmp(key, "IF_MODIFIED_SINCE") == 0) || (strcmp(key, "IF_UNMODIFIED_SINCE") == 0)) {
                MprTime     newDate = 0;
                char        *cp;
                bool        ifModified = (key[3] == 'M');

                if ((cp = strchr(value, ';')) != 0) {
                    *cp = '\0';
                }
                if (mprParseTime(conn, &newDate, value, MPR_UTC_TIMEZONE, NULL) < 0) {
                    mprAssert(0);
                    break;
                }
                if (newDate) {
                    setIfModifiedDate(conn, newDate, ifModified);
                    req->flags |= MA_REQ_IF_MODIFIED;
                }

            } else if ((strcmp(key, "IF_MATCH") == 0) || (strcmp(key, "IF_NONE_MATCH") == 0)) {
                char    *word, *tok;
                bool    ifMatch = key[3] == 'M';

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                req->ifMatch = ifMatch;
                req->flags |= MA_REQ_IF_MODIFIED;

                value = mprStrdup(conn, value);
                word = mprStrTok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = mprStrTok(0, " ,", &tok);
                }

            } else if (strcmp(key, "IF_RANGE") == 0) {
                char    *word, *tok;

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                req->ifMatch = 1;
                req->flags |= MA_REQ_IF_MODIFIED;

                value = mprStrdup(conn, value);
                word = mprStrTok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = mprStrTok(0, " ,", &tok);
                }
            }
            break;

        case 'P':
            if (strcmp(key, "PRAGMA") == 0) {
                req->pragma = value;
            }
            break;

        case 'R':
            if (strcmp(key, "RANGE") == 0) {
                if (!parseRange(conn, value)) {
                    maFailRequest(conn, MPR_HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad range");
                }
            } else if (strcmp(key, "REFERER") == 0) {
                /* NOTE: yes the header is misspelt in the spec */
                req->referer = value;
            }
            break;

        case 'T':
            if (strcmp(key, "TRANSFER_ENCODING") == 0) {
                mprStrLower(value);
                if (strcmp(value, "chunked") == 0) {
                    req->flags |= MA_REQ_CHUNKED;
                    /*
                     *  This will be revised by the chunk filter as chunks are processed and will be set to zero when the
                     *  last chunk has been received.
                     */
                    req->remainingContent = MAXINT;
                }
            }
            break;
        
#if BLD_DEBUG
        case 'X':
            if (strcmp(key, "X_APPWEB_CHUNK_SIZE") == 0) {
                mprStrUpper(value);
                resp->chunkSize = atoi(value);
                if (resp->chunkSize <= 0) {
                    resp->chunkSize = 0;
                } else if (resp->chunkSize > conn->http->limits.maxChunkSize) {
                    resp->chunkSize = conn->http->limits.maxChunkSize;
                }
            }
            break;
#endif

        case 'U':
            if (strcmp(key, "USER_AGENT") == 0) {
                req->userAgent = value;
            }
            break;
        }
    }
    if (conn->protocol == 0 && !keepAlive) {
        conn->keepAliveCount = 0;
    }
    if (!(req->flags & MA_REQ_CHUNKED)) {
        /*  
         *  Step over "\r\n" after headers. As an optimization, don't do this if chunked so chunking can parse a single
         *  chunk delimiter of "\r\nSIZE ...\r\n"
         */
        mprAdjustBufStart(content, 2);
    }
    return 1;
}


/*
 *  Optimization to correctly size the packets to the chunk filter.
 */
static int getChunkPacketSize(MaConn *conn, MprBuf *buf)
{
    MaRequest   *req;
    char        *start, *cp;
    int         need, size;

    req = conn->request;
    need = 0;

    switch (req->chunkState) {
    case MA_CHUNK_START:
        start = mprGetBufStart(buf);
        if (mprGetBufLength(buf) < 3) {
            return 0;
        }
        if (start[0] != '\r' || start[1] != '\n') {
            maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return 0;
        }
        for (cp = &start[2]; cp < (char*) buf->end && *cp != '\n'; cp++) {}
        if ((cp - start) < 2 || (cp[-1] != '\r' || cp[0] != '\n')) {
            /* Insufficient data */
            if ((cp - start) > 80) {
                maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 0;
            }
            return 0;
        }
        need = cp - start + 1;
        size = (int) mprAtoi(&start[2], 16);
        if (size == 0 && &cp[2] < buf->end && cp[1] == '\r' && cp[2] == '\n') {
            /* 
             *  This is the last chunk (size == 0). Now need to consume the trailing "\r\n". 
             *  We are lenient if the request does not have the trailing "\r\n" as required by the spec.
             */
            need += 2;
        }
        break;
    case MA_CHUNK_DATA:
        need = req->remainingContent;
        break;

    default:
        mprAssert(0);
    }
    req->remainingContent = need;
    return need;
}

/*
 *  Process request body data (typically post or put content). Packet will be null if the client closed the 
 *  connection to signify end of data.
 */
static bool processContent(MaConn *conn, MaPacket *packet)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaQueue         *q;
    MaHost          *host;
    MprBuf          *content;
    int             nbytes, remaining;

    req = conn->request;
    resp = conn->response;
    host = conn->host;
    q = &resp->queue[MA_QUEUE_RECEIVE];

    mprAssert(packet);
    if (packet == 0) {
        return 0;
    }

    content = packet->content;
    if (req->flags & MA_REQ_CHUNKED) {
        if ((remaining = getChunkPacketSize(conn, content)) == 0) {
            /* Need more data or bad chunk specification */
            if (mprGetBufLength(content) > 0) {
                conn->input = packet;
            }
            return 0;
        }
    } else {
        remaining = req->remainingContent;
    }
    nbytes = min(remaining, mprGetBufLength(content));
    mprAssert(nbytes >= 0);
    mprLog(conn, 7, "processContent: packet of %d bytes, remaining %d", mprGetBufLength(content), remaining);

    if (maShouldTrace(conn, MA_TRACE_REQUEST | MA_TRACE_BODY)) {
        maTraceContent(conn, packet, 0, 0, MA_TRACE_REQUEST | MA_TRACE_BODY);
    }
    if (nbytes > 0) {
        mprAssert(maGetPacketLength(packet) > 0);
        remaining -= nbytes;
        req->remainingContent -= nbytes;
        req->receivedContent += nbytes;

        if (req->receivedContent >= host->limits->maxBody) {
            conn->keepAliveCount = 0;
            maFailConnection(conn, MPR_HTTP_CODE_REQUEST_TOO_LARGE, 
                "Request content body is too big %d vs limit %d", 
                req->receivedContent, host->limits->maxBody);
            return 1;
        } 

        if (packet == req->headerPacket) {
            /* Preserve headers if more data to come. Otherwise handlers may free the packet and destory the headers */
            packet = maSplitPacket(resp, packet, 0);
        } else {
            mprStealBlock(resp, packet);
        }
        conn->input = 0;
        if (remaining == 0 && mprGetBufLength(packet->content) > nbytes) {
            /*
             *  Split excess data belonging to the next pipelined request.
             */
            mprLog(conn, 7, "processContent: Split packet of %d at %d", maGetPacketLength(packet), nbytes);
            conn->input = maSplitPacket(conn, packet, nbytes);
            mprAssert(mprGetParent(conn->input) == conn);
        }
        if ((q->count + maGetPacketLength(packet)) > q->max) {
            conn->keepAliveCount = 0;
            maFailConnection(q->conn, MPR_HTTP_CODE_REQUEST_TOO_LARGE, "Too much body data");
            return 1;
        }
        maPutNext(q, packet);

    } else {
        conn->input = 0;
        mprStealBlock(resp, packet);
    }

    if (req->remainingContent == 0) {
        /*
         *  End of input. Send a zero packet EOF signal and enable the handler send queue.
         */
        if (req->remainingContent > 0 && conn->protocol > 0) {
            maFailConnection(conn, MPR_HTTP_CODE_COMMS_ERROR, "Insufficient content data sent with request");

        } else {
            maPutNext(q, maCreateEndPacket(resp));
            conn->state = MPR_HTTP_STATE_PROCESSING;
            maRunPipeline(conn);
        }
        return 1;
    }
    maServiceQueues(conn);
    return conn->input ? mprGetBufLength(conn->input->content) : 0;
}


/*
 *  Complete the request and return true if there is a pipelined request following.
 */
bool maProcessCompletion(MaConn *conn)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaPacket    *packet;
    bool        more;

    mprAssert(conn->state == MPR_HTTP_STATE_COMPLETE);

    req = conn->request;
    resp = conn->response;

    maLogRequest(conn);

#if BLD_DEBUG
    mprLog(req, 4, "Request complete used %,d K, conn usage %,d K, mpr usage %,d K, page usage %,d K", 
        req->arena->allocBytes / 1024, conn->arena->allocBytes / 1024, mprGetMpr(conn)->heap.allocBytes / 1024, 
        mprGetMpr(conn)->pageHeap.allocBytes / 1024);
    /* mprPrintAllocReport(mprGetMpr(conn), "Before completing request"); */
#endif

    packet = conn->input;
    more = packet && (mprGetBufLength(packet->content) > 0);
    if (mprGetParent(packet) != conn) {
        if (more) {
            conn->input = maSplitPacket(conn, packet, 0);
            mprAssert(mprGetParent(conn->input) == conn);
        } else {
            conn->input = 0;
        }
    }

    /*
     *  This will free the request, response, pipeline and call maPrepConnection to reset the state.
     */
    mprFree(req->arena);
    return (conn->disconnected) ? 0 : more;
}


static void traceBuf(MaConn *conn, cchar *buf, int len, int mask)
{
    cchar   *cp, *tag, *digits;
    char    *data, *dp;
    int     level, i, printable;

    level = conn->host->traceLevel;

    for (printable = 1, i = 0; i < len; i++) {
        if (!isascii(buf[i])) {
            printable = 0;
        }
    }
    tag = (mask & MA_TRACE_RESPONSE) ? "Response" : "Request";
    if (printable) {
        data = mprAlloc(conn, len + 1);
        memcpy(data, buf, len);
        data[len] = '\0';
        mprRawLog(conn, level, "%s packet, conn %d, len %d >>>>>>>>>>\n%s", tag, conn->seqno, len, data);
        mprFree(data);
    } else {
        mprRawLog(conn, level, "%s packet, conn %d, len %d >>>>>>>>>> (binary)\n", tag, conn->seqno, len);
        data = mprAlloc(conn, len * 3 + ((len / 16) + 1) + 1);
        digits = "0123456789ABCDEF";
        for (i = 0, cp = buf, dp = data; cp < &buf[len]; cp++) {
            *dp++ = digits[(*cp >> 4) & 0x0f];
            *dp++ = digits[*cp++ & 0x0f];
            *dp++ = ' ';
            if ((++i % 16) == 0) {
                *dp++ = '\n';
            }
        }
        *dp++ = '\n';
        *dp = '\0';
        mprRawLog(conn, level, "%s", data);
    }
    mprRawLog(conn, level, "<<<<<<<<<< %s packet end, conn %d\n\n", tag, conn->seqno);
}


void maTraceContent(MaConn *conn, MaPacket *packet, int size, int offset, int mask)
{
    MaHost  *host;
    int     len;

    mprAssert(conn->trace);
    host = conn->host;

    if (offset >= host->traceMaxLength) {
        mprLog(conn, conn->host->traceLevel, "Abbreviating response trace for conn %d", conn->seqno);
        conn->trace = 0;
        return;
    }
    if (size <= 0) {
        size = INT_MAX;
    }
    if (packet->prefix) {
        len = mprGetBufLength(packet->prefix);
        len = min(len, size);
        traceBuf(conn, mprGetBufStart(packet->prefix), len, mask);
    }
    if (packet->content) {
        len = mprGetBufLength(packet->content);
        len = min(len, size);
        traceBuf(conn, mprGetBufStart(packet->content), len, mask);
    }
}


static void reportFailure(MaConn *conn, int code, cchar *fmt, va_list args)
{
    MaResponse  *resp;
    MaRequest   *req;
    cchar       *url, *status;
    char        *emsg, *msg, *filename;

    mprAssert(fmt);
    
    if (conn->requestFailed) {
        return;
    }
    conn->requestFailed = 1;
    if (fmt == 0) {
        fmt = "";
    }
    req = conn->request;
    resp = conn->response;

    msg = mprVasprintf(conn, MA_BUFSIZE, fmt, args);

    if (resp == 0 || req == 0) {
        mprLog(conn, 2, "\"%s\", code %d: %s.", mprGetHttpCodeString(conn, code), code, msg);

    } else {
        resp->code = code;
        filename = resp->filename ? resp->filename : 0;
        /* 711 is a custom error used by the test suite. */
        if (code != 711) {
            mprLog(resp, 2, "Error: \"%s\", code %d for URI \"%s\", file \"%s\": %s.", 
                mprGetHttpCodeString(conn, code), code, req->url ? req->url : "", filename ? filename : "", msg);
        }
        /*
         *  Use an error document rather than standard error boilerplate.
         */
        if (req->location) {
            url = maLookupErrorDocument(req->location, code);
            if (url && *url) {
                maRedirect(conn, 302, url);
                mprFree(msg);
                return;
            }
        }

        /*
         *  If the headers have already been filled, this alternate response body will be ignored.
         */
        if (resp->altBody == 0) {
            status = mprGetHttpCodeString(conn, code);
            /*
             *  For security, escape the message
             */
            emsg = mprEscapeHtml(resp, msg);
            resp->altBody = mprAsprintf(resp, -1, 
                "<!DOCTYPE html>\r\n"
                "<html><head><title>Document Error: %s</title></head>\r\n"
                "<body><h2>Access Error: %d -- %s</h2>\r\n<p>%s</p>\r\n</body>\r\n</html>\r\n",
                status, code, status, emsg);
        }
        resp->flags |= MA_RESP_NO_BODY;
    }
    mprFree(msg);
}


void maFailRequest(MaConn *conn, int code, cchar *fmt, ...)
{
    va_list     args;

    mprAssert(fmt);

    va_start(args, fmt);
    reportFailure(conn, code, fmt, args);
    va_end(args);
}


/*
 *  Stop all requests on the current connection. Fail the current request and the processing pipeline. 
 *  Force a connection closure as the client has disconnected or is seriously messed up.
 */
void maFailConnection(MaConn *conn, int code, cchar *fmt, ...)
{
    va_list     args;

    mprAssert(fmt);

    if (!conn->requestFailed) {
        conn->keepAliveCount = -1;
        conn->connectionFailed = 1;
        va_start(args, fmt);
        reportFailure(conn, code, fmt, args);
        va_end(args);
    }
}


void maAbortConnection(MaConn *conn, int code, cchar *fmt, ...)
{
    va_list     args;

    mprAssert(fmt);

    if (!conn->requestFailed) {
        va_start(args, fmt);
        reportFailure(conn, code, fmt, args);
        va_end(args);
        maDisconnectConn(conn);
    }
}


int maSetRequestUri(MaConn *conn, cchar *uri)
{
    MaRequest   *req;

    req = conn->request;

    /*
     *  Parse (tokenize) the request uri first. Then decode and lastly validate the URI path portion.
     *  This allows URLs to have '?' in the URL.
     */
    req->parsedUri = mprParseUri(req, uri);
    if (req->parsedUri == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    conn->response->extension = req->parsedUri->ext;
    req->url = mprValidateUrl(req, mprUrlDecode(req, req->parsedUri->url));
    return 0;
}


/*
 *  Format is:  Range: bytes=n1-n2,n3-n4,...
 *  Where n1 is first byte pos and n2 is last byte pos
 *
 *  Examples:
 *      Range: 0-49             first 50 bytes
 *      Range: 50-99,200-249    Two 50 byte ranges from 50 and 200
 *      Range: -50              Last 50 bytes
 *      Range: 1-               Skip first byte then emit the rest
 *
 *  Return 1 if more ranges, 0 if end of ranges, -1 if bad range.
 */
static bool parseRange(MaConn *conn, char *value)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaRange     *range, *last, *next;
    char        *tok, *ep;

    req = conn->request;
    resp = conn->response;

    value = mprStrdup(conn, value);
    if (value == 0) {
        return 0;
    }

    /*
     *  Step over the "bytes="
     */
    tok = mprStrTok(value, "=", &value);

    for (last = 0; value && *value; ) {
        range = mprAllocObjZeroed(req, MaRange);
        if (range == 0) {
            return 0;
        }

        /*
         *  A range "-7" will set the start to -1 and end to 8
         */
        tok = mprStrTok(value, ",", &value);
        if (*tok != '-') {
            range->start = (int) mprAtoi(tok, 10);
        } else {
            range->start = -1;
        }
        range->end = -1;

        if ((ep = strchr(tok, '-')) != 0) {
            if (*++ep != '\0') {
                /*
                 *  End is one beyond the range. Makes the math easier.
                 */
                range->end = (int) mprAtoi(ep, 10) + 1;
            }
        }
        if (range->start >= 0 && range->end >= 0) {
            range->len = range->end - range->start;
        }
        if (last == 0) {
            req->ranges = range;
        } else {
            last->next = range;
        }
        last = range;
    }

    /*
     *  Validate ranges
     */
    for (range = req->ranges; range; range = range->next) {
        if (range->end != -1 && range->start >= range->end) {
            return 0;
        }
        if (range->start < 0 && range->end < 0) {
            return 0;
        }
        next = range->next;
        if (range->start < 0 && next) {
            /* This range goes to the end, so can't have another range afterwards */
            return 0;
        }
        if (next) {
            if (next->start >= 0 && range->end > next->start) {
                return 0;
            }
        }
    }
    resp->currentRange = req->ranges;
    return (last) ? 1: 0;
}


/*
 *  Called by connectors to complete a request
 */
void maCompleteRequest(MaConn *conn)
{
    conn->state = MPR_HTTP_STATE_COMPLETE;
}


/*
 *  Connector is write blocked and can't proceed
 */
void maRequestWriteBlocked(MaConn *conn)
{
    mprLog(conn, 7, "Write blocked");
    conn->canProceed = 0;
}


void maSetNoKeepAlive(MaConn *conn)
{
    conn->keepAliveCount = 0;
}


bool maContentNotModified(MaConn *conn)
{
    MaRequest   *req;
    MaResponse  *resp;
    MprTime     modified;
    bool        same;

    req = conn->request;
    resp = conn->response;

    if (req->flags & MA_REQ_IF_MODIFIED) {
        /*
         *  If both checks, the last modification time and etag, claim that the request doesn't need to be
         *  performed, skip the transfer.
         */
        modified = (MprTime) resp->fileInfo.mtime * MPR_TICKS_PER_SEC;
        same = maMatchModified(conn, modified) && maMatchEtag(conn, resp->etag);

        if (req->ranges && !same) {
            /*
             *  Need to transfer the entire resource
             */
            mprFree(req->ranges);
            req->ranges = 0;
        }
        return same;
    }
    return 0;
}


MaRange *maCreateRange(MaConn *conn, int start, int end)
{
    MaRange     *range;

    range = mprAllocObjZeroed(conn->request, MaRange);
    if (range == 0) {
        return 0;
    }
    range->start = start;
    range->end = end;
    range->len = end - start;

    return range;
}


static void addMatchEtag(MaConn *conn, char *etag)
{
    MaRequest   *req;

    req = conn->request;

    if (req->etags == 0) {
        req->etags = mprCreateList(req);
    }
    mprAddItem(req->etags, etag);
}


/*
 *  Return TRUE if the client's cached copy matches an entity's etag.
 */
bool maMatchEtag(MaConn *conn, char *requestedEtag)
{
    MaRequest   *req;
    char        *tag;
    int         next;

    req = conn->request;

    if (req->etags == 0) {
        return 1;
    }
    if (requestedEtag == 0) {
        return 0;
    }
    for (next = 0; (tag = mprGetNextItem(req->etags, &next)) != 0; ) {
        if (strcmp(tag, requestedEtag) == 0) {
            return (req->ifMatch) ? 0 : 1;
        }
    }
    return (req->ifMatch) ? 1 : 0;
}


/*
 *  If an IF-MODIFIED-SINCE was specified, then return true if the resource has not been modified. If using
 *  IF-UNMODIFIED, then return true if the resource was modified.
 */
bool maMatchModified(MaConn *conn, MprTime time)
{
    MaRequest   *req;

    req = conn->request;

    if (req->since == 0) {
        /*  If-Modified or UnModified not supplied. */
        return 1;
    }

    if (req->ifModified) {
        /*
         *  Return true if the file has not been modified.
         */
        return !(time > req->since);

    } else {
        /*
         *  Return true if the file has been modified.
         */
        return (time > req->since);
    }
}


static void setIfModifiedDate(MaConn *conn, MprTime when, bool ifMod)
{
    MaRequest   *req;

    req = conn->request;
    req->since = when;
    req->ifModified = ifMod;
}


/*
 *  Get the next input token. The content buffer is advanced to the next token. This routine always returns a 
 *  non-zero token. The empty string means the delimiter was not found.
 */
static char *getToken(MaConn *conn, cchar *delim)
{
    MprBuf  *buf;
    char    *token, *nextToken;
    int     len;

    mprAssert(mprGetParent(conn->input) == conn);

    buf = conn->input->content;
    len = mprGetBufLength(buf);
    if (len == 0) {
        return "";
    }

    token = mprGetBufStart(buf);
    nextToken = mprStrnstr(mprGetBufStart(buf), delim, len);
    if (nextToken) {
        *nextToken = '\0';
        len = (int) strlen(delim);
        nextToken += len;
        buf->start = nextToken;

    } else {
        buf->start = mprGetBufEnd(buf);
        mprAddNullToBuf(buf);
    }
    return token;
}


cchar *maGetCookies(MaConn *conn)
{
    return conn->request->cookie;
}


void maSetRequestUser(MaConn *conn, cchar *user)
{
    MaRequest   *req;

    req = conn->request;

    mprFree(req->user);
    req->user = mprStrdup(conn->request, user);
}


void maSetRequestGroup(MaConn *conn, cchar *group)
{
    MaRequest   *req;

    req = conn->request;

    mprFree(req->group);
    req->group = mprStrdup(conn->request, group);
}


cchar *maGetQueryString(MaConn *conn)
{
    MaRequest   *req;

    req = conn->request;

    return conn->request->parsedUri->query;
}


void maSetStageData(MaConn *conn, cchar *key, cvoid *data)
{
    MaRequest      *req;

    req = conn->request;
    if (req->requestData == 0) {
        req->requestData = mprCreateHash(conn, -1);
    }
    mprAddHash(req->requestData, key, data);
}


cvoid *maGetStageData(MaConn *conn, cchar *key)
{
    MaRequest      *req;

    req = conn->request;
    if (req->requestData == 0) {
        return NULL;
    }
    return mprLookupHash(req->requestData, key);
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
