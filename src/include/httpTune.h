/**
 *  httpTune.h - Tunable parameters for the Embedthis Http Web Server
 *
 *  See httpServer.dox for additional documentation.
 *
 *  Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Copyright **********************************/

/*
 *  @copy   default
 *
 *  Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
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
 *  details at: http: *www.embedthis.com/downloads/gplLicense.html
 *
 *  This program is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  This GPL license does NOT permit incorporating this software into
 *  proprietary programs. If you are unable to comply with the GPL, you must
 *  acquire a commercial license to use this software. Commercial licenses
 *  for this software and support services are available from Embedthis
 *  Software at http: *www.embedthis.com
 *
 *  @end
 */

/********************************* Includes ***********************************/

#ifndef _h_HTTP_TUNE
#define _h_HTTP_TUNE 1

/********************************** Defines ***********************************/

#define MA_SERVER_NAME              "Embedthis-Appweb/" BLD_VERSION
#define MA_SERVER_DEFAULT_PORT_NUM  80

/*
 *  These constants can be overridden in http.conf
 */
#if BLD_TUNE == MPR_TUNE_SIZE || DOXYGEN
    /*
     *  Tune for size
     */
    #define MA_REQ_MEM              ((1 * 1024 * 1024) - MPR_HEAP_OVERHEAD) /**< Initial virt memory arena size */
    #define MA_BUFSIZE              (4 * 1024)          /**< Default I/O buffer size */
    #define MA_MAX_STAGE_BUFFER     (4 * 1024)          /**< Max buffer for any stage */
    #define MA_MAX_IOVEC            16                  /**< Number of fragments in a single socket write */

    /*
     *  Limit defaults. Usually overridden in appweb.conf
     */
    #define MA_MAX_BODY             (64 * 1024)         /* Maximum incoming request content body size */
    #define MA_MAX_CHUNK_SIZE       (8 * 1024)          /* Max buffer for any stage */
    #define MA_MAX_HEADERS          2048                /* Max size of the headers */
    #define MA_MAX_NUM_HEADERS      20                  /* Max number of header lines */
    #define MA_MAX_RESPONSE_BODY    (128 * 1024 * 1024) /* Max buffer for generated data */
    #define MA_MAX_UPLOAD_SIZE      (10 * 1024 * 1024)  /* Max size of uploaded document */
    #define MA_MAX_PASS             64                  /**< Size of password */
    #define MA_MAX_SECRET           32                  /**< Number of random bytes to use */

#elif BLD_TUNE == MPR_TUNE_BALANCED
    /*
     *  Tune balancing speed and size
     */
    #define MA_REQ_MEM              ((2 * 1024 * 1024) - MPR_HEAP_OVERHEAD)
    #define MA_BUFSIZE              (4 * 1024)
    #define MA_MAX_STAGE_BUFFER     (32 * 1024)
    #define MA_MAX_IOVEC            24

    #define MA_MAX_BODY             (1024 * 1024)
    #define MA_MAX_CHUNK_SIZE       (8 * 1024)
    #define MA_MAX_HEADERS          (8 * 1024)
    #define MA_MAX_NUM_HEADERS      40
    #define MA_MAX_RESPONSE_BODY    (256 * 1024 * 1024)
    #define MA_MAX_UPLOAD_SIZE      0x7fffffff

    #define MA_MAX_PASS             128
    #define MA_MAX_SECRET           32
#else
    /*
     *  Tune for speed
     */
    #define MA_REQ_MEM              ((4 * 1024 * 1024) - MPR_HEAP_OVERHEAD)
    #define MA_BUFSIZE              (8 * 1024)
    #define MA_MAX_STAGE_BUFFER     (64 * 1024)
    #define MA_MAX_IOVEC            32

    #define MA_MAX_BODY             (1024 * 1024)
    #define MA_MAX_CHUNK_SIZE       (8 * 1024) 
    #define MA_MAX_HEADERS          (8 * 1024)
    #define MA_MAX_NUM_HEADERS      256
    #define MA_MAX_RESPONSE_BODY    0x7fffffff
    #define MA_MAX_UPLOAD_SIZE      0x7fffffff

    #define MA_MAX_PASS             128
    #define MA_MAX_SECRET           32
#endif


#define MA_MIN_PACKET           512             /**< Minimum packet size */
#define MA_PACKET_ALIGN(x)      (((x) + 0x3FF) & ~0x3FF)
#define MA_DEFAULT_MAX_THREADS  10              /**< Default number of threads */
#define MA_KEEP_TIMEOUT         60000           /**< Keep connection alive timeout */
#define MA_CGI_TIMEOUT          4000            /**< Time to wait to reap exit status */
#define MA_MAX_KEEP_ALIVE       100             /**< Default requests per TCP conn */
#define MA_TIMER_PERIOD         1000            /**< Timer checks ever 1 second */
#define MA_CGI_PERIOD           20              /**< CGI poll period (only for windows) */
#define MA_MAX_ACCESS_LOG       (20971520)      /**< Access file size (20 MB) */
#define MA_SERVER_TIMEOUT       (300 * 1000)
#define MA_MAX_CONFIG_DEPTH     (16)            /* Max nest of directives in config file */
#define MA_RANGE_BUFSIZE        (128)           /* Size of a range boundary */
#define MA_MAX_REWRITE          (10)            /* Maximum recursive URI rewrites */

/*
 *  Hash sizes (primes work best)
 */
#define MA_COOKIE_HASH_SIZE     11              /* Size of cookie hash */
#define MA_EGI_HASH_SIZE        31              /* Size of EGI hash */
#define MA_ERROR_HASH_SIZE      11              /* Size of error document hash */
#define MA_HEADER_HASH_SIZE     31              /* Size of header hash */
#define MA_MIME_HASH_SIZE       53              /* Mime type hash */
#define MA_VAR_HASH_SIZE        31              /* Size of query var hash */
#define MA_HANDLER_HASH_SIZE    17              /* Size of handler hash */
#define MA_ACTION_HASH_SIZE     13              /* Size of action program hash */

/*
 *  These constants are to sanity check user input in the http.conf
 */
#define MA_TOP_THREADS          100

#define MA_BOT_BODY             512
#define MA_TOP_BODY             (0x7fffffff)        /* 2 GB */

#define MA_BOT_CHUNK_SIZE       512
#define MA_TOP_CHUNK_SIZE       (4 * 1024 * 1024)   /* 4 MB */

#define MA_BOT_NUM_HEADERS      8
#define MA_TOP_NUM_HEADERS      (20 * 1024)

#define MA_BOT_HEADER           512
#define MA_TOP_HEADER           (20 * 1024 * 1024)

#define MA_BOT_URL              64
#define MA_TOP_URL              (255 * 1024 * 1024) /* 256 MB */

#define MA_BOT_RESPONSE_BODY    512
#define MA_TOP_RESPONSE_BODY    0x7fffffff          /* 2 GB */

#define MA_BOT_STACK            (16 * 1024)
#define MA_TOP_STACK            (4 * 1024 * 1024)

#define MA_BOT_STAGE_BUFFER     (2 * 1024)
#define MA_TOP_STAGE_BUFFER     (1 * 1024 * 1024)   /* 1 MB */

#define MA_BOT_UPLOAD_SIZE      1
#define MA_TOP_UPLOAD_SIZE      0x7fffffff          /* 2 GB */

#define MA_MAX_USER             MPR_HTTP_MAX_USER

#endif /* _h_HTTP_TUNE */


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
