/**
 *  config.c - Parse the configuration file.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_CONFIG_PARSE

/***************************** Forward Declarations ****************************/

static bool featureSupported(MprCtx ctx, char *key);
static MaConfigState *pushState(MprCtx ctx, MaConfigState *state, int *top);
static int processSetting(MaServer *server, char *key, char *value, MaConfigState *state);

#if BLD_FEATURE_CONFIG_SAVE
static void tabs(int fd, int indent);
static void printAuth(int fd, MaHost *host, MaAuth *auth, int indent);
#endif

/******************************************************************************/
/*
 *  Configure the server
 */
int maConfigureServer(MprCtx ctx, MaHttp *http, MaServer *server, cchar *configFile, cchar *ipAddr, int port, cchar *docRoot)
{
    MaHost      *host;
    MaLocation  *location, *loc;
    MaAlias     *ap;
    char        *path, *searchPath;

    if (ipAddr && docRoot) {

        mprLog(http, 2, "DocumentRoot %s", docRoot);
        if ((host = maCreateDefaultHost(server, docRoot, ipAddr, port)) == 0) {
            mprUserError(ctx, "Can't open server on %s", ipAddr);
            return MPR_ERR_CANT_OPEN;
        }
        location = host->location;

#if WIN
        searchPath = mprAsprintf(ctx, -1, "%s" MPR_SEARCH_SEP ".", mprGetAppDir(ctx));
#else
        searchPath = mprAsprintf(ctx, -1, "%s" MPR_SEARCH_SEP "%s" MPR_SEARCH_SEP ".", mprGetAppDir(ctx),
            mprSamePath(ctx, BLD_BIN_PREFIX, mprGetAppDir(ctx)) ? BLD_MOD_PREFIX: BLD_ABS_MOD_DIR);
#endif
        mprSetModuleSearchPath(server, searchPath);
        mprFree(searchPath);

        maSetConnector(http, location, "netConnector");

        /*
         *  Auth must be added first to authorize all requests. File is last as a catch all.
         */
        maLoadModule(http, "authFilter", "mod_auth");
        if (maLookupStage(http, "authFilter")) {
            maAddHandler(http, location, "authFilter", "");
        }

        maLoadModule(http, "cgiHandler", "mod_cgi");
        if (maLookupStage(http, "cgiHandler")) {
            maAddHandler(http, location, "cgiHandler", ".cgi .cgi-nph .bat .cmd .pl .py");
            /*
             *  Add cgi-bin with a location block for the /cgi-bin URL prefix.
             */
            path = "cgi-bin";
            if (mprPathExists(host, path, X_OK)) {
                ap = maCreateAlias(host, "/cgi-bin/", path, 0);
                mprLog(host, 4, "ScriptAlias \"/cgi-bin/\":\"%s\"", path);
                maInsertAlias(host, ap);
                loc = maCreateLocation(host, host->location);
                maSetLocationPrefix(loc, "/cgi-bin/");
                maSetHandler(http, host, loc, "cgiHandler");
                maAddLocation(host, loc);
            }
        }
        maLoadModule(http, "ejsHandler", "mod_ejs");
        if (maLookupStage(http, "ejsHandler")) {
            maAddHandler(http, location, "ejsHandler", ".ejs");
        }
        maLoadModule(http, "phpHandler", "mod_php");
        if (maLookupStage(http, "phpHandler")) {
            maAddHandler(http, location, "phpHandler", ".php");
        }
        maLoadModule(http, "fileHandler", "mod_file");
        if (maLookupStage(http, "fileHandler")) {
            maAddHandler(http, location, "fileHandler", "");
        }

#if BLD_FEATURE_CONFIG_PARSE
    } else {
        /*
         *  Configure the http service and hosts specified in the config file.
         */
        path = mprGetAbsPath(server, configFile);
        if (maParseConfig(server, path) < 0) {
            /* mprUserError(ctx, "Can't configure server using %s", path); */
            return MPR_ERR_CANT_INITIALIZE;
        }
        mprFree(path);
#endif
    }
    return 0;
}


int maParseConfig(MaServer *server, cchar *configFile)
{
    Mpr             *mpr;
    MprList         *includes;
    MaHttp          *http;
    MaHost          *defaultHost;
    MaConfigState   stack[MA_MAX_CONFIG_DEPTH], *state;
    MaHostAddress   *address;
    MaListen        *listen;
    MaDir           *dir, *bestDir;
    MaHost          *host, *hp;
    MaAlias         *alias;
    MprDirEntry     *dp;
    char            buf[MPR_MAX_STRING];
    char            *cp, *tok, *key, *value, *path;
    int             i, rc, top, next, nextAlias, len;

    mpr = mprGetMpr(server);

    http = server->http;
    memset(stack, 0, sizeof(stack));

#if BLD_FEATURE_LOG
    server->alreadyLogging = mprGetLogHandler(server) ? 1 : 0;
#endif

    /*
     *  Create the default host and directory
     */
    defaultHost = host = maCreateHost(server, NULL, NULL);
    server->defaultHost = defaultHost;
    mprAddItem(server->hosts, host);

    top = 0;
    state = &stack[top];
    state->server = server;
    state->host = host;
    state->dir = maCreateBareDir(host, ".");
    state->location = defaultHost->location;
    state->location->connector = http->netConnector;
    state->enabled = 1;
    state->lineNumber = 0;

    state->filename = (char*) configFile;

    state->file = mprOpen(server, configFile, O_RDONLY | O_TEXT, 0444);
    if (state->file == 0) {
        mprError(server, "Can't open %s for config directives", configFile);
        return MPR_ERR_CANT_OPEN;
    }

    /*
     *  Set the default location authorization definition to match the default directory auth
     */
    state->location->auth = state->dir->auth;
    state->auth = state->dir->auth;

    maInsertDir(host, state->dir);
    maSetHostName(host, "Main Server");

    /*
     *  Parse each line in the config file
     */
    for (state->lineNumber = 1; top >= 0; state->lineNumber++) {

        state = &stack[top];
        mprAssert(state->file);
        if (mprGets(state->file, buf, sizeof(buf) - 1) == 0) {
            if (--top > 0 && strcmp(state->filename, stack[top].filename) == 0) {
                mprError(server, "Unclosed directives in %s", state->filename);
                while (top >= 0 && strcmp(state->filename, state[top].filename) == 0) {
                    top--;
                }
            }
            if (top >= 0 && state->file == stack[top].file) {
                stack[top].lineNumber = state->lineNumber + 1;
            } else {
                mprFree(state->file);
                state->file = 0;
                if (top >= 0) {
                    state = &stack[top];
                }
            }
            continue;
        }

        buf[sizeof(buf) - 1] = '\0';
        cp = buf;
        while (isspace((int) *cp)) {
            cp++;
        }
        if (*cp == '\0' || *cp == '#') {
            continue;
        }

        cp = mprStrTrim(cp, "\r\n\t ");
        key = mprStrTok(cp, " \t\n", &tok);
        value = mprStrTok(0, "\n", &tok);
        if (key == 0 || *key == '\0') {
            goto syntaxErr;
        }
        if (value) {
            while (isspace((int) *value)) {
                value++;
            }
            if (*value) {
                cp = &value[strlen(value) - 1];
                while (cp > value && isspace((int) *cp)) {
                    cp--;
                }
                *++cp = '\0';
            }

        } else {
            value = "";
        }

        if (mprStrcmpAnyCase(key, "Include") == 0) {
            state->lineNumber++;
            value = mprStrTrim(value, "\"");
            if ((cp = strchr(value, '*')) == 0) {
                state = pushState(server, state, &top);
                state->lineNumber = 0;
                state->filename = mprJoinPath(server, server->serverRoot, value);
                state->file = mprOpen(server, state->filename, O_RDONLY | O_TEXT, 0444);
                if (state->file == 0) {
                    mprError(server, "Can't open include file %s for config directives", state->filename);
                    return MPR_ERR_CANT_OPEN;
                }
                mprLog(server, 5, "Parsing config file: %s", state->filename);

            } else {
                /*
                 *  Process wild cards. This is very simple - only "*" is supported.
                 */
                *cp = '\0';
                len = (int) strlen(value);
                cp = mprJoinPath(server, server->serverRoot, value);
                includes = mprGetPathFiles(server, cp, 0);
                if (includes == 0) {
                    continue;
                }
                value = mprJoinPath(server, server->serverRoot, value);
                for (next = 0; (dp = mprGetNextItem(includes, &next)) != 0; ) {
                    state = pushState(server, state, &top);
                    state->filename = mprJoinPath(server, value, dp->name);
                    state->file = mprOpen(server, state->filename, O_RDONLY | O_TEXT, 0444);
                    if (state->file == 0) {
                        mprError(server, "Can't open include file %s for config directives", state->filename);
                        return MPR_ERR_CANT_OPEN;
                    }
                    mprLog(server, 5, "Parsing config file: %s", state->filename);
                }
                mprFree(includes);
            }
            continue;

        } else if (*key != '<') {

            if (!state->enabled) {
                mprLog(server, 8, "Skipping key %s at %s:%d", key, state->filename, state->lineNumber);
                continue;
            }

            /*
             *  Keywords outside of a virtual host or directory section
             */
            rc = processSetting(server, key, value, state);
            if (rc == 0) {
                char    *extraMsg;
                if (strcmp(key, "SSLEngine") == 0) {
                    extraMsg =
                        "\n\nFor SSL, you may have one SSL provider loaded.\n"
                        "Make sure that either OpenSSL or MatrixSSL is loaded.";
                } else {
                    extraMsg = "";
                }
                mprError(server,
                    "Ignoring unknown directive \"%s\"\nAt line %d in %s\n\n"
                    "Make sure the required modules are loaded. %s\n",
                    key, state->lineNumber, state->filename, extraMsg);
                continue;

            } else if (rc < 0) {
                mprError(server, "Ignoring bad directive \"%s\" at %s:%d", key, state->filename, state->lineNumber);
            }
            continue;
        }

        mprLog(server, 9, "AT %d, key %s", state->lineNumber, key);

        /*
         *  Directory, Location and virtual host sections
         */
        key++;
        i = (int) strlen(key) - 1;
        if (key[i] == '>') {
            key[i] = '\0';
        }
        if (*key != '/') {
            if (!state->enabled) {
                state = pushState(server, state, &top);
                mprLog(server, 8, "Skipping key %s at %s:%d", key, state->filename, state->lineNumber);
                continue;
            }

            i = (int) strlen(value) - 1;
            if (value[i] == '>') {
                value[i] = '\0';
            }

            /*
             *  Opening tags
             */
            if (mprStrcmpAnyCase(key, "If") == 0) {
                value = mprStrTrim(value, "\"");

                /*
                 *  Want to be able to nest <if> directives.
                 */
                state = pushState(server, state, &top);
                state->enabled = featureSupported(server, value);
                if (!state->enabled) {
                    mprLog(server, 3, "If \"%s\" conditional is false at %s:%d", value, state->filename, state->lineNumber);
                }

            } else if (mprStrcmpAnyCase(key, "VirtualHost") == 0) {

                value = mprStrTrim(value, "\"");

                state = pushState(server, state, &top);
                state->host = host = maCreateVirtualHost(server, value, host);
                state->location = host->location;
                state->auth = host->location->auth;

                maAddHost(server, host);
                maSetVirtualHost(host);

                state->dir = maCreateDir(host, stack[top - 1].dir->path, stack[top - 1].dir);
                state->auth = state->dir->auth;
                maInsertDir(host, state->dir);

                mprLog(server, 2, "Virtual Host: %s ", value);
                if (maCreateHostAddresses(server, host, value) < 0) {
                    mprFree(host);
                    goto err;
                }

            } else if (mprStrcmpAnyCase(key, "Directory") == 0) {
                path = maMakePath(host, mprStrTrim(value, "\""));
                state = pushState(server, state, &top);

                if ((dir = maLookupDir(host, path)) != 0) {
                    /*
                     *  Allow multiple occurences of the same directory. Append directives.
                     */  
                    state->dir = dir;

                } else {
                    /*
                     *  Create a new directory inherit and parent directory settings. This means inherit authorization from
                     *  the enclosing host.
                     */
                    state->dir = maCreateDir(host, path, stack[top - 1].dir);
                    maInsertDir(host, state->dir);
                }
                state->auth = state->dir->auth;

            } else if (mprStrcmpAnyCase(key, "Location") == 0) {
                value = mprStrTrim(value, "\"");
                if (maLookupLocation(host, value)) {
                    mprError(http, "Location block already exists for \"%s\"", value);
                    goto err;
                }
                state = pushState(server, state, &top);
                state->location = maCreateLocation(host, state->location);
                state->auth = state->location->auth;

                maSetLocationPrefix(state->location, value);

                if (maAddLocation(host, state->location) < 0) {
                    mprError(server, "Can't add location %s", value);
                    goto err;
                }
                mprAssert(host->location->prefix);
            }

        } else {

            stack[top - 1].lineNumber = state->lineNumber + 1;
            key++;

            /*
             *  Closing tags
             */
            if (state->enabled && state->location != stack[top-1].location) {
                maFinalizeLocation(state->location);
            }
            if (mprStrcmpAnyCase(key, "If") == 0) {
                top--;
                host = stack[top].host;

            } else if (mprStrcmpAnyCase(key, "VirtualHost") == 0) {
                top--;
                host = stack[top].host;

            } else if (mprStrcmpAnyCase(key, "Directory") == 0) {
                top--;

            } else if (mprStrcmpAnyCase(key, "Location") == 0) {
                top--;
            }
            if (top < 0) {
                goto syntaxErr;
            }
        }
    }

    /*
     *  Validate configuration
     */
    if (mprGetListCount(server->listens) == 0) {
        mprError(server, "Must have a Listen directive");
        return MPR_ERR_BAD_SYNTAX;
    }
    if (defaultHost->documentRoot == 0) {
        maSetDocumentRoot(defaultHost, ".");
    }

#if BLD_FEATURE_MULTITHREAD
{
    MaLimits *limits = &http->limits;
    if (limits->maxThreads > 0) {
        mprSetMaxWorkers(http, limits->maxThreads);
        mprSetMinWorkers(http, limits->minThreads);
    }
}
#endif

    /*
     *  Add default server listening addresses to the HostAddress hash. We pretend it is a vhost. Insert at the
     *  end of the vhost list so we become the default if no other vhost matches. Ie. vhosts take precedence
     */
    for (next = 0; (listen = mprGetNextItem(server->listens, &next)) != 0; ) {
        address = (MaHostAddress*) maLookupHostAddress(server, listen->ipAddr, listen->port);
        if (address == 0) {
            address = maCreateHostAddress(server, listen->ipAddr, listen->port);
            mprAddItem(server->hostAddresses, address);
        }
        maInsertVirtualHost(address, defaultHost);
    }

    if (strcmp(defaultHost->name, "Main Server") == 0) {
        defaultHost->name = 0;
    }

#if KEEP
    natServerName = 0;
    needServerName = (defaultHost->name == 0);

    for (next = 0; (listen = mprGetNextItem(server->listens, &next)) != 0; ) {
        ipAddr = listen->ipAddr;
        if (needServerName && *ipAddr != '\0') {
            /*
             *  Try to get the most accessible server name possible.
             */
            if (strncmp(ipAddr, "127.", 4) == 0 || strncmp(ipAddr, "localhost:", 10) == 0) {
                if (! natServerName) {
                    maSetHostName(defaultHost, ipAddr);
                    needServerName = 0;
                }
            } else {
                if (strncmp(ipAddr, "10.", 3) == 0 || strncmp(ipAddr, "192.168.", 8) == 0 || 
                        strncmp(ipAddr, "172.16.", 7) == 0) {
                    natServerName = 1;
                } else {
                    maSetHostName(defaultHost, ipAddr);
                    needServerName = 0;
                }
            }
        }
    }

    /*
     *  Last try to setup the server name if we don't have a non-local name.
     */
    if (needServerName && !natServerName) {
        /*
         *  This code is undesirable as it makes us dependent on DNS -- bad
         */
        if (natServerName) {
            cchar *hostName = mprGetServerName(http);
            mprLog(server, 0, "WARNING: Missing ServerName directive, doing DNS lookup.");
            listen = mprGetFirstItem(server->listens);
            mprSprintf(ipAddrPort, sizeof(ipAddrPort), "%s:%d", hostName, listen->port);
            maSetHostName(defaultHost, hostName);

        } else {
            maSetHostName(defaultHost, defaultHost->ipAddrPort);
        }
        mprLog(server, 2, "Missing ServerName directive, ServerName set to: \"%s\"", defaultHost->name);
    }
#endif

    /*
     *  Validate all the hosts
     */
    for (next = 0; (hp = mprGetNextItem(server->hosts, &next)) != 0; ) {

        if (hp->documentRoot == 0) {
            maSetDocumentRoot(hp, defaultHost->documentRoot);
        }

        /*
         *  Ensure all hosts have mime types.
         */
        if (hp->mimeTypes == 0 || mprGetHashCount(hp->mimeTypes) == 0) {
            if (hp == defaultHost && defaultHost->mimeTypes) {
                hp->mimeTypes = defaultHost->mimeTypes;

            } else if (maOpenMimeTypes(hp, "mime.types") < 0) {
                static int once = 0;
                /*
                 *  Do minimal mime types configuration
                 */
                maAddStandardMimeTypes(hp);
                if (once++ == 0) {
                    mprLog(server, 2, "No mime.types file, using minimal mime configuration");
                }
            }
        }

        /*
         *  Check aliases have directory blocks. We must be careful to inherit authorization from the best 
         *  matching directory.
         */
        for (nextAlias = 0; (alias = mprGetNextItem(hp->aliases, &nextAlias)) != 0; ) {
            if (alias->filename) {
                // mprLog(hp, 0, "Alias \"%s\" %s", alias->prefix, alias->filename);
                path = maMakePath(hp, alias->filename);
                bestDir = maLookupBestDir(hp, path);
                if (bestDir == 0) {
                    bestDir = maCreateDir(hp, alias->filename, stack[0].dir);
                    maInsertDir(hp, bestDir);
                }
                mprFree(path);
            }
        }

        /*
         *  Define a ServerName if one has not been defined. Just use the IP:Port if defined.
         */
        if (host->name == 0) {
            if (host->ipAddrPort) {
                maSetHostName(hp, host->ipAddrPort);
            } else {
                maSetHostName(hp, mprGetHostName(host));
            }
        }
    }

    if (mprHasAllocError(mpr)) {
        mprError(server, "Memory allocation error when initializing");
        return MPR_ERR_NO_MEMORY;
    }
    return 0;

syntaxErr:
    mprError(server, "Syntax error in %s at %s:%d", configFile, state->filename, state->lineNumber);
    return MPR_ERR_BAD_SYNTAX;

err:
    mprError(server, "Error in %s at %s:%d", configFile, state->filename, state->lineNumber);
    return MPR_ERR_BAD_SYNTAX;
}


/*
 *  Process the configuration settings. Permissible to modify key and value.
 *  Return < 0 for errors, zero if directive not found, otherwise 1 is success.
 */
static int processSetting(MaServer *server, char *key, char *value, MaConfigState *state)
{
    MaHttp          *http;
    MaAuth          *auth;
    MaAlias         *alias;
    MaStage         *stage;
    MaLocation      *location;
    MaHost          *host;
    MaDir           *dir;
    MaLimits        *limits;
    MprHash         *hp;
    char            ipAddrPort[MPR_MAX_IP_ADDR_PORT];
    char            *name, *path, *prefix, *cp, *tok, *ext, *mimeType, *url, *newUrl, *extensions, *codeStr, *hostName;
    char            *items, *include, *exclude, *when, *mimeTypes;
    int             port, rc, code, processed, num, flags, colonCount, len, mask, level;

    mprAssert(state);
    mprAssert(key);

    http = server->http;
    host = state->host;
    dir = state->dir;
    location = state->location;
    mprAssert(state->location->prefix);

    mprAssert(host);
    mprAssert(dir);
    auth = state->auth;
    processed = 0;
    limits = host->limits;
    flags = 0;

    switch (toupper((int) key[0])) {
    case 'A':
        if (mprStrcmpAnyCase(key, "Alias") == 0) {
            /* Scope: server, host */
            if (maSplitConfigValue(server, &prefix, &path, value, 1) < 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            prefix = maReplaceReferences(host, prefix);
            path = maMakePath(host, path);
            if (maLookupAlias(host, prefix)) {
                mprError(server, "Alias \"%s\" already exists", prefix);
                return MPR_ERR_BAD_SYNTAX;
            }
            alias = maCreateAlias(host, prefix, path, 0);
            mprLog(server, 4, "Alias: \"%s for \"%s\"", prefix, path);
            if (maInsertAlias(host, alias) < 0) {
                mprError(server, "Can't insert alias: %s", prefix);
                return MPR_ERR_BAD_SYNTAX;
            }
            mprFree(path);
            mprFree(prefix);
            return 1;

        } else if (mprStrcmpAnyCase(key, "AddFilter") == 0) {
            /* Scope: server, host, location */
            name = mprStrTok(value, " \t", &extensions);
            if (maAddFilter(http, location, name, extensions, MA_FILTER_INCOMING | MA_FILTER_OUTGOING) < 0) {
                mprError(server, "Can't add filter %s", name);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "AddInputFilter") == 0) {
            /* Scope: server, host, location */
            name = mprStrTok(value, " \t", &extensions);
            if (maAddFilter(http, location, name, extensions, MA_FILTER_INCOMING) < 0) {
                mprError(server, "Can't add filter %s", name);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "AddOutputFilter") == 0) {
            /* Scope: server, host, location */
            name = mprStrTok(value, " \t", &extensions);
            if (maAddFilter(http, location, name, extensions, MA_FILTER_OUTGOING) < 0) {
                mprError(server, "Can't add filter %s", name);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "AddHandler") == 0) {
            /* Scope: server, host, location */
            name = mprStrTok(value, " \t", &extensions);
            if (maAddHandler(http, state->location, name, extensions) < 0) {
                mprError(server, "Can't add handler %s", name);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "AddType") == 0) {
            if (maSplitConfigValue(server, &mimeType, &ext, value, 1) < 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            maAddMimeType(host, ext, mimeType);
            return 1;

#if BLD_FEATURE_AUTH
        } else if (mprStrcmpAnyCase(key, "Allow") == 0) {
            char *from, *spec;
            if (maSplitConfigValue(server, &from, &spec, value, 1) < 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            /* spec can be: all, host, ipAddr */
            maSetAuthAllow(auth, spec);
            return 1;
#endif
        }
        break;

    case 'B':
        if (mprStrcmpAnyCase(key, "BrowserMatch") == 0) {
            return 1;
        }
        break;

    case 'C':
        if (mprStrcmpAnyCase(key, "Chroot") == 0) {
#if BLD_UNIX_LIKE
            path = maMakePath(host, mprStrTrim(value, "\""));
            if (chdir(path) < 0) {
                mprError(server, "Can't change directory to %s", path);
                mprFree(path);
                return MPR_ERR_CANT_OPEN;
            }
            if (chroot(path) < 0) {
                if (errno == EPERM) {
                    mprError(server, "Must be super user to use the --chroot option");
                } else {
                    mprError(server, "Can't change change root directory to %s, errno %d", path, errno);
                }
                mprFree(path);
                return MPR_ERR_BAD_SYNTAX;
            }
            mprLog(server, MPR_CONFIG, "Chroot to: \"%s\"", path);
            mprFree(path);
            return 1;
#else
            mprError(server, "Chroot directive not supported on this operating system\n");
            return MPR_ERR_BAD_SYNTAX;
#endif

        } else if (mprStrcmpAnyCase(key, "CustomLog") == 0) {
#if BLD_FEATURE_ACCESS_LOG && !BLD_FEATURE_ROMFS
            char *format, *end;
            if (*value == '\"') {
                end = strchr(++value, '\"');
                if (end == 0) {
                    mprError(server, "Missing closing quote");
                    return MPR_ERR_BAD_SYNTAX;
                }
                *end++ = '\0';
                path = value;
                format = end;
                while ((int) isspace((int) *format)) {
                    format++;
                }

            } else {
                path = mprStrTok(value, " \t", &format);
            }
            if (path == 0 || format == 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            path = maMakePath(host, path);
            maSetAccessLog(host, path, mprStrTrim(format, "\""));
            mprFree(path);
            maSetLogHost(host, host);
#endif
            return 1;
        }
        break;

    case 'D':
        if (0) {
#if BLD_FEATURE_AUTH
        } else if (mprStrcmpAnyCase(key, "Deny") == 0) {
            char *from, *spec;
            if (maSplitConfigValue(server, &from, &spec, value, 1) < 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            maSetAuthDeny(auth, spec);
            return 1;
#endif

        } else if (mprStrcmpAnyCase(key, "DirectoryIndex") == 0) {
            value = mprStrTrim(value, "\"");
            if (dir == 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            maSetDirIndex(dir, value);
            return 1;

        } else if (mprStrcmpAnyCase(key, "DocumentRoot") == 0) {
            path = maMakePath(host, mprStrTrim(value, "\""));
            maSetDocumentRoot(host, path);
            maSetDirPath(dir, path);
            mprLog(server, MPR_CONFIG, "DocRoot (%s): \"%s\"", host->name, path);
            mprFree(path);
            return 1;
        }
        break;

    case 'E':
        if (mprStrcmpAnyCase(key, "ErrorDocument") == 0) {
            codeStr = mprStrTok(value, " \t", &url);
            if (codeStr == 0 || url == 0) {
                mprError(server, "Bad ErrorDocument directive");
                return MPR_ERR_BAD_SYNTAX;
            }
            maAddErrorDocument(location, codeStr, url);
            return 1;

        } else if (mprStrcmpAnyCase(key, "ErrorLog") == 0) {
            path = mprStrTrim(value, "\"");
            if (path && *path) {
#if BLD_FEATURE_LOG
                if (server->alreadyLogging) {
                    mprLog(server, 4, "Already logging. Ignoring ErrorLog directive");
                } else {
                    maStopLogging(server);

                    if (strncmp(path, "stdout", 6) != 0) {
                        path = maMakePath(host, path);
                        rc = maStartLogging(server, path);
                        mprFree(path);
                    } else {
                        rc = maStartLogging(server, path);
                    }
                    if (rc < 0) {
                        mprError(server, "Can't write to ErrorLog");
                        return MPR_ERR_BAD_SYNTAX;
                    }
                }
#endif
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "Expires") == 0) {
            value = mprStrTrim(value, "\"");
            when = mprStrTok(value, " \t", &mimeTypes);
            maAddLocationExpiry(location, (MprTime) mprAtoi(when, 10), mimeTypes);
            return 1;
        }
        break;

    case 'G':
        if (mprStrcmpAnyCase(key, "Group") == 0) {
            value = mprStrTrim(value, "\"");
            maSetHttpGroup(http, value);
            return 1;
        }
        break;

    case 'H':
        break;

    case 'K':
        if (mprStrcmpAnyCase(key, "KeepAlive") == 0) {
            if (mprStrcmpAnyCase(value, "on") == 0) {
                maSetKeepAlive(host, 1);
            } else {
                maSetKeepAlive(host, 0);
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "KeepAliveTimeout") == 0) {
            if (! mprGetDebugMode(server)) {
                maSetKeepAliveTimeout(host, atoi(value) * 1000);
            }
            return 1;
        }
        break;

    case 'L':
        if (mprStrcmpAnyCase(key, "LimitChunkSize") == 0) {
            num = atoi(value);
            if (num < MA_BOT_CHUNK_SIZE || num > MA_TOP_CHUNK_SIZE) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxChunkSize = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitClients") == 0) {
            mprSetMaxSocketClients(server, atoi(value));
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitRequestBody") == 0) {
            num = atoi(value);
            if (num < MA_BOT_BODY || num > MA_TOP_BODY) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxBody = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitRequestFields") == 0) {
            num = atoi(value);
            if (num < MA_BOT_NUM_HEADERS || num > MA_TOP_NUM_HEADERS) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxNumHeaders = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitRequestFieldSize") == 0) {
            num = atoi(value);
            if (num < MA_BOT_HEADER || num > MA_TOP_HEADER) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxHeader = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitResponseBody") == 0) {
            num = atoi(value);
            if (num < MA_BOT_RESPONSE_BODY || num > MA_TOP_RESPONSE_BODY) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxResponseBody = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitStageBuffer") == 0) {
            num = atoi(value);
            if (num < MA_BOT_STAGE_BUFFER || num > MA_TOP_STAGE_BUFFER) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxStageBuffer = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitUrl") == 0) {
            num = atoi(value);
            if (num < MA_BOT_URL || num > MA_TOP_URL){
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxUrl = num;
            return 1;

        } else if (mprStrcmpAnyCase(key, "LimitUploadSize") == 0) {
            num = atoi(value);
            if (num != -1 && (num < MA_BOT_UPLOAD_SIZE || num > MA_TOP_UPLOAD_SIZE)){
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxUploadSize = num;
            return 1;

#if DEPRECATED
        } else if (mprStrcmpAnyCase(key, "ListenIF") == 0) {
            MprList         *ipList;
            MprInterface    *ip;

            /*
             *  Options:
             *      interface:port
             *      interface   (default port MA_SERVER_DEFAULT_PORT_NUM)
             */
            if ((cp = strchr(value, ':')) != 0) {           /* interface:port */
                do {                                        /* find last colon */
                    tok = cp;
                    cp = strchr(cp + 1, ':');
                } while (cp != 0);
                cp = tok;
                *cp++ ='\0';

                port = atoi(cp);
                if (port <= 0 || port > 65535) {
                    mprError(server, "Bad listen port number %d", port);
                    return MPR_ERR_BAD_SYNTAX;
                }

            } else {            /* interface */
                port = MA_SERVER_DEFAULT_PORT_NUM;
            }

            ipList = mprGetMpr()->socketService->getInterfaceList();
            ip = (MprInterface*) ipList->getFirst();
            if (ip == 0) {
                mprError(server, "Can't find interfaces, use Listen-directive with IP address.");
                return MPR_ERR_BAD_SYNTAX;
            }

            while (ip) {
                if (mprStrcmpAnyCase(ip->name, value) != 0) {
                    ip = (MprInterface*) ipList->getNext(ip);
                    continue;
                }

                listens.insert(new MaListen(ip->ipAddr, port));
                if (host->ipAddrPort == 0) {
                    mprSprintf(ipAddrPort, sizeof(ipAddrPort), "%s:%d", ip->ipAddr, port);
                    maSetIpAddrPort(host, ipAddrPort);
                }
                break;
            }
            return 1;
#endif

        } else if (mprStrcmpAnyCase(key, "Listen") == 0) {

            /*
             *  Options:
             *      ipAddr:port
             *      ipAddr          default port MA_SERVER_DEFAULT_PORT_NUM
             *      port            All ip interfaces on this port
             *
             *  Where ipAddr may be "::::::" for ipv6 addresses or may be enclosed in "[]" if appending a port.
             */

            value = mprStrTrim(value, "\"");

            if (isdigit((int) *value) && strchr(value, '.') == 0 && strchr(value, ':') == 0) {
                /*
                 *  Port only, listen on all interfaces (ipv4 + ipv6)
                 */
                port = atoi(value);
                if (port <= 0 || port > 65535) {
                    mprError(server, "Bad listen port number %d", port);
                    return MPR_ERR_BAD_SYNTAX;
                }
                hostName = "";
                flags = MA_LISTEN_WILD_IP;

            } else {
                colonCount = 0;
                for (cp = (char*) value; ((*cp != '\0') && (colonCount < 2)) ; cp++) {
                    if (*cp == ':') {
                        colonCount++;
                    }
                }
                /*
                 *  Hostname with possible port number. Port number parsed if address enclosed in "[]"
                 */
                if (colonCount > 1) {
                    /* ipv6 */
                    hostName = value;
                    if (*value == '[' && (cp = strrchr(cp, ':')) != 0) {
                        *cp++ = '\0';
                        port = atoi(cp);
                    } else {
                        port = MA_SERVER_DEFAULT_PORT_NUM;
                        flags = MA_LISTEN_DEFAULT_PORT;
                    }
                    if (*hostName == '[') {
                        hostName++;
                    }
                    len = strlen(hostName);
                    if (hostName[len - 1] == ']') {
                        hostName[len - 1] = '\0';
                    }

                } else {
                    /* ipv4 */
                    hostName = value;
                    if ((cp = strrchr(value, ':')) != 0) {
                        *cp++ = '\0';
                        port = atoi(cp);

                    } else {
                        port = MA_SERVER_DEFAULT_PORT_NUM;
                        flags = MA_LISTEN_DEFAULT_PORT;
                    }
                }
            }
            mprAddItem(server->listens, maCreateListen(server, hostName, port, flags));

            /*
             *  Set the host ip spec if not already set
             */
            if (host->ipAddrPort == 0) {
                mprSprintf(ipAddrPort, sizeof(ipAddrPort), "%s:%d", hostName, port);
                maSetHostIpAddrPort(host, ipAddrPort);
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "LogFormat") == 0) {
            return 1;

        } else if (mprStrcmpAnyCase(key, "LogLevel") == 0) {
#if BLD_FEATURE_LOG
            if (server->alreadyLogging) {
                mprLog(server, 4, "Already logging. Ignoring LogLevel directive");

            } else {
                int level;
                value = mprStrTrim(value, "\"");
                level = atoi(value);
                mprSetLogLevel(server, level);
            }
#endif
            return 1;

        } else if (mprStrcmpAnyCase(key, "LogTrace") == 0) {
            cp = mprStrTok(value, " \t", &tok);
            level = atoi(cp);
            if (level < 0 || level > 9) {
                mprError(server, "Bad LogTrace level %d, must be 0-9", level);
                return MPR_ERR_BAD_SYNTAX;
            }
            items = mprStrTok(0, "\n", &tok);
            mprStrLower(items);
            mask = 0;
            if (strstr(items, "headers")) {
                mask |= MA_TRACE_HEADERS;
            }
            if (strstr(items, "body")) {
                mask |= MA_TRACE_BODY;
            }
            if (strstr(items, "request")) {
                mask |= MA_TRACE_REQUEST;
            }
            if (strstr(items, "response")) {
                mask |= MA_TRACE_RESPONSE;
            }
            maSetHostTrace(host, level, mask);
            return 1;

        } else if (mprStrcmpAnyCase(key, "LogTraceFilter") == 0) {
            cp = mprStrTok(value, " \t", &tok);
            len = atoi(cp);
            include = mprStrTok(0, " \t", &tok);
            exclude = mprStrTok(0, " \t", &tok);
            include = mprStrTrim(include, "\"");
            exclude = mprStrTrim(exclude, "\"");
            maSetHostTraceFilter(host, len, include, exclude);
            return 1;

        } else if (mprStrcmpAnyCase(key, "LoadModulePath") == 0) {
            value = mprStrTrim(value, "\"");
            path = mprStrcat(server, -1, value, MPR_SEARCH_SEP, mprGetAppDir(server), NULL);
            mprSetModuleSearchPath(server, path);
            mprFree(path);
            return 1;

        } else if (mprStrcmpAnyCase(key, "LoadModule") == 0) {
            name = mprStrTok(value, " \t", &tok);
            if (name == 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            path = mprStrTok(0, "\n", &tok);
            if (path == 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            if (maLoadModule(http, name, path) < 0) {
                /*  Error messages already done */
                return MPR_ERR_CANT_CREATE;
            }
            return 1;
        }
        break;

    case 'M':
        if (mprStrcmpAnyCase(key, "MaxKeepAliveRequests") == 0) {
            maSetMaxKeepAlive(host, atoi(value));
            return 1;
        }
        break;

    case 'N':
        if (mprStrcmpAnyCase(key, "NameVirtualHost") == 0) {
            mprLog(server, 2, "NameVirtual Host: %s ", value);
            if (maCreateHostAddresses(server, 0, value) < 0) {
                return -1;
            }
            return 1;
        }
        break;

    case 'O':
        if (0) {
#if BLD_FEATURE_AUTH
        } else if (mprStrcmpAnyCase(key, "Order") == 0) {
            if (mprStrcmpAnyCase(mprStrTrim(value, "\""), "Allow,Deny") == 0) {
                maSetAuthOrder(auth, MA_ALLOW_DENY);
            } else {
                maSetAuthOrder(auth, MA_DENY_ALLOW);
            }
            return 1;
#endif
        }
        break;

    case 'P':
        if (mprStrcmpAnyCase(key, "Protocol") == 0) {
            if (strcmp(value, "HTTP/1.0") == 0) {
                maSetHttpVersion(host, MPR_HTTP_1_0);

            } else if (strcmp(value, "HTTP/1.1") == 0) {
                maSetHttpVersion(host, MPR_HTTP_1_1);
            }
            return 1;
        } else if (mprStrcmpAnyCase(key, "PutMethod") == 0) {
            if (mprStrcmpAnyCase(value, "on") == 0) {
                location->flags |= MA_LOC_PUT_DELETE;
            } else {
                location->flags &= ~MA_LOC_PUT_DELETE;
            }
            return 1;
        }
        break;

    case 'R':
        if (mprStrcmpAnyCase(key, "Redirect") == 0) {
            if (value[0] == '/' || value[0] == 'h') {
                code = 302;
                url = mprStrTok(value, " \t", &tok);

            } else if (isdigit((int) value[0])) {
                cp = mprStrTok(value, " \t", &tok);
                code = atoi(cp);
                url = mprStrTok(0, " \t\n", &tok);

            } else {
                cp = mprStrTok(value, " \t", &tok);
                if (strcmp(value, "permanent") == 0) {
                    code = 301;
                } else if (strcmp(value, "temp") == 0) {
                    code = 302;
                } else if (strcmp(value, "seeother") == 0) {
                    code = 303;
                } else if (strcmp(value, "gone") == 0) {
                    code = 410;
                } else {
                    return MPR_ERR_BAD_SYNTAX;
                }
                url = mprStrTok(0, " \t\n", &tok);
            }
            if (code >= 300 && code <= 399) {
                newUrl = mprStrTok(0, "\n", &tok);
            } else {
                newUrl = "";
            }
            if (code <= 0 || url == 0 || newUrl == 0) {
                return MPR_ERR_BAD_SYNTAX;
            }
            url = mprStrTrim(url, "\"");
            newUrl = mprStrTrim(newUrl, "\"");
            mprLog(server, 4, "insertAlias: Redirect %d from \"%s\" to \"%s\"", code, url, newUrl);
            alias = maCreateAlias(host, url, newUrl, code);
            maInsertAlias(host, alias);
            return 1;

        } else if (mprStrcmpAnyCase(key, "ResetPipeline") == 0) {
            maResetPipeline(location);
            return 1;
        }
        break;

    case 'S':
        if (mprStrcmpAnyCase(key, "ServerName") == 0) {
            value = mprStrTrim(value, "\"");
            if (strncmp(value, "http://", 7) == 0) {
                maSetHostName(host, &value[7]);
            } else {
                maSetHostName(host, value);
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "ServerRoot") == 0) {
            path = maMakePath(host, mprStrTrim(value, "\""));
            maSetServerRoot(server, path);
#if BLD_FEATURE_ROMFS
            mprLog(server, MPR_CONFIG, "Server Root \"%s\" in ROM", path);
#else
            mprLog(server, MPR_CONFIG, "Server Root \"%s\"", path);
#endif
            mprFree(path);
            return 1;

        } else if (mprStrcmpAnyCase(key, "SetConnector") == 0) {
            /* Scope: server, host, location */
            value = mprStrTrim(value, "\"");
            if (maSetConnector(http, location, value) < 0) {
                mprError(server, "Can't add handler %s", value);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "SetHandler") == 0) {
            /* Scope: server, host, location */
            name = mprStrTok(value, " \t", &extensions);
            maResetHandlers(state->location);
            if (maSetHandler(http, host, state->location, name) < 0) {
                mprError(server, "Can't add handler %s", name);
                return MPR_ERR_CANT_CREATE;
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "StartThreads") == 0) {
#if BLD_FEATURE_MULTITHREAD
            num = atoi(value);
            if (num < 0 || num > MA_TOP_THREADS) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->minThreads = num;
#endif
            return 1;
        }
        break;

    case 'T':
        if (mprStrcmpAnyCase(key, "ThreadLimit") == 0) {
#if BLD_FEATURE_MULTITHREAD
            num = atoi(value);
            if (num < 0 || num > MA_TOP_THREADS) {
                return MPR_ERR_BAD_SYNTAX;
            }
            limits->maxThreads = num;
#endif
            return 1;

        } else if (mprStrcmpAnyCase(key, "ThreadStackSize") == 0) {
#if BLD_FEATURE_MULTITHREAD
            num = atoi(value);
            if (num < MA_BOT_STACK || num > MA_TOP_STACK) {
                return MPR_ERR_BAD_SYNTAX;
            }
            mprSetThreadStackSize(server, num);
            return 1;
#endif

        } else if (mprStrcmpAnyCase(key, "TimeOut") == 0) {
            maSetTimeout(host, atoi(value) * 1000);
            return 1;

        } else if (mprStrcmpAnyCase(key, "TraceMethod") == 0) {
            if (mprStrcmpAnyCase(value, "on") == 0) {
                maSetTraceMethod(host, 1);
            } else {
                maSetTraceMethod(host, 0);
            }
            return 1;

        } else if (mprStrcmpAnyCase(key, "TypesConfig") == 0) {
            path = maMakePath(host, mprStrTrim(value, "\""));
            if (maOpenMimeTypes(host, path) < 0) {
                mprError(server, "Can't open TypesConfig mime file %s", path);
                maAddStandardMimeTypes(host);
                mprFree(path);
                return MPR_ERR_BAD_SYNTAX;
            }
            mprFree(path);
            return 1;
        }
        break;

    case 'U':
        if (mprStrcmpAnyCase(key, "User") == 0) {
            maSetHttpUser(http, mprStrTrim(value, "\""));
            return 1;
        }
        break;
    }

    rc = 0;

    /*
     *  Allow all stages to parse the request
     */
    hp = mprGetFirstHash(http->stages);
    while (hp) {
        stage = (MaStage*) hp->data;
        if (stage->parse) {
            rc = stage->parse(http, key, value, state);
        }
        if (rc < 0) {
            return rc;
        } else if (rc > 0) {
            break;
        }
        hp = mprGetNextHash(http->stages, hp);
    }

    return rc;
}


/*
 *  Create a location block for a handler and an alias. Convenience routine for ScriptAlias, EjsAppAlias, EjsAppDirAlias.
 */
MaLocation *maCreateLocationAlias(MaHttp *http, MaConfigState *state, cchar *prefixArg, cchar *pathArg, cchar *handlerName, 
        int flags)
{
    MaHost      *host;
    MaAlias     *alias;
    MaLocation  *location;
    char        *path, *prefix;

    host = state->host;

    prefix = maReplaceReferences(host, prefixArg);
    path = maMakePath(host, pathArg);

    /*
     *  Create an ejs application location block and alias
     */
    alias = maCreateAlias(host, prefix, path, 0);
    maInsertAlias(host, alias);
    mprLog(http, 4, "Alias \"%s\" for \"%s\"", prefix, path);
    mprFree(path);

    if (maLookupLocation(host, prefix)) {
        mprError(http, "Location block already exists for \"%s\"", prefix);
        return 0;
    }
    location = maCreateLocation(host, state->location);
    maSetLocationAuth(location, state->dir->auth);
    maSetLocationPrefix(location, prefix);
    maAddLocation(host, location);
    maSetLocationFlags(location, flags);
    maSetHandler(http, host, location, handlerName);
    mprFree(prefix);
    return location;
}


/*
 *  Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
 */
char *maMakePath(MaHost *host, cchar *file)
{
    MaServer    *server;
    char        *result, *path;

    mprAssert(file);

    server = host->server;

    if ((path = maReplaceReferences(host, file)) == 0) {
        /*  Overflow */
        return 0;
    }
    if (*path == '\0' || strcmp(path, ".") == 0) {
        result = mprStrdup(host, server->serverRoot);
    } else if (!mprIsAbsPath(host, path)) {
        result = mprJoinPath(host, server->serverRoot, path);
    } else {
        result = mprGetNormalizedPath(host, path);
    }
    mprFree(path);
    return result;
}


static int matchRef(cchar *key, char **src)
{
    int     len;

    mprAssert(src);
    mprAssert(key && *key);

    len = strlen(key);
    if (strncmp(*src, key, len) == 0) {
        *src += len;
        return 1;
    }
    return 0;
}


/*
 *  Replace a limited set of $VAR references. Currently support DOCUMENT_ROOT, SERVER_ROOT and PRODUCT
 */
char *maReplaceReferences(MaHost *host, cchar *str)
{
    MprBuf  *buf;
    char    *src;
    char    *result;

    buf = mprCreateBuf(host, 0, 0);
    if (str) {
        for (src = (char*) str; *src; ) {
            if (*src == '$') {
                ++src;
                if (matchRef("DOCUMENT_ROOT", &src) && host->documentRoot) {
                    mprPutStringToBuf(buf, host->documentRoot);
                    continue;

                } else if (matchRef("SERVER_ROOT", &src) && host->server->serverRoot) {
                    mprPutStringToBuf(buf, host->server->serverRoot);
                    continue;

                } else if (matchRef("PRODUCT", &src)) {
                    mprPutStringToBuf(buf, BLD_PRODUCT);
                    continue;
                }
            }
            mprPutCharToBuf(buf, *src++);
        }
    }
    mprAddNullToBuf(buf);
    result = mprStealBuf(host, buf);
    mprFree(buf);
    return result;
}


/*
 *  Max stack depth is:
 *      Default Server          Level 1
 *          <VirtualHost>       Level 2
 *              <Directory>     Level 3
 *                  <Location>  Level 4
 *
 */
static MaConfigState *pushState(MprCtx ctx, MaConfigState *state, int *top)
{
    MaConfigState   *next;

    (*top)++;
    if (*top >= MA_MAX_CONFIG_DEPTH) {
        mprError(ctx, "Too many nested directives in config file at %s:%d", state->filename, state->lineNumber);
        return 0;
    }
    next = state + 1;
    next->server = state->server;
    next->host = state->host;
    next->location = state->location;
    next->dir = state->dir;
    next->auth = state->auth;
    next->lineNumber = state->lineNumber;
    next->enabled = state->enabled;
    next->filename = state->filename;
    next->file = state->file;

    return next;
}


static bool featureSupported(MprCtx ctx, char *key)
{
    if (mprStrcmpAnyCase(key, "BLD_COMMERCIAL") == 0) {
        return strcmp(BLD_COMMERCIAL, "0") == 0;

#ifdef BLD_DEBUG
    } else if (mprStrcmpAnyCase(key, "BLD_DEBUG") == 0) {
        return BLD_DEBUG;
#endif

#ifdef BLD_FEATURE_ACCESS_LOG
    } else if (mprStrcmpAnyCase(key, "ACCESS_LOG") == 0) {
        return BLD_FEATURE_ACCESS_LOG;
#endif

#ifdef BLD_FEATURE_AUTH
    } else if (mprStrcmpAnyCase(key, "AUTH_MODULE") == 0) {
        return BLD_FEATURE_AUTH;
#endif

#ifdef BLD_FEATURE_CGI
    } else if (mprStrcmpAnyCase(key, "CGI_MODULE") == 0) {
        return BLD_FEATURE_CGI;
#endif

#ifdef BLD_FEATURE_CHUNK
    } else if (mprStrcmpAnyCase(key, "CHUNK_MODULE") == 0) {
        return BLD_FEATURE_CHUNK;
#endif

#ifdef BLD_FEATURE_AUTH_DIGEST
    } else if (mprStrcmpAnyCase(key, "DIGEST") == 0) {
        return BLD_FEATURE_AUTH_DIGEST;
#endif

#ifdef BLD_FEATURE_DIR
    } else if (mprStrcmpAnyCase(key, "DIR_MODULE") == 0) {
        return BLD_FEATURE_DIR;
#endif

#ifdef BLD_FEATURE_DOC
    } else if (mprStrcmpAnyCase(key, "DOC") == 0) {
        return BLD_FEATURE_DOC;
#endif

#ifdef BLD_FEATURE_EGI
    } else if (mprStrcmpAnyCase(key, "EGI_MODULE") == 0) {
        return BLD_FEATURE_EGI;
#endif

#ifdef BLD_FEATURE_EJS
    } else if (mprStrcmpAnyCase(key, "EJS_MODULE") == 0) {
        return BLD_FEATURE_EJS;
#endif

#ifdef BLD_FEATURE_FILE
    } else if (mprStrcmpAnyCase(key, "FILE_MODULE") == 0) {
        return BLD_FEATURE_FILE;
#endif

#ifdef BLD_FEATURE_LOG
    } else if (mprStrcmpAnyCase(key, "LOG") == 0) {
        return BLD_FEATURE_LOG;
#endif

#ifdef BLD_FEATURE_MULTITHREAD
    } else if (mprStrcmpAnyCase(key, "MULTITHREAD") == 0) {
        return BLD_FEATURE_MULTITHREAD;
#endif

#ifdef BLD_FEATURE_NET
    } else if (mprStrcmpAnyCase(key, "NET") == 0) {
        return BLD_FEATURE_NET;
#endif

#ifdef BLD_FEATURE_PHP
    } else if (mprStrcmpAnyCase(key, "PHP_MODULE") == 0) {
        return BLD_FEATURE_PHP;
#endif

#ifdef BLD_FEATURE_RANGE
    } else if (mprStrcmpAnyCase(key, "RANGE_MODULE") == 0) {
        return BLD_FEATURE_RANGE;
#endif

#ifdef BLD_FEATURE_SAMPLES
    } else if (mprStrcmpAnyCase(key, "SAMPLES") == 0) {
        return BLD_FEATURE_SAMPLES;
#endif

#ifdef BLD_FEATURE_SEND
    } else if (mprStrcmpAnyCase(key, "SEND") == 0) {
        return BLD_FEATURE_SEND;
#endif

#ifdef BLD_FEATURE_SSL
    } else if (mprStrcmpAnyCase(key, "SSL_MODULE") == 0) {
        return BLD_FEATURE_SSL;
#endif

#ifdef BLD_FEATURE_TEST
    } else if (mprStrcmpAnyCase(key, "TEST") == 0) {
        return BLD_FEATURE_TEST;
#endif

#ifdef BLD_FEATURE_UPLOAD
    } else if (mprStrcmpAnyCase(key, "UPLOAD_MODULE") == 0) {
        return BLD_FEATURE_UPLOAD;
#endif
    }

    mprError(ctx, "Unknown conditional \"%s\"", key);
    return 0;
}
#endif  /* BLD_FEATURE_CONFIG_PARSE */


#if BLD_FEATURE_CONFIG_SAVE
/*
 *  Save the configuration to the named config file
 */
int MaServer::saveConfig(char *configFile)
{
    MaAlias         *alias;
    MaDir           *dp, *defaultDp;
    MprFile         out;
    MaStage     *hanp;
    MaHost          *host, *defaultHost;
    MaLimits        *limits;
    MaListen        *listen;
    MaLocation      *loc;
    MaMimeHashEntry *mt;
    MprHashTable    *mimeTypes;
    MprList         *aliases;
    MaModule        *mp;
    MprList         *modules;
    char            *ext, *path, *cp, *mimeFile;
    char            *hostName, *actionProgram;
    int             fd, indent, flags, first, code;
#if BLD_FEATURE_LOG
    MprLogService   *logService;
    char            *logSpec;
#endif

    indent = 0;
    host = 0;
    defaultHost = (MaHost*) hosts.getFirst();

    fd = open(configFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0666);
    if (fd < 0) {
        mprLog(server, 0, "saveConfig: Can't open %s", configFile);
        return MPR_ERR_CANT_OPEN;
    }

    mprFprintf(fd, \
    "#\n"
    "#  Configuration for %s\n"
    "#\n"
    "#  This file is dynamically generated. If you edit this file, your\n"
    "#  changes may not be preserved by the manager. PLEASE keep a backup of\n"
    "#  the file before and after all manual changes.\n"
    "#\n"
    "#  The order of configuration directives matters as this file is parsed\n"
    "#  only once. You must put the server root and error log definitions\n"
    "#  first ensure configuration errors are logged.\n"
    "#\n\n", BLD_NAME);

    mprFprintf(fd, "ServerRoot \"%s\"\n", serverRoot);

#if BLD_FEATURE_LOG
    logService = mprGetMpr()->logService;
    logSpec = mprStrdup(logService->getLogSpec());
    if ((cp = strchr(logSpec, ':')) != 0) {
        *cp = '\0';
    }
    mprFprintf(fd, "ErrorLog \"%s\"\n", logSpec);
    mprFprintf(fd, "LogLevel %d\n", logService->getDefaultLevel());
#endif

    /*
     *  Listen directives
     */
    listen = (MaListen*) listens.getFirst();
    while (listen) {
        flags = listen->getFlags();
        if (flags & MA_LISTEN_DEFAULT_PORT) {
            mprFprintf(fd, "Listen %s # %d\n", listen->getIpAddr(), listen->getPort());
        } else if (flags & MA_LISTEN_WILD_IP) {
            mprFprintf(fd, "Listen %d\n", listen->getPort());
        } else if (flags & MA_LISTEN_WILD_IP2) {
            /*  Ignore */
        } else {
            if (strchr(listen->getIpAddr(), '.') != 0) {
                mprFprintf(fd, "Listen %s:%d\n", listen->getIpAddr(),
                    listen->getPort());
            } else {
                mprFprintf(fd, "Listen [%s]:%d\n", listen->getIpAddr(),
                    listen->getPort());
            }
        }
        listen = (MaListen*) listens.getNext(listen);
    }

    /*
     *  Global directives
     */
    mprFprintf(fd, "User %s\n", http->getUser());
    mprFprintf(fd, "Group %s\n", http->getGroup());

    mprFprintf(fd, "\n#\n#  Loadable Modules\n#\n");
    mprFprintf(fd, "LoadModulePath %s\n", defaultHost->getModuleDirs());
    modules = http->getModules();
    mp = (MaModule*) modules->getFirst();
    while (mp) {
        mprFprintf(fd, "LoadModule %s lib%sModule\n", mp->name, mp->name);
        mp = (MaModule*) modules->getNext(mp);
    }

    /*
     *  For clarity, always print the ThreadLimit even if default.
     */
    limits = http->getLimits();
    mprFprintf(fd, "ThreadLimit %d\n", limits->maxThreads);
    if (limits->threadStackSize != 0) {
        mprFprintf(fd, "ThreadStackSize %d\n", limits->threadStackSize);
    }
    if (limits->minThreads != 0) {
        mprFprintf(fd, "\nStartThreads %d\n", limits->minThreads);
    }
    if (limits->maxBody != MA_MAX_BODY) {
        mprFprintf(fd, "LimitRequestBody %d\n", limits->maxBody);
    }
    if (limits->maxResponseBody != MA_MAX_RESPONSE_BODY) {
        mprFprintf(fd, "LimitResponseBody %d\n", limits->maxResponseBody);
    }
    if (limits->maxNumHeaders != MA_MAX_NUM_HEADERS) {
        mprFprintf(fd, "LimitRequestFields %d\n", limits->maxNumHeaders);
    }
    if (limits->maxHeader != MA_MAX_HEADERS) {
        mprFprintf(fd, "LimitRequestFieldSize %d\n", limits->maxHeader);
    }
    if (limits->maxUrl != MPR_MAX_URL) {
        mprFprintf(fd, "LimitUrl %d\n", limits->maxUrl);
    }
    if (limits->maxUploadSize != MA_MAX_UPLOAD_SIZE) {
        mprFprintf(fd, "LimitUploadSize %d\n", limits->maxUploadSize);
    }

    /*
     *  Virtual hosts. The first host is the default server
     */
    host = (MaHost*) hosts.getFirst();
    while (host) {
        mprFprintf(fd, "\n");
        if (host->isVhost()) {
            if (host->isNamedVhost()) {
                mprFprintf(fd, "NameVirtualHost %s\n", host->getIpSpec());
            }
            mprFprintf(fd, "<VirtualHost %s>\n", host->getIpSpec());
            indent++;
        }

        hostName = host->getName();
        if (strcmp(hostName, "default") != 0) {
            tabs(fd, indent);
            mprFprintf(fd, "ServerName http://%s\n", hostName);
        }

        tabs(fd, indent);
        mprFprintf(fd, "DocumentRoot %s\n", host->getDocumentRoot());

        /*
         *  Handlers
         */
        flags = host->getFlags();
        if (flags & MA_ADD_HANDLER) {
            mprFprintf(fd, "\n");
            if (flags & MA_RESET_HANDLERS) {
                tabs(fd, indent);
                mprFprintf(fd, "ResetPipeline\n");
            }
            hanp = (MaStage*) host->getHandlers()->getFirst();
            while (hanp) {
                ext = (char*) (hanp->getExtensions() ?
                    hanp->getExtensions() : "");
                tabs(fd, indent);
                mprFprintf(fd, "AddHandler %s %s\n", hanp->getName(), ext);
                hanp = (MaStage*) host->getHandlers()->getNext(hanp);
            }
        }

#if BLD_FEATURE_SSL
        /*
         *  SSL configuration
         */
        if (host->isSecure()) {
            MaSslConfig *sslConfig;
            MaSslModule *sslModule;

            mprFprintf(fd, "\n");
            tabs(fd, indent);
            mprFprintf(fd, "SSLEngine on\n");
            sslModule = (MaSslModule*) http->findModule("ssl");
            if (sslModule != 0) {
                sslConfig = sslModule->getSslConfig(host->getName());
                if (sslConfig != 0) {

                    tabs(fd, indent);
                    mprFprintf(fd, "SSLCipherSuite %s\n",
                        sslConfig->getCipherSuite());

                    tabs(fd, indent);
                    mprFprintf(fd, "SSLProtocol ");
                    int protoMask = sslConfig->getSslProto();
                    if (protoMask == MA_PROTO_ALL) {
                        mprFprintf(fd, "ALL");
                    } else if (protoMask ==
                        (MA_PROTO_ALL & ~MA_PROTO_SSLV2)) {
                        mprFprintf(fd, "ALL -SSLV2");
                    } else {
                        if (protoMask & MA_PROTO_SSLV2) {
                            mprFprintf(fd, "SSLv2 ");
                        }
                        if (protoMask & MA_PROTO_SSLV3) {
                            mprFprintf(fd, "SSLv3 ");
                        }
                        if (protoMask & MA_PROTO_TLSV1) {
                            mprFprintf(fd, "TLSv1 ");
                        }
                    }
                    mprFprintf(fd, "\n");

                    if ((path = sslConfig->getCertFile()) != 0) {
                        tabs(fd, indent);
                        mprFprintf(fd, "SSLCertificateFile %s\n", path);
                    }
                    if ((path = sslConfig->getKeyFile()) != 0) {
                        tabs(fd, indent);
                        mprFprintf(fd, "SSLCertificateKeyFile %s\n", path);
                    }
                    if (sslConfig->getVerifyClient()) {
                        tabs(fd, indent);
                        mprFprintf(fd, "SSLVerifyClient require\n");
                        if ((path = sslConfig->getCaFile()) != 0) {
                            tabs(fd, indent);
                            mprFprintf(fd, "SSLCaCertificateFile %s\n", path);
                        }
                        if ((path = sslConfig->getCaPath()) != 0) {
                            tabs(fd, indent);
                            mprFprintf(fd, "SSLCertificatePath %s\n", path);
                        }
                    }
                }
            }
        }
#endif
        /*
         *  General per-host directives
         */
        if (! host->getKeepAlive()) {
            tabs(fd, indent);
            mprFprintf(fd, "KeepAlive off\n");
        } else {
            if (host->getMaxKeepAlive() != defaultHost->getMaxKeepAlive()) {
                tabs(fd, indent);
                mprFprintf(fd, "MaxKeepAliveRequests %d\n",
                    host->getMaxKeepAlive());
            }
            if (host->getKeepAliveTimeout() !=
                    defaultHost->getKeepAliveTimeout()) {
                tabs(fd, indent);
                mprFprintf(fd, "KeepAliveTimeout %d\n",
                    host->getKeepAliveTimeout() / 1000);
            }
        }
        mimeFile = host->getMimeFile();
        if (mimeFile && *mimeFile) {
            mprFprintf(fd, "TypesConfig %s\n", mimeFile);
        }
        if (host->getTimeout() != defaultHost->getTimeout()) {
            tabs(fd, indent);
            mprFprintf(fd, "Timeout %d\n", host->getTimeout() / 1000);
        }

        if (host->getSessionTimeout() != defaultHost->getSessionTimeout()) {
            tabs(fd, indent);
            mprFprintf(fd, "SessionTimeout %d\n", host->getSessionTimeout());
        }
#if BLD_FEATURE_ACCESS_LOG && !BLD_FEATURE_ROMFS
        if (host->getLogHost() == host) {
            char    format[MPR_MAX_FNAME * 2];
            char    *fp;
            fp = format;
            format[0] = '\0';
            for (cp = host->getLogFormat();
                    cp && *cp && fp < &format[sizeof(format) - 4]; cp++) {
                if (*cp == '\"') {
                    *fp++ = '\\';
                    *fp++ = *cp;
                } else {
                    *fp++ = *cp;
                }
            }
            *fp++ = '\0';
            tabs(fd, indent);
            mprFprintf(fd, "CustomLog %s \"%s\"\n", host->getLogPath(), format);
        }
#endif

        /*
         *  ActionPrograms. One mimeTypes table is shared among all hosts.
         */
        if (host == defaultHost) {
            mimeTypes = host->getMimeTypes();
            mt = (MaMimeHashEntry*) mimeTypes->getFirst();
            first = 1;
            while (mt) {
                actionProgram = mt->getActionProgram();
                if (actionProgram && *actionProgram) {
                    if (first) {
                        mprFprintf(fd, "\n");
                        first = 0;
                    }
                    tabs(fd, indent);
                    mprFprintf(fd, "Action %s %s\n", mt->getMimeType(),
                        mt->getActionProgram());
                }
                mt = (MaMimeHashEntry*) mimeTypes->getNext(mt);
            }
        }

        /*
         *  Aliases
         */
        aliases = host->getAliases();
        alias = (MaAlias*) aliases->getFirst();
        first = 1;
        while (alias) {
            /*
             *  Must skip the catchall alias which has an empty prefix
             */
            if (alias->getPrefix()[0] != '\0' && !alias->isInherited()) {
                if (first) {
                    mprFprintf(fd, "\n");
                    first = 0;
                }
                tabs(fd, indent);
                code = alias->getRedirectCode();
                if (code != 0) {
                    mprFprintf(fd, "Redirect %d %s \"%s\"\n", code, alias->getPrefix(), alias->getName());
                } else {
                    mprFprintf(fd, "Alias %s \"%s\"\n", alias->getPrefix(), alias->getName());
                }
            }
            alias = (MaAlias*) aliases->getNext(alias);
        }

        /*
         *  Directories -- Do in reverse order
         */
        defaultDp = dp = (MaDir*) host->getDirs()->getLast();
        first = 1;
        while (dp) {
            if (dp->isInherited()) {
                dp = (MaDir*) host->getDirs()->getPrev(dp);
                continue;
            }
            path = dp->getPath();
            if (*path) {
                if (!first) {
                    mprFprintf(fd, "\n");
                    tabs(fd, indent++);
                    mprFprintf(fd, "<Directory %s>\n", dp->getPath());
                }
            }
            if (strcmp(dp->getIndex(), defaultDp->getIndex()) != 0) {
                tabs(fd, indent);
                mprFprintf(fd, "DirectoryIndex %s\n", dp->getIndex());
            }

            printAuth(fd, host, dp, indent);

            if (*path && !first) {
                tabs(fd, --indent);
                mprFprintf(fd, "</Directory>\n");
            }
            first = 0;
            dp = (MaDir*) host->getDirs()->getPrev(dp);
        }

        /*
         *  Locations
         */
        loc = (MaLocation*) host->getLocations()->getLast();
        while (loc) {
            if (loc->isInherited()) {
                loc = (MaLocation*) host->getLocations()->getPrev(loc);
                continue;
            }
            mprFprintf(fd, "\n");
            tabs(fd, indent++);
            mprFprintf(fd, "<Location %s>\n", loc->getPrefix());

            if (loc->getHandlerName()) {
                tabs(fd, indent);
                mprFprintf(fd, "SetHandler %s\n", loc->getHandlerName());
            }
            printAuth(fd, host, loc, indent);

            tabs(fd, --indent);
            mprFprintf(fd, "</Location>\n");

            loc = (MaLocation*) host->getLocations()->getPrev(loc);
        }

        /*
         *  Close out the VirtualHosts
         */
        if (host->isVhost()) {
            tabs(fd, --indent);
            mprFprintf(fd, "</VirtualHost>\n");
        }
        host = (MaHost*) hosts.getNext(host);
    }
    close(fd);
    return 0;
}


/*
 *  Print Authorization configuration
 */
static void printAuth(int fd, MaHost *host, MaAuth *auth, int indent)
{
    MaAuthType      authType;
    MaAcl           acl;
    char            *users, *groups, *realm;
#if BLD_FEATURE_AUTH
    MaAuthHandler   *handler;
#endif

    if (auth->isAuthInherited()) {
        return;
    }

#if BLD_FEATURE_AUTH
    handler = (MaAuthHandler*) host->lookupHandler("auth");
    if (handler) {
        char    *path;
        path = handler->getGroupFile();
        if (path) {
            tabs(fd, indent);
            mprFprintf(fd, "AuthGroupFile %s\n", path);
        }
        path = handler->getUserFile();
        if (path) {
            tabs(fd, indent);
            mprFprintf(fd, "AuthUserFile %s\n", path);
        }
    }
#endif

    authType = auth->getType();
    if (authType == MA_AUTH_BASIC) {
        tabs(fd, indent);
        mprFprintf(fd, "AuthType basic\n");
    } else if (authType == MA_AUTH_DIGEST) {
        char *qop = auth->getQop();

        tabs(fd, indent);
        mprFprintf(fd, "AuthType digest\n");
        tabs(fd, indent);
        if (qop && *qop) {
            mprFprintf(fd, "AuthDigestQop %s\n", qop);
        }
    }

    realm = auth->getRealm();
    groups = auth->getRequiredGroups();
    users = auth->getRequiredUsers();
    acl = auth->getRequiredAcl();

    if (realm && *realm) {
        tabs(fd, indent);
        mprFprintf(fd, "AuthName \"%s\"\n", realm);
    }
    if (auth->getAnyValidUser()) {
        tabs(fd, indent);
        mprFprintf(fd, "Require valid-user\n");
    } else if (groups && *groups) {
        tabs(fd, indent);
        mprFprintf(fd, "Require group %s\n", groups);
    } else if (users && *users) {
        tabs(fd, indent);
        mprFprintf(fd, "Require user %s\n", users);
    } else if (acl) {
        tabs(fd, indent);
        mprFprintf(fd, "Require acl 0x%x\n", acl);
    }
}


static void tabs(int fd, int indent)
{
    for (int i = 0; i < indent; i++) {
        write(fd, "\t", 1);
    }
}


#endif  /* BLD_FEATURE_CONFIG_SAVE */

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
