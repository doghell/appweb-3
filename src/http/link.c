/*
 *  link.c -- Link in static modules and initialize.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "appweb.h"

#if BLD_FEATURE_STATIC || (!BLD_APPWEB_PRODUCT && BLD_APPWEB_BUILTIN)
/*********************************** Locals ***********************************/

static MprModule *staticModules[64];        /* List of static modules */
static int maxStaticModules;                /* Max static modules */

/*********************************** Code *************************************/

void maLoadStaticModules(MaHttp *http)
{
    int     index = 0;

    staticModules[index] = 0;

#if BLD_FEATURE_AUTH
    staticModules[index++] = maAuthFilterInit(http, NULL);
#endif
#if BLD_FEATURE_CGI
    staticModules[index++] = maCgiHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_CHUNK
    staticModules[index++] = maChunkFilterInit(http, NULL);
#endif
#if BLD_FEATURE_DIR
    staticModules[index++] = maDirHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_EJS
    staticModules[index++] = maEjsHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_FILE
    staticModules[index++] = maFileHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_EGI
    staticModules[index++] = maEgiHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_SSL
    staticModules[index++] = maSslModuleInit(http, NULL);
#endif
#if BLD_FEATURE_PHP
    staticModules[index++] = maPhpHandlerInit(http, NULL);
#endif
#if BLD_FEATURE_RANGE
    staticModules[index++] = maRangeFilterInit(http, NULL);
#endif
#if BLD_FEATURE_UPLOAD
    staticModules[index++] = maUploadFilterInit(http, NULL);
#endif
#ifdef BLD_STATIC_MODULE
    staticModules[index++] = BLD_STATIC_MODULE(http, NULL);
#endif
    maxStaticModules = index;
}


void maUnloadStaticModules(MaHttp *http)
{
    int     i;

    for (i = 0; i < maxStaticModules; i++) {
        mprUnloadModule(staticModules[i]);
    }
}

#else
void maLoadStaticModules(MaHttp *http) {}
void maUnloadStaticModules(MaHttp *http) {}

#endif /* BLD_FEATURE_STATIC */

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
