/*
 *  response.c - Http response management
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static int destroyResponse(MaResponse *resp);
static void putFormattedHeader(MaConn *conn, MaPacket *packet, cchar *key, cchar *fmt, ...);
static void putHeader(MaConn *conn, MaPacket *packet, cchar *key, cchar *value);

/*********************************** Code *************************************/

MaResponse *maCreateResponse(MaConn *conn)
{
    MaResponse  *resp;
    MaHttp      *http;

    http = conn->http;

    resp = mprAllocObjWithDestructorZeroed(conn->request->arena, MaResponse, destroyResponse);
    if (resp == 0) {
        return 0;
    }

    resp->conn = conn;
    resp->code = MPR_HTTP_CODE_OK;
    resp->mimeType = "text/html";
    resp->handler = http->passHandler;
    resp->length = -1;
    resp->entityLength = -1;
    resp->chunkSize = -1;

    resp->headers = mprCreateHash(resp, MA_HEADER_HASH_SIZE);
    maInitQueue(http, &resp->queue[MA_QUEUE_SEND], "responseSendHead");
    maInitQueue(http, &resp->queue[MA_QUEUE_RECEIVE], "responseReceiveHead");
    return resp;
}


static int destroyResponse(MaResponse *resp)
{
    mprLog(resp, 5, "destroyResponse");
    maDestroyPipeline(resp->conn);
    return 0;
}


void maFillHeaders(MaConn *conn, MaPacket *packet)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaHost          *host;
    MaRange         *range;
    MprHash         *hp;
    MprBuf          *buf;
    struct tm       tm;
    char            *hdr;
    int             expires;

    mprAssert(packet->flags == MA_PACKET_HEADER);

    req = conn->request;
    resp = conn->response;
    host = req->host;
    buf = packet->content;

    if (resp->flags & MA_RESP_HEADERS_CREATED) {
        return;
    }    
    if (req->method ==  MA_REQ_TRACE || req->method == MA_REQ_OPTIONS) {
        maTraceOptions(conn);
    }
    mprPutStringToBuf(buf, req->httpProtocol);
    mprPutCharToBuf(buf, ' ');
    mprPutIntToBuf(buf, resp->code);
    mprPutCharToBuf(buf, ' ');
    mprPutStringToBuf(buf, mprGetHttpCodeString(resp, resp->code));
    mprPutStringToBuf(buf, "\r\n");

    putHeader(conn, packet, "Date", req->host->currentDate);
    putHeader(conn, packet, "Server", MA_SERVER_NAME);

    if (resp->flags & MA_RESP_DONT_CACHE) {
        putHeader(conn, packet, "Cache-Control", "no-cache");
    } else if (req->location->expires) {
        expires = PTOI(mprLookupHash(req->location->expires, resp->mimeType));
        if (expires == 0) {
            expires = PTOI(mprLookupHash(req->location->expires, ""));
        }
        if (expires) {
            mprDecodeUniversalTime(conn, &tm, mprGetTime(conn) + (expires * MPR_TICKS_PER_SEC));
            hdr = mprFormatTime(conn, MPR_HTTP_DATE, &tm);
            putFormattedHeader(conn, packet, "Cache-Control", "max-age=%d", expires);
            putFormattedHeader(conn, packet, "Expires", "%s", hdr);
            mprFree(hdr);
        }
    }
    if (resp->etag) {
        putFormattedHeader(conn, packet, "ETag", "%s", resp->etag);
    }
    if (resp->altBody) {
        resp->length = (int) strlen(resp->altBody);
    }
    if (resp->chunkSize > 0 && !resp->altBody) {
        if (!(req->method & MA_REQ_HEAD)) {
            maSetHeader(conn, 0, "Transfer-Encoding", "chunked");
        }

    } else if (resp->length >= 0) {
        putFormattedHeader(conn, packet, "Content-Length", "%d", resp->length);
    }

    if (req->ranges) {
        if (req->ranges->next == 0) {
            range = req->ranges;
            if (resp->entityLength > 0) {
                putFormattedHeader(conn, packet, "Content-Range", "bytes %d-%d/%d", 
                    range->start, range->end, resp->entityLength);
            } else {
                putFormattedHeader(conn, packet, "Content-Range", "bytes %d-%d/*", range->start, range->end);
            }
        } else {
            putFormattedHeader(conn, packet, "Content-Type", "multipart/byteranges; boundary=%s", resp->rangeBoundary);
        }
        putHeader(conn, packet, "Accept-Ranges", "bytes");

    } else if (resp->code != MPR_HTTP_CODE_MOVED_TEMPORARILY && resp->mimeType[0]) {
        if (!mprLookupHash(resp->headers, "Content-Type")) {
            putHeader(conn, packet, "Content-Type", resp->mimeType);
        }
    }

    if (--conn->keepAliveCount > 0) {
        putHeader(conn, packet, "Connection", "keep-alive");
        putFormattedHeader(conn, packet, "Keep-Alive", "timeout=%d, max=%d", 
            host->keepAliveTimeout / 1000, conn->keepAliveCount);
    } else {
        putHeader(conn, packet, "Connection", "close");
    }

    /*
     *  Output any remaining custom headers
     */
    hp = mprGetFirstHash(resp->headers);
    while (hp) {
        putHeader(conn, packet, hp->key, hp->data);
        hp = mprGetNextHash(resp->headers, hp);
    }

#if BLD_DEBUG
{
    static int seq = 0;
    putFormattedHeader(conn, packet, "X-Appweb-Seq", "%d", seq++);
}
#endif

    /*
     *  By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (resp->chunkSize <= 0 || resp->altBody) {
        mprPutStringToBuf(buf, "\r\n");
    }
    if (resp->altBody) {
        mprPutStringToBuf(buf, resp->altBody);
        maDiscardData(resp->queue[MA_QUEUE_SEND].nextQ, 0);
    }
    resp->headerSize = mprGetBufLength(buf);
    resp->flags |= MA_RESP_HEADERS_CREATED;
#if UNUSED
    mprLog(conn, 3, "\n@@@ Response => \n%s", mprGetBufStart(buf));
#endif
}


void maTraceOptions(MaConn *conn)
{
    MaResponse  *resp;
    MaRequest   *req;
    int         flags;

    if (conn->requestFailed) {
        return;
    }
    resp = conn->response;
    req = conn->request;

    if (req->method & MA_REQ_TRACE) {
        if (req->host->flags & MA_HOST_NO_TRACE) {
            resp->code = MPR_HTTP_CODE_NOT_ACCEPTABLE;
            maFormatBody(conn, "Trace Request Denied", "<p>The TRACE method is disabled on this server.</p>");
        } else {
            resp->altBody = mprAsprintf(resp, -1, "%s %s %s\r\n", req->methodName, req->parsedUri->originalUri, 
                req->httpProtocol);
        }

    } else if (req->method & MA_REQ_OPTIONS) {
        if (resp->handler == 0) {
            maSetHeader(conn, 0, "Allow", "OPTIONS,TRACE");

        } else {
            flags = resp->handler->flags;
            maSetHeader(conn, 0, "Allow", "OPTIONS,TRACE%s%s%s%s%s",
                (flags & MA_STAGE_GET) ? ",GET" : "",
                (flags & MA_STAGE_HEAD) ? ",HEAD" : "",
                (flags & MA_STAGE_POST) ? ",POST" : "",
                (flags & MA_STAGE_PUT) ? ",PUT" : "",
                (flags & MA_STAGE_DELETE) ? ",DELETE" : "");
        }
        resp->length = 0;
    }
}


static void putFormattedHeader(MaConn *conn, MaPacket *packet, cchar *key, cchar *fmt, ...)
{
    va_list     args;
    char        *value;

    va_start(args, fmt);
    value = mprVasprintf(packet, MA_MAX_HEADERS, fmt, args);
    va_end(args);

    putHeader(conn, packet, key, value);
    mprFree(value);
}


static void putHeader(MaConn *conn, MaPacket *packet, cchar *key, cchar *value)
{
    MprBuf      *buf;

    buf = packet->content;

    mprPutStringToBuf(buf, key);
    mprPutStringToBuf(buf, ": ");
    if (value) {
        mprPutStringToBuf(buf, value);
    }
    mprPutStringToBuf(buf, "\r\n");
}


int maFormatBody(MaConn *conn, cchar *title, cchar *fmt, ...)
{
    MaResponse  *resp;
    va_list     args;
    char        *body;

    resp = conn->response;
    mprAssert(resp->altBody == 0);

    va_start(args, fmt);

    body = mprVasprintf(resp, MA_MAX_HEADERS, fmt, args);

    resp->altBody = mprAsprintf(resp, -1,
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body>\r\n%s\r\n</body>\r\n</html>\r\n",
        title, body);
    mprFree(body);
    va_end(args);
    return strlen(resp->altBody);
}


/*
 *  Redirect the user to another web page. The targetUri may or may not have a scheme.
 */
void maRedirect(MaConn *conn, int code, cchar *targetUri)
{
    MaResponse  *resp;
    MaRequest   *req;
    MaHost      *host;
    MprUri      *target, *prev;
    char        *path, *uri, *dir, *cp, *hostName;
    int         port;

    mprAssert(targetUri);

    req = conn->request;
    resp = conn->response;
    host = req->host;

    mprLog(conn, 3, "redirect %d %s", code, targetUri);

    if (resp->redirectCallback) {
        targetUri = (resp->redirectCallback)(conn, &code, targetUri);
    }

    uri = 0;
    resp->code = code;

    prev = req->parsedUri;
    target = mprParseUri(resp, targetUri);

    if (strstr(targetUri, "://") == 0) {
        /*
         *  Redirect does not have a host specifier. Use request "Host" header by preference, then the host ServerName.
         */
        if (req->hostName) {
            hostName = req->hostName;
        } else {
            hostName = host->name;
        }
        port = strchr(targetUri, ':') ? prev->port : conn->address->port;
        if (target->url[0] == '/') {
            /*
             *  Absolute URL. If hostName has a port specifier, it overrides prev->port.
             */
            uri = mprFormatUri(resp, prev->scheme, hostName, port, target->url, target->query);
        } else {
            /*
             *  Relative file redirection to a file in the same directory as the previous request.
             */
            dir = mprStrdup(resp, req->url);
            if ((cp = strrchr(dir, '/')) != 0) {
                /* Remove basename */
                *cp = '\0';
            }
            path = mprStrcat(resp, -1, dir, "/", target->url, NULL);
            uri = mprFormatUri(resp, prev->scheme, hostName, port, path, target->query);
        }
        targetUri = uri;
    }

    maSetHeader(conn, 0, "Location", "%s", targetUri);
    mprAssert(resp->altBody == 0);
    resp->altBody = mprAsprintf(resp, -1,
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body><h1>%s</h1>\r\n<p>The document has moved <a href=\"%s\">here</a>.</p>\r\n"
        "<address>%s at %s Port %d</address></body>\r\n</html>\r\n",
        mprGetHttpCodeString(conn, code), mprGetHttpCodeString(conn, code), targetUri,
        MA_SERVER_NAME, host->name, prev->port);
    mprFree(uri);

    /*
     *  The original request has failed and is being redirected. This is regarded as a request failure and
     *  will prevent further processing of the pipeline.
     */
    conn->requestFailed = 1;
}


void maDontCacheResponse(MaConn *conn)
{
    conn->response->flags |= MA_RESP_DONT_CACHE;
}


void maSetHeader(MaConn *conn, bool allowMultiple, cchar *key, cchar *fmt, ...)
{
    MaResponse      *resp;
    char            *value;
    va_list         vargs;

    resp = conn->response;

    va_start(vargs, fmt);
    value = mprVasprintf(resp, MA_MAX_HEADERS, fmt, vargs);

    if (allowMultiple) {
        mprAddDuplicateHash(resp->headers, key, value);
    } else {
        mprAddHash(resp->headers, key, value);
    }
}


void maSetCookie(MaConn *conn, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, int lifetime, bool isSecure)
{
    MaRequest   *req;
    MaResponse  *resp;
    struct tm   tm;
    char        *cp, *expiresAtt, *expires, *domainAtt, *domain, *secure;
    int         webkitVersion;

    req = conn->request;
    resp = conn->response;

    if (path == 0) {
        path = "/";
    }

    /*
     *  Fix for Safari >= 3.2.1 with Bonjour addresses with a trailing ".". Safari discards cookies without a domain 
     *  specifier or with a domain that includes a trailing ".". Solution: include an explicit domain and trim the 
     *  trailing ".".
     *
     *   User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_6; en-us) 
     *       AppleWebKit/530.0+ (KHTML, like Gecko) Version/3.1.2 Safari/525.20.1
     */
    webkitVersion = 0;
    if (cookieDomain == 0 && req->userAgent && strstr(req->userAgent, "AppleWebKit") != 0) {
        if ((cp = strstr(req->userAgent, "Version/")) != NULL && strlen(cp) >= 13) {
            cp = &cp[8];
            webkitVersion = (cp[0] - '0') * 100 + (cp[2] - '0') * 10 + (cp[4] - '0');
        }
    }
    if (webkitVersion >= 312) {
        domain = mprStrdup(resp, req->hostName);
        if ((cp = strchr(domain, ':')) != 0) {
            *cp = '\0';
        }
        if (*domain && domain[strlen(domain) - 1] == '.') {
            domain[strlen(domain) - 1] = '\0';
        } else {
            domain = 0;
        }
    } else {
        domain = 0;
    }
    if (domain) {
        domainAtt = "; domain=";
    } else {
        domainAtt = "";
    }
    if (lifetime > 0) {
        mprDecodeUniversalTime(resp, &tm, conn->time + (lifetime * MPR_TICKS_PER_SEC));
        expiresAtt = "; expires=";
        expires = mprFormatTime(resp, MPR_HTTP_DATE, &tm);

    } else {
        expires = expiresAtt = "";
    }
    if (isSecure) {
        secure = "; secure";
    } else {
        secure = ";";
    }

    /*
     *  Allow multiple cookie headers. Even if the same name. Later definitions take precedence
     */
    maSetHeader(conn, 1, "Set-Cookie", 
        mprStrcat(resp, -1, name, "=", value, "; path=", path, domainAtt, domain, expiresAtt, expires, secure, NULL));

    maSetHeader(conn, 0, "Cache-control", "no-cache=\"set-cookie\"");
}


void maSetEntityLength(MaConn *conn, int len)
{
    MaRequest       *req;
    MaResponse      *resp;

    resp = conn->response;
    req = conn->request;

    resp->entityLength = len;
    if (req->ranges == 0) {
        resp->length = len;
    }
}


void maSetResponseCode(MaConn *conn, int code)
{
    conn->response->code = code;
}


void maSetResponseMimeType(MaConn *conn, cchar *mimeType)
{
    MaResponse      *resp;

    resp = conn->response;

    /*  Can't free old mime type, as it is sometimes a literal constant */
    resp->mimeType = mprStrdup(resp, mimeType);
}


void maOmitResponseBody(MaConn *conn)
{
    if (conn->response) {
        conn->response->flags |= MA_RESP_NO_BODY;
        // conn->response->mimeType = "";
    }
}


void maSetRedirectCallback(MaConn *conn, MaRedirectCallback redirectCallback)
{
    conn->response->redirectCallback = redirectCallback;
}


void maSetEnvCallback(MaConn *conn, MaEnvCallback envCallback)
{
    conn->response->envCallback = envCallback;
}


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
