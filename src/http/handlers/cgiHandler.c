/* 
 *  cgiHandler.c -- Common Gateway Interface Handler
 *
 *  Support the CGI/1.1 standard for external gateway programs to respond to HTTP requests.
 *  This CGI handler uses async-pipes and non-blocking I/O for all communications.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

#if BLD_FEATURE_CGI
/*********************************** Forwards *********************************/

static void buildArgs(MaConn *conn, MprCmd *cmd, int *argcp, char ***argvp);
static int cgiCallback(MprCmd *cmd, int channel, void *data);
static void cgiEvent(MaQueue *q, MprCmd *cmd, int channel);
static void enableCgiEvents(MaQueue *q, MprCmd *cmd, int channel);
static char *getCgiToken(MprBuf *buf, cchar *delim);
static bool parseFirstCgiResponse(MaConn *conn, MprCmd *cmd);
static bool parseHeader(MaConn *conn, MprCmd *cmd);
static void pushDataToCgi(MaQueue *q);
static void startCmd(MaQueue *q);

#if BLD_DEBUG
static void traceCGIData(MprCmd *cmd, char *src, int size);
#define traceData(cmd, src, size) traceCGIData(cmd, src, size)
#else
#define traceData(cmd, src, size)
#endif

#if BLD_WIN_LIKE || VXWORKS
static void findExecutable(MaConn *conn, char **program, char **script, char **bangScript, char *fileName);
#endif
#if BLD_WIN_LIKE
static void checkCompletion(MaQueue *q, MprEvent *event);
#endif

#undef lock
#undef unlock
#define lock(q) mprLock(q->conn->mutex)
#define unlock(q) mprUnlock(q->conn->mutex)

/************************************* Code ***********************************/
/*
 *  Open this handler instance for a new request
 */
static void openCgi(MaQueue *q)
{
    MaRequest       *req;
    MaConn          *conn;

    conn = q->conn;
    req = conn->request;

    maSetHeader(conn, 0, "Last-Modified", req->host->currentDate);
    maDontCacheResponse(conn);
    
    /*
     *  Create an empty header packet and add to the service queue
     */
    maPutForService(q, maCreateHeaderPacket(q), 0);

    /*
     *  Start the command. This commences the CGI gateway program. Body data from the client may flow to the command
     *  and response data may be received back.
     */
    startCmd(q);

#if BLD_FEATURE_MULTITHREAD
    /*
     *  If there is body content, it may arrive on another thread.  Some platforms can't wait accross thread groups (uclibc),
     *  so we must ensure that further callback events come on the same thread that creates the CGI process.
     */
    if (req->remainingContent > 0) {
        maEnableConnEvents(conn, 0);
        mprDedicateWorkerToHandler(conn->sock->handler, mprGetCurrentWorker(conn));
        mprAssert(conn->sock->handler->requiredWorker == mprGetCurrentWorker(conn));
    }
#endif
}


static void closeCgi(MaQueue *q)
{
    /*
     *  Disconnect the command handlers so it won't receive callbacks anymore
     */
    mprDisconnectCmd((MprCmd*) q->queueData);
}


/*
 *  Prepare and start the CGI command. Called from openCgi(). Called with the queue locked.
 */
static void startCmd(MaQueue *q)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaConn          *conn;
    MprCmd          *cmd;
    MprHash         *hp;
    cchar           *baseName;
    char            **argv, **envv, *fileName;
    int             index, argc, varCount;

    argv = 0;
    argc = 0;

    conn = q->conn;
    req = conn->request;
    resp = conn->response;

    cmd = q->queueData = mprCreateCmd(req);

    if (conn->http->forkCallback) {
        cmd->forkCallback = conn->http->forkCallback;
        cmd->forkData = conn->http->forkData;
    }

    /*
     *  Build the commmand line arguments
     */
    argc = 1;                                   /* argv[0] == programName */
    buildArgs(conn, cmd, &argc, &argv);
    fileName = argv[0];

    baseName = mprGetPathBase(q, fileName);
    if (strncmp(baseName, "nph-", 4) == 0 || 
            (strlen(baseName) > 4 && strcmp(&baseName[strlen(baseName) - 4], "-nph") == 0)) {
        /*
         *  Pretend we've seen the header for Non-parsed Header CGI programs
         */
        cmd->userFlags |= MA_CGI_SEEN_HEADER;
    }

    /*
     *  Build environment variables
     */
    varCount = mprGetHashCount(req->headers) + mprGetHashCount(req->formVars);
    envv = (char**) mprAlloc(cmd, (varCount + 1) * sizeof(char*));

    index = 0;
    hp = mprGetFirstHash(req->headers);
    while (hp) {
        if (hp->data) {
            envv[index] = mprStrcat(cmd, -1, hp->key, "=", (char*) hp->data, NULL);
            index++;
        }
        hp = mprGetNextHash(req->headers, hp);
    }
    hp = mprGetFirstHash(req->formVars);
    while (hp) {
        if (hp->data) {
            envv[index] = mprStrcat(cmd, -1, hp->key, "=", (char*) hp->data, NULL);
            index++;
        }
        hp = mprGetNextHash(req->formVars, hp);
    }
    envv[index] = 0;
    mprAssert(index <= varCount);

    cmd->stdoutBuf = mprCreateBuf(cmd, MA_BUFSIZE, -1);
    cmd->stderrBuf = mprCreateBuf(cmd, MA_BUFSIZE, -1);

    mprSetCmdDir(cmd, mprGetPathDir(q, fileName));
    mprSetCmdCallback(cmd, cgiCallback, conn);

    if (mprStartCmd(cmd, argc, argv, envv, MPR_CMD_IN | MPR_CMD_OUT | MPR_CMD_ERR) < 0) {
        maFailRequest(conn, MPR_HTTP_CODE_SERVICE_UNAVAILABLE, "Can't run CGI process: %s, URI %s", fileName, req->url);
    }
}


/*
 *  Wait for the CGI process to complete and collect status. This is run by the original request thread.
 *  Must run on the same thread as that which initiated the CGI process.
 */
static void waitForCgiCompletion(MaQueue *q)
{
    MaResponse  *resp;
    MaConn      *conn;
    MprCmd      *cmd;
    int         rc;

    conn = q->conn;
    resp = conn->response;
    cmd = (MprCmd*) q->queueData;

    /*
     *  Because some platforms can't wait accross thread groups (uclibc) on the spawned child, the parent thread 
     *  must block here and wait for the child and collect exit status.
     */
    do {
        unlock(q);
        rc = mprWaitForCmd(cmd, 5 * 1000);
        lock(q);
    } while (rc > 0 && mprGetElapsedTime(cmd, cmd->lastActivity) <= conn->host->timeout);

    if (cmd->pid) {
        mprStopCmd(cmd);
        cmd->status = 250;
    }

    if (cmd->status != 0) {
        maFailRequest(conn, MPR_HTTP_CODE_SERVICE_UNAVAILABLE,
            "CGI process %s: exited abnormally with exit status: %d.\nSee the server log for more information.\n", 
            resp->filename, cmd->status);
    }
    maPutForService(q, maCreateEndPacket(q), 1);
    maServiceQueues(conn);
}


/*
 *  This routine runs after all incoming data has been received
 *  Must run on the same thread as that which initiated the CGI process. And must be called with the connection locked.
 */
static void runCgi(MaQueue *q)
{
    MaResponse  *resp;
    MaConn      *conn;
    MprCmd      *cmd;

    conn = q->conn;
    resp = conn->response;
    cmd = (MprCmd*) q->queueData;

    /*
     *  Close the CGI program's stdin. This will allow it to exit if it was expecting input data.
     */
    if (q->queueData) {
        mprCloseCmdFd(cmd, MPR_CMD_STDIN);
    }
    if (conn->requestFailed) {
        maPutForService(q, maCreateEndPacket(q), 1);
        return;
    }
    waitForCgiCompletion(q);
    mprAssert(cmd->pid == 0);

#if BLD_FEATURE_MULTITHREAD
    if (conn->sock->handler && conn->sock->handler->requiredWorker) {
        mprAssert(conn->sock->handler->requiredWorker == mprGetCurrentWorker(conn));
        mprReleaseWorkerFromHandler(conn->sock->handler, mprGetCurrentWorker(conn));
    }
#endif
}


/*
 *  Service outgoing data destined for the browser. This is response data from the CGI program. See writeToClient.
 *  WARNING: this may be called by maRunPipeline => maServiceQueues and also from the cgiCallback => maServiceQueues.
 */ 
static void outgoingCgiService(MaQueue *q)
{
    MprCmd      *cmd;

    cmd = (MprCmd*) q->queueData;

    /*
     *  This will copy outgoing packets downstream toward the network connector and on to the browser. This may disable this 
     *  queue if the downstream net connector queue overflows because the socket is full. In that case, conn.c:setupConnIO() 
     *  will setup to listen for writable events. When the socket is writable again, the connector will drain its queue
     *  which will re-enable this queue and schedule it for service again.
     */ 
    lock(q);
    maDefaultOutgoingServiceStage(q);
    if (cmd->userFlags & MA_CGI_FLOW_CONTROL && q->count < q->low) {
        cmd->userFlags &= ~MA_CGI_FLOW_CONTROL;
        mprEnableCmdEvents(cmd, MPR_CMD_STDOUT);
    }
    unlock(q);
}


/*
 *  Accept incoming body data from the client (via the pipeline) destined for the CGI gateway. This is typically
 *  POST or PUT data.
 */
static void incomingCgiData(MaQueue *q, MaPacket *packet)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaRequest   *req;
    MprCmd      *cmd;

    mprAssert(q);
    mprAssert(packet);
    
    lock(q);
    conn = q->conn;
    resp = conn->response;
    req = conn->request;

    cmd = (MprCmd*) q->pair->queueData;
    mprAssert(cmd);
    cmd->lastActivity = mprGetTime(cmd);

    if (maGetPacketLength(packet) == 0) {
        /*
         *  End of input
         */
        if (req->remainingContent > 0) {
            /*
             *  Short incoming body data. Just kill the CGI process.
             */
            mprFree(cmd);
            q->queueData = 0;
            maFailRequest(conn, MPR_HTTP_CODE_BAD_REQUEST, "Client supplied insufficient body data");
        }
        maAddVarsFromQueue(q);

    } else {
        /*
         *  No service routine, we just need it to be queued for pushDataToCgi
         */
        maPutForService(q, packet, 0);
    }
    pushDataToCgi(q);
    unlock(q);
}


/*
 *  Write data to the CGI program. (may block). This is called from incomingCgiData and from the cgiCallback when the pipe
 *  to the CGI program becomes writable. Must be locked when called.
 */
static void pushDataToCgi(MaQueue *q)
{
    MaConn      *conn;
    MaPacket    *packet;
    MprCmd      *cmd;
    MprBuf      *buf;
    int         len, rc;

    cmd = (MprCmd*) q->pair->queueData;
    mprAssert(cmd);
    conn = q->conn;

    for (packet = maGet(q); packet && !conn->requestFailed; packet = maGet(q)) {
        buf = packet->content;
        len = mprGetBufLength(buf);
        mprAssert(len > 0);
        rc = mprWriteCmdPipe(cmd, MPR_CMD_STDIN, mprGetBufStart(buf), len);
        if (rc < 0) {
            mprLog(q, 2, "CGI: write to gateway failed for %d bytes, rc %d, errno %d\n", len, rc, mprGetOsError());
            mprCloseCmdFd(cmd, MPR_CMD_STDIN);
            maFailRequest(conn, MPR_HTTP_CODE_BAD_GATEWAY, "Can't write body data to CGI gateway");
            break;

        } else {
            mprLog(q, 5, "CGI: write to gateway %d bytes asked to write %d\n", rc, len);
            mprAdjustBufStart(buf, rc);
            if (mprGetBufLength(buf) > 0) {
                maPutBack(q, packet);
            } else {
                maFreePacket(q, packet);
            }
            if (rc < len) {
                /*
                 *  CGI gateway didn't accept all the data. Enable CGI write events to be notified when the gateway
                 *  can read more data.
                 */
                mprEnableCmdEvents(cmd, MPR_CMD_STDIN);
            }
        }
    }
}


/*
 *  Write data back to the client (browser). Must be locked when called.
 */
static int writeToClient(MaQueue *q, MprCmd *cmd, MprBuf *buf, int channel)
{
    MaConn  *conn;
    int     servicedQueues, rc, len;

    conn = q->conn;

    /*
     *  Write to the browser. We write as much as we can. Service queues to get the filters and connectors pumping.
     */
    for (servicedQueues = 0; (len = mprGetBufLength(buf)) > 0 ; ) {
        if (!conn->requestFailed) {
            rc = maWriteBlock(q, mprGetBufStart(buf), len, 0);
            mprLog(cmd, 5, "Write to browser ask %d, actual %d", len, rc);
        } else {
            rc = len;
        }
        if (rc > 0) {
            mprAdjustBufStart(buf, rc);
            mprResetBufIfEmpty(buf);

        } else if (rc == 0) {
            if (servicedQueues) {
                /*
                 *  Can't write anymore data. Block the CGI gateway. outgoingCgiService will enable.
                 */
                mprAssert(q->count >= q->max);
                mprAssert(q->flags & MA_QUEUE_DISABLED);
                cmd->userFlags |= MA_CGI_FLOW_CONTROL;
                mprDisableCmdEvents(cmd, channel);
                return MPR_ERR_CANT_WRITE;
                
            } else {
                maServiceQueues(conn);
                servicedQueues++;
            }
        }
    }
    return 0;
}


/*
 *  Read the output data from the CGI script and return it to the client. This is called for stdout/stderr data from
 *  the CGI script and for EOF from the CGI's stdin.
 */
static int cgiCallback(MprCmd *cmd, int channel, void *data)
{
    MaQueue     *q;
    MaConn      *conn;

    conn = (MaConn*) data;

    mprAssert(cmd);
    mprAssert(conn);
    mprAssert(conn->response);

    /*
     *  Locking note: the connection front-side can't delete this request/connection while in this callback.
     *  The MPR ensures that handlers won't be deleted while in-use (handler->inUse). Lock the connection here
     *  to be thread-safe with other connection front-side events.
     */
    q = conn->response->queue[MA_QUEUE_SEND].nextQ;
    mprAssert(q);
    lock(q);
    cgiEvent(q, cmd, channel);
    enableCgiEvents(q, cmd, channel);
    unlock(q);
    return 0;
}


static void cgiEvent(MaQueue *q, MprCmd *cmd, int channel)
{
    MaConn      *conn;
    MaResponse  *resp;
    MprBuf      *buf;
    int         space, nbytes, err;

    mprLog(cmd, 6, "CGI callback channel %d", channel);
    
    buf = 0;
    conn = q->conn;
    resp = conn->response;
    mprAssert(resp);

    cmd->lastActivity = mprGetTime(cmd);

    switch (channel) {
    case MPR_CMD_STDIN:
        /*
         *  CGI's stdin is now accepting more data
         */
        mprDisableCmdEvents(cmd, MPR_CMD_STDIN);
        pushDataToCgi(q->pair);
        return;

    case MPR_CMD_STDOUT:
        buf = cmd->stdoutBuf;
        break;

    case MPR_CMD_STDERR:
        buf = cmd->stderrBuf;
        break;
    }
    mprAssert(buf);
    mprResetBufIfEmpty(buf);

    /*
     *  Come here for CGI stdout, stderr events. ie. reading data from the CGI program.
     */
    while (mprGetCmdFd(cmd, channel) >= 0) {
        /*
         *  Read as much data from the CGI as possible
         */
        do {
            if ((space = mprGetBufSpace(buf)) == 0) {
                mprGrowBuf(buf, MA_BUFSIZE);
                if ((space = mprGetBufSpace(buf)) == 0) {
                    break;
                }
            }
            nbytes = mprReadCmdPipe(cmd, channel, mprGetBufEnd(buf), space);
            if (nbytes < 0) {
                err = mprGetError();
                if (err == EINTR) {
                    continue;
                } else if (err == EAGAIN || err == EWOULDBLOCK) {
                    break;
                }
                mprLog(cmd, 5, "CGI read error %d for %", mprGetError(), (channel == MPR_CMD_STDOUT) ? "stdout" : "stderr");
                mprCloseCmdFd(cmd, channel);
                
            } else if (nbytes == 0) {
                /*
                 *  This may reap the terminated child and thus clear cmd->process if both stderr and stdout are closed.
                 */
                mprLog(cmd, 5, "CGI EOF for %s", (channel == MPR_CMD_STDOUT) ? "stdout" : "stderr");
                mprCloseCmdFd(cmd, channel);
                break;

            } else {
                mprLog(cmd, 5, "CGI read %d bytes from %s", nbytes, (channel == MPR_CMD_STDOUT) ? "stdout" : "stderr");
                mprAdjustBufEnd(buf, nbytes);
                traceData(cmd, mprGetBufStart(buf), nbytes);
            }
        } while ((space = mprGetBufSpace(buf)) > 0);

        if (mprGetBufLength(buf) == 0) {
            return;
        }
        if (channel == MPR_CMD_STDERR) {
            /*
             *  If we have an error message, send that to the client
             */
            if (mprGetBufLength(buf) > 0) {
                mprAddNullToBuf(buf);
                mprLog(conn, 4, mprGetBufStart(buf));
                if (writeToClient(q, cmd, buf, channel) < 0) {
                    maDisconnectConn(conn);
                    return;
                }
                maSetResponseCode(conn, MPR_HTTP_CODE_SERVICE_UNAVAILABLE);
                cmd->userFlags |= MA_CGI_SEEN_HEADER;
                cmd->status = 0;
            }
        } else {
            if (!(cmd->userFlags & MA_CGI_SEEN_HEADER) && !parseHeader(conn, cmd)) {
                return;
            } 
            if (cmd->userFlags & MA_CGI_SEEN_HEADER) {
                if (writeToClient(q, cmd, buf, channel) < 0) {
                    maDisconnectConn(conn);
                    return;
                }
            }
        }
    }
}


static void enableCgiEvents(MaQueue *q, MprCmd *cmd, int channel)
{
    if (cmd->pid == 0) {
        return;
    }
    if (channel == MPR_CMD_STDOUT && mprGetCmdFd(cmd, channel) < 0) {
        /*
         *  Now that stdout is complete, enable stderr to receive an EOF or any error output. This is 
         *  serialized to eliminate both stdin and stdout events on different threads at the same time.
         */
        mprLog(cmd, 8, "CGI enable stderr");
        mprEnableCmdEvents(cmd, MPR_CMD_STDERR);
        
    } else if (cmd->pid) {
        mprEnableCmdEvents(cmd, channel);
    }
}


/*
 *  Parse the CGI output first line
 */
static bool parseFirstCgiResponse(MaConn *conn, MprCmd *cmd)
{
    MaResponse      *resp;
    MprBuf          *buf;
    char            *protocol, *code, *message;
    
    resp = conn->response;
    buf = mprGetCmdBuf(cmd, MPR_CMD_STDOUT);
    
    protocol = getCgiToken(buf, " ");
    if (protocol == 0 || protocol[0] == '\0') {
        maFailRequest(conn, MPR_HTTP_CODE_BAD_GATEWAY, "Bad CGI HTTP protocol response");
        return 0;
    }
    if (strncmp(protocol, "HTTP/1.", 7) != 0) {
        maFailRequest(conn, MPR_HTTP_CODE_BAD_GATEWAY, "Unsupported CGI protocol");
        return 0;
    }
    code = getCgiToken(buf, " ");
    if (code == 0 || *code == '\0') {
        maFailRequest(conn, MPR_HTTP_CODE_BAD_GATEWAY, "Bad CGI header response");
        return 0;
    }
    message = getCgiToken(buf, "\n");
    mprLog(conn, 4, "CGI status line: %s %s %s", protocol, code, message);
    return 1;
}


/*
 *  Parse the CGI output headers. 
 *  Sample CGI program:
 *
 *  Content-type: text/html
 * 
 *  <html.....
 */
static bool parseHeader(MaConn *conn, MprCmd *cmd)
{
    MaResponse      *resp;
    MaQueue         *q;
    MprBuf          *buf;
    char            *endHeaders, *headers, *key, *value, *location;
    int             fd, len;

    resp = conn->response;
    location = 0;
    value = 0;

    buf = mprGetCmdBuf(cmd, MPR_CMD_STDOUT);
    mprAddNullToBuf(buf);
    headers = mprGetBufStart(buf);

    /*
     *  Split the headers from the body.
     */
    len = 0;
    fd = mprGetCmdFd(cmd, MPR_CMD_STDOUT);
    if ((endHeaders = strstr(headers, "\r\n\r\n")) == NULL) {
        if ((endHeaders = strstr(headers, "\n\n")) == NULL) {
            if (fd >= 0 && strlen(headers) < MA_MAX_HEADERS) {
                /* Not EOF and less than max headers and have not yet seen an end of headers delimiter */
                return 0;
            }
        } else len = 2;
    } else {
        len = 4;
    }

    endHeaders[len - 1] = '\0';
    endHeaders += len;

    /*
     *  Want to be tolerant of CGI programs that omit the status line.
     */
    if (strncmp((char*) buf->start, "HTTP/1.", 7) == 0) {
        if (!parseFirstCgiResponse(conn, cmd)) {
            /* maFailConnection already called */
            return 0;
        }
    }
    
    if (strchr(mprGetBufStart(buf), ':')) {
        mprLog(conn, 4, "CGI: parseHeader: header\n%s\n", headers);

        while (mprGetBufLength(buf) > 0 && buf->start[0] && (buf->start[0] != '\r' && buf->start[0] != '\n')) {

            if ((key = getCgiToken(buf, ":")) == 0) {
                maFailConnection(conn, MPR_HTTP_CODE_BAD_REQUEST, "Bad header format");
                return 0;
            }
            value = getCgiToken(buf, "\n");
            while (isspace((int) *value)) {
                value++;
            }
            len = (int) strlen(value);
            while (len > 0 && (value[len - 1] == '\r' || value[len - 1] == '\n')) {
                value[len - 1] = '\0';
                len--;
            }
            mprStrLower(key);

            if (strcmp(key, "location") == 0) {
                location = value;

            } else if (strcmp(key, "status") == 0) {
                maSetResponseCode(conn, atoi(value));

            } else if (strcmp(key, "content-type") == 0) {
                maSetResponseMimeType(conn, value);

            } else {
                /*
                 *  Now pass all other headers back to the client
                 */
                maSetHeader(conn, 0, key, "%s", value);
            }
        }
        buf->start = endHeaders;
    }
    if (location) {
        maRedirect(conn, resp->code, location);
        q = resp->queue[MA_QUEUE_SEND].nextQ;
        maPutForService(q, maCreateEndPacket(q), 1);
    }
    cmd->userFlags |= MA_CGI_SEEN_HEADER;
    return 1;
}


/*
 *  Build the command arguments. NOTE: argv is untrusted input.
 */
static void buildArgs(MaConn *conn, MprCmd *cmd, int *argcp, char ***argvp)
{
    MaRequest   *req;
    MaResponse  *resp;
    char        *fileName, **argv, *program, *cmdScript, status[8], *indexQuery, *cp, *tok;
    cchar       *actionProgram;
    int         argc, argind, len;

    req = conn->request;
    resp = conn->response;

    fileName = resp->filename;
    mprAssert(fileName);

    program = cmdScript = 0;
    actionProgram = 0;
    argind = 0;
    argc = *argcp;

    if (resp->extension) {
        actionProgram = maGetMimeActionProgram(req->host, resp->extension);
        if (actionProgram != 0) {
            argc++;
        }
    }
    /*
     *  This is an Apache compatible hack for PHP 5.3
     */
    mprItoa(status, sizeof(status), MPR_HTTP_CODE_MOVED_TEMPORARILY, 10);
    mprAddHash(req->headers, "REDIRECT_STATUS", mprStrdup(req, status));

    /*
     *  Count the args for ISINDEX queries. Only valid if there is not a "=" in the query. 
     *  If this is so, then we must not have these args in the query env also?
     */
    indexQuery = req->parsedUri->query;
    if (indexQuery && !strchr(indexQuery, '=')) {
        argc++;
        for (cp = indexQuery; *cp; cp++) {
            if (*cp == '+') {
                argc++;
            }
        }
    } else {
        indexQuery = 0;
    }

#if BLD_WIN_LIKE || VXWORKS
{
    char    *bangScript, *cmdBuf;

    /*
     *  On windows we attempt to find an executable matching the fileName.
     *  We look for *.exe, *.bat and also do unix style processing "#!/program"
     */
    findExecutable(conn, &program, &cmdScript, &bangScript, fileName);
    mprAssert(program);

    if (cmdScript) {
        /*
         *  Cmd/Batch script (.bat | .cmd)
         *  Convert the command to the form where there are 4 elements in argv
         *  that cmd.exe can interpret.
         *
         *      argv[0] = cmd.exe
         *      argv[1] = /Q
         *      argv[2] = /C
         *      argv[3] = ""script" args ..."
         */
        argc = 4;

        len = (argc + 1) * sizeof(char*);
        argv = (char**) mprAlloc(cmd, len);
        memset(argv, 0, len);

        argv[argind++] = program;               /* Duped in findExecutable */
        argv[argind++] = mprStrdup(cmd, "/Q");
        argv[argind++] = mprStrdup(cmd, "/C");

        len = (int) strlen(cmdScript) + 2 + 1;
        cmdBuf = (char*) mprAlloc(cmd, len);
        mprSprintf(cmdBuf, len, "\"%s\"", cmdScript);
        argv[argind++] = cmdBuf;

        mprSetCmdDir(cmd, cmdScript);
        mprFree(cmdScript);
        /*  program will get freed when argv[] gets freed */
        
    } else if (bangScript) {
        /*
         *  Script used "#!/program". NOTE: this may be overridden by a mime
         *  Action directive.
         */
        argc++;     /* Adding bangScript arg */

        len = (argc + 1) * sizeof(char*);
        argv = (char**) mprAlloc(cmd, len);
        memset(argv, 0, len);

        argv[argind++] = program;       /* Will get freed when argv[] is freed */
        argv[argind++] = bangScript;    /* Will get freed when argv[] is freed */
        mprSetCmdDir(cmd, bangScript);

    } else {
        /*
         *  Either unknown extension or .exe (.out) program.
         */
        len = (argc + 1) * sizeof(char*);
        argv = (char**) mprAlloc(cmd, len);
        memset(argv, 0, len);

        if (actionProgram) {
            argv[argind++] = mprStrdup(cmd, actionProgram);
        }
        argv[argind++] = program;
    }
}
#else
    len = (argc + 1) * sizeof(char*);
    argv = (char**) mprAlloc(cmd, len);
    memset(argv, 0, len);

    if (actionProgram) {
        argv[argind++] = mprStrdup(cmd, actionProgram);
    }
    argv[argind++] = mprStrdup(cmd, fileName);

#endif

    /*
     *  ISINDEX queries. Only valid if there is not a "=" in the query. If this is so, then we must not
     *  have these args in the query env also?
     */
    if (indexQuery) {
        indexQuery = mprStrdup(cmd, indexQuery);

        cp = mprStrTok(indexQuery, "+", &tok);
        while (cp) {
            argv[argind++] = mprEscapeCmd(resp, mprUrlDecode(resp, cp), 0);
            cp = mprStrTok(NULL, "+", &tok);
        }
    }
    
    mprAssert(argind <= argc);
    argv[argind] = 0;
    *argcp = argc;
    *argvp = argv;
}


#if BLD_WIN_LIKE || VXWORKS
/*
 *  If the program has a UNIX style "#!/program" string at the start of the file that program will be selected 
 *  and the original program will be passed as the first arg to that program with argv[] appended after that. If 
 *  the program is not found, this routine supports a safe intelligent search for the command. If all else fails, 
 *  we just return in program the fileName we were passed in. script will be set if we are modifying the program 
 *  to run and we have extracted the name of the file to run as a script.
 */
static void findExecutable(MaConn *conn, char **program, char **script, char **bangScript, char *fileName)
{
    MaRequest       *req;
    MaResponse      *resp;
    MaLocation      *location;
    MprHash         *hp;
    MprFile         *file;
    cchar           *actionProgram, *ext, *cmdShell;
    char            *tok, buf[MPR_MAX_FNAME + 1], *path;

    req = conn->request;
    resp = conn->response;
    location = req->location;

    *bangScript = 0;
    *script = 0;
    *program = 0;
    path = 0;

    actionProgram = maGetMimeActionProgram(conn->host, req->mimeType);
    ext = resp->extension;

    /*
     *  If not found, go looking for the fileName with the extensions defined in appweb.conf. 
     *  NOTE: we don't use PATH deliberately!!!
     */
    if (access(fileName, X_OK) < 0 && *ext == '\0') {
        for (hp = 0; (hp = mprGetNextHash(location->extensions, hp)) != 0; ) {
            path = mprStrcat(resp, -1, fileName, ".", hp->key, NULL);
            if (access(path, X_OK) == 0) {
                break;
            }
            mprFree(path);
            path = 0;
        }
        if (hp) {
            ext = hp->key;
        } else {
            path = fileName;
        }

    } else {
        path = fileName;
    }
    mprAssert(path && *path);

#if BLD_WIN_LIKE
    if (ext && (strcmp(ext, ".bat") == 0 || strcmp(ext, ".cmd") == 0)) {
        /*
         *  Let a mime action override COMSPEC
         */
        if (actionProgram) {
            cmdShell = actionProgram;
        } else {
            cmdShell = getenv("COMSPEC");
        }
        if (cmdShell == 0) {
            cmdShell = "cmd.exe";
        }
        *script = mprStrdup(resp, path);
        *program = mprStrdup(resp, cmdShell);
        return;
    }
#endif

    if ((file = mprOpen(resp, path, O_RDONLY, 0)) != 0) {
        if (mprRead(file, buf, MPR_MAX_FNAME) > 0) {
            mprFree(file);
            buf[MPR_MAX_FNAME] = '\0';
            if (buf[0] == '#' && buf[1] == '!') {
                cmdShell = mprStrTok(&buf[2], " \t\r\n", &tok);
                if (cmdShell[0] != '/' && (cmdShell[0] != '\0' && cmdShell[1] != ':')) {
                    /*
                     *  If we can't access the command shell and the command is not an absolute path, 
                     *  look in the same directory as the script.
                     */
                    if (mprPathExists(resp, cmdShell, X_OK)) {
                        cmdShell = mprJoinPath(resp, mprGetPathDir(resp, path), cmdShell);
                    }
                }
                if (actionProgram) {
                    *program = mprStrdup(resp, actionProgram);
                } else {
                    *program = mprStrdup(resp, cmdShell);
                }
                *bangScript = mprStrdup(resp, path);
                return;
            }
        } else {
            mprFree(file);
        }
    }

    if (actionProgram) {
        *program = mprStrdup(resp, actionProgram);
        *bangScript = mprStrdup(resp, path);
    } else {
        *program = mprStrdup(resp, path);
    }
    return;
}
#endif
 

/*
 *  Get the next input token. The content buffer is advanced to the next token. This routine always returns a 
 *  non-zero token. The empty string means the delimiter was not found.
 */
static char *getCgiToken(MprBuf *buf, cchar *delim)
{
    char    *token, *nextToken;
    int     len;

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
    }
    return token;
}


#if BLD_DEBUG
/*
 *  Trace output received from the cgi process
 */
static void traceCGIData(MprCmd *cmd, char *src, int size)
{
    char    dest[512];
    int     index, i;

    mprRawLog(cmd, 5, "@@@ CGI process wrote => \n");

    for (index = 0; index < size; ) { 
        for (i = 0; i < (sizeof(dest) - 1) && index < size; i++) {
            dest[i] = src[index];
            index++;
        }
        dest[i] = '\0';
        mprRawLog(cmd, 5, "%s", dest);
    }
    mprRawLog(cmd, 5, "\n");
}
#endif


#if BLD_FEATURE_CONFIG_PARSE
static int parseCgi(MaHttp *http, cchar *key, char *value, MaConfigState *state)
{
    MaLocation  *location;
    MaServer    *server;
    MaHost      *host;
    MaAlias     *alias;
    MaDir       *dir, *parent;
    char        *program, *mimeType, *prefix, *path;

    host = state->host;
    server = state->server;
    location = state->location;

    if (mprStrcmpAnyCase(key, "Action") == 0) {
        if (maSplitConfigValue(http, &mimeType, &program, value, 1) < 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        maSetMimeActionProgram(host, mimeType, program);
        return 1;

    } else if (mprStrcmpAnyCase(key, "ScriptAlias") == 0) {
        if (maSplitConfigValue(server, &prefix, &path, value, 1) < 0 || path == 0 || prefix == 0) {
            return MPR_ERR_BAD_SYNTAX;
        }

        /*
         *  Create an alias and location with a cgiHandler and pathInfo processing
         */
        path = maMakePath(host, path);

        dir = maLookupDir(host, path);
        if (maLookupDir(host, path) == 0) {
            parent = mprGetFirstItem(host->dirs);
            dir = maCreateDir(host, path, parent);
        }
        alias = maCreateAlias(host, prefix, path, 0);
        mprLog(server, 4, "ScriptAlias \"%s\" for \"%s\"", prefix, path);
        mprFree(path);

        maInsertAlias(host, alias);

        if ((location = maLookupLocation(host, prefix)) == 0) {
            location = maCreateLocation(host, state->location);
            maSetLocationAuth(location, state->dir->auth);
            maSetLocationPrefix(location, prefix);
            maAddLocation(host, location);
        } else {
            maSetLocationPrefix(location, prefix);
        }
        maSetHandler(http, host, location, "cgiHandler");
        return 1;
    }
    return 0;
}
#endif


/*
    Dynamic module initialization
 */
MprModule *maCgiHandlerInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *handler;

    if ((module = mprCreateModule(http, "cgiHandler", BLD_VERSION, NULL, NULL, NULL)) == NULL) {
        return 0;
    }
    handler = maCreateHandler(http, "cgiHandler", 
        MA_STAGE_ALL | MA_STAGE_VARS | MA_STAGE_ENV_VARS | MA_STAGE_PATH_INFO | MA_STAGE_MISSING_EXT);
    if (handler == 0) {
        mprFree(module);
        return 0;
    }
    http->cgiHandler = handler;
    handler->open = openCgi; 
    handler->close = closeCgi; 
    handler->outgoingService = outgoingCgiService;
    handler->incomingData = incomingCgiData; 
    handler->run = runCgi; 
    handler->parse = parseCgi; 
    return module;
}


#else

MprModule *maCgiHandlerInit(MaHttp *http, cchar *path)
{
    return 0;
}

#endif /* BLD_FEATURE_CGI */

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
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
    
    @end
 */
