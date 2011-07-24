#
#	Makefile for Windows Visual Studio projects
#
#	Copyright (c) Embedthis Software LLC, 2003-2009. All Rights Reserved.
#

include     .makedep
include     buildConfig.override

SRC			= ../../../src
MPR_HDR 	= $(BLD_TOP)/buildConfig.h $(shell find $(SRC)/include -name 'mpr*.h')
EJS_HDR 	= $(shell find $(SRC)/include -name 'ejs*.h')
EC_HDR  	= $(shell find $(SRC)/include -name 'ec*.h')
APPWEB_HDR 	= $(SRC)/include/appweb.h $(SRC)/include/http.h
MPR_SRC		= $(SRC)/mpr/mprLib.c
APPWEB_SRC	= $(SRC)/http/handlers/passHandler.c $(shell find $(SRC)/http -name '*.c' | \
				egrep -v 'filters|handlers|modules')
LIBS		= ws2_32.lib
PROJECTS	= libmpr.vcproj libsqlite3.vcproj libmprssl.vcproj libpcre.vcproj libajs.vcproj \
			  libac.vcproj libappweb.vcproj \
			  appweb.vcproj angel.vcproj appwebMonitor.vcproj ajsmod.vcproj ajsc.vcproj ajs.vcproj ajssql.vcproj \
			  http.vcproj httpPassword.vcproj \
			  mod_auth.vcproj mod_cgi.vcproj mod_chunk.vcproj mod_dir.vcproj mod_egi.vcproj mod_ejs.vcproj \
              mod_file.vcproj mod_php.vcproj mod_range.vcproj mod_ssl.vcproj mod_upload.vcproj

MAKE_IFLAGS	= $(BLD_REGEXP_IFLAGS) $(BLD_MPR_IFLAGS)
ifeq ($(BLD_FEATURE_OPENSSL),1)
	SSL_IFLAGS	+= $(BLD_OPENSSL_IFLAGS)
endif
ifeq ($(BLD_FEATURE_MATRIXSSL),1)
	SSL_IFLAGS	+= $(BLD_MATRIXSSL_IFLAGS)
endif
ifeq ($(BLD_FEATURE_PHP),1)
	PHP_IFLAGS	+= $(BLD_PHP_IFLAGS)
endif

compileExtra: prep $(PROJECTS)

prep:
	$(call log) "[Generate]" "appweb-dynamic VS project"
	if ls ../../../bin/*.def >/dev/null 2>&1 ; then \
		cp ../../../bin/*.def . ; \
	fi

libmpr.vcproj: Makefile buildConfig.override
	@makeVsProj --project libmpr.vcproj --library mpr --dflags "$(DFLAGS)" \
		--def libmpr.def --libs "$(LIBS)" $(MPR_HDR) $(MPR_SRC)
	
libmprssl.vcproj: Makefile buildConfig.override
	@makeVsProj --project libmprssl.vcproj --library mprssl --dflags "$(DFLAGS)" \
		--iflags "$(SSL_IFLAGS)" --def libmprssl.def --libs "libmpr.lib $(LIBS)" \
		$(MPR_HDR) $(SRC)/mpr/mprSsl.c
	
libpcre.vcproj: Makefile buildConfig.override
	@makeVsProj --project libpcre.vcproj --library pcre --dflags "$(DFLAGS)" \
		--def libpcre.def --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(SRC)/mpr/mprPcre.c
	
libsqlite3.vcproj: Makefile buildConfig.override
	@makeVsProj --project libsqlite3.vcproj --library sqlite3 --dflags "$(DFLAGS)" \
		--def libsqlite3.def --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(SRC)/ejs/sqlite3.c
	
#
#	Ejscript core library
#
libajs.vcproj: Makefile buildConfig.override
	@makeVsProj --project libajs.vcproj --library ajs --dflags "$(DFLAGS)" \
		--def libajs.def --libs "libmpr.lib libsqlite3.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(SRC)/ejs/ejsLib.c
	
#
#	Ejscript compiler library
#
libac.vcproj: Makefile buildConfig.override
	@makeVsProj --project libac.vcproj --library ac --dflags "$(DFLAGS)" \
		--def libac.def --libs "libmpr.lib libsqlite3.lib libajs.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(SRC)/ejs/ecLib.c

libappweb.vcproj: Makefile buildConfig.override
	@makeVsProj --project libappweb.vcproj --library appweb --dflags "$(DFLAGS)" \
		--def libappweb.def --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(APPWEB_HDR) $(APPWEB_SRC)

#
#	Modules
#
mod_auth.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_auth.vcproj --library auth --dflags "$(DFLAGS)" \
		--def mod_auth.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/filters/authFilter.c

mod_cgi.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_cgi.vcproj --library cgi --dflags "$(DFLAGS)" \
		--def mod_cgi.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/handlers/cgiHandler.c

mod_chunk.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_chunk.vcproj --library chunk --dflags "$(DFLAGS)" \
		--def mod_chunk.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/filters/chunkFilter.c

mod_dir.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_dir.vcproj --library dir --dflags "$(DFLAGS)" \
		--def mod_dir.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/handlers/dirHandler.c

mod_egi.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_egi.vcproj --library egi --dflags "$(DFLAGS)" \
		--def mod_egi.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/handlers/egiHandler.c

mod_ejs.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_ejs.vcproj --library ejs --dflags "$(DFLAGS)" \
		--def mod_ejs.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/ejs/ejsAppweb.c

mod_file.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_file.vcproj --library file --dflags "$(DFLAGS)" \
		--def mod_file.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/handlers/fileHandler.c

mod_php.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_php.vcproj --library php --dflags "$(DFLAGS)" \
		--iflags "$(PHP_IFLAGS)" --def mod_php.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/handlers/phpHandler.c

mod_range.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_range.vcproj --library range --dflags "$(DFLAGS)" \
		--def mod_range.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/filters/rangeFilter.c

mod_ssl.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_ssl.vcproj --library ssl --dflags "$(DFLAGS)" \
		--def mod_ssl.def --libs "libmpr.lib libmprssl.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/modules/sslModule.c

mod_upload.vcproj: Makefile buildConfig.override
	@makeVsProj --project mod_upload.vcproj --library upload --dflags "$(DFLAGS)" \
		--def mod_upload.def --libs "libmpr.lib libsqlite3.lib libajs.lib libappweb.lib $(LIBS)" \
		$(MPR_HDR) $(EJS_HDR) $(SRC)/http/filters/uploadFilter.c

#
#	Executables
#
appweb.vcproj: Makefile buildConfig.override
	@makeVsProj --project appweb.vcproj --exe appweb.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "libmpr.lib libappweb.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(APPWEB_HDR) $(SRC)/server/appweb.c

angel.vcproj: Makefile buildConfig.override
	@makeVsProj --project angel.vcproj --graphical --exe angel.exe --search "$(BLD_EJS_LIBPATHS)" \
		--dflags "$(DFLAGS)" --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(APPWEB_HDR) $(SRC)/server/WIN/angel.c

appwebMonitor.vcproj: Makefile buildConfig.override
	@makeVsProj --project appwebMonitor.vcproj --graphical --exe appwebMonitor.exe --search "$(BLD_EJS_LIBPATHS)" \
		--resources appweb.rc \
		--dflags "$(DFLAGS)" --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(APPWEB_HDR) \
        $(SRC)/include/appwebMonitor.h $(SRC)/server/WIN/appwebMonitor.c

ajs.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajs.vcproj --exe ajs.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "libmpr.lib libajs.lib libac.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(SRC)/ejs/ejs.c

ajsmod.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajsmod.vcproj --exe ajsmod.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "libmpr.lib libajs.lib libac.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(EC_HDR) \
		$(EC_SRC) $(SRC)/ejs/ejsmod.c

ajsc.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajsc.vcproj --exe ajsc.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--custom ajsc.custom --libs "libmpr.lib libajs.lib libac.lib $(LIBS)" $(MPR_HDR) $(EJS_HDR) $(EC_HDR) \
		$(EC_SRC) $(SRC)/ejs/ejsc.c

ajssql.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajssql.vcproj --exe ajssql.exe --search "$(BLD_MPR_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(MPR_SRC) $(SRC)/ejs/ejssql.c $(SRC)/ejs/sqlite3.c

http.vcproj: Makefile buildConfig.override
	@makeVsProj --project http.vcproj --exe http.exe --search "$(BLD_MPR_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(SRC)/mpr/http.c

httpPassword.vcproj: Makefile buildConfig.override
	@makeVsProj --project httpPassword.vcproj --exe httpPassword.exe --search "$(BLD_MPR_LIBPATHS)" \
        --dflags "$(DFLAGS)" --libs "libmpr.lib $(LIBS)" $(MPR_HDR) $(SRC)/utils/httpPassword.c

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
