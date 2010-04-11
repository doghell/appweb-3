/*
 *	simpleEgi.c - Demonstrate the use of the Embedded Gateway Interface (EGI) 
 *			in a simple multi-threaded application.
 *
 *  Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */
 
/******************************* Includes *****************************/

#include	"appweb.h"

/********************************* Code *******************************/
/*
 *	This method is run when the EGI form is called from the web
 *	page. Rq is the request context. URI is the bare URL minus query.
 *	Query is the string after a "?" in the URL. Post data is posted
 *	HTTP form data.
 */

static void myEgi(MaQueue *q)
{
	MaConn	*conn;

	conn = q->conn;

	maSetResponseCode(conn, 200);
	maWrite(q, "<HTML><TITLE>simpleEgi</TITLE><BODY>\r\n");
	maWrite(q, "<p>Name: %s</p>\n", maGetFormVar(conn, "name", "-"));
	maWrite(q, "<p>Address: %s</p>\n", maGetFormVar(conn, "address", "-"));
	maWrite(q, "</BODY></HTML>\r\n");

#if POSSIBLE
	/*
	 *	Useful things to do in egi forms
	 */
	maSetResponseCode(conn, 200);
	maSetResponseMimeType(conn, "text/plain");
	maDontCacheResponse(conn);
	maRedirect(conn, 302, "/myURl");
	maFailRequest(conn, 409, "My message : %d", 5);
#endif
}


/*
 *	Create a simple stand-alone web server
 */
MAIN(simpleEgi, int argc, char **argv)
{
	MaHttp		*http;

	if ((http = maCreateWebServer("simpleEgi.conf")) == 0) {
        return MPR_ERR_CANT_CREATE;
    }

	/*
	 *	Define our EGI form
	 */
	maDefineEgiForm(http, "/myEgi.egi", myEgi);
	
	if (maServiceWebServer(http) < 0) {
        return MPR_ERR_CANT_CREATE;
	}
	mprFree(http);
	return 0;
}


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
