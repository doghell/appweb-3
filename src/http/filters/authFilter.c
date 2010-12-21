/*
 *  authFilter.c - Authorization filter for basic and digest authentication.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_AUTH
/********************************** Defines ***********************************/
/*
 *  Per-request authorization data
 */
typedef struct AuthData 
{
    char            *password;          /* User password or digest */
    char            *userName;
#if BLD_FEATURE_AUTH_DIGEST
    char            *cnonce;
    char            *nc;
    char            *nonce;
    char            *opaque;
    char            *qop;
    char            *realm;
    char            *uri;
#endif
} AuthData;

/********************************** Forwards **********************************/

static void decodeBasicAuth(MaQueue *q);
static void formatAuthResponse(MaConn *conn, MaAuth *auth, int code, char *msg, char *logMsg);
static cchar *getPassword(MaConn *conn, cchar *realm, cchar *user);
static bool validateUserCredentials(MaConn *conn, cchar *realm, char *user, cchar *password, cchar *requiredPass, 
        char **msg);
#if BLD_FEATURE_AUTH_DIGEST
static int  decodeDigestDetails(MaQueue *q);
#endif

/*********************************** Code *************************************/
/*
 *  Open the authorization filter AND check if the request has the required authorization.
 *  This runs when the pipeline is created. 
 */
static void openAuth(MaQueue *q)
{
    MaConn      *conn;
    MaRequest   *req;
    MaAuth      *auth;
    AuthData    *ad;
    cchar       *requiredPassword;
    char        *url, *msg;
    int         actualAuthType;

    conn = q->conn;
    req = conn->request;
    url = req->url;
    auth = req->auth;

    if (auth == 0) {
        maFailRequest(conn, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, Authorization enabled.");
        return;
    }

    ad = q->queueData = mprAllocObjZeroed(q, AuthData);
    if (ad == 0) {
        return;
    }

    if (auth->type == 0) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, Authorization required.", 0);
        return;
    }
    if (req->authDetails == 0) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing authorization details.", 0);
        return;
    }

    if (mprStrcmpAnyCase(req->authType, "basic") == 0) {
        decodeBasicAuth(q);
        actualAuthType = MA_AUTH_BASIC;

#if BLD_FEATURE_AUTH_DIGEST
    } else if (mprStrcmpAnyCase(req->authType, "digest") == 0) {
        if (decodeDigestDetails(q) < 0) {
            maFailRequest(conn, 400, "Bad authorization header");
            return;
        }
        actualAuthType = MA_AUTH_DIGEST;
#endif
    } else {
        actualAuthType = MA_AUTH_UNKNOWN;
    }
    mprLog(q, 4, "run: type %d, url %s\nDetails %s\n", auth->type, req->url, req->authDetails);

    if (ad->userName == 0) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing user name.", 0);
        return;
    }

    if (auth->type != actualAuthType) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, Wrong authentication protocol.", 0);
        return;
    }

    /*
     *  Some backend methods can't return the password and will simply do everything in validateUserCredentials. 
     *  In this case, they and will return "". That is okay.
     */
    if ((requiredPassword = getPassword(conn, auth->requiredRealm, ad->userName)) == 0) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, "Access Denied, authentication error.", 
            "User not defined");
        return;
    }

#if BLD_FEATURE_AUTH_DIGEST
    if (auth->type == MA_AUTH_DIGEST) {
        char *requiredDigest;
        if (strcmp(ad->qop, auth->qop) != 0) {
            formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, 
                "Access Denied, Quality of protection does not match.", 0);
            return;
        }
        mprCalcDigest(req, &requiredDigest, 0, requiredPassword, ad->realm, req->url, ad->nonce, ad->qop, ad->nc, ad->cnonce,
            req->methodName);
        requiredPassword = requiredDigest;
    }
#endif
    if (!validateUserCredentials(conn, auth->requiredRealm, ad->userName, ad->password, requiredPassword, &msg)) {
        formatAuthResponse(conn, auth, MPR_HTTP_CODE_UNAUTHORIZED, 
            "Access denied, authentication error", msg);
    }
}


/*
 *  Validate the user credentials with the designated authorization backend method.
 */
static bool validateUserCredentials(MaConn *conn, cchar *realm, char *user, cchar *password, cchar *requiredPass, char **msg)
{
    MaAuth      *auth;

    auth = conn->request->auth;

    /*
     *  Use this funny code construct incase no backend method is configured. Still want the code to compile.
     */
    if (0) {
#if BLD_FEATURE_AUTH_FILE
    } else if (auth->method == MA_AUTH_METHOD_FILE) {
        return maValidateNativeCredentials(conn, realm, user, password, requiredPass, msg);
#endif
#if BLD_FEATURE_AUTH_PAM
    } else if (auth->method == MA_AUTH_METHOD_PAM) {
        return maValidatePamCredentials(conn, realm, user, password, NULL, msg);
#endif
    } else {
        *msg = "Required authorization backend method is not enabled or configured";
    }
    return 0;
}


/*
 *  Get the password (if the designated authorization backend method will give it to us)
 */
static cchar *getPassword(MaConn *conn, cchar *realm, cchar *user)
{
    MaAuth      *auth;

    auth = conn->request->auth;

    /*
     *  Use this funny code construct incase no backend method is configured. Still want the code to compile.
     */
    if (0) {
#if BLD_FEATURE_AUTH_FILE
    } else if (auth->method == MA_AUTH_METHOD_FILE) {
        return maGetNativePassword(conn, realm, user);
#endif
#if BLD_FEATURE_AUTH_PAM
    } else if (auth->method == MA_AUTH_METHOD_PAM) {
        return maGetPamPassword(conn, realm, user);
#endif
    }
    return 0;
}


/*
 *  Decode basic authorization details
 */
static void decodeBasicAuth(MaQueue *q)
{
    MaConn      *conn;
    MaRequest   *req;
    AuthData    *ad;
    char        decodedDetails[64], *cp;

    conn = q->conn;
    req = conn->request;
    ad = q->queueData;

    mprDecode64(decodedDetails, sizeof(decodedDetails), req->authDetails);
    if ((cp = strchr(decodedDetails, ':')) != 0) {
        *cp++ = '\0';
    }
    if (cp) {
        ad->userName = mprStrdup(req, decodedDetails);
        ad->password = mprStrdup(req, cp);

    } else {
        ad->userName = mprStrdup(req, "");
        ad->password = mprStrdup(req, "");
    }
    maSetRequestUser(conn, ad->userName);
}


#if BLD_FEATURE_AUTH_DIGEST
/*
 *  Decode the digest authentication details.
 */
static int decodeDigestDetails(MaQueue *q)
{
    MaConn      *conn;
    MaRequest   *req;
    AuthData    *ad;
    char        *authDetails, *value, *tok, *key, *dp, *sp;
    int         seenComma;

    ad = q->queueData;
    conn = q->conn;
    req = conn->request;

    key = authDetails = mprStrdup(q, req->authDetails);

    while (*key) {
        while (*key && isspace((int) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((int) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        *tok++ = '\0';

        while (isspace((int) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
                tok++;
            }
        } else {
            value = tok;
            while (*tok != ',' && *tok != '\0') {
                tok++;
            }
            seenComma++;
        }
        *tok++ = '\0';

        /*
         *  Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (dp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *dp++ = *sp++;
            }
            *dp = '\0';
        }

        /*
         *  username, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((int) *key)) {
        case 'a':
            if (mprStrcmpAnyCase(key, "algorithm") == 0) {
                break;
            } else if (mprStrcmpAnyCase(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (mprStrcmpAnyCase(key, "cnonce") == 0) {
                ad->cnonce = mprStrdup(q, value);
            }
            break;

        case 'd':
            if (mprStrcmpAnyCase(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (mprStrcmpAnyCase(key, "nc") == 0) {
                ad->nc = mprStrdup(q, value);
            } else if (mprStrcmpAnyCase(key, "nonce") == 0) {
                ad->nonce = mprStrdup(q, value);
            }
            break;

        case 'o':
            if (mprStrcmpAnyCase(key, "opaque") == 0) {
                ad->opaque = mprStrdup(q, value);
            }
            break;

        case 'q':
            if (mprStrcmpAnyCase(key, "qop") == 0) {
                ad->qop = mprStrdup(q, value);
            }
            break;

        case 'r':
            if (mprStrcmpAnyCase(key, "realm") == 0) {
                ad->realm = mprStrdup(q, value);
            } else if (mprStrcmpAnyCase(key, "response") == 0) {
                /* Store the response digest in the password field */
                ad->password = mprStrdup(q, value);
            }
            break;

        case 's':
            if (mprStrcmpAnyCase(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (mprStrcmpAnyCase(key, "uri") == 0) {
                ad->uri = mprStrdup(q, value);
            } else if (mprStrcmpAnyCase(key, "username") == 0) {
                ad->userName = mprStrdup(q, value);
            }
            break;

        default:
            /*  Just ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    mprFree(authDetails);
    if (ad->userName == 0 || ad->realm == 0 || ad->nonce == 0 || ad->uri == 0 || ad->password == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop && (ad->cnonce == 0 || ad->nc == 0)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop == 0) {
        ad->qop = mprStrdup(q, "");
    }

    maSetRequestUser(conn, ad->userName);
    return 0;
}
#endif


#if BLD_FEATURE_CONFIG_PARSE
/*
 *  Parse the appweb.conf directives for authorization
 */
static int parseAuth(MaHttp *http, cchar *key, char *value, MaConfigState *state)
{
    MaServer    *server;
    MaHost      *host;
    MaAuth      *auth;
    MaDir       *dir;
    MaAcl       acl;
    char        *path, *names, *tok, *type, *aclSpec;

    server = state->server;
    host = state->host;
    auth = state->auth;
    dir = state->dir;

    if (mprStrcmpAnyCase(key, "AuthGroupFile") == 0) {
        path = maMakePath(host, mprStrTrim(value, "\""));
        if (maReadGroupFile(server, auth, path) < 0) {
            mprError(http, "Can't open AuthGroupFile %s", path);
            return MPR_ERR_BAD_SYNTAX;
        }
        mprFree(path);
        return 1;

    } else if (mprStrcmpAnyCase(key, "AuthMethod") == 0) {
        value = mprStrTrim(value, "\"");
        if (mprStrcmpAnyCase(value, "pam") == 0) {
            auth->method = MA_AUTH_METHOD_PAM;
            return 1;

        } else if (mprStrcmpAnyCase(value, "file") == 0) {
            auth->method = MA_AUTH_METHOD_FILE;
            return 1;

        } else {
            return MPR_ERR_BAD_SYNTAX;
        }

    } else if (mprStrcmpAnyCase(key, "AuthName") == 0) {
        maSetAuthRealm(auth, mprStrTrim(value, "\""));
        return 1;
        
    } else if (mprStrcmpAnyCase(key, "AuthType") == 0) {
        value = mprStrTrim(value, "\"");
        if (mprStrcmpAnyCase(value, "Basic") == 0) {
            auth->type = MA_AUTH_BASIC;

#if BLD_FEATURE_AUTH_DIGEST
        } else if (mprStrcmpAnyCase(value, "Digest") == 0) {
            auth->type = MA_AUTH_DIGEST;
#endif

        } else {
            mprError(http, "Unsupported authorization protocol");
            return MPR_ERR_BAD_SYNTAX;
        }
        return 1;
        
    } else if (mprStrcmpAnyCase(key, "AuthUserFile") == 0) {
        path = maMakePath(host, mprStrTrim(value, "\""));
        if (maReadUserFile(server, auth, path) < 0) {
            mprError(http, "Can't open AuthUserFile %s", path);
            return MPR_ERR_BAD_SYNTAX;
        }
        mprFree(path);
        return 1;

#if BLD_FEATURE_AUTH_DIGEST
    } else if (mprStrcmpAnyCase(key, "AuthDigestQop") == 0) {
        value = mprStrTrim(value, "\"");
        mprStrLower(value);
        if (strcmp(value, "none") != 0 && strcmp(value, "auth") != 0 && strcmp(value, "auth-int") != 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        maSetAuthQop(auth, value);
        return 1;

    } else if (mprStrcmpAnyCase(key, "AuthDigestAlgorithm") == 0) {
        return 1;

    } else if (mprStrcmpAnyCase(key, "AuthDigestDomain") == 0) {
        return 1;

    } else if (mprStrcmpAnyCase(key, "AuthDigestNonceLifetime") == 0) {
        return 1;

#endif
    } else if (mprStrcmpAnyCase(key, "Require") == 0) {
        if (maGetConfigValue(http, &type, value, &tok, 1) < 0) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if (mprStrcmpAnyCase(type, "acl") == 0) {
            aclSpec = mprStrTrim(tok, "\"");
            acl = maParseAcl(auth, aclSpec);
            maSetRequiredAcl(auth, acl);

        } else if (mprStrcmpAnyCase(type, "valid-user") == 0) {
            maSetAuthAnyValidUser(auth);

        } else {
            names = mprStrTrim(tok, "\"");
            if (mprStrcmpAnyCase(type, "user") == 0) {
                maSetAuthRequiredUsers(auth, names);

            } else if (mprStrcmpAnyCase(type, "group") == 0) {
                maSetAuthRequiredGroups(auth, names);

            } else {
                mprError(http, "Bad Require syntax: %s", type);
                return MPR_ERR_BAD_SYNTAX;
            }
        }
        return 1;
    }
    return 0;
}
#endif


/*
 *  Format an authentication response. This is typically a 401 response code.
 */
static void formatAuthResponse(MaConn *conn, MaAuth *auth, int code, char *msg, char *logMsg)
{
    MaRequest       *req;
#if BLD_FEATURE_AUTH_DIGEST
    char            *qopClass, *nonceStr, *etag;
#endif

    req = conn->request;
    if (logMsg == 0) {
        logMsg = msg;
    }

    mprLog(conn, 3, "formatAuthResponse: code %d, %s\n", code, logMsg);

    if (auth->type == MA_AUTH_BASIC) {
        maSetHeader(conn, 0, "WWW-Authenticate", "Basic realm=\"%s\"", auth->requiredRealm);

#if BLD_FEATURE_AUTH_DIGEST
    } else if (auth->type == MA_AUTH_DIGEST) {

        qopClass = auth->qop;

        /*
         *  Use the etag as our opaque string
         */
        etag = conn->response->etag;
        if (etag == 0) {
            etag = "";
        }
        mprCalcDigestNonce(req, &nonceStr, conn->host->secret, etag, auth->requiredRealm);

        if (strcmp(qopClass, "auth") == 0) {
            maSetHeader(conn, 0, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->host->name, nonceStr, etag);

        } else if (strcmp(qopClass, "auth-int") == 0) {
            maSetHeader(conn, 0, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->host->name, nonceStr, etag);

        } else {
            maSetHeader(conn, 0, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", auth->requiredRealm, nonceStr);
        }
        mprFree(nonceStr);
#endif
    }

    maFailRequest(conn, code, "Authentication Error: %s", msg);
}


void maSetAuthQop(MaAuth *auth, cchar *qop)
{
    mprFree(auth->qop);
    if (strcmp(qop, "auth") == 0 || strcmp(qop, "auth-int") == 0) {
        auth->qop = mprStrdup(auth, qop);
    } else {
        auth->qop = mprStrdup(auth, "");
    }
}


void maSetAuthRealm(MaAuth *auth, cchar *realm)
{
    mprFree(auth->requiredRealm);
    auth->requiredRealm = mprStrdup(auth, realm);
}


void maSetAuthRequiredGroups(MaAuth *auth, cchar *groups)
{
    mprFree(auth->requiredGroups);
    auth->requiredGroups = mprStrdup(auth, groups);
    auth->flags |= MA_AUTH_REQUIRED;
}


void maSetAuthRequiredUsers(MaAuth *auth, cchar *users)
{
    mprFree(auth->requiredUsers);
    auth->requiredUsers = mprStrdup(auth, users);
    auth->flags |= MA_AUTH_REQUIRED;
}


/*
 *  Loadable module initialization
 */
MprModule *maAuthFilterInit(MaHttp *http, cchar *path)
{
    MprModule   *module;
    MaStage     *filter;

    module = mprCreateModule(http, "authFilter", BLD_VERSION, NULL, NULL, NULL);
    if (module == 0) {
        return 0;
    }

    filter = maCreateFilter(http, "authFilter", MA_STAGE_ALL);
    if (filter == 0) {
        mprFree(module);
        return 0;
    }
    http->authFilter = filter;
    filter->open = openAuth; 
    filter->parse = parseAuth; 
    return module;
}


#else
MprModule *maAuthFilterInit(MaHttp *http, cchar *path)
{
    return 0;
}
#endif /* BLD_FEATURE_AUTH */

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
