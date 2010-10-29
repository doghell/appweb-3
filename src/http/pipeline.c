/*
 *  pipeline.c -- HTTP pipeline request processing.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static char *addIndexToUrl(MaConn *conn, cchar *index);
static MaStage *checkStage(MaConn *conn, MaStage *stage);
static char *getExtension(MaConn *conn);
static MaStage *findHandlerByExtension(MaConn *conn);
static bool mapToFile(MaConn *conn, MaStage *handler, bool *rescan);
static bool matchFilter(MaConn *conn, MaFilter *filter);
static bool modifyRequest(MaConn *conn);
static void openQ(MaQueue *q);
static void processDirectory(MaConn *conn, bool *rescan);
static void setEnv(MaConn *conn);
static void setPathInfo(MaConn *conn);
static void startQ(MaQueue *q);

/*********************************** Code *************************************/
/*
 *  Find the matching handler for a request. If any errors occur, the pass handler is used to pass errors onto the 
 *  net/sendfile connectors to send to the client. This routine may rewrite the request URI and may redirect the request.
 */
void maMatchHandler(MaConn *conn)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaHost          *host;
    MaAlias         *alias;
    MaStage         *handler;
    MaLocation      *location;
    bool            rescan;
    int             loopCount;

    req = conn->request;
    resp = conn->response;
    host = req->host;

    /*
     *  Find the alias that applies for this url. There is always a catch-all alias for the document root.
     */
    alias = req->alias = maGetAlias(host, req->url);
    mprAssert(alias);
    if (alias->redirectCode) {
        maRedirect(conn, alias->redirectCode, alias->uri);
        return;
    }
    location = req->location = maLookupBestLocation(req->host, req->url);
    mprAssert(location);
    req->auth = location->auth;

    if (conn->requestFailed || conn->request->method & (MA_REQ_OPTIONS | MA_REQ_TRACE)) {
        handler = conn->http->passHandler;
        return;
    }
    if (modifyRequest(conn)) {
        return;
    }
    /*
     *  Get the best (innermost) location block and see if a handler is explicitly set for that location block.
     *  Possibly rewrite the url and retry.
     */
    loopCount = MA_MAX_REWRITE;
    do {
        rescan = 0;
        if ((handler = checkStage(conn, req->location->handler)) == 0) {
            /*
             *  Didn't find a location block handler, so try to match by extension and by handler match() routines.
             *  This may invoke processDirectory which may redirect and thus require reprocessing -- hence the loop.
             */
            handler = findHandlerByExtension(conn);
        }
        if (handler && !(handler->flags & MA_STAGE_VIRTUAL)) {
            if (!mapToFile(conn, handler, &rescan)) {
                return;
            }
        }
    } while (handler && rescan && loopCount-- > 0);
    if (handler == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_BAD_METHOD, "Requested method %s not supported for URL: %s", 
            req->methodName, req->url);
        handler = conn->http->passHandler;
    }
    resp->handler = handler;
    mprLog(resp, 4, "Select handler: \"%s\" for \"%s\"", handler->name, req->url);
    setEnv(conn);
}


static bool modifyRequest(MaConn *conn)
{
    MaStage         *handler;
    MaLocation      *location;
    MaResponse      *resp;
    MaRequest       *req;
    MprPath         *info;
    MprHash         *he;
    int             next;

    req = conn->request;
    resp = conn->response;

    if (resp->filename == 0) {
        resp->filename = maMakeFilename(conn, req->alias, req->url, 1);
    }
    info = &resp->fileInfo;
    if (!info->checked) {
        mprGetPathInfo(conn, resp->filename, info);
    }
    location = conn->request->location;
    if (location) {
        for (next = 0; (handler = mprGetNextItem(location->handlers, &next)) != 0; ) {
            if (handler->modify) {
                if (handler->modify(conn, handler)) {
                    return 1;
                }
            }
        }
        for (he = 0; (he = mprGetNextHash(location->extensions, he)) != 0; ) {
            handler = (MaStage*) he->data;
            if (handler->modify) {
                if (handler->modify(conn, handler)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}


/*
 *  Create stages for the request pipeline.
 */
void maCreatePipeline(MaConn *conn)
{
    MaHttp          *http;
    MaHost          *host;
    MaResponse      *resp;
    MaRequest       *req;
    MaStage         *handler;
    MaLocation      *location;
    MaStage         *stage, *connector;
    MaFilter        *filter;
    MaQueue         *q, *qhead, *rq, *rqhead;
    int             next;

    req = conn->request;
    resp = conn->response;
    host = req->host;
    location = req->location;
    handler = resp->handler;
    http = conn->http;

    mprAssert(req);
    mprAssert(location->outputStages);

    /*
     *  Create the output pipeline for this request. Handler first, then filters, connector last.
     */
    resp->outputPipeline = mprCreateList(resp);

    /*
     *  Add the handler and filters. If the request has failed, switch to the pass handler which will just 
     *  pass data along.
     */
    if (conn->requestFailed) {
        handler = resp->handler = http->passHandler;
        mprAddItem(resp->outputPipeline, resp->handler);
    } else {
        mprAddItem(resp->outputPipeline, resp->handler);
        for (next = 0; (filter = mprGetNextItem(location->outputStages, &next)) != 0; ) {
            if (filter->stage == http->authFilter) {
                if (req->auth->type == 0) {
                    continue;
                }
            }
            if (filter->stage == http->rangeFilter && (req->ranges == 0 || handler == http->fileHandler)) {
                continue;
            }
            if ((filter->stage->flags & MA_STAGE_ALL & req->method) == 0) {
                continue;
            }
            /*
             *  Remove the chunk filter chunking if it is explicitly turned off vi a the X_APPWEB_CHUNK_SIZE header 
             *  setting the chunk size to zero. Also remove if using the fileHandler which always knows the entity 
             *  length and an explicit chunk size has not been requested.
             */
            if (filter->stage == http->chunkFilter) {
                if ((handler == http->fileHandler && resp->chunkSize < 0) || resp->chunkSize == 0) {
                    continue;
                }
            }
            if (matchFilter(conn, filter)) {
                mprAddItem(resp->outputPipeline, filter->stage);
            }
        }
    }
    
    connector = location->connector;
#if BLD_FEATURE_SEND
    if (resp->handler == http->fileHandler && connector == http->netConnector && 
            http->sendConnector && !req->ranges && !host->secure && resp->chunkSize <= 0 && !conn->trace) {
        /*
            Switch (transparently) to the send connector if serving whole static file content via the net connector
            and not tracing.
        */
        connector = http->sendConnector;
    }
#endif
    resp->connector = connector;
    if ((connector->flags & MA_STAGE_ALL & req->method) == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_BAD_REQUEST, "Connector \"%s\" does not support the \"%s\" method \"%s\"", 
            connector->name, req->methodName);
    }
    mprAddItem(resp->outputPipeline, connector);

    /*
     *  Create the outgoing queue heads and open the queues
     */
    q = &resp->queue[MA_QUEUE_SEND];
    for (next = 0; (stage = mprGetNextItem(resp->outputPipeline, &next)) != 0; ) {
        q = maCreateQueue(conn, stage, MA_QUEUE_SEND, q);
    }

    /*
     *  Create the receive pipeline for this request. Connector first, handler last. Must be created even if the request
     *  has failed so input chunking can be processed to keep the connection alive.
     */
    if (req->remainingContent > 0 || (req->method == MA_REQ_PUT || req->method == MA_REQ_POST)) {
        req->inputPipeline = mprCreateList(resp);

        mprAddItem(req->inputPipeline, connector);
        for (next = 0; (filter = mprGetNextItem(location->inputStages, &next)) != 0; ) {
            if (filter->stage == http->authFilter || !matchFilter(conn, filter)) {
                continue;
            }
            if (filter->stage == http->chunkFilter && !(req->flags & MA_REQ_CHUNKED)) {
                continue;
            }
            if ((filter->stage->flags & MA_STAGE_ALL & req->method) == 0) {
                continue;
            }
            mprAddItem(req->inputPipeline, filter->stage);
        }
        mprAddItem(req->inputPipeline, handler);

        /*
         *  Create the incoming queue heads and open the queues.
         */
        q = &resp->queue[MA_QUEUE_RECEIVE];
        for (next = 0; (stage = mprGetNextItem(req->inputPipeline, &next)) != 0; ) {
            q = maCreateQueue(conn, stage, MA_QUEUE_RECEIVE, q);
        }
    }

    /*
     *  Pair up the send and receive queues. NOTE: can't use a stage multiple times.
     */
    qhead = &resp->queue[MA_QUEUE_SEND];
    rqhead = &resp->queue[MA_QUEUE_RECEIVE];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        for (rq = rqhead->nextQ; rq != rqhead; rq = rq->nextQ) {
            if (q->stage == rq->stage) {
                q->pair = rq;
                rq->pair = q;
            }
        }
    }

    /*
     *  Open the queues (keep going on errors)
     */
    qhead = &resp->queue[MA_QUEUE_SEND];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->open && !(q->flags & MA_QUEUE_OPEN)) {
            q->flags |= MA_QUEUE_OPEN;
            openQ(q);
        }
    }

    if (req->remainingContent > 0) {
        qhead = &resp->queue[MA_QUEUE_RECEIVE];
        for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
            if (q->open && !(q->flags & MA_QUEUE_OPEN)) {
                if (q->pair == 0 || !(q->pair->flags & MA_QUEUE_OPEN)) {
                    q->flags |= MA_QUEUE_OPEN;
                    openQ(q);
                }
            }
        }
    }

    /*
        Invoke any start routines
     */
    qhead = &resp->queue[MA_QUEUE_SEND];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->start && !(q->flags & MA_QUEUE_STARTED)) {
            q->flags |= MA_QUEUE_STARTED;
            startQ(q);
        }
    }

    if (req->remainingContent > 0) {
        qhead = &resp->queue[MA_QUEUE_RECEIVE];
        for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
            if (q->start && !(q->flags & MA_QUEUE_STARTED)) {
                if (q->pair == 0 || !(q->pair->flags & MA_QUEUE_STARTED)) {
                    q->flags |= MA_QUEUE_STARTED;
                    startQ(q);
                }
            }
        }
    }
    conn->flags |= MA_CONN_PIPE_CREATED;
}


void maDestroyPipeline(MaConn *conn)
{
    MaResponse      *resp;
    MaQueue         *q, *qhead;
    int             i;

    resp = conn->response;

    if (conn->flags & MA_CONN_PIPE_CREATED && resp) {
        for (i = 0; i < MA_MAX_QUEUE; i++) {
            qhead = &resp->queue[i];
            for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
                if (q->close && q->flags & MA_QUEUE_OPEN) {
                    q->flags &= ~MA_QUEUE_OPEN;
                    q->close(q);
                }
            }
        }
        conn->flags &= ~MA_CONN_PIPE_CREATED;
    }
}


/*
 *  Invoke the run routine for the handler and then pump the pipeline by servicing all scheduled queues.
 */
bool maRunPipeline(MaConn *conn)
{
    MaQueue     *q;
    
    q = conn->response->queue[MA_QUEUE_SEND].nextQ;
    
    if (q->stage->run) {
        q->stage->run(q);
    }
    return maServiceQueues(conn);
}


/*
 *  Run the queue service routines until there is no more work to be done. NOTE: all I/O is non-blocking.
 */
bool maServiceQueues(MaConn *conn)
{
    MaQueue     *q;
    bool        workDone;

    workDone = 0;
    while (!conn->disconnected && (q = maGetNextQueueForService(&conn->serviceq)) != NULL) {
        maServiceQueue(q);
        workDone = 1;
    }
    return workDone;
}


void maDiscardPipeData(MaConn *conn)
{
    MaResponse      *resp;
    MaQueue         *q, *qhead;

    resp = conn->response;
    if (resp == 0) {
        return;
    }

    qhead = &resp->queue[MA_QUEUE_SEND];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        maDiscardData(q, 0);
    }

    qhead = &resp->queue[MA_QUEUE_RECEIVE];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        maDiscardData(q, 0);
    }
}


static char *addIndexToUrl(MaConn *conn, cchar *index)
{
    MaRequest       *req;
    char            *path;

    req = conn->request;

    path = mprJoinPath(req, req->url, index);
    if (req->parsedUri->query && req->parsedUri->query[0]) {
        return mprReallocStrcat(req, -1, path, "?", req->parsedUri->query, NULL);
    }
    return path;
}


static MaStage *checkStage(MaConn *conn, MaStage *stage)
{
    MaRequest   *req;

    req = conn->request;

    if (stage == 0) {
        return 0;
    }
    if ((stage->flags & MA_STAGE_ALL & req->method) == 0) {
        return 0;
    }
    if (stage->match && !stage->match(conn, stage, req->url)) {
        return 0;
    }
    return stage;
}


/*
 *  Get an extension used for mime type matching. NOTE: this does not permit any kind of platform specific filename.
 *  Rather only those suitable as mime type extensions (simple alpha numeric extensions)
 */
static char *getExtension(MaConn *conn)
{
    MaRequest   *req;
    char        *cp;
    char        *ep, *ext;

    req = conn->request;
    if ((cp = strrchr(&req->url[req->alias->prefixLen], '.')) != 0) {
        ext = mprStrdup(req, ++cp);
        for (ep = ext; *ep && isalnum((int)*ep); ep++) {
            ;
        }
        *ep = '\0';
        return ext;
    }
    return "";
}


/*
 *  Search for a handler by request extension. If that fails, use handler custom matching.
 *  If all that fails, return the catch-all handler (fileHandler)
 */
static MaStage *findHandlerByExtension(MaConn *conn)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaStage     *handler;
    MaLocation  *location;
    int         next;

    req = conn->request;
    resp = conn->response;
    location = req->location;
    
    resp->extension = getExtension(conn);
#if UNUSED
    if (resp->filename == 0) {
        resp->filename = maMakeFilename(conn, req->alias, req->url, 1);
    }
    info = &resp->fileInfo;
    if (!info->checked) {
        mprGetPathInfo(conn, resp->filename, info);
    }
#endif

    if (*resp->extension) {
        handler = maGetHandlerByExtension(location, resp->extension);
        if (checkStage(conn, handler)) {
            return handler;
        }
    }

    /*
     *  Failed to match by extension, so perform custom handler matching
     */
    for (next = 0; (handler = mprGetNextItem(location->handlers, &next)) != 0; ) {
        if (handler->match && handler->match(conn, handler, req->url)) {
            if (checkStage(conn, handler)) {
                resp->handler = handler;
                return handler;
            }
        }
    }

    /*
     *  Failed to match. Return any catch-all handler.
     */
    handler = maGetHandlerByExtension(location, "");
    if (handler == 0) {
        /*
         *  Could be missing a catch-all in the config file, so invoke the file handler.
         */
        handler = maLookupStage(conn->http, "fileHandler");
    }
    if (handler == 0) {
        handler = conn->http->passHandler;
    }
    mprAssert(handler);
    resp->handler = handler;
    
    return checkStage(conn, handler);
}


char *maMakeFilename(MaConn *conn, MaAlias *alias, cchar *url, bool skipAliasPrefix)
{
    char        *cleanPath, *path;

    mprAssert(alias);
    mprAssert(url);

    if (skipAliasPrefix) {
        url += alias->prefixLen;
    }
    while (*url == '/') {
        url++;
    }
    if ((path = mprJoinPath(conn->request, alias->filename, url)) == 0) {
        return 0;
    }
    cleanPath = mprGetNativePath(conn, path);
    mprFree(path);
    return cleanPath;
}


static bool mapToFile(MaConn *conn, MaStage *handler, bool *rescan)
{
    MaRequest   *req;
    MaResponse  *resp;

    req = conn->request;
    resp = conn->response;

    if (resp->filename == 0) {
        resp->filename = maMakeFilename(conn, req->alias, req->url, 1);
    }
    req->dir = maLookupBestDir(req->host, resp->filename);

    if (req->dir == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Missing directory block for %s", resp->filename);
        return 0;
    }
    req->auth = req->dir->auth;

    if (!resp->fileInfo.checked && mprGetPathInfo(conn, resp->filename, &resp->fileInfo) < 0) {
        mprAssert(handler);
        if (req->method != MA_REQ_PUT && handler->flags & MA_STAGE_VERIFY_ENTITY) {
            maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't open document: %s", resp->filename);
            return 0;
        }
    }
    if (resp->fileInfo.isDir) {
        processDirectory(conn, rescan);
    }
    return 1;
}


/*
 *  Match a filter by extension
 */
static bool matchFilter(MaConn *conn, MaFilter *filter)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaLocation      *location;
    MaStage         *stage;

    req = conn->request;
    resp = conn->response;
    location = req->location;
    stage = filter->stage;

    if (stage->match) {
        return stage->match(conn, stage, req->url);
    }
    if (filter->extensions && *resp->extension) {
        return maMatchFilterByExtension(filter, resp->extension);
    }
    return 1;
}


static void openQ(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;

    conn = q->conn;
    resp = conn->response;

    if (resp->chunkSize > 0) {
        q->packetSize = min(q->packetSize, resp->chunkSize);
    }
    q->flags |= MA_QUEUE_OPEN;
    if (q->open) {
        q->open(q);
    }
}


static void startQ(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;

    conn = q->conn;
    resp = conn->response;

    if (resp->chunkSize > 0) {
        q->packetSize = min(q->packetSize, resp->chunkSize);
    }
    q->flags |= MA_QUEUE_STARTED;
    if (q->start) {
        q->start(q);
    }
}


/*
 *  Manage requests to directories. This will either do an external redirect back to the browser or do an internal 
 *  (transparent) redirection and serve different content back to the browser. This routine may modify the requested 
 *  URI and/or the request handler.
 */
static void processDirectory(MaConn *conn, bool *rescan)
{
    MaRequest       *req;
    MaResponse      *resp;
    MprPath         *info;
    char            *path, *index;

    req = conn->request;
    resp = conn->response;
    info = &resp->fileInfo;

    mprAssert(info->isDir);
    index = req->dir->indexName;
    if (req->url[strlen(req->url) - 1] == '/') {
        /*
            Internal directory redirections
         */
        path = mprJoinPath(resp, resp->filename, index);
        if (mprPathExists(resp, path, R_OK)) {
            /*
             *  Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
             *  Must rematch the handler on return.
             */
            maSetRequestUri(conn, addIndexToUrl(conn, index));
            resp->filename = path;
            mprGetPathInfo(conn, resp->filename, &resp->fileInfo);
            resp->extension = getExtension(conn);
            if ((resp->mimeType = (char*) maLookupMimeType(conn->host, resp->extension)) == 0) {
                resp->mimeType = (char*) "text/html";
            }
            *rescan = 1;
        } else {
            mprFree(path);
        }
        return;
    }

    /*
     *  External redirect. Ask the client to re-issue a request for a new location. See if an index exists and if so, 
     *  construct a new location for the index. If the index can't be accessed, just append a "/" to the URI and redirect.
     */
    if (req->parsedUri->query && req->parsedUri->query[0]) {
        path = mprAsprintf(resp, -1, "%s/%s?%s", req->url, index, req->parsedUri->query);
    } else {
        path = mprJoinPath(resp, req->url, index);
    }
    if (!mprPathExists(resp, path, R_OK)) {
        path = mprStrcat(resp, -1, req->url, "/", NULL);
    }
    maRedirect(conn, MPR_HTTP_CODE_MOVED_PERMANENTLY, path);
    resp->handler = conn->http->passHandler;
}


static bool fileExists(MprCtx ctx, cchar *path) {
    if (mprPathExists(ctx, path, R_OK)) {
        return 1;
    }
#if BLD_WIN_LIKE
{
    char    *file;
    file = mprStrcat(ctx, -1, path, ".exe", NULL);
    if (mprPathExists(ctx, file, R_OK)) {
        return 1;
    }
    file = mprStrcat(ctx, -1, path, ".bat", NULL);
    if (mprPathExists(ctx, file, R_OK)) {
        return 1;
    }
}
#endif
    return 0;
}


/*
 *  Set the pathInfo (PATH_INFO) and update the request uri. This may set the response filename if convenient.
 */
static void setPathInfo(MaConn *conn)
{
    MaStage     *handler;
    MaAlias     *alias;
    MaRequest   *req;
    MaResponse  *resp;
    char        *last, *start, *cp, *pathInfo;
    int         found, sep, offset;

    req = conn->request;
    resp = conn->response;
    alias = req->alias;
    handler = resp->handler;

    mprAssert(handler);

    if (handler && handler->flags & MA_STAGE_PATH_INFO) {
        if (!(handler->flags & MA_STAGE_VIRTUAL)) {
            /*
             *  Find the longest subset of the filename that matches a real file. Test each segment to see if 
             *  it corresponds to a real physical file. This also defines a new response filename after trimming the 
             *  extra path info.
             */
            last = 0;
            sep = mprGetPathSeparator(req, resp->filename);
            for (cp = start = &resp->filename[strlen(alias->filename)]; cp; ) {
                if ((cp = strchr(cp, sep)) != 0) {
                    *cp = '\0';
                }
                found = fileExists(conn, resp->filename);
                if (cp) {
                    *cp = sep;
                }
                if (found) {
                    if (cp) {
                        last = cp++;
                    } else {
                        last = &resp->filename[strlen(resp->filename)];
                        break;
                    }
                } else {
                    break;
                }
            }
            if (last) {
                offset = alias->prefixLen + last - start;
                if (offset <= strlen(req->url)) {
                    pathInfo = &req->url[offset];
                    req->pathInfo = mprStrdup(req, pathInfo);
                    *last = '\0';
                    pathInfo[0] = '\0';
                }
                if (req->pathInfo[0]) {
                    req->pathTranslated = maMakeFilename(conn, alias, req->pathInfo, 0);
                }
            }
        }
        if (req->pathInfo == 0) {
            req->pathInfo = req->url;
            req->url = "";

            if ((cp = strrchr(req->pathInfo, '.')) != 0) {
                resp->extension = mprStrdup(req, ++cp);
            } else {
                resp->extension = "";
            }
            req->pathTranslated = maMakeFilename(conn, alias, req->pathInfo, 0); 
        }
    }
}


static void setEnv(MaConn *conn)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaStage         *handler;
    MprPath         *info;

    req = conn->request;
    resp = conn->response;
    handler = resp->handler;

    setPathInfo(conn);

    if (resp->extension == 0) {
        resp->extension = getExtension(conn);
    }
    if (resp->filename == 0) {
        resp->filename = maMakeFilename(conn, req->alias, req->url, 1);
    }
    if ((resp->mimeType = (char*) maLookupMimeType(conn->host, resp->extension)) == 0) {
        resp->mimeType = (char*) "text/html";
    }
    if (!(resp->handler->flags & MA_STAGE_VIRTUAL)) {
        /*
         *  Define an Etag for physical entities. Redo the file info if not valid now that extra path has been removed.
         */
        info = &resp->fileInfo;
        if (!info->checked) {
            mprGetPathInfo(conn, resp->filename, info);
        }
        if (info->valid) {
            resp->etag = mprAsprintf(resp, -1, "\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
        }
    }
    if (handler->flags & MA_STAGE_VARS) {
        if (req->parsedUri->query) {
            maAddVars(conn, req->parsedUri->query, (int) strlen(req->parsedUri->query));
        }
    }
    if (handler->flags & MA_STAGE_ENV_VARS) {
        maCreateEnvVars(conn);
        if (resp->envCallback) {
            resp->envCallback(conn);
        }
    }
}


char *maMapUriToStorage(MaConn *conn, cchar *url)
{
    MaAlias     *alias;

    alias = maGetAlias(conn->request->host, url);
    if (alias == 0) {
        return 0;
    }
    return maMakeFilename(conn, alias, url, 1);
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
