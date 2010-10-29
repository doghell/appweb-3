/*
 *  http.c -- Http client program
 *
 *  The http program is a client to issue HTTP requests. It is also a test platform for loading and testing web servers. 
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */
 
/******************************** Includes ***********************************/

#include    "mpr.h"

#if BLD_FEATURE_HTTP_CLIENT
/*********************************** Locals ***********************************/

static int      activeLoadThreads;  /* Still running test threads */
static int      benchmark;          /* Output benchmarks */
static char     *chunkSize;         /* Request response data to be chunked in this quanta */
static int      continueOnErrors;   /* Continue testing even if an error occurs. Default is to stop */
static int      success;            /* Total success flag */
static int      fetchCount;         /* Total count of fetches */
static MprList  *fileData;          /* File arguments */
static MprList  *formData;          /* Form body data. Overrides body data */
static Mpr      *mpr;               /* Portable runtime */
static MprList  *headers;           /* Request headers */
static int      iterations;         /* URLs to fetch */
static int      isBinary;           /* Looks like a binary output file */
static int      httpVersion;        /* HTTP/1.x */
static char     *host;              /* Host to connect to */
static int      loadThreads;        /* Number of threads to use for URL requests */
static char     *method;            /* HTTP method when URL on cmd line */
static int      nextArg;            /* Next arg to parse */
static int      noout;              /* Don't output files */
static int      nofollow;           /* Don't automatically follow redirects */
static int      printable;          /* Make binary output printable */
static char     *password;          /* Password for authentication */
static char     *ranges;            /* Request ranges */
static int      retries;            /* Times to retry a failed request */
static int      sequence;           /* Sequence requests with a custom header */
static int      showCode;           /* Output the Http response code */
static int      showHeaders;        /* Output the response headers */
static int      singleStep;         /* Pause between requests */
static char     *target;            /* Destination url */
static int      timeout;            /* Timeout in secs for a non-responsive server */
static int      upload;             /* Upload using multipart mime */
static char     *username;          /* User name for authentication of requests */
static int      verbose;            /* Trace progress */
static int      workers;            /* Worker threads. >0 if multi-threaded */

#if BLD_FEATURE_MULTITHREAD
static MprMutex *mutex;
#endif

/***************************** Forward Declarations ***************************/

static void     addFormVars(MprCtx ctx, cchar *buf);
static void     processing();
static int      doRequest(MprHttp *http, cchar *url, MprList *fields, MprList *files);
static void     finishThread(MprThread *tp);
static char     *getPassword(MprCtx ctx);
static void     initSettings(Mpr *mpr);
static bool     isPort(cchar *name);
static bool     iterationsComplete(MprCtx ctx);
static bool     parseArgs(Mpr *mpr, int argc, char **argv);
static void     processThread(MprCtx ctx);
static void     threadMain(void *data, MprThread *tp);
static char     *resolveUrl(MprHttp *http, cchar *url);
static int      setContentLength(MprHttp *http, MprList *fields, MprList *files);
static void     showOutput(MprHttp *http, cchar *content, int contentLen);
static int      startLogging(Mpr *mpr, char *logSpec);
static void     showUsage(Mpr *mpr);
static void     trace(MprHttp *http, cchar *url, int fetchCount, cchar *method, int code, int contentLen);
static void     waitForUser(MprCtx ctx);
static int      writeBody(MprHttp *http, MprList *fields, MprList *files);

#undef lock
#undef unlock
#if BLD_FEATURE_MULTITHREAD
#define lock()   mprLock(mutex)
#define unlock() mprUnlock(mutex)
#else
#define lock()
#define unlock()
#endif

/*********************************** Code *************************************/

MAIN(httpMain, int argc, char *argv[])
{
    MprTime         start;
    double          elapsed;

    /*
     *  Explicit initialization of globals for re-entrancy on Vxworks
     */
    activeLoadThreads = benchmark = continueOnErrors = fetchCount = iterations = isBinary = httpVersion = 0;
    success = loadThreads = nextArg = noout = nofollow = showHeaders = printable = workers = 0;
    retries = singleStep = timeout = verbose = 0;

    chunkSize = host = method = password = ranges = 0;
    username = 0;
    mpr = 0;
    headers = 0;
    formData = 0;

    mpr = mprCreate(argc, argv, NULL);

    initSettings(mpr);
    if (!parseArgs(mpr, argc, argv)) {
        showUsage(mpr);
        return MPR_ERR_BAD_ARGS;
    }
#if BLD_FEATURE_MULTITHREAD
    mprSetMaxWorkers(mpr, workers);
#endif

#if BLD_FEATURE_SSL
    if (!mprLoadSsl(mpr, 1)) {
        mprError(mpr, "Can't load SSL");
        exit(1);
    }
#endif

    /*
     *  Start the Timer, Socket and Worker services
     */
    if (mprStart(mpr, 0) < 0) {
        mprError(mpr, "Can't start MPR for %s", mprGetAppTitle(mpr));
        exit(2);
    }
    start = mprGetTime(mpr);
    processing();

    /*
     *  Wait for all the threads to complete (simple but effective). Keep servicing events as we wind down.
     */
    while (activeLoadThreads > 0) {
        mprServiceEvents(mprGetDispatcher(mpr), 250, MPR_SERVICE_EVENTS | MPR_SERVICE_IO);
    }
    if (benchmark) {
        elapsed = (double) (mprGetTime(mpr) - start);
        if (fetchCount == 0) {
            elapsed = 0;
            fetchCount = 1;
        }
        mprPrintf(mpr, "\nRequest Count:       %13d\n", fetchCount);
        mprPrintf(mpr, "Time elapsed:        %13.4f sec\n", elapsed / 1000.0);
        mprPrintf(mpr, "Time per request:    %13.4f sec\n", elapsed / 1000.0 / fetchCount);
        mprPrintf(mpr, "Requests per second: %13.4f\n", fetchCount * 1.0 / (elapsed / 1000.0));
        mprPrintf(mpr, "Load threads:        %13d\n", loadThreads);
        mprPrintf(mpr, "Worker threads:      %13d\n", workers);
    }
    if (!success && verbose) {
        mprError(mpr, "Request failed");
    }
    return (success) ? 0 : 255;
}


static void initSettings(Mpr *mpr)
{
    method = 0;
    verbose = continueOnErrors = showHeaders = 0;

    /*
     *  Default to HTTP/1.1
     */
    httpVersion = 1;
    success = 1;
    host = "localhost";
    retries = MPR_HTTP_RETRIES;
    iterations = 1;
    loadThreads = 1;
    workers = 1;            
    timeout = (60 * 1000);
    headers = mprCreateList(mpr);
#if BLD_FEATURE_MULTITHREAD
    mutex = mprCreateLock(mpr);
#endif
}


static bool parseArgs(Mpr *mpr, int argc, char **argv)
{
    char    *argp, *key, *value;
    int     i, setWorkers;

    setWorkers = 0;

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }

        if (strcmp(argp, "--benchmark") == 0 || strcmp(argp, "-b") == 0) {
            benchmark++;

        } else if (strcmp(argp, "--chunk") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                chunkSize = argv[++nextArg];
                i = atoi(chunkSize);
                if (i < 0) {
                    mprError(mpr, "Bad chunksize %d", i);
                    return 0;
                }
            }

        } else if (strcmp(argp, "--continue") == 0) {
            continueOnErrors++;

        } else if (strcmp(argp, "--cookie") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                mprAddItem(headers, mprCreateKeyPair(headers, "Cookie", argv[++nextArg]));
            }

        } else if (strcmp(argp, "--debug") == 0 || strcmp(argp, "-d") == 0) {
            mprSetDebugMode(mpr, 1);
            retries = 0;

        } else if (strcmp(argp, "--delete") == 0) {
            method = "DELETE";

        } else if (strcmp(argp, "--form") == 0 || strcmp(argp, "-f") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                if (formData == 0) {
                    formData = mprCreateList(mpr);
                }
                addFormVars(mpr, argv[++nextArg]);
            }

        } else if (strcmp(argp, "--header") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                key = argv[++nextArg];
                if ((value = strchr(key, ':')) == 0) {
                    mprError(mpr, "Bad header format. Must be \"key: value\"");
                    return 0;
                }
                *value++ = '\0';
                while (isspace((int) *value)) {
                    value++;
                }
                mprAddItem(headers, mprCreateKeyPair(headers, key, value));
            }

        } else if (strcmp(argp, "--host") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                host = argv[++nextArg];
            }

        } else if (strcmp(argp, "--http") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                httpVersion = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--iterations") == 0 || strcmp(argp, "-i") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                iterations = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--log") == 0 || strcmp(argp, "-l") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                startLogging(mpr, argv[++nextArg]);
            }

        } else if (strcmp(argp, "--method") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                method = argv[++nextArg];
            }

        } else if (strcmp(argp, "--noout") == 0 || strcmp(argp, "-n") == 0  ||
                   strcmp(argp, "--quiet") == 0 || strcmp(argp, "-q") == 0) {
            noout++;

        } else if (strcmp(argp, "--nofollow") == 0) {
            nofollow++;

        } else if (strcmp(argp, "--password") == 0 || strcmp(argp, "-p") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                password = argv[++nextArg];
            }

        } else if (strcmp(argp, "--post") == 0) {
            method = "POST";

        } else if (strcmp(argp, "--printable") == 0) {
            printable++;

        } else if (strcmp(argp, "--put") == 0) {
            method = "PUT";

        } else if (strcmp(argp, "--range") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                if (ranges == 0) {
                    ranges = mprAsprintf(mpr, -1, "bytes=%s", argv[++nextArg]);
                } else {
                    ranges = mprStrcat(mpr, -1, ranges, ",", argv[++nextArg], NULL);
                }
            }
            
        } else if (strcmp(argp, "--retries") == 0 || strcmp(argp, "-r") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                retries = atoi(argv[++nextArg]);
            }
            
        } else if (strcmp(argp, "--sequence") == 0) {
            sequence++;

        } else if (strcmp(argp, "--showCode") == 0) {
            showCode++;

        } else if (strcmp(argp, "--show") == 0 || strcmp(argp, "--showHeaders") == 0) {
            showHeaders++;

        } else if (strcmp(argp, "--single") == 0 || strcmp(argp, "-s") == 0) {
            singleStep++;

        } else if (strcmp(argp, "--threads") == 0 || strcmp(argp, "-t") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                loadThreads = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--timeout") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                timeout = atoi(argv[++nextArg]) * 1000;
            }

        } else if (strcmp(argp, "--upload") == 0 || strcmp(argp, "-u") == 0) {
            upload++;

        } else if (strcmp(argp, "--user") == 0 || strcmp(argp, "--username") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                username = argv[++nextArg];
            }

        } else if (strcmp(argp, "--verbose") == 0 || strcmp(argp, "-v") == 0) {
            verbose++;

        } else if (strcmp(argp, "--workerTheads") == 0 || strcmp(argp, "-w") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                workers = atoi(argv[++nextArg]);
            }
            setWorkers++;

        } else {
            return 0;
            break;
        }
    }
    if (argc == nextArg) {
        return 0;
    }
    argc = argc - nextArg;
    argv = &argv[nextArg];
    target = argv[argc - 1];
    argc--;
    if (argc > 0) {
        /*
         *  Files present on command line
         */
        fileData = mprCreateList(mpr);
        for (i = 0; i < argc; i++) {
            mprAddItem(fileData, argv[i]);
        }
    }
    if (!setWorkers) {
        workers = loadThreads + 2;
    }
    if (method == 0) {
        if (upload || formData) {
            method = "POST";
        } else if (fileData) {
            method = "PUT";
        } else {
            method = "GET";
        }
    }
    return 1;
}


static void showUsage(Mpr *mpr)
{
    mprPrintfError(mpr,
        "usage: %s [options] [files] url\n"
        "  Options:\n"
        "  --benchmark           # Compute benchmark results.\n"
        "  --chunk size          # Request response data to use this chunk size.\n"
        "  --continue            # Continue on errors.\n"
        "  --cookie CookieString # Define a cookie header. Multiple uses okay.\n"
        "  --debug               # Run in debug mode. No timeouts.\n"
        "  --delete              # Use the DELETE method. Shortcut for --method DELETE..\n"
        "  --form string         # Form data. Must already be form-www-urlencoded.\n"
        "  --header 'key: value' # Add a custom request header.\n"
        "  --host hostName       # Host name or IP address for unqualified URLs.\n"
        "  --http 0|1            # HTTP version. 0 for HTTP 1.0, 1 for HTTP 1.1.\n"
        "  --iterations count    # Number of times to fetch the urls (default 1).\n"
        "  --log logFile:level   # Log to the file at the verbosity level.\n"
        "  --method KIND         # HTTP request method GET|OPTIONS|POST|PUT|TRACE (default GET).\n"
        "  --nofollow            # Don't automatically follow redirects.\n"
        "  --noout               # Don't output files to stdout.\n"
        "  --password pass       # Password for authentication.\n"
        "  --post                # Use POST method. Shortcut for --method POST.\n"
        "  --printable           # Make binary output printable.\n"
        "  --put                 # Use PUT method. Shortcut for --method PUT.\n"
        "  --range byteRanges    # Request a subset range of the document.\n"
        "  --retries count       # Number of times to retry failing requests.\n"
        "  --sequence            # Sequence requests with a custom header.\n"
        "  --showCode            # Output the Http response code.\n"
        "  --showHeaders         # Output response headers.\n"
        "  --single              # Single step. Pause for input between requests.\n"
        "  --timeout secs        # Request timeout period in seconds.\n"
        "  --threads count       # Number of thread instances to spawn.\n"
        "  --upload              # Use multipart mime upload.\n"
        "  --user name           # User name for authentication.\n"
        "  --verbose             # Verbose operation. Trace progress.\n"
        "  --workers count       # Set maximum worker threads.\n",
        mprGetAppName(mpr));
}


/*
 *  Process the requests
 */
static void processing()
{
    if (chunkSize) {
        mprAddItem(headers, mprCreateKeyPair(headers, "X-Appweb-Chunk-Size", chunkSize));
    }
#if BLD_FEATURE_MULTITHREAD
    {
        MprThread   *tp;
        int         j;

        activeLoadThreads = loadThreads;
        for (j = 0; j < loadThreads; j++) {
            char name[64];
            mprSprintf(name, sizeof(name), "http.%d", j);
            tp = mprCreateThread(mpr, name, threadMain, mpr, MPR_NORMAL_PRIORITY, 0); 
            mprStartThread(tp);
        }
    }
#else
    threadMain(mpr, NULL);
#endif
}


/*
 *  Per-thread execution. Called for main thread and helper threads.
 */ 
static void threadMain(void *data, MprThread *tp)
{
    MprCtx  ctx;

    ctx = (tp) ? (MprCtx) tp : (MprCtx) mpr;
    processThread(ctx);
    finishThread(tp);
}


static void processThread(MprCtx ctx)
{
    MprHttp     *http;
    MprList     *files;
    cchar       *path;
    char        *url;
    int         next;

    http = mprCreateHttp(ctx);
    mprSetHttpTimeout(http, timeout);
    mprSetHttpFollowRedirects(http, !nofollow);

    if (chunkSize) {
        mprSetHttpChunked(http, 1);
    }
    if (httpVersion == 0) {
        mprSetHttpKeepAlive(http, 0);
        mprSetHttpProtocol(http, "HTTP/1.0");
    }
    if (username) {
        if (password == 0 && !strchr(username, ':')) {
            password = getPassword(http);
        }
        mprSetHttpCredentials(http, username, password);
    }
    while (!mprIsExiting(http) && (success || continueOnErrors)) {
        if (singleStep) waitForUser(http);
        if (fileData && !upload) {
            for (next = 0; (path = mprGetNextItem(fileData, &next)) != 0; ) {
                if (target[strlen(target) - 1] == '/') {
                    url = mprJoinPath(http, target, mprGetPathBase(http, path));
                } else {
                    url = target;
                }
                files = mprCreateList(http);
                mprAddItem(files, path);
                url = resolveUrl(http, url);
                if (verbose) {
                    mprPrintf(http, "putting: %s to %s\n", path, url);
                }
                if (doRequest(http, url, formData, files) < 0) {
                    success = 0;
                    mprFree(files);
                    mprFree(url);
                    break;
                }
                mprFree(files);
                mprFree(url);
            }
        } else {
            url = resolveUrl(http, target);
            if (doRequest(http, url, formData, fileData) < 0) {
                success = 0;
                mprFree(url);
                break;
            }
        }
        if (iterationsComplete(http)) 
            break;
    }
    mprFree(http);
}


static int doRequest(MprHttp *http, cchar *url, MprList *fields, MprList *files)
{
    MprHttpResponse *resp;
    MprKeyValue     *header;
    MprFile         *file;
    MprTime         mark;
    cchar           *msg;
    char            buf[MPR_HTTP_BUFSIZE], seqBuf[16], *responseHeaders, *redirect;
    int             code, contentLen, elapsed, next, count, bytes, transCount;

    file = 0;
    mprAssert(url && *url);

    mprLog(http, MPR_DEBUG, "fetch: %s %s", method, url);
    mark = mprGetTime(mpr);

    /*
     *  Send the request
     */
    count = -1;
    transCount = 0;
    do {
        for (next = 0; (header = mprGetNextItem(headers, &next)) != 0; ) {
            mprSetHttpHeader(http, 0, header->key, header->value);
        }
        if (sequence) {
            static int next = 0;
            mprItoa(seqBuf, sizeof(seqBuf), next++, 10);
            mprSetHttpHeader(http, 1, "X-Http-Seq", seqBuf);
        }
        if (ranges) {
            mprSetHttpHeader(http, 1, "Range", ranges);
        }
        if (fields) {
            mprSetHttpHeader(http, 1, "Content-Type", "application/x-www-form-urlencoded");
        }
        if (chunkSize) {
            mprSetHttpChunked(http, 1);
        }
        if (setContentLength(http, fields, files) < 0) {
            return MPR_ERR_CANT_OPEN;
        }
        if (mprStartHttpRequest(http, method, url) < 0) {
            mprError(http, "Can't process request for \"%s\". %s", url, mprGetHttpError(http));
            return MPR_ERR_CANT_OPEN;
        }
        /*
         *  This program does not do full-duplex writes with reads. ie. if you have a request that sends and receives
         *  data in parallel -- http will do the writes first then read the response.
         */
        if (files || fields) {
            if (writeBody(http, fields, files) < 0) {
                mprError(http, "Can't write body data to \"%s\". %s", url, mprGetHttpError(http));
                return MPR_ERR_CANT_WRITE;
            }
        } else {
            if (chunkSize) {
                mprFinalizeHttpWriting(http);
            }
            mprWaitForHttpResponse(http, -1);
        }
#if WIN
        _setmode(fileno(stdout), O_BINARY);
#endif
        while ((bytes = mprReadHttp(http, buf, sizeof(buf))) > 0) {
            showOutput(http, buf, bytes);
        }
        mprWaitForHttp(http, MPR_HTTP_STATE_COMPLETE, -1);
        /* May not be complete if a disconnect occurs */
        if (http->state >= MPR_HTTP_STATE_CONTENT) {
            if (mprNeedHttpRetry(http, &redirect)) {
                if (redirect) {
                    url = resolveUrl(http, redirect);
                }
                count--;
                transCount++;
                continue;
            }
            break;
        }
    } while (++count < http->retries && transCount < 4 && !mprIsExiting(http));

    if (count >= http->retries) {
        mprError(http, "http: failed \"%s\" request for %s after %d attempt(s)", method, url, count);
        return MPR_ERR_TOO_MANY;
    }
    if (mprIsExiting(http)) {
        return MPR_ERR_BAD_STATE;
    }

    /*
     *  Request now complete
     */
    code = mprGetHttpCode(http);
    contentLen = mprGetHttpContentLength(http);
    msg = mprGetHttpCodeString(http, code);

    elapsed = (int) (mprGetTime(mpr) - mark);
    mprLog(http, 6, "Response code %d, content len %d", code, contentLen);

    if (http->response) {
        if (showCode) {
            mprPrintf(http, "%d\n", code);
        }
        if (showHeaders) {
            responseHeaders = mprGetHttpHeaders(http);
            resp = http->response;
            mprPrintfError(http, "%s %d %s\n", resp->protocol, resp->code, resp->message);
            mprPrintfError(http, "%s\n", responseHeaders);
            mprFree(responseHeaders);
        }
    }

    if (code < 0) {
        mprError(http, "Can't process request for \"%s\" %s", url, mprGetHttpError(http));
        return MPR_ERR_CANT_READ;

    } else if (code == 0 && http->protocolVersion == 0) {
        ;

    } else if (!(200 <= code && code <= 206) && !(301 <= code && code <= 304)) {
        if (!showCode) {
            mprError(http, "Can't process request for \"%s\" (%d) %s", url, code, mprGetHttpError(http));
            return MPR_ERR_CANT_READ;
        }
    }

    lock();
    if (verbose && noout) {
        trace(http, url, fetchCount, method, code, contentLen);
    }
    unlock();
    return 0;
}


static int setContentLength(MprHttp *http, MprList *fields, MprList *files)
{
    MprPath     info;
    char        *path, *pair;
    int         len;
    int         next, count;

    len = 0;
    if (upload) {
        /*  Easier to use chunking than to calculate the full multipart-mime upload content */
        mprSetHttpChunked(http, 1);
        mprEnableHttpUpload(http);
        return 0;
    } else {
        for (next = 0; (path = mprGetNextItem(files, &next)) != 0; ) {
            if (mprGetPathInfo(http, path, &info) < 0) {
                mprError(http, "Can't access file %s", path);
                return MPR_ERR_CANT_ACCESS;
            }
            len += (int) info.size;
        }
        if (fields) {
            count = mprGetListCount(fields);
            for (next = 0; (pair = mprGetNextItem(fields, &next)) != 0; ) {
                len += strlen(pair);
            }
            len += mprGetListCount(fields) - 1;
        }
    }
    mprSetHttpBody(http, NULL, len);
    return 0;
}


static int writeBody(MprHttp *http, MprList *fields, MprList *files)
{
    MprFile     *file;
    char        buf[MPR_HTTP_BUFSIZE], *path, *pair;
    int         bytes, next, count, rc, len;

    rc = 0;

    mprSetSocketBlockingMode(http->sock, 1);
    if (upload) {
        if (mprWriteHttpUploadData(http, files, fields) < 0) {
            mprError(http, "Can't write upload data %s", mprGetHttpError(http));
            mprSetSocketBlockingMode(http->sock, 0);
            return MPR_ERR_CANT_WRITE;
        }
    } else {
        if (fields) {
            count = mprGetListCount(fields);
            for (next = 0; !rc && (pair = mprGetNextItem(fields, &next)) != 0; ) {
                len = strlen(pair);
                if (next < count) {
                    len = strlen(pair);
                    if (mprWriteSocket(http->sock, pair, len) != len || mprWriteSocket(http->sock, "&", 1) != 1) {
                        return MPR_ERR_CANT_WRITE;
                    }
                } else {
                    if (mprWriteSocket(http->sock, pair, len) != len) {
                        return MPR_ERR_CANT_WRITE;
                    }
                }
            }
        }
        if (files) {
            mprAssert(mprGetListCount(files) == 1);
            for (rc = next = 0; !rc && (path = mprGetNextItem(files, &next)) != 0; ) {
                file = mprOpen(http, path, O_RDONLY | O_BINARY, 0);
                if (file == 0) {
                    mprError(http, "Can't open \"%s\"", path);
                    return MPR_ERR_CANT_OPEN;
                }
                if (verbose && upload) {
                    mprPrintf(http, "uploading: %s\n", path);
                }
                while ((bytes = mprRead(file, buf, sizeof(buf))) > 0) {
                    if (mprWriteHttp(http, buf, bytes) != bytes) {
                        mprFree(file);
                        return MPR_ERR_CANT_WRITE;
                    }
                }
                mprFree(file);
            }
        }
        if (mprFinalizeHttpWriting(http) < 0) {
            return MPR_ERR_CANT_WRITE;
        }
    }
    mprSetSocketBlockingMode(http->sock, 0);
    return rc;
}


static bool iterationsComplete(MprCtx ctx)
{
    lock();
    if (verbose > 1) mprPrintf(ctx, ".");
    if (++fetchCount >= iterations) {
        unlock();
        return 1;
    }
    unlock();
    return 0;
}


static void finishThread(MprThread *tp)
{
#if BLD_FEATURE_MULTITHREAD
    if (tp) {
        lock();
        activeLoadThreads--;
        unlock();
    }
#endif
}


static void waitForUser(MprCtx ctx)
{
    int     value, junk;

    lock();
    mprPrintf(ctx, "Pause: ");
    junk = read(0, (char*) &value, 1);
    unlock();
}


static void addFormVars(MprCtx ctx, cchar *buf)
{
    char    *pair, *tok;

    pair = mprStrTok(mprStrdup(ctx, buf), "&", &tok);
    while (pair != 0) {
        mprAddItem(formData, pair);
        pair = mprStrTok(0, "&", &tok);
    }
}


static bool isPort(cchar *name)
{
    cchar   *cp;

    for (cp = name; *cp && *cp != '/'; cp++) {
        if (!isdigit((int) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


static char *resolveUrl(MprHttp *http, cchar *url)
{
    if (*url == '/') {
        if (host) {
            if (mprStrcmpAnyCaseCount(host, "http://", 7) != 0 && mprStrcmpAnyCaseCount(host, "https://", 8) != 0) {
                return mprAsprintf(http, -1, "http://%s%s", host, url);
            } else {
                return mprAsprintf(http, -1, "%s%s", host, url);
            }
        } else {
            return mprAsprintf(http, -1, "http://127.0.0.1%s", url);
        }
    } 
    if (mprStrcmpAnyCaseCount(url, "http://", 7) != 0 && mprStrcmpAnyCaseCount(url, "https://", 8) != 0) {
        if (isPort(url)) {
            return mprAsprintf(http, -1, "http://127.0.0.1:%s", url);
        } else {
            return mprAsprintf(http, -1, "http://%s", url);
        }
    }
    return mprStrdup(http, url);
}


static void showOutput(MprHttp *http, cchar *buf, int count)
{
    MprHttpResponse     *resp;
    int                 i, c, junk;
    
    resp = http->response;

    if (noout) {
        return;
    }
    if (resp->code == 401 || (http->followRedirects && (301 <= resp->code && resp->code <= 302))) {
        return;
    }
    if (!printable) {
        junk = write(1, (char*) buf, count);
        return;
    }

    for (i = 0; i < count; i++) {
        if (!isprint((int) buf[i]) && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
            isBinary = 1;
            break;
        }
    }
    if (!isBinary) {
        junk = write(1, (char*) buf, count);
        return;
    }
    for (i = 0; i < count; i++) {
        c = (uchar) buf[i];
        if (printable && isBinary) {
            mprPrintf(http, "%02x ", c & 0xff);
        } else {
            mprPrintf(http, "%c", (int) buf[i]);
        }
    }
}


static void trace(MprHttp *http, cchar *url, int fetchCount, cchar *method, int code, int contentLen)
{
    if (mprStrcmpAnyCaseCount(url, "http://", 7) != 0) {
        url += 7;
    }
    if ((fetchCount % 200) == 1) {
        if (fetchCount == 1 || (fetchCount % 5000) == 1) {
            if (fetchCount > 1) {
                mprPrintf(http, "\n");
            }
            mprPrintf(http, "  Count  Thread   Op  Code   Bytes  Url\n");
        }
        mprPrintf(http, "%7d %7s %4s %5d %7d  %s\n", fetchCount - 1,
            mprGetCurrentThreadName(http), method, code, contentLen, url);
    }
}


static void logHandler(MprCtx ctx, int flags, int level, const char *msg)
{
    Mpr         *mpr;
    MprFile     *file;
    char        *prefix;

    mpr = mprGetMpr(ctx);
    file = (MprFile*) mpr->logHandlerData;
    prefix = mpr->name;

    while (*msg == '\n') {
        mprFprintf(file, "\n");
        msg++;
    }

    if (flags & MPR_LOG_SRC) {
        mprFprintf(file, "%s: %d: %s\n", prefix, level, msg);

    } else if (flags & MPR_ERROR_SRC) {
        /*
         *  Use static printing to avoid malloc when the messages are small.
         *  This is important for memory allocation errors.
         */
        if (strlen(msg) < (MPR_MAX_STRING - 32)) {
            mprStaticPrintf(file, "%s: Error: %s\n", prefix, msg);
        } else {
            mprFprintf(file, "%s: Error: %s\n", prefix, msg);
        }

    } else if (flags & MPR_FATAL_SRC) {
        mprFprintf(file, "%s: Fatal: %s\n", prefix, msg);
        
    } else if (flags & MPR_ASSERT_SRC) {
        mprFprintf(file, "%s: Assertion %s, failed\n", prefix, msg);

    } else if (flags & MPR_RAW) {
        mprFprintf(file, "%s", msg);
    }
    
    if (flags & (MPR_ERROR_SRC | MPR_FATAL_SRC | MPR_ASSERT_SRC)) {
        mprBreakpoint();
    }
}



static int startLogging(Mpr *mpr, char *logSpec)
{
    MprFile     *file;
    char        *levelSpec;
    int         level;

    level = 0;

    if ((levelSpec = strchr(logSpec, ':')) != 0) {
        *levelSpec++ = '\0';
        level = atoi(levelSpec);
    }
    if (strcmp(logSpec, "stdout") == 0) {
        file = mpr->fileSystem->stdOutput;
    } else {
        if ((file = mprOpen(mpr, logSpec, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprPrintfError(mpr, "Can't open log file %s\n", logSpec);
            return -1;
        }
    }
    mprSetLogLevel(mpr, level);
    mprSetLogHandler(mpr, logHandler, (void*) file);
    return 0;
}


#if (BLD_WIN_LIKE && !WINCE) || VXWORKS
static char *getpass(char *prompt)
{
    static char password[MPR_MAX_STRING];
    int     c, i;

    fputs(prompt, stderr);
    for (i = 0; i < (int) sizeof(password) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            password[--i] = '\0';
            fputs("\b \b", stderr);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stderr);
            exit(255);
        } else if (!iscntrl(c) && (i < (int) sizeof(password) - 1)) {
            password[i] = c;
            fputc('*', stderr);
        } else {
            fputc('', stderr);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stderr);
    password[i] = '\0';
    return password;
}

#endif /* WIN */


static char *getPassword(MprCtx ctx)
{
#if !WINCE
    char    *password;

    password = getpass("Password: ");
#else
    password = "no-user-interaction-support";
#endif
    return mprStrdup(ctx, password);
}


#if VXWORKS
/*
 *  VxWorks link resolution
 */
int _cleanup() {
    return 0;
}
int _exit() {
    return 0;
}
#endif

#else /* BLD_FEATURE_HTTP_CLIENT */
void __dummy_httpClient() {}
#endif

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
