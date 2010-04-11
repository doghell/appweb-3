README.TXT for the Appweb Server.
--------------------------------------------------------------------------------

The Appweb server directory builds and packages the Appweb product. The 
source in this directory builds the appweb main program.

You can run Appweb by hand or use "make run" to run it. To debug on linux,
type "make debug". To debug on Windows, run Visual Studio and open 
../bin/Debug/appweb.exe.

Appweb can be configured via the command line or it can parse a configuration
file of directives.

To run appweb using a configuration file:

    ../bin/appweb -f appweb.conf

To run appweb via the command line:

    ../bin/appweb -a IP:PORT -d webDirectory

To display trace to the console:

    ../bin/appweb -f appweb.conf -l stdout:2


Windows
-------
On windows, Appweb can run in three modes. It can run as a windows service, 
as a graphical windows application or as a command line application. By
default, it is installed as a windows service. Once Appweb is installed, you
can use the system services applet under Control Panel -> administration tools
to start and stop the Appweb service.

Appweb Command Line Options
---------------------------
    appweb [-Abdkmv] [-a IP:PORT] [-d docRoot] [-f configFile] \
        [-l logSpec] [-r serverRootDir] [-t numThreads]

    Options:\n");
      -a IP:PORT     Address to listen on
      -A             Auto-scan for a free port
      -b             Run in background as a daemon
      -c             Suppress the console display on windows
      -d docRoot     Web directory (DocumentRoot)
      -f configFile  Configuration file name
      -k             Kill existing running http
      -l file:level  Log to file at verbosity level
      -r serverRoot  Alternate Home directory

    Debug options
      -D             Debug mode (no timeouts)
      -m             Output memory stats
      -t number      Use number of pool threads
      -v             Output version information

    Windows Service options
      -i option      Install appweb as a service. Use "-i default" to install 
                     Appweb with the default options "-b -c -f appweb.conf".
      -g             Go (start) service
      -s             Stop service
      -u             Uninstall service

Other important files to read:

Objects and libraries from these module directories are used when linking Appweb.

The key directories and files are:

Directory            Purpose
----------------------------
conf                 - appweb.conf files for various build configurations
logs                 - Directory to hold Appweb log files
package              - Directory for package file lists
test                 - Integration test 
web                  - Default web content

File                 Purpose
----------------------------
appweb.bmp           - Appweb bitmap
appweb.conf          - Appweb test configuration file
appweb.cpp           - Appweb (console) main program
appweb.dsp           - Visual Studio project to build appweb
appweb.h             - Appweb header included by user programs
appweb.ico           - Appweb icon
appweb.rc            - Appweb icon and menu resources
appwebStaticLink.c  - Source file to link builtin modules for appweb
ca.crt               - Test certificate authority certificate
ca.key               - Test certificate authority key
ca.key.pem           - Test certificate authority decrypted key
conf                 - Directory holding master appweb.config files
groups.db            - User access group file
logs                 - Directory for appweb log files
make.dep             - Makefile dependencies
Makefile             - Makefile to build appweb and winAppweb
mime.types           - Appweb mime types
openssl.ca           - Test OpenSSL certificate authority configuration
openssl.conf         - Test OpenSSL configuration
php.ini              - PHP initialiation file for the phpHandler
response.client      - Client responses to create test client certificate
response.server      - Server responses to create test server certificate
romFiles.cpp         - Dummy ROM source file of web content
server.crt           - Test server certificate
server.key           - Test server key
server.key.pem       - Test server decrypted key
test.urls            - Test URLs for httpClient
users.db             - Test user authoriation database file
web                  - Document root for wappweb

--------------------------------------------------------------------------------

Copyright (c) 2003-2010 Embedthis Software, LLC. All Rights Reserved.
Embedthis and Appweb are trademarks of Embedthis Software, LLC. Other 
brands and their products are trademarks of their respective holders.

See LICENSE.TXT for software license details.
