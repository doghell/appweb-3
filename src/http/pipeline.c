/*
 *  pipeline.c -- HTTP pipeline request processing.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static MaStage *checkStage(MaConn *conn, MaStage *stage);
static MaStage *findHandler(MaConn *conn);
static MaStage *mapToFile(MaConn *conn, MaStage *handler);
static bool matchFilter(MaConn *conn, MaFilter *filter);
static bool rewriteRequest(MaConn *conn);
static void openQ(MaQueue *q);
static MaStage *processDirectory(MaConn *conn, MaStage *handler);
static void setEnv(MaConn *conn);
static void setPathInfo(MaConn *conn);
static void startQ(MaQueue *q);

/*********************************** Code *************************************/
/*
    Find the matching handler for a request. If any errors occur, the pass handler is used to pass errors onto the 
    net/sendfile connectors to send to the client. This routine may rewrite the request URI and may redirect the request.
 */
void maMatchHandler(MaConn *conn)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaStage         *handler;

    req = conn->request;
    resp = conn->response;
    handler = 0;

    mprAssert(req->url);
    mprAssert(req->alias);
    mprAssert(resp->filename);
    mprAssert(resp->fileInfo.checked);

    /*
        Get the best (innermost) location block and see if a handler is explicitly set for that location block.
     */
    while (!handler && !conn->requestFailed && req->rewrites++ < MA_MAX_REWRITE) {
        /*
            Give stages a cance to rewrite the request, then match the location handler. If that doesn't match, 
            try to match by extension and/or handler match() routines. This may invoke processDirectory which 
            may redirect and thus require reprocessing -- hence the loop.
         */
        if (!rewriteRequest(conn)) {
            if ((handler = checkStage(conn, req->location->handler)) == 0) {
                handler = findHandler(conn);
            }
            handler = mapToFile(conn, handler);
        }
    }
    if (handler == 0) {
        handler = conn->http->passHandler;
        if (!conn->requestFailed) {
            if (req->rewrites >= MA_MAX_REWRITE) {
                maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
            } else if (!(req->method & (MA_REQ_OPTIONS | MA_REQ_TRACE))) {
                maFailRequest(conn, MPR_HTTP_CODE_BAD_METHOD, "Requested method %s not supported for URL: %s", 
                    req->methodName, req->url);
            }
        }

    } else if (req->method & (MA_REQ_OPTIONS | MA_REQ_TRACE)) {
        if ((req->flags & MA_REQ_OPTIONS) != !(handler->flags & MA_STAGE_OPTIONS)) {
            handler = conn->http->passHandler;

        } else if ((req->flags & MA_REQ_TRACE) != !(handler->flags & MA_STAGE_TRACE)) {
            handler = conn->http->passHandler;
        }
    }
    resp->handler = handler;
    mprLog(resp, 3, "Select handler: \"%s\" for \"%s\"", handler->name, req->url);
}


static bool rewriteRequest(MaConn *conn)
{
    MaResponse      *resp;
    MaRequest       *req;
    MaStage         *handler;
    MprHash         *he;
    int             next;

    req = conn->request;
    resp = conn->response;
    mprAssert(resp->filename);
    mprAssert(resp->fileInfo.checked);
    mprAssert(req->alias);

    if (req->alias->redirectCode) {
        maRedirect(conn, req->alias->redirectCode, req->alias->uri);
        return 1;
    }
    for (next = 0; (handler = mprGetNextItem(req->location->handlers, &next)) != 0; ) {
        if (handler->modify && handler->modify(conn, handler)) {
            return 1;
        }
    }
    for (he = 0; (he = mprGetNextHash(req->location->extensions, he)) != 0; ) {
        handler = (MaStage*) he->data;
        if (handler->modify && handler->modify(conn, handler)) {
            return 1;
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
    mprAssert(location);
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
            if (filter->stage == http->rangeFilter && req->ranges == 0) {
                continue;
            }
            if ((filter->stage->flags & MA_STAGE_ALL & req->method) == 0) {
                continue;
            }
            if (matchFilter(conn, filter)) {
                mprAddItem(resp->outputPipeline, filter->stage);
            }
        }
    }
    connector = location->connector;
    if (connector == 0) {
        mprError(conn, "No connector defined, using net connector");
        connector = http->netConnector;
    }
#if BLD_FEATURE_SEND
    if (resp->handler == http->fileHandler && connector == http->netConnector && req->method == MA_REQ_GET && 
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
     *  Open the queues (keep going on errors). Open in reverse order to open the handler last.
     *  This ensures the authFilter runs before the handler.
     */
    qhead = &resp->queue[MA_QUEUE_SEND];
    for (q = qhead->prevQ; q != qhead; q = q->prevQ) {
        if (q->open && !(q->flags & MA_QUEUE_OPEN)) {
            q->flags |= MA_QUEUE_OPEN;
            openQ(q);
        }
    }

    if (req->remainingContent > 0) {
        qhead = &resp->queue[MA_QUEUE_RECEIVE];
        for (q = qhead->prevQ; q != qhead; q = q->prevQ) {
            if (q->open && !(q->flags & MA_QUEUE_OPEN)) {
                if (q->pair == 0 || !(q->pair->flags & MA_QUEUE_OPEN)) {
                    q->flags |= MA_QUEUE_OPEN;
                    openQ(q);
                }
            }
        }
    }
    /*
        Now that all stages are open, we can set the environment
     */
    setEnv(conn);

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
        MEASURE(conn, q->stage->name, "run", q->stage->run(q));
    }
    if (conn->request) {
        return maServiceQueues(conn);
    }
    return 0;
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
    if (stage->match && !(stage->flags & MA_STAGE_UNLOADED)) {
        /* Can't have match routines on unloadable modules */
        if (!stage->match(conn, stage, req->url)) {
            return 0;
        }
    }
    return stage;
}


static cchar *getExtension(MaConn *conn, cchar *path)
{
    cchar   *cp;
    char    *ext, *ep;

    if ((cp = strrchr(path, '.')) != 0) {
        ext = mprStrdup(conn->request, ++cp);
        for (ep = ext; *ep && isalnum((int)*ep); ep++) {
            ;
        }
        *ep = '\0';
        return ext;
    }
    return 0;
}

/*
    Get an extension used for mime type matching. This finds the last extension in the Uri 
    (or filename if absent in the Uri). Note, the extension may be followed by extra path information.
 */
cchar *maGetExtension(MaConn *conn)
{
    MaRequest   *req;
    cchar       *ext;

    req = conn->request;
    ext = getExtension(conn, &req->url[req->alias->prefixLen]);
    if (ext == 0) {
        ext = getExtension(conn, conn->response->filename);
    }
    if (ext == 0) {
        ext = "";
    }
    return ext;
}


/*
 *  Search for a handler by request extension. If that fails, use handler custom matching.
 *  If all that fails, return the catch-all handler (fileHandler)
 */
static MaStage *findHandler(MaConn *conn)
{
    MaRequest   *req;
    MaResponse  *resp;
    MaStage     *handler;
    MaLocation  *location;
    MprHash     *hp;
    cchar       *ext;
    char        *path;
    int         next;

    req = conn->request;
    resp = conn->response;
    location = req->location;
    handler = 0;
    
    /*
        Do custom handler matching first
     */
    for (next = 0; (handler = mprGetNextItem(location->handlers, &next)) != 0; ) {
        if (handler->match && checkStage(conn, handler)) {
            resp->handler = handler;
            return handler;
        }
    }

    ext = resp->extension;
    if (*ext) {
        handler = maGetHandlerByExtension(location, resp->extension);
        if (checkStage(conn, handler)) {
            return handler;
        }

    } else {
        /*
            URI has no extension, check if the addition of configured  extensions results in a valid filename.
         */
        for (path = 0, hp = 0; (hp = mprGetNextHash(location->extensions, hp)) != 0; ) {
            handler = (MaStage*) hp->data;
            if (*hp->key && (handler->flags & MA_STAGE_MISSING_EXT)) {
                path = mprStrcat(resp, -1, resp->filename, ".", hp->key, NULL);
                if (mprGetPathInfo(conn, path, &resp->fileInfo) == 0) {
                    mprLog(conn, 5, "findHandler: Adding extension, new path %s\n", path);
                    maSetRequestUri(conn, mprStrcat(resp, -1, req->url, ".", hp->key, NULL), NULL);
                    return handler;
                }
                mprFree(path);
            }
        }
    }

    /*
        Failed to match. Return any catch-all handler
     */
    handler = maGetHandlerByExtension(location, "");
    if (handler == 0) {
        /*
            Could be missing a catch-all in the config file, so invoke the file handler.
         */
        handler = maLookupStage(conn->http, "fileHandler");
    }
    if ((handler = checkStage(conn, handler)) == 0) {
        handler = conn->http->passHandler;
    }
    return handler;
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


static MaStage *mapToFile(MaConn *conn, MaStage *handler)
{
    MaRequest   *req;
    MaResponse  *resp;
    MprPath     *info, ginfo;
    char        *gfile;

    req = conn->request;
    resp = conn->response;
    info = &resp->fileInfo;

    mprAssert(resp->filename);
    mprAssert(info->checked);

    if (!handler || (handler->flags & MA_STAGE_VIRTUAL)) {
        return handler;
    }
    if ((req->dir = maLookupBestDir(req->host, resp->filename)) == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Missing directory block for %s", resp->filename);
    } else {
        if (req->dir->auth) {
            req->auth = req->dir->auth;
        }
        if (info->isDir) {
            handler = processDirectory(conn, handler);
        } else if (info->valid) {
            /*
                Define an Etag for physical entities. Redo the file info if not valid now that extra path has been removed.
             */
            resp->etag = mprAsprintf(resp, -1, "\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
        } else {
            if (req->acceptEncoding) {
                if (strstr(req->acceptEncoding, "gzip") != 0) {
                    gfile = mprAsprintf(resp, -1, "%s.gz", resp->filename);
                    if (mprGetPathInfo(resp, gfile, &ginfo) == 0) {
                        resp->filename = gfile;
                        resp->fileInfo = ginfo;
                        maSetHeader(conn, 0, "Content-Encoding", "gzip");
                        return handler;
                    }
                }
            }
            if (req->method != MA_REQ_PUT && handler->flags & MA_STAGE_VERIFY_ENTITY && 
                    (req->auth == 0 || req->auth->type == 0)) {
                /* If doing Authentication, must let authFilter generate the response */
                maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Can't open document: %s", resp->filename);
            }
        }
    }
    return handler;
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
    MaStage     *stage;
    MaResponse  *resp;

    conn = q->conn;
    resp = conn->response;
    stage = q->stage;

    if (resp->chunkSize > 0) {
        q->packetSize = min(q->packetSize, resp->chunkSize);
    }
    if (stage->flags & MA_STAGE_UNLOADED) {
        mprAssert(stage->path);
        mprLog(q, 2, "Loading module %s", stage->name);
        stage->module = maLoadModule(conn->http, stage->name, stage->path);
    }
    if (stage->module) {
        stage->module->lastActivity = conn->host->now;
    }
    q->flags |= MA_QUEUE_OPEN;
    if (q->open) {
        MEASURE(conn, stage->name, "open", stage->open(q));
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
        MEASURE(q, q->stage->name, "start", q->start(q));
    }
}


/*
 *  Manage requests to directories. This will either do an external redirect back to the browser or do an internal 
 *  (transparent) redirection and serve different content back to the browser. This routine may modify the requested 
 *  URI and/or the request handler.
 */
static MaStage *processDirectory(MaConn *conn, MaStage *handler)
{
    MaRequest       *req;
    MaResponse      *resp;
    MprUri          *prior;
    MprPath         *info;
    char            *path, *index, *uri, *pathInfo;

    req = conn->request;
    resp = conn->response;
    info = &resp->fileInfo;
    prior = req->parsedUri;
    mprAssert(info->isDir);

    index = req->dir->indexName;
    path = mprJoinPath(resp, resp->filename, index);
   
    if (req->url[strlen(req->url) - 1] == '/') {
        /*
            Internal directory redirections
         */
        if (mprPathExists(resp, path, R_OK)) {
            /*
                Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
                Return zero so the request will be rematched on return.
             */
            pathInfo = mprJoinPath(req, req->url, index);
            uri = mprFormatUri(req, prior->scheme, prior->host, prior->port, pathInfo, prior->query);
            maSetRequestUri(conn, uri, NULL);
            return 0;
        }
        mprFree(path);

    } else {
        /*
         *  External redirect. If the index exists, redirect to it. If not, append a "/" to the URI and redirect.
         */
        if (mprPathExists(resp, path, R_OK)) {
            pathInfo = mprJoinPath(req, req->url, index);
        } else {
            pathInfo = mprJoinPath(req, req->url, "/");
        }
        uri = mprFormatUri(req, prior->scheme, prior->host, prior->port, pathInfo, prior->query);
        maRedirect(conn, MPR_HTTP_CODE_MOVED_PERMANENTLY, uri);
        handler = conn->http->passHandler;
    }
    return handler;
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
                offset = alias->prefixLen + (int) (last - start);
                if (offset < (int) strlen(req->url)) {
                    pathInfo = &req->url[offset];
                    req->pathInfo = mprStrdup(req, pathInfo);
                    pathInfo[0] = '\0';
                    maSetRequestUri(conn, req->url, NULL);
                } else {
                    req->pathInfo = "";
                }
                if (req->pathInfo && req->pathInfo[0]) {
                    req->pathTranslated = maMakeFilename(conn, alias, req->pathInfo, 0);
                }
            }
        }
        if (req->pathInfo == 0) {
            req->pathInfo = req->url;
            maSetRequestUri(conn, "/", NULL);
            req->pathTranslated = maMakeFilename(conn, alias, req->pathInfo, 0); 
        }
    }
}


static void setEnv(MaConn *conn)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaStage         *handler;

    req = conn->request;
    resp = conn->response;
    handler = resp->handler;

    mprAssert(resp->filename);
    mprAssert(resp->extension);
    mprAssert(resp->mimeType);

    setPathInfo(conn);

    if (handler->flags & MA_STAGE_VARS && req->parsedUri->query) {
        maAddVars(req->formVars, req->parsedUri->query, (int) strlen(req->parsedUri->query));
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
    @copy   default
  
    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.
  
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.
  
    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://www.embedthis.com/downloads/gplLicense.html
  
    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com
  
    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
