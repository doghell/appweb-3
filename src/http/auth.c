/*
 *  auth.c - Generic authorization code
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_AUTH
/*********************************** Code *************************************/

MaAuth *maCreateAuth(MprCtx ctx, MaAuth *parent)
{
    MaAuth      *auth;

    auth = mprAllocObjZeroed(ctx, MaAuth);

    if (parent) {
        auth->allow = parent->allow;
        auth->anyValidUser = parent->anyValidUser;
        auth->type = parent->type;
        auth->deny = parent->deny;
        auth->method = parent->method;
        auth->flags = parent->flags;
        auth->order = parent->order;
        auth->qop = parent->qop;
        auth->requiredRealm = parent->requiredRealm;
        auth->requiredUsers = parent->requiredUsers;
        auth->requiredGroups = parent->requiredGroups;
#if BLD_FEATURE_AUTH_FILE
        auth->userFile = parent->userFile;
        auth->groupFile = parent->groupFile;
        auth->users = parent->users;
        auth->groups = parent->groups;
#endif

    } else{
#if BLD_FEATURE_AUTH_PAM
        auth->method = MA_AUTH_METHOD_PAM;
#elif BLD_FEATURE_AUTH_FILE
        auth->method = MA_AUTH_METHOD_FILE;
#endif
    }

    return auth;
}


void maSetAuthAllow(MaAuth *auth, cchar *allow)
{
    mprFree(auth->allow);
    auth->allow = mprStrdup(auth, allow);
}


void maSetAuthAnyValidUser(MaAuth *auth)
{
    auth->anyValidUser = 1;
    auth->flags |= MA_AUTH_REQUIRED;
}


void maSetAuthDeny(MaAuth *auth, cchar *deny)
{
    mprFree(auth->deny);
    auth->deny = mprStrdup(auth, deny);
}


void maSetAuthOrder(MaAuth *auth, int o)
{
    auth->order = o;
}


#else
void __dummyAuth() {}
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
