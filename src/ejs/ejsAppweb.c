/*
 *  ejsAppweb.c -- Appweb in-memory handler for the Ejscript Web Framework.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */
/********************************** Includes **********************************/

#include    "ejs.h"

#if BLD_FEATURE_EJS
#if BLD_APPWEB_PRODUCT || BLD_FEATURE_APPWEB

 #include    "appweb.h"

/*********************************** Locals ***********************************/

#if BLD_FEATURE_MULTITHREAD
static void ejsWebLock(void *lockData);
static void ejsWebUnlock(void *lockData);
#endif

/***************************** Forward Declarations *****************************/

static void error(void *handle, int code, cchar *fmt, ...);
static void redirect(void *handle, int code, cchar *url);
static void setCookie(void *handle, cchar *name, cchar *value, cchar *path, cchar *domain, int lifetime, bool secure);
static void setHeader(void *handle, bool allowMultiple, cchar *key, cchar *fmt, ...);
static int  writeBlock(void *handle, cchar *buf, int size);

/************************************* Code ***********************************/

static bool matchEjs(MaConn *conn, MaStage *handler, cchar *url)
{
    MaRequest       *req;
    cchar           *ext;
    char            urlbuf[MPR_MAX_FNAME];

    /*
     *  Send non-ejs content under web to another handler, typically the file handler.
     */
    req = conn->request;
    ext = conn->response->extension;

    if (ext && strcmp(ext, "mod") == 0) {
        maFormatBody(conn, "Bad Request", "Can't serve *.mod files");
        maFailRequest(conn, MPR_HTTP_CODE_BAD_REQUEST, "Can't server *.mod files");
    }
    if (req->location->handler != conn->http->ejsHandler && strcmp(ext, "ejs") != 0) {
        return 0;
    }
    if (*url == '\0' || strcmp(url, "/") == 0) {
        mprSprintf(urlbuf, sizeof(urlbuf), "%s/web/index.ejs", req->alias->prefix);
        maSetRequestUri(conn, urlbuf);

    } else if (strcmp(url, "/favicon.ico") == 0) {
        mprSprintf(urlbuf, sizeof(urlbuf), "%s/web/favicon.ico", req->alias->prefix);
        maSetRequestUri(conn, urlbuf);
        return 0;

    } else if (strncmp(url, "/web/", 5) == 0 || *url == '\0') {
        if (!(ext && strcmp(ext, "ejs") == 0)) {
            return 0;
        }
    } else {
        if (req->location->flags & (MA_LOC_APP | MA_LOC_APP_DIR) && ext && strcmp(ext, "ejs") == 0) {
            maFormatBody(conn, "Bad Request", "Can't serve *.ejs files outside web directory");
            maFailRequest(conn, MPR_HTTP_CODE_BAD_REQUEST, "Can't server *.ejs files outside web directory");
            return 1;
        }
    }
    return 1;
}


/*
 *  When we come here, we've already matched either a location block or an extension
 */
static void openEjs(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;
    MaResponse      *resp;
    MaLocation      *loc;
    EjsWeb          *web;
    EjsWebControl   *control;
    MprCtx          ctx;
    cchar           *ext;
    MaAlias         *alias;
    char            *url, *baseDir, *cp, *baseUrl, *searchPath, *appUrl;
    int             flags;

    /*
     *  Send non-ejs content under web to another handler, typically the file handler.
     */
    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    control = conn->http->ejsHandler->stageData;
    loc = req->location;    
    ext = resp->extension;
    alias = req->alias;
    url = req->url;
    baseUrl = 0;
    flags = 0;

    if (loc->flags & MA_LOC_APP_DIR) {
        flags |= EJS_WEB_FLAG_APP;
        baseDir = mprStrcat(resp, -1, alias->filename, &req->url[alias->prefixLen], NULL);
        if ((cp = strchr(&baseDir[strlen(alias->filename) + 1], '/')) != 0) {
            *cp = '\0';
        }
        baseUrl = mprStrcat(resp, -1, alias->prefix, &req->url[alias->prefixLen], NULL);
        if ((cp = strchr(&baseUrl[alias->prefixLen + 1], '/')) != 0) {
            *cp = '\0';
        }
        if (*url) {
            /* Step over the directory and app name */
            while (*++url != '/') ;
            if (*url) {
                while (*++url != '/') ;
            }
        }
            
    } else {
        if (loc->flags & MA_LOC_APP) {
            flags |= EJS_WEB_FLAG_APP;
        }
        baseDir = alias->filename;
        if (alias->prefixLen > 0) {
            /* Step over the application name (same as alias prefix) */
            url = &url[alias->prefixLen];
            if (*url != '/' && url[-1] == '/') {
                url--;
            }
        }
        baseUrl = alias->prefix;
    }
    appUrl = (flags & EJS_WEB_FLAG_APP) ? baseUrl : 0;

    if (loc->flags & MA_LOC_BROWSER) {
        flags |= EJS_WEB_FLAG_BROWSER_ERRORS;
    }
    if (loc->flags & MA_LOC_AUTO_SESSION) {
        flags |= EJS_WEB_FLAG_SESSION;
    }

    /*
     *  The search path for apps is:
     *      APP/modules  : [Location EjsPath] : SERVER_ROOT/modules : [EJSPATH] : APP_EXE_DIR : MOD_DIR : .
     *  For solo pages:
     *      WEB_PAGE_DIR : [Location EjsPath] : SERVER_ROOT/modules : [EJSPATH] : APP_EXE_DIR : MOD_DIR : .
     *
     *  Location EjsPaths and the EJSPATH environment variable are optional. APP_EXE_DIR is necessary for windows. The
     *  final "." will equate to the web server ServerRoot.
     *  The SERVER_ROOT/modules is supplied via control->searchPath. ejsCreateWebRequest calls ejsCreate which supplies
     *  the rest (EJSPATH : APP_EXE_DIR : MOD_DIR : .)
     */
    searchPath = NULL;
    if (flags & EJS_WEB_FLAG_APP) {
        searchPath = mprJoinPath(req, baseDir, "modules");
    } else {
        searchPath = mprStrdup(req, baseDir);
    }
    if (loc->ejsPath) {
        searchPath = mprAsprintf(req, -1, "%s" MPR_SEARCH_SEP "%s" MPR_SEARCH_SEP "%s", 
            searchPath, loc->ejsPath, control->searchPath);
    } else {
        searchPath = mprAsprintf(req, -1, "%s" MPR_SEARCH_SEP "%s", searchPath, control->searchPath);
    }
    /*
     *  Ejs works best with a heap-based allocator for longer running apps.
     */
    ctx = mprAllocHeap(req, "Ejs Interpreter", 1, 0, NULL);
    web = ejsCreateWebRequest(ctx, control, conn, baseUrl, url, baseDir, searchPath, flags);
    if (web == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't create Ejs web object for %s", url);
        return;
    }
    resp->handlerData = web;
}


/*
 *  Accept incoming body data from the browser destined for the CGI gateway (POST | PUT dat).
 *  Form vars will already have been created by the pipeline.
 */
static void incomingEjsData(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaRequest   *req;

    conn = q->conn;
    resp = conn->response;
    req = conn->request;

    if (maGetPacketLength(packet) == 0) {
        /*
         *  End of input
         */
        if (req->remainingContent > 0) {
            /*
             *  Short incoming body data
             */
            maFailRequest(conn, MPR_HTTP_CODE_BAD_REQUEST, "Client supplied insufficient body data");
        }
        maAddVarsFromQueue(q);

    } else {
        maJoinForService(q, packet, 0);
    }
    //  FUTURE - Store post data in a body property
}


/*
 *  Run the Ejscript request. The routine runs when all input data has been received.
 */
static void runEjs(MaQueue *q)
{
    MaConn      *conn;
    MaRequest   *req;
    EjsWeb      *web;
    char        *msg;

    conn = q->conn;
    req = conn->request;
    web = q->queueData = conn->response->handlerData;

    maSetHeader(conn, 0, "Last-Modified", req->host->currentDate);
    maDontCacheResponse(conn);
    maPutForService(q, maCreateHeaderPacket(conn), 0);

    if (ejsRunWebRequest(web) < 0) {
        mprAssert(web->error);
        if (web->error == 0) {
            web->error = "";
        }
        if (web->flags & EJS_WEB_FLAG_BROWSER_ERRORS) {
            msg = mprEscapeHtml(web, web->error);
            maFormatBody(conn, "Request Failed", 
                "<h1>Ejscript error for \"%s\"</h1>\r\n<h2>%s</h2>\r\n"
                "<p>To prevent errors being displayed in the browser, "
                "use <b>\"EjsErrors log\"</b> in the config file.</p>\r\n",
            web->url, msg);
        }
        maFailRequest(conn, MPR_HTTP_CODE_BAD_GATEWAY, web->error);
    }
    maPutForService(q, maCreateEndPacket(conn), 1);
}


/****************************** Control Callbacks ****************************/
/*
 *  Define params[]
 */
static void defineParams(void *handle)
{
    Ejs             *ejs;
    MaConn          *conn;
    MprHashTable    *formVars;
    MprHash         *hp;

    conn = (MaConn*) handle;
    formVars = conn->request->formVars;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;

    hp = mprGetFirstHash(formVars);
    while (hp) {
        ejsDefineWebParam(ejs, hp->key, hp->data);
        hp = mprGetNextHash(formVars, hp);
    }
}


static void discardOutput(void *handle)
{
    MaConn      *conn;

    conn = (MaConn*) handle;
    maDiscardData(conn->response->queue[MA_QUEUE_SEND].nextQ->nextQ, 0);
}


static void error(void *handle, int code, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    va_start(args, fmt);
    msg = mprVasprintf(handle, -1, fmt, args);
    maFailRequest(handle, code, "%s", msg);
}


static EjsVar *createString(Ejs *ejs, cchar *value)
{
    if (value == 0) {
        return ejs->nullValue;
    }
    return (EjsVar*) ejsCreateString(ejs, value);
}


static EjsVar *createHeaders(Ejs *ejs, MprHashTable *table)
{
    MprHash     *hp;
    EjsVar      *headers;
    EjsName     n;
    
    mprAssert(table);

    headers = (EjsVar*) ejsCreateSimpleObject(ejs);
    for (hp = 0; (hp = mprGetNextHash(table, hp)) != 0; ) {
        ejsSetPropertyByName(ejs, headers, EN(&n, hp->key), (EjsVar*) ejsCreateString(ejs, hp->data));
    }
    return headers;
}


/*
 *  Create the Request.files[] object. Each file entry contains the properties: clientFilename, contentType, filename,
 *  name and size.
 */
static EjsVar *createWebFiles(Ejs *ejs, MprHashTable *table)
{
    MprHash         *hp;
    EjsVar          *files, *file;
    EjsName         n;
    MaUploadFile    *up;
    int             index;

    if (table == 0) {
        return ejs->nullValue;
    }
    files = (EjsVar*) ejsCreateSimpleObject(ejs);

    for (index = 0, hp = 0; (hp = mprGetNextHash(table, hp)) != 0; index++) {
        up = (MaUploadFile*) hp->data;
        file = (EjsVar*) ejsCreateSimpleObject(ejs);
        ejsSetPropertyByName(ejs, file, EN(&n, "filename"), (EjsVar*) ejsCreateString(ejs, up->filename));
        ejsSetPropertyByName(ejs, file, EN(&n, "clientFilename"), (EjsVar*) ejsCreateString(ejs, up->clientFilename));
        ejsSetPropertyByName(ejs, file, EN(&n, "contentType"), (EjsVar*) ejsCreateString(ejs, up->contentType));
        ejsSetPropertyByName(ejs, file, EN(&n, "name"), (EjsVar*) ejsCreateString(ejs, hp->key));
        ejsSetPropertyByName(ejs, file, EN(&n, "size"), (EjsVar*) ejsCreateNumber(ejs, up->size));
        ejsSetPropertyByName(ejs, files, EN(&n, hp->key), file);
    }
    return files;
}

static EjsVar *getRequestVar(void *handle, int field)
{
    Ejs         *ejs;
    EjsWeb      *web;
    MaConn      *conn;
    MaQueue     *q;
    MaRequest   *req;
    MprBuf      *content;

    conn = handle;
    req = conn->request;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;
    web = ejs->handle;

    switch (field) {
    case ES_ejs_web_Request_accept:
        return createString(ejs, req->accept);

    case ES_ejs_web_Request_acceptCharset:
        return createString(ejs, req->acceptCharset);

    case ES_ejs_web_Request_acceptEncoding:
        return createString(ejs, req->acceptEncoding);

    case ES_ejs_web_Request_authAcl:
    case ES_ejs_web_Request_authGroup:
        return (EjsVar*) ejs->undefinedValue;

    case ES_ejs_web_Request_authUser:
        return (EjsVar*) ejsCreateString(ejs, req->user);

    case ES_ejs_web_Request_authType:
        return createString(ejs, req->authType);

#if ES_ejs_web_Request_body
    case ES_ejs_web_Request_body:
        q = conn->response->queue[MA_QUEUE_RECEIVE].prevQ;
        if (q->first == 0) {
            return (EjsVar*) ejs->emptyStringValue;
        }
        content = q->first->content;
        mprAddNullToBuf(content);
        return (EjsVar*) ejsCreateString(ejs, mprGetBufStart(content));
#endif

    case ES_ejs_web_Request_connection:
        return createString(ejs, req->connection);

    case ES_ejs_web_Request_contentLength:
        return (EjsVar*) ejsCreateNumber(ejs, req->length);

    case ES_ejs_web_Request_cookies:
        if (req->cookie) {
            return (EjsVar*) ejsCreateCookies(ejs);
        } else {
            return ejs->nullValue;
        }
        break;

    case ES_ejs_web_Request_extension:
        return createString(ejs, req->parsedUri->ext);

    case ES_ejs_web_Request_files:
        if (web->files == 0) {
            web->files = createWebFiles(ejs, req->files);
        }
        return (EjsVar*) web->files;

    case ES_ejs_web_Request_headers:
        if (web->headers == 0) {
            web->headers = createHeaders(ejs, conn->request->headers);
        }
        return (EjsVar*) web->headers;

    case ES_ejs_web_Request_hostName:
        return createString(ejs, req->hostName);

    case ES_ejs_web_Request_method:
        return createString(ejs, req->methodName);

    case ES_ejs_web_Request_mimeType:
        return createString(ejs, req->mimeType);

    case ES_ejs_web_Request_pathInfo:
        return createString(ejs, req->pathInfo);

    case ES_ejs_web_Request_pathTranslated:
        return createString(ejs, req->pathTranslated);

    case ES_ejs_web_Request_pragma:
        return createString(ejs, req->pragma);

    case ES_ejs_web_Request_query:
        return createString(ejs, req->parsedUri->query);

    case ES_ejs_web_Request_originalUri:
        return createString(ejs, req->parsedUri->originalUri);

    case ES_ejs_web_Request_referrer:
        return createString(ejs, req->referer);

    case ES_ejs_web_Request_remoteAddress:
        return createString(ejs, conn->remoteIpAddr);

    case ES_ejs_web_Request_remoteHost:
        /*
         *  OPT Cache the value
         */
#if BLD_FEATURE_REVERSE_DNS && BLD_UNIX_LIKE
        {
            /*
             *  This feature has denial of service risks. Doing a reverse DNS will be slower,
             *  and can potentially hang the web server. Use at your own risk!!  Not supported for windows.
             */
            struct addrinfo *result;
            char            name[MPR_MAX_STRING];
            int             rc;

            if (getaddrinfo(remoteIpAddr, NULL, NULL, &result) == 0) {
                rc = getnameinfo(result->ai_addr, sizeof(struct sockaddr), name, sizeof(name), NULL, 0, NI_NAMEREQD);
                freeaddrinfo(result);
                if (rc == 0) {
                    return createString(ejs, name);
                }
            }
        }
#endif
        return createString(ejs, conn->remoteIpAddr);

    case ES_ejs_web_Request_sessionID:
        return createString(ejs, web->session ? web->session->id : "");

    case ES_ejs_web_Request_url:
        return createString(ejs, req->url);

    case ES_ejs_web_Request_userAgent:
        return createString(ejs, req->userAgent);
    }

    ejsThrowOutOfBoundsError(ejs, "Bad property slot reference");
    return 0;
}


static EjsVar *getHostVar(void *handle, int field)
{
    Ejs         *ejs;
    MaConn      *conn;
    MaHost      *host;
    EjsWeb      *web;

    conn = handle;
    host = conn->host;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;
    web = ejs->handle;

    switch (field) {
    case ES_ejs_web_Host_documentRoot:
        return createString(ejs, host->documentRoot);

    case ES_ejs_web_Host_name:
        return createString(ejs, host->name);

    case ES_ejs_web_Host_protocol:
        return createString(ejs, host->secure ? "https" : "http");

    case ES_ejs_web_Host_isVirtualHost:
        return (EjsVar*) ejsCreateBoolean(ejs, host->flags & MA_HOST_VHOST);

    case ES_ejs_web_Host_isNamedVirtualHost:
        return (EjsVar*) ejsCreateBoolean(ejs, host->flags & MA_HOST_NAMED_VHOST);

    case ES_ejs_web_Host_software:
        return createString(ejs, MA_SERVER_NAME);

    case ES_ejs_web_Host_logErrors:
        return (EjsVar*) ((web->flags & EJS_WEB_FLAG_BROWSER_ERRORS) ? ejs->falseValue : ejs->trueValue);
    }

    ejsThrowOutOfBoundsError(ejs, "Bad property slot reference");
    return 0;
}


static EjsVar *getResponseVar(void *handle, int field)
{
    Ejs         *ejs;
    EjsWeb      *web;
    MaConn      *conn;
    MaResponse  *resp;

    conn = handle;
    resp = conn->response;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;
    web = ejs->handle;

    switch (field) {
    case ES_ejs_web_Response_code:
        return (EjsVar*) ejsCreateNumber(ejs, resp->code);

    case ES_ejs_web_Response_filename:
        return (EjsVar*) createString(ejs, resp->filename);

    case ES_ejs_web_Response_headers:
        return (EjsVar*) createHeaders(ejs, conn->response->headers);

    case ES_ejs_web_Response_mimeType:
        return (EjsVar*) createString(ejs, resp->mimeType);

    default:
        ejsThrowOutOfBoundsError(ejs, "Bad property slot reference");
        return 0;
    }
}


static cchar *getHeader(void *handle, cchar *key)
{
    MaRequest   *req;
    MaConn      *conn;

    conn = handle;
    req = conn->request;
    return (cchar*) mprLookupHash(req->headers, key);
}


static EjsVar *getVar(void *handle, int collection, int field)
{
    switch (collection) {
    case EJS_WEB_REQUEST_VAR:
        return getRequestVar(handle, field);
    case EJS_WEB_RESPONSE_VAR:
        return getResponseVar(handle, field);
    case EJS_WEB_HOST_VAR:
        return getHostVar(handle, field);
    default:
        return 0;
    }
}


static void redirect(void *handle, int code, cchar *url)
{
    maRedirect(handle, code, url);
}


static void setCookie(void *handle, cchar *name, cchar *value, cchar *path, cchar *domain, int lifetime, bool secure)
{
    maSetCookie(handle, name, value, path, domain, lifetime, secure);
}


static void setHeader(void *handle, bool allowMultiple, cchar *key, cchar *fmt, ...)
{
    char        *value;
    va_list     vargs;

    va_start(vargs, fmt);
    value = mprVasprintf(handle, -1, fmt, vargs);
    maSetHeader(handle, allowMultiple, key, "%s", value);
}


static void setHttpCode(void *handle, int code)
{
    maSetResponseCode(handle, code);
}


static void setMimeType(void *handle, cchar *mimeType)
{
    maSetResponseMimeType(handle, mimeType);
}


static int setResponseVar(void *handle, int field, EjsVar *value)
{
    Ejs         *ejs;
    EjsWeb      *web;
    EjsNumber   *code;
    MaConn      *conn;
    MaResponse  *resp;

    conn = handle;
    resp = conn->response;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;
    web = ejs->handle;

    switch (field) {
    case ES_ejs_web_Response_code:
        code = ejsToNumber(ejs, value);
        maSetResponseCode(handle, ejsGetInt(code));
        return 0;

    case ES_ejs_web_Response_filename:
    case ES_ejs_web_Response_headers:
    case ES_ejs_web_Response_mimeType:
        ejsThrowOutOfBoundsError(ejs, "Property is readonly");
        return 0;

    default:
        ejsThrowOutOfBoundsError(ejs, "Bad property slot reference");
        return 0;
    }
    return 0;
}


static int setVar(void *handle, int collection, int field, EjsVar *value)
{
    MaConn  *conn;
    Ejs     *ejs;

    conn = handle;
    ejs = ((EjsWeb*) maGetHandlerQueueData(conn))->ejs;

    switch (collection) {
    case EJS_WEB_RESPONSE_VAR:
        return setResponseVar(handle, field, value);

    default:
        ejsThrowReferenceError(ejs, "Object is read-only");
        return 0;
    }
    return 0;
}


static int writeBlock(void *handle, cchar *buf, int size)
{
    MaConn      *conn;
    MaQueue     *q;

    conn = (MaConn*) handle;

    /*
     *  We write to the service queue of the handler
     */
    q = conn->response->queue[MA_QUEUE_SEND].nextQ;
    mprAssert(q->stage->flags & MA_STAGE_HANDLER);

    return maWriteBlock(q, buf, size, 1);
}


#if BLD_FEATURE_CONFIG_PARSE
static int parseEjs(MaHttp *http, cchar *key, char *value, MaConfigState *state)
{
    MaLocation      *location;
    MaServer        *server;
    MaHost          *host;
    char            *prefix, *path;
    int             flags;
    
    host = state->host;
    server = state->server;
    location = state->location;
    
    flags = location->flags & (MA_LOC_BROWSER | MA_LOC_AUTO_SESSION);

#if KEEP
    MaStage         *ejsHandler;
    EjsWebControl   *web;
    if (mprStrcmpAnyCase(key, "Ejs") == 0) {
        path = mprStrTrim(value, "\"");
        mprNormalizePath(http, path);
        if (!mprAccess(http, path, X_OK)) {
            mprError(http, "Can't access Ejs path %s", path);
            return MPR_ERR_BAD_SYNTAX;
        }
        if ((ejsHandler = maLookupStage(http, "ejsHandler")) == 0) {
            mprError(http, "Ejscript module is not loaded");
            return MPR_ERR_BAD_SYNTAX;
        }
        web = (EjsWebControl*) ejsHandler->stageData;
        web->ejsLibDir = path;

    } else 
#endif
    if (mprStrcmpAnyCase(key, "EjsApp") == 0) {
        if (mprStrcmpAnyCase(value, "on") == 0) {
            location->flags |= MA_LOC_APP;
        } else {
            location->flags &= ~MA_LOC_APP;
        }
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsAppDir") == 0) {
        if (mprStrcmpAnyCase(value, "on") == 0) {
            location->flags |= MA_LOC_APP_DIR;
        } else {
            location->flags &= ~MA_LOC_APP_DIR;
        }
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsAppAlias") == 0) {
        if (maSplitConfigValue(server, &prefix, &path, value, 1) < 0 || path == 0 || prefix == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        location = maCreateLocationAlias(http, state, prefix, path, "ejsHandler", MA_LOC_APP | flags);
        if (location == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if (maAddFilter(http, location, "chunkFilter", "", MA_FILTER_INCOMING) < 0) {
            mprError(server, "Can't add chunkFilter for ejs");
        }
        if (maAddFilter(http, location, "uploadFilter", "", MA_FILTER_INCOMING) < 0) {
            mprError(server, "Can't add uploadFilter for ejs");
        }
#if BLD_FEATURE_UPLOAD
        location->autoDelete = 1;
#endif
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsAppDirAlias") == 0) {
        if (maSplitConfigValue(server, &prefix, &path, value, 1) < 0 || path == 0 || prefix == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        location = maCreateLocationAlias(http, state, prefix, path, "ejsHandler", MA_LOC_APP_DIR | flags);
        if (location == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
#if BLD_FEATURE_UPLOAD
        location->autoDelete = 1;
#endif
        if (maAddFilter(http, location, "chunkFilter", "", MA_FILTER_INCOMING) < 0) {
            mprError(server, "Can't add chunkFilter for ejs");
        }
        if (maAddFilter(http, location, "uploadFilter", "", MA_FILTER_INCOMING) < 0) {
            mprError(server, "Can't add uploadFilter for ejs");
        }
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsErrors") == 0) {
        if (mprStrcmpAnyCase(value, "browser") == 0) {
            location->flags |= MA_LOC_BROWSER;
        } else {
            location->flags &= ~MA_LOC_BROWSER;
        }
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsPath") == 0) {
        path = mprStrTrim(value, "\"");
        location->ejsPath = mprStrdup(location, path);
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsSessionTimeout") == 0) {
        if (value == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if (! mprGetDebugMode(http)) {
            location->sessionTimeout = atoi(mprStrTrim(value, "\""));
        }
        return 1;

    } else if (mprStrcmpAnyCase(key, "EjsSession") == 0) {
        if (mprStrcmpAnyCase(value, "on") == 0) {
            location->flags |= MA_LOC_AUTO_SESSION;
        } else {
            location->flags &= ~MA_LOC_AUTO_SESSION;
        }
        return 1;
    }

    return 0;
}
#endif


#if BLD_FEATURE_MULTITHREAD
static void ejsWebLock(void *lockData)
{
    MprMutex    *mutex = (MprMutex*) lockData;

    mprAssert(mutex);
    mprLock(mutex);
}


static void ejsWebUnlock(void *lockData)
{
    MprMutex    *mutex = (MprMutex*) lockData;

    mprAssert(mutex);
    mprUnlock(mutex);
}
#endif /* BLD_FEATURE_MULTITHREAD */


/*
 *  Dynamic module initialization
 */
MprModule *maEjsHandlerInit(MaHttp *http, cchar *path)
{
    MprModule       *module;
    MaStage         *handler;
    EjsWebControl   *control;

    control = mprAllocObjZeroed(http, EjsWebControl);

    control->defineParams = defineParams;
    control->discardOutput = discardOutput;
    control->error = error;
    control->getHeader = getHeader;
    control->getVar = getVar;
    control->redirect = redirect;
    control->setCookie = setCookie;
    control->setHeader = setHeader;
    control->setHttpCode = setHttpCode;
    control->setMimeType = setMimeType;
    control->setVar = setVar;
    control->write = writeBlock;
    control->serverRoot = mprStrdup(control, http->defaultServer->serverRoot);
    control->searchPath = mprJoinPath(control, control->serverRoot, "modules");

#if BLD_FEATURE_MULTITHREAD
    {
        MprMutex   *mutex;
        /*
         *  This mutex is used very sparingly and must be an application global lock.
         */
        mutex = mprCreateLock(control);
        control->lock = ejsWebLock;
        control->unlock = ejsWebUnlock;
        control->lockData = mutex;
    }
#endif
    if (ejsOpenWebFramework(control, 1) < 0) {
        mprError(http, "Could not initialize the Ejscript web framework");
        mprFree(control);
        return 0;
    }

    handler = maCreateHandler(http, "ejsHandler", 
        MA_STAGE_GET | MA_STAGE_HEAD | MA_STAGE_POST | MA_STAGE_PUT | MA_STAGE_VARS | MA_STAGE_VIRTUAL);
    if (handler == 0) {
        mprFree(control);
        return 0;
    }
    http->ejsHandler = handler;
    handler->match = matchEjs;
    handler->open = openEjs;
    handler->run = runEjs;
    handler->incomingData = incomingEjsData;
    handler->parse = parseEjs;
    handler->stageData = control;

    module = mprCreateModule(http, "ejsHandler", BLD_VERSION, 0, 0, 0);
    if (module == 0) {
        mprFree(handler);
        mprFree(control);
        return 0;
    }
    return module;
}


#if BLD_FEATURE_FLOATING_POINT
/*
 *  Create a routine to pull in the GCC support routines for double and int64 manipulations. Do this
 *  incase any modules reference these routines. Without this, the modules have to reference them. Which leads to
 *  multiple defines if two modules include them.
 */
double  __ejsAppweb_floating_point_resolution(double a, double b, int64 c, int64 d, uint64 e, uint64 f) {
    /*
     *  Code to pull in moddi3, udivdi3, umoddi3, etc .
     */
    a = a / b; a = a * b; c = c / d; c = c % d; e = e / f; e = e % f;
    c = (int64) a;
    d = (uint64) a;
    a = (double) c;
    a = (double) e;
    if (a == b) {
        return a;
    } return b;
}
#endif

#else

MprModule *maEjsHandlerInit(MaHttp *http, cchar *path)
{
    return 0;
}
#endif /* BLD_APPWEB_PRODUCT || BLD_FEATURE_APPWEB */
#endif /* BLD_FEATURE_EJS */


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
