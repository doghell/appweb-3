/*
    phpHandler.c - Appweb PHP handler
  
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

#if BLD_FEATURE_PHP

#if BLD_WIN_LIKE
    /*
     *  Workaround for VS 2005 and PHP headers. Need to include before PHP headers include it.
     */
    #if _MSC_VER >= 1400
        #include    <sys/utime.h>
    #endif
    #undef  WIN32
    #define WIN32 1
    #define WINNT 1
    #define TIME_H
    #undef _WIN32_WINNT
    #undef chdir
    #undef popen
    #undef pclose
    #define PHP_WIN32 1
    #define ZEND_WIN32 1
    #define ZTS 1
#endif
    #undef ulong
    #undef HAVE_SOCKLEN_T

    /*
     *  Indent headers to side-step make depend if PHP is not enabled
     */
    #include <main/php.h>
    #include <main/php_globals.h>
    #include <main/php_variables.h>
    #include <Zend/zend_modules.h>
    #include <main/SAPI.h>
#ifdef PHP_WIN32
    #include <win32/time.h>
    #include <win32/signal.h>
    #include <process.h>
#else
    #include <main/build-defs.h>
#endif
    #include <Zend/zend.h>
    #include <Zend/zend_extensions.h>
    #include <main/php_ini.h>
    #include <main/php_globals.h>
    #include <main/php_main.h>
#if ZTS
    #include <TSRM/TSRM.h>
#endif

/********************************** Defines ***********************************/

typedef struct MaPhp {
    zval    *var_array;             /* Track var array */
} MaPhp;

/****************************** Forward Declarations **********************/

static void flushOutput(void *context);
static void logMessage(char *message);
static int  initializePhp(MaHttp *http);
static char *readCookies(TSRMLS_D);
static int  readPostData(char *buffer, uint len TSRMLS_DC);
static void registerServerVars(zval *varArray TSRMLS_DC);
static int  startup(sapi_module_struct *sapiModule);
static int  sendHeaders(sapi_headers_struct *sapiHeaders TSRMLS_DC);
static int  writeBlock(cchar *str, uint len TSRMLS_DC);

#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
static int  writeHeader(sapi_header_struct *sapiHeader, sapi_header_op_enum op, sapi_headers_struct *sapiHeaders TSRMLS_DC);
#else
static int  writeHeader(sapi_header_struct *sapiHeader, sapi_headers_struct *sapiHeaders TSRMLS_DC);
#endif

/************************************ Locals **********************************/
/*
 *  PHP Module Interface
 */
static sapi_module_struct phpSapiBlock = {
    BLD_PRODUCT,                    /* Sapi name */
    BLD_NAME,                       /* Full name */
    startup,                        /* Start routine */
    php_module_shutdown_wrapper,    /* Stop routine  */
    0,                              /* Activate */
    0,                              /* Deactivate */
    writeBlock,                     /* Write */
    flushOutput,                    /* Flush */
    0,                              /* Getuid */
    0,                              /* Getenv */
    php_error,                      /* Errors */
    writeHeader,                    /* Write headers */
    sendHeaders,                    /* Send headers */
    0,                              /* Send single header */
    readPostData,                   /* Read body data */
    readCookies,                    /* Read session cookies */
    registerServerVars,             /* Define request variables */
    logMessage,                     /* Emit a log message */
    NULL,                           /* Override for the php.ini */
    0,                              /* Block */
    0,                              /* Unblock */
    STANDARD_SAPI_MODULE_PROPERTIES
};

/********************************** Code ***************************/

static void openPhp(MaQueue *q)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaConn          *conn;
    MaAlias         *alias;

    conn = q->conn;
    if (!q->stage->stageData) {
        if (initializePhp(conn->http) < 0) {
            maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "PHP initialization failed");
        }
        q->stage->stageData = (void*) 1;
    }
    resp = conn->response;
    req = conn->request;
    alias = req->alias;

    switch (req->method) {
    case MA_REQ_GET:
    case MA_REQ_HEAD:
    case MA_REQ_POST:
    case MA_REQ_PUT:
        q->queueData = mprAllocObjZeroed(resp, MaPhp);
        maDontCacheResponse(conn);
        maSetHeader(conn, 0, "Last-Modified", req->host->currentDate);
        break;
                
    case MA_REQ_DELETE:
    default:
        maFailRequest(q->conn, MPR_HTTP_CODE_BAD_METHOD, "Method not supported by file handler: %s", req->methodName);
        break;
    }
}


static void runPhp(MaQueue *q)
{
    MaConn              *conn;
    MaRequest           *req;
    MaResponse          *resp;
    MprHash             *hp;
    MaPhp               *php;
    FILE                *fp;
    char                shebang[MPR_MAX_STRING];
    zend_file_handle    file_handle;

    TSRMLS_FETCH();

    conn = q->conn;
    req = conn->request;
    resp = conn->response;
    php = q->queueData;

    maPutForService(q, maCreateHeaderPacket(q), 0);

    /*
     *  Set the request context
     */
    zend_first_try {
        php->var_array = 0;
        SG(server_context) = conn;
        SG(request_info).auth_user = req->user;
        SG(request_info).auth_password = req->password;
        SG(request_info).content_type = req->mimeType;
        SG(request_info).content_length = req->length;
        SG(sapi_headers).http_response_code = MPR_HTTP_CODE_OK;
        SG(request_info).path_translated = resp->filename;
        SG(request_info).query_string = req->parsedUri->query;
        SG(request_info).request_method = req->methodName;
        SG(request_info).request_uri = req->url;

        /*
         *  Workaround on MAC OS X where the SIGPROF is given to the wrong thread
         */
        PG(max_input_time) = -1;
        EG(timeout_seconds) = 0;

        php_request_startup(TSRMLS_C);
        CG(zend_lineno) = 0;

    } zend_catch {
        mprError(q, "Can't start PHP request");
        zend_try {
            php_request_shutdown(0);
        } zend_end_try();
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR, "PHP initialization failed");
        return;
    } zend_end_try();

    /*
     *  Define header variables
     */
    zend_try {
        hp = mprGetFirstHash(req->headers);
        while (hp) {
            if (hp->data) {
                php_register_variable(hp->key, (char*) hp->data, php->var_array TSRMLS_CC);
            }
            hp = mprGetNextHash(req->headers, hp);
        }
        hp = mprGetFirstHash(req->formVars);
        while (hp) {
            if (hp->data) {
                php_register_variable(hp->key, (char*) hp->data, php->var_array TSRMLS_CC);
            }
            hp = mprGetNextHash(req->formVars, hp);
        }
    } zend_end_try();

    /*
     *  Execute the script file
     */
    file_handle.filename = resp->filename;
    file_handle.free_filename = 0;
    file_handle.opened_path = 0;

#if LOAD_FROM_FILE
    file_handle.type = ZEND_HANDLE_FILENAME;
#else
    file_handle.type = ZEND_HANDLE_FP;
    if ((fp = fopen(resp->filename, "r")) == NULL) {
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR,  "PHP can't open script");
        return;
    }
    /*
        Check for shebang and skip
     */
    file_handle.handle.fp = fp;
    shebang[0] = '\0';
    fgets(shebang, sizeof(shebang), file_handle.handle.fp);
    if (shebang[0] != '#' || shebang[1] != '!') {
        fseek(fp, 0L, SEEK_SET);
    }
#endif

    zend_try {
        php_execute_script(&file_handle TSRMLS_CC);
        if (!SG(headers_sent)) {
            sapi_send_headers(TSRMLS_C);
        }
    } zend_catch {
        php_request_shutdown(0);
        maFailRequest(conn, MPR_HTTP_CODE_INTERNAL_SERVER_ERROR,  "PHP script execution failed");
        return;
    } zend_end_try();

    zend_try {
        php_request_shutdown(0);
    } zend_end_try();

    maPutForService(q, maCreateEndPacket(q), 1);
}

/*************************** PHP Support Functions ***************************/
/*
 *
 *  Flush write data back to the client
 */
static void flushOutput(void *server_context)
{
    MaConn      *conn;

    conn = (MaConn*) server_context;
    if (conn) {
        maServiceQueues(conn);
    }
}


static int writeBlock(cchar *str, uint len TSRMLS_DC)
{
    MaConn      *conn;
    int         written;

    conn = (MaConn*) SG(server_context);
    if (conn == 0) {
        return -1;
    }
    written = maWriteBlock(conn->response->queue[MA_QUEUE_SEND].nextQ, str, len, 1);
    mprLog(mprGetMpr(0), 6, "php: write %d", written);
    if (written <= 0) {
        php_handle_aborted_connection();
    }
    return written;
}


static void registerServerVars(zval *track_vars_array TSRMLS_DC)
{
    MaConn      *conn;
    MaPhp       *php;

    conn = (MaConn*) SG(server_context);
    if (conn == 0) {
        return;
    }
    php_import_environment_variables(track_vars_array TSRMLS_CC);

    if (SG(request_info).request_uri) {
        php_register_variable("PHP_SELF", SG(request_info).request_uri,  track_vars_array TSRMLS_CC);
    }
    php = maGetHandlerQueueData(conn);
    mprAssert(php);
    php->var_array = track_vars_array;
}


static void logMessage(char *message)
{
    mprLog(mprGetMpr(0), 0, "phpModule: %s", message);
}


static char *readCookies(TSRMLS_D)
{
    MaConn      *conn;

    conn = (MaConn*) SG(server_context);
    return conn->request->cookie;
}


static int sendHeaders(sapi_headers_struct *phpHeaders TSRMLS_DC)
{
    MaConn      *conn;

    conn = (MaConn*) SG(server_context);
    maSetResponseCode(conn, phpHeaders->http_response_code);
    maSetResponseMimeType(conn, phpHeaders->mimetype);
    mprLog(mprGetMpr(0), 6, "php: send headers");
    return SAPI_HEADER_SENT_SUCCESSFULLY;
}


#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
static int writeHeader(sapi_header_struct *sapiHeader, sapi_header_op_enum op, sapi_headers_struct *sapiHeaders TSRMLS_DC)
#else
static int writeHeader(sapi_header_struct *sapiHeader, sapi_headers_struct *sapi_headers TSRMLS_DC)
#endif
{
    MaConn      *conn;
    MaResponse  *resp;
    bool        allowMultiple;
    char        *key, *value;

    conn = (MaConn*) SG(server_context);
    resp = conn->response;
    allowMultiple = 1;

    key = mprStrdup(resp, sapiHeader->header);
    if ((value = strchr(key, ':')) == 0) {
        return -1;
    }
    *value++ = '\0';
    while (!isalnum(*value) && *value) {
        value++;
    }
#if PHP_MAJOR_VERSION >=5 && PHP_MINOR_VERSION >= 3
    switch(op) {
        case SAPI_HEADER_DELETE_ALL:
            return 0;

        case SAPI_HEADER_DELETE:
            return 0;

        case SAPI_HEADER_REPLACE:
            maSetHeader(conn, 0, key, value);
            return SAPI_HEADER_ADD;

        case SAPI_HEADER_ADD:
            maSetHeader(conn, 1, key, value);
            return SAPI_HEADER_ADD;

        default:
            return 0;
    }
#else
    allowMultiple = !sapiHeader->replace;
    maSetHeader(conn, allowMultiple, key, value);
    return SAPI_HEADER_ADD;
#endif
}


static int readPostData(char *buffer, uint bufsize TSRMLS_DC)
{
    MaConn      *conn;
    MaQueue     *q;
    MprBuf      *content;
    int         len, nbytes;

    conn = (MaConn*) SG(server_context);
    q = conn->response->queue[MA_QUEUE_RECEIVE].prevQ;
    if (q->first == 0) {
        return 0;
    }
    content = q->first->content;
    len = min(mprGetBufLength(content), (int) bufsize);
    if (len > 0) {
        nbytes = mprMemcpy(buffer, len, mprGetBufStart(content), len);
        mprAssert(nbytes == len);
        mprAdjustBufStart(content, len);
    }
    mprLog(mprGetMpr(0), 6, "php: read post data %d remaining %d", len, mprGetBufLength(content));
    return len;
}


static int startup(sapi_module_struct *sapi_module)
{
    return php_module_startup(sapi_module, 0, 0);
}


/*
 *  Initialze the php module
 */
static int initializePhp(MaHttp *http)
{
#if ZTS
    void                    ***tsrm_ls;
    php_core_globals        *core_globals;
    sapi_globals_struct     *sapi_globals;
    zend_llist              global_vars;
    zend_compiler_globals   *compiler_globals;
    zend_executor_globals   *executor_globals;

    tsrm_startup(128, 1, 0, 0);
    compiler_globals = (zend_compiler_globals*)  ts_resource(compiler_globals_id);
    executor_globals = (zend_executor_globals*)  ts_resource(executor_globals_id);
    core_globals = (php_core_globals*) ts_resource(core_globals_id);
    sapi_globals = (sapi_globals_struct*) ts_resource(sapi_globals_id);
    tsrm_ls = (void***) ts_resource(0);
#endif

    mprLog(http, 2, "php: initialize php library");
#ifdef BLD_FEATURE_PHP_INI
    phpSapiBlock.php_ini_path_override = BLD_FEATURE_PHP_INI;
#else
    phpSapiBlock.php_ini_path_override = http->defaultServer->serverRoot;
#endif
    sapi_startup(&phpSapiBlock);
    if (php_module_startup(&phpSapiBlock, 0, 0) == FAILURE) {
        mprError(http, "PHP did not initialize");
        return MPR_ERR_CANT_INITIALIZE;
    }
#if ZTS
    zend_llist_init(&global_vars, sizeof(char *), 0, 0);
#endif

    SG(options) |= SAPI_OPTION_NO_CHDIR;
    zend_alter_ini_entry("register_argc_argv", 19, "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
    zend_alter_ini_entry("html_errors", 12, "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
    zend_alter_ini_entry("implicit_flush", 15, "0", 1, PHP_INI_SYSTEM, PHP_INI_STAGE_ACTIVATE);
    return 0;
}


static int finalizePhp(MprModule *mp)
{
    MaHttp      *http;
    MaStage     *stage;

    mprLog(mp, 4, "php: Finalize library before unloading");

    TSRMLS_FETCH();
    php_module_shutdown(TSRMLS_C);
    sapi_shutdown();
#if ZTS
    tsrm_shutdown();
#endif
    http = mprGetMpr(ctx)->appwebHttpService;
    if ((stage = maLookupStage(http, "phpHandler")) != 0) {
        stage->stageData = 0;
    }
    return 0;
}


/*
 *  Dynamic module initialization
 */
MprModule *maPhpHandlerInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *handler;

    if ((module = mprCreateModule(http, "phpHandler", BLD_VERSION, http, NULL, finalizePhp)) == 0) {
        return 0;
    }
    module->path = mprStrdup(module, path);

    if ((handler = maLookupStage(http, "phpHandler")) == 0) {
        handler = maCreateHandler(http, "phpHandler", 
            MA_STAGE_ALL | MA_STAGE_ENV_VARS | MA_STAGE_PATH_INFO | MA_STAGE_VERIFY_ENTITY | MA_STAGE_MISSING_EXT);
        if (handler == 0) {
            mprFree(module);
            return 0;
        }
    }
    http->phpHandler = handler;
    handler->open = openPhp;
    handler->run = runPhp;
    handler->module = module;
    mprFree(handler->path);
    handler->path = mprStrdup(handler, path);
    return module;
}


#else
MprModule *maPhpHandlerInit(MaHttp *http, cchar *path)
{
    return 0;
}

#endif /* BLD_FEATURE_PHP */

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
