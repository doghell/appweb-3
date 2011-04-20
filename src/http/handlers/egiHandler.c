/*
 *  egiHandler.c -- Embedded Gateway Interface (EGI) handler. Fast in-process replacement for CGI.
 *
 *  The EGI handler implements a very fast in-process CGI scheme.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

#if BLD_FEATURE_EGI
/*********************************** Defines **********************************/
#if BLD_DEBUG
/*
 *  Non-production in-line test code
 */
#define EGI_TEST 1

static int  egiTestInit(MaHttp *http, cchar *path);
static int  getVars(MaQueue *q, char ***keys, char *buf, int len);
static void printFormVars(MaQueue *q);
static void printRequestHeaders(MaQueue *q);
static void printQueryData(MaQueue *q);
static void printBodyData(MaQueue *q);
#endif

/************************************* Code ***********************************/
/*
 *  This runs when all input data has been received. The egi form must write all the data.
 *  It currently does not support forms that return before writing all the data.
 */
static void runEgi(MaQueue *q)
{
    MaConn          *conn;
    MaRequest       *req;
    MaEgiForm       *form;
    MaEgi           *egi;

    conn = q->conn;
    req = conn->request;
    egi = (MaEgi*) q->stage->stageData;
    
    maSetHeader(conn, 0, "Last-Modified", req->host->currentDate);
    maDontCacheResponse(conn);
    maPutForService(q, maCreateHeaderPacket(q), 0);

    form = (MaEgiForm*) mprLookupHash(egi->forms, req->url);
    if (form == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_NOT_FOUND, "Egi Form: \"%s\" is not defined", req->url);
    } else {
        (*form)(q);
    }
    maPutForService(q, maCreateEndPacket(q), 1);
}


/*
 *  User API to define a form
 */
int maDefineEgiForm(MaHttp *http, cchar *name, MaEgiForm *form)
{
    MaEgi       *egi;
    MaStage     *handler;

    handler = http->egiHandler;
    if (handler) {
        egi = (MaEgi*) handler->stageData;
        mprAddHash(egi->forms, name, form);
    }
    return 0;
}


/*
 *  Dynamic module initialization
 */
MprModule *maEgiHandlerInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *handler;
    MaEgi       *egi;

    module = mprCreateModule(http, "egiHandler", BLD_VERSION, NULL, NULL, NULL);
    if (module == 0) {
        return 0;
    }
    handler = maCreateHandler(http, "egiHandler", 
        MA_STAGE_GET | MA_STAGE_HEAD | MA_STAGE_POST | MA_STAGE_PUT | MA_STAGE_VARS | MA_STAGE_ENV_VARS | MA_STAGE_VIRTUAL);
    if (handler == 0) {
        mprFree(module);
        return 0;
    }
    http->egiHandler = handler;

    handler->run = runEgi; 
    handler->stageData = egi = mprAllocObjZeroed(handler, MaEgi);
    egi->forms = mprCreateHash(egi, MA_EGI_HASH_SIZE);
#if EGI_TEST
    egiTestInit(http, path);
#endif
    return module;
}


/************************************* Test ***********************************/
/*
 *  This code is never in release product.
 */

#if EGI_TEST

#if UNUSED
static cchar *cback(MaConn *conn, int *code, cchar *targetUri)
{
    *code = 302;
    // return mprAsprintf(conn, -1, "https://www.embedthis.com/vpn");
    return mprAsprintf(conn, -1, "/vpn");
}
#endif

static void simpleTest(MaQueue *q)
{
#if UNUSED
    maSetRedirectCallback(q->conn, cback);
    maRedirect(q->conn, 302, "/abc/anything");
#endif
    maWrite(q, "Hello %s\r\n", maGetFormVar(q->conn, "name", "unknown"));
}


static void bigTest(MaQueue *q)
{
    int     i;

    for (i = 0; i < 200; i++) {
        maWrite(q, "line %04d 012345678901234567890123456789012345678901234567890123456789\r\n", i);
    }
}


static void exitApp(MaQueue *q)
{
    mprLog(q, 0, "Instructed to exit ...");
    mprTerminate(q, 1);
}


static void printVars(MaQueue *q)
{
    MaConn      *conn;
    MaResponse  *resp;
    MaRequest   *req;
    char        *sw;
    char        *newLocation;
    int         responseStatus;

    conn = q->conn;
    resp = conn->response;
    req = conn->request;
    newLocation = 0;
    responseStatus = 0;
    sw = 0;

    /*
     *  Parse the switches
     */
    if (req->parsedUri->query) {
        sw = (char*) strstr(req->parsedUri->query, "SWITCHES=");
        if (sw) {
            sw = mprUrlDecode(resp, &sw[9]);
            if (*sw == '-') {
                if (sw[1] == 'l') {
                    newLocation = sw + 3;
                } else if (sw[1] == 's') {
                    responseStatus = atoi(sw + 3);
                }
            }
        }
    }

    maSetResponseCode(conn, 200);
    maSetResponseMimeType(conn, "text/html");
    maDontCacheResponse(conn);

    /*
     *  Test writing headers. The Server header overwrote the "Server" header
     *
     *  maSetHeader(conn, "MyCustomHeader", "true");
     *  maSetHeader(conn, "Server", "private");
     */

    if (maGetCookies(conn) == 0) {
        maSetCookie(conn, "appwebTest", "Testing can be fun", "/", NULL, 43200, 0);
    }

    if (newLocation) {
        maRedirect(conn, 302, newLocation);

    } else if (responseStatus) {
        maFailRequest(conn, responseStatus, "Custom Status");

    } else {
        maWrite(q, "<HTML><TITLE>egiProgram: EGI Output</TITLE><BODY>\r\n");

        printRequestHeaders(q);
        printFormVars(q);
        printQueryData(q);
        printBodyData(q);

        maWrite(q, "</BODY></HTML>\r\n");
    }
    if (sw) {
        mprFree(sw);
    }
}


static void printRequestHeaders(MaQueue *q)
{
    MprHashTable    *env;
    MprHash         *hp;

    maWrite(q, "<H2>Request Headers</H2>\r\n");

    env = q->conn->request->headers;

    for (hp = 0; (hp = mprGetNextHash(env, hp)) != 0; ) {
        maWrite(q, "<P>%s=%s</P>\r\n", hp->key, hp->data ? hp->data: "");
    }
    maWrite(q, "\r\n");
}


static void printFormVars(MaQueue *q)
{
    MprHashTable    *env;
    MprHash         *hp;

    maWrite(q, "<H2>Request Form Variables</H2>\r\n");

    env = q->conn->request->formVars;

    for (hp = 0; (hp = mprGetNextHash(env, hp)) != 0; ) {
        maWrite(q, "<P>%s=%s</P>\r\n", hp->key, hp->data ? hp->data: "");
    }
    maWrite(q, "\r\n");
}


static void printQueryData(MaQueue *q)
{
    MaRequest   *req;
    char        buf[MPR_MAX_STRING], **keys, *value;
    int         i, numKeys;

    req = q->conn->request;
    if (req->parsedUri->query == 0) {
        return;
    }
    mprStrcpy(buf, sizeof(buf), req->parsedUri->query);
    numKeys = getVars(q, &keys, buf, (int) strlen(buf));

    if (numKeys == 0) {
        maWrite(q, "<H2>No Query Data Found</H2>\r\n");
    } else {
        maWrite(q, "<H2>Decoded Query Data Variables</H2>\r\n");
        for (i = 0; i < (numKeys * 2); i += 2) {
            value = keys[i+1];
            maWrite(q, "<p>QVAR %s=%s</p>\r\n", keys[i], value ? value: "");
        }
    }
    maWrite(q, "\r\n");
    mprFree(keys);
}


static void printBodyData(MaQueue *q)
{
    MprBuf  *buf;
    char    **keys, *value;
    int     i, numKeys;
    
    if (q->pair == 0 || q->pair->first == 0) {
        return;
    }
    
    buf = q->pair->first->content;
    mprAddNullToBuf(buf);
    
    numKeys = getVars(q, &keys, mprGetBufStart(buf), mprGetBufLength(buf));

    if (numKeys == 0) {
        maWrite(q, "<H2>No Body Data Found</H2>\r\n");
    } else {
        maWrite(q, "<H2>Decoded Body Data</H2>\r\n");
        for (i = 0; i < (numKeys * 2); i += 2) {
            value = keys[i+1];
            maWrite(q, "<p>PVAR %s=%s</p>\r\n", keys[i], value ? value: "");
        }
    }
    maWrite(q, "\r\n");
    mprFree(keys);
}


static int getVars(MaQueue *q, char ***keys, char *buf, int len)
{
    char**  keyList;
    char    *eq, *cp, *pp, *tok;
    int     i, keyCount;

    *keys = 0;

    /*
     *  Change all plus signs back to spaces
     */
    keyCount = (len > 0) ? 1 : 0;
    for (cp = buf; cp < &buf[len]; cp++) {
        if (*cp == '+') {
            *cp = ' ';
        } else if (*cp == '&' && (cp > buf && cp < &buf[len - 1])) {
            keyCount++;
        }
    }

    if (keyCount == 0) {
        return 0;
    }

    /*
     *  Crack the input into name/value pairs 
     */
    keyList = (char**) mprAlloc(q, (keyCount * 2) * (int) sizeof(char**));

    i = 0;
    tok = 0;
    for (pp = mprStrTok(buf, "&", &tok); pp; pp = mprStrTok(0, "&", &tok)) {
        if ((eq = strchr(pp, '=')) != 0) {
            *eq++ = '\0';
            pp = mprUrlDecode(q, pp);
            eq = mprUrlDecode(q, eq);
        } else {
            pp = mprUrlDecode(q, pp);
            eq = 0;
        }
        if (i < (keyCount * 2)) {
            keyList[i++] = pp;
            keyList[i++] = eq;
        }
    }
    *keys = keyList;
    return keyCount;
}


static void upload(MaQueue *q)
{
    MaConn      *conn;
    char        *sw;
    char        *newLocation;
    int         responseStatus;

    conn = q->conn;
    newLocation = 0;
    responseStatus = 0;

    sw = (char*) strstr(maGetFormVar(conn, "QUERY_STRING", ""), "SWITCHES=");
    if (sw) {
        sw = mprUrlDecode(sw, &sw[9]);
        if (*sw == '-') {
            if (sw[1] == 'l') {
                newLocation = sw + 3;
            } else if (sw[1] == 's') {
                responseStatus = atoi(sw + 3);
            }
        }
    }

    maSetResponseCode(conn, 200);
    maSetResponseMimeType(conn, "text/html");
    maDontCacheResponse(conn);

    /*
     *  Test writing headers. The Server header overwrote the "Server" header
     *
     *  maSetHeader(conn, "MyCustomHeader: true");
     *  maSetHeader(conn, "Server: private");
     */

    if (maGetCookies(conn) == 0) {
        maSetCookie(conn, "appwebTest", "Testing can be fun", "/", NULL, 43200, 0);
    }

    if (newLocation) {
        maRedirect(conn, 302, newLocation);

    } else if (responseStatus) {
        maFailRequest(conn, responseStatus, "Custom Status");

    } else {
        maWrite(q, "<HTML><TITLE>egiProgram: EGI Output</TITLE><BODY>\r\n");

        printRequestHeaders(q);
        printQueryData(q);
        printBodyData(q);

        maWrite(q, "</BODY></HTML>\r\n");
    }
    if (sw) {
        mprFree(sw);
    }
}


static int egiTestInit(MaHttp *http, cchar *path)
{
    /*
     *  Five instances of the same program. Location blocks must be defined in appweb.conf to test these.
     */
    maDefineEgiForm(http, "/egi/exit", exitApp);
    maDefineEgiForm(http, "/egi/egiProgram", printVars);
    maDefineEgiForm(http, "/egi/egiProgram.egi", printVars);
    maDefineEgiForm(http, "/egi/egi Program.egi", printVars);
    maDefineEgiForm(http, "/egiProgram.egi", printVars);
    maDefineEgiForm(http, "/MyInProcScripts/egiProgram.egi", printVars);
    maDefineEgiForm(http, "/myEgi/egiProgram.egi", printVars);
    maDefineEgiForm(http, "/myEgi/egiProgram", printVars);
    maDefineEgiForm(http, "/upload/upload.egi", upload);
    maDefineEgiForm(http, "/egi/test", simpleTest);
    maDefineEgiForm(http, "/test.egi", simpleTest);
    maDefineEgiForm(http, "/big.egi", bigTest);

    return 0;
}
#endif /* EGI_TEST */


#else

MprModule *maEgiHandlerInit(MaHttp *http, cchar *path)
{
    return 0;
}

#endif /* BLD_FEATURE_EGI */


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
