/*
 *  location.c -- Implement Location directives.
 *
 *  Location directives provide authorization and handler matching based on URL prefixes.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

MaLocation *maCreateBareLocation(MprCtx ctx)
{
    MaLocation  *location;

    location = mprAllocObjZeroed(ctx, MaLocation);
    if (location == 0) {
        return 0;
    }
    location->errorDocuments = mprCreateHash(location, MA_ERROR_HASH_SIZE);
    location->handlers = mprCreateList(location);
    location->extensions = mprCreateHash(location, MA_HANDLER_HASH_SIZE);
    location->expires = mprCreateHash(location, MA_HANDLER_HASH_SIZE);
    location->inputStages = mprCreateList(location);
    location->outputStages = mprCreateList(location);
    location->prefix = mprStrdup(location, "");
    location->prefixLen = (int) strlen(location->prefix);
#if BLD_FEATURE_AUTH
    location->auth = maCreateAuth(location, 0);
#endif
    return location;
}


/*
 *  Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
MaLocation *maCreateLocation(MprCtx ctx, MaLocation *parent)
{
    MaLocation  *location;

    if (parent == 0) {
        return maCreateBareLocation(ctx);
    }
    location = mprAllocObjZeroed(ctx, MaLocation);
    if (location == 0) {
        return 0;
    }
    location->prefix = mprStrdup(location, parent->prefix);
    location->parent = parent;
    location->prefixLen = parent->prefixLen;
    location->flags = parent->flags;
    location->inputStages = parent->inputStages;
    location->outputStages = parent->outputStages;
    location->handlers = parent->handlers;
    location->extensions = parent->extensions;
    location->expires = parent->expires;
    location->connector = parent->connector;
    location->errorDocuments = parent->errorDocuments;
    location->sessionTimeout = parent->sessionTimeout;
#if BLD_FEATURE_EJS
    location->ejsPath = parent->ejsPath;
#endif
#if BLD_FEATURE_UPLOAD
    location->uploadDir = parent->uploadDir;
    location->autoDelete = parent->autoDelete;
#endif
#if BLD_FEATURE_SSL
    location->ssl = parent->ssl;
#endif
#if BLD_FEATURE_AUTH
    location->auth = maCreateAuth(location, parent->auth);
#endif
    return location;
}


void maFinalizeLocation(MaLocation *location)
{
#if BLD_FEATURE_SSL
    if (location->ssl) {
        mprConfigureSsl(location->ssl);
    }
#endif
}


void maSetLocationAuth(MaLocation *location, MaAuth *auth)
{
    location->auth = auth;
}


/*
 *  Add a handler. This adds a handler to the set of possible handlers for a set of file extensions.
 */
int maAddHandler(MaHttp *http, MaLocation *location, cchar *name, cchar *extensions)
{
    MaStage     *handler;
    char        *extlist, *word, *tok;

    mprAssert(location);
    
    if (mprGetParent(location->handlers) == location->parent) {
        location->extensions = mprCopyHash(location, location->parent->extensions);
        location->handlers = mprDupList(location, location->parent->handlers);
    }
    handler = maLookupStage(http, name);
    if (handler == 0) {
        mprError(http, "Can't find stage %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    if (extensions && *extensions) {
        /*
         *  Add to the handler extension hash
         */ 
        extlist = mprStrdup(location, extensions);
        word = mprStrTok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddHash(location->extensions, word, handler);
            word = mprStrTok(NULL, " \t\r\n", &tok);
        }
        mprFree(extlist);
        mprAddItem(location->handlers, handler);

    } else {
        if (handler->match == 0) {
            /*
             *  Only match by extension if the handler does not provide a match() routine
             */
            mprAddHash(location->extensions, "", handler);
        }
        mprAddItem(location->handlers, handler);
    }
    if (extensions && *extensions) {
        mprLog(location, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, extensions);
    } else {
        mprLog(location, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, location->prefix);
    }
    return 0;
}


/*
 *  Set a handler to universally apply to requests in this location block.
 */
int maSetHandler(MaHttp *http, MaHost *host, MaLocation *location, cchar *name)
{
    MaStage     *handler;

    mprAssert(location);
    
    if (mprGetParent(location->handlers) == location->parent) {
        location->extensions = mprCopyHash(location, location->parent->extensions);
        location->handlers = mprDupList(location, location->parent->handlers);
    }
    handler = maLookupStage(http, name);
    if (handler == 0) {
        mprError(http, "Can't find handler %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    location->handler = handler;
    mprLog(location, MPR_CONFIG, "SetHandler \"%s\" \"%s\", prefix %s", name, (host) ? host->name: "unknown", 
        location->prefix);
    return 0;
}


/*
 *  Add a filter. Direction defines what direction the stage filter be defined.
 */
int maAddFilter(MaHttp *http, MaLocation *location, cchar *name, cchar *extensions, int direction)
{
    MaStage     *stage;
    MaFilter    *filter;
    char        *extlist, *word, *tok;

    mprAssert(location);
    
    stage = maLookupStage(http, name);
    if (stage == 0) {
        mprError(http, "Can't find filter %s", name); 
        return MPR_ERR_NOT_FOUND;
    }

    filter = mprAllocObjZeroed(location, MaFilter);
    filter->stage = stage;

    if (extensions && *extensions) {
        filter->extensions = mprCreateHash(filter, 0);
        extlist = mprStrdup(location, extensions);
        word = mprStrTok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddHash(filter->extensions, word, filter);
            word = mprStrTok(0, " \t\r\n", &tok);
        }
        mprFree(extlist);
    }

    if (direction & MA_FILTER_INCOMING) {
        if (mprGetParent(location->inputStages) == location->parent) {
            location->inputStages = mprDupList(location, location->parent->inputStages);
        }
        mprAddItem(location->inputStages, filter);
    }
    if (direction & MA_FILTER_OUTGOING) {
        if (mprGetParent(location->outputStages) == location->parent) {
            location->outputStages = mprDupList(location, location->parent->outputStages);
        }
        mprAddItem(location->outputStages, filter);
    }

    if (extensions && *extensions) {
        mprLog(location, MPR_CONFIG, "Add filter \"%s\" to location \"%s\" for extensions \"%s\"", name, 
            location->prefix, extensions);
    } else {
        mprLog(location, MPR_CONFIG, "Add filter \"%s\" to location \"%s\" for all extensions", name, location->prefix);
    }

    return 0;
}


/*
 *  Set the network connector.
 */
int maSetConnector(MaHttp *http, MaLocation *location, cchar *name)
{
    MaStage     *stage;

    mprAssert(location);
    
    stage = maLookupStage(http, name);
    if (stage == 0) {
        mprError(http, "Can't find connector %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    location->connector = stage;
    mprLog(location, MPR_CONFIG, "Set connector \"%s\"", name);
    return 0;
}


void maAddLocationExpiry(MaLocation *location, MprTime when, cchar *mimeTypes)
{
    char    *types, *mime, *tok;

    if (mimeTypes && *mimeTypes) {
        if (mprGetParent(location->expires) == location->parent) {
            location->expires = mprCopyHash(location, location->parent->expires);
        }
        types = mprStrdup(location, mimeTypes);
        mime = mprStrTok(types, " ,\t\r\n", &tok);
        while (mime) {
            mprAddHash(location->expires, mime, ITOP(when));
            mime = mprStrTok(0, " \t\r\n", &tok);
        }
        mprFree(types);
    }
}


void maResetHandlers(MaLocation *location)
{
    if (mprGetParent(location->handlers) == location) {
        mprFree(location->handlers);
    }
    location->handlers = mprCreateList(location);
}


void maResetPipeline(MaLocation *location)
{
    if (mprGetParent(location->extensions) == location) {
        mprFree(location->extensions);
    }
    location->extensions = mprCreateHash(location, 0);
    
    if (mprGetParent(location->handlers) == location) {
        mprFree(location->handlers);
    }
    location->handlers = mprCreateList(location);
    
    if (mprGetParent(location->inputStages) == location) {
        mprFree(location->inputStages);
    }
    location->inputStages = mprCreateList(location);
    
    if (mprGetParent(location->outputStages) == location) {
        mprFree(location->outputStages);
    }
    location->outputStages = mprCreateList(location);
}


MaStage *maGetHandlerByExtension(MaLocation *location, cchar *ext)
{
    return (MaStage*) mprLookupHash(location->extensions, ext);
}


void maSetLocationPrefix(MaLocation *location, cchar *uri)
{
    mprAssert(location);

    mprFree(location->prefix);
    location->prefix = mprStrdup(location, uri);
    location->prefixLen = (int) strlen(location->prefix);
#if UNUSED
    /*
     *  Always strip trailing "/". Note this is a URI not a path.
     */
    if (location->prefixLen > 0 && location->prefix[location->prefixLen - 1] == '/') {
        location->prefix[--location->prefixLen] = '\0';
    }
#endif
}


void maSetLocationFlags(MaLocation *location, int flags)
{
    location->flags = flags;
}


void maAddErrorDocument(MaLocation *location, cchar *code, cchar *url)
{
    if (mprGetParent(location->errorDocuments) == location->parent) {
        location->errorDocuments = mprCopyHash(location, location->parent->errorDocuments);
    }
    mprAddHash(location->errorDocuments, code, mprStrdup(location, url));
}


cchar *maLookupErrorDocument(MaLocation *location, int code)
{
    char        numBuf[16];

    if (location->errorDocuments == 0) {
        return 0;
    }
    mprItoa(numBuf, sizeof(numBuf), code, 10);
    return (cchar*) mprLookupHash(location->errorDocuments, numBuf);
}


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
