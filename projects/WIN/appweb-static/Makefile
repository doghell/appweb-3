#
#	Makefile for Windows Visual Studio projects
#
#	Copyright (c) Embedthis Software LLC, 2003-2009. All Rights Reserved.
#

include     .makedep

# include		buildConfig.override

SRC			= ../../../src
MPR_HDR 	= $(BLD_TOP)/buildConfig.h $(shell find $(SRC)/include -name 'mpr*.h')
EJS_HDR 	= $(shell find $(SRC)/include -name 'ejs*.h')
EC_HDR  	= $(shell find $(SRC)/include -name 'ec*.h')
APP_HDR 	= $(SRC)/include/appweb.h $(SRC)/include/http.h
MPR_SRC 	= $(shell find $(SRC)/mpr -name '*.c' | egrep -v 'http.c|makerom.c')
EJS_SRC 	= $(SRC)/ejs/ejsLib.c $(SRC)/ejs/sqlite3.c
EC_SRC 		= $(SRC)/ejs/ecLib.c
APPWEB_SRC 	= $(shell find $(SRC)/http -name '*.c' | egrep -v 'XXXXX.c')
LIBS		= ws2_32.lib
PROJECTS	= appweb.vcproj angel.vcproj appwebMonitor.vcproj ajsmod.vcproj ajsc.vcproj ajs.vcproj ajssql.vcproj \
				http.vcproj httpPassword.vcproj

MAKE_IFLAGS	+= -I$(BLD_TOP)/src/include/master $(BLD_REGEXP_IFLAGS) $(BLD_MPR_IFLAGS)
ifeq ($(BLD_FEATURE_OPENSSL),1)
	MAKE_IFLAGS	+= $(BLD_OPENSSL_IFLAGS)
endif
ifeq ($(BLD_FEATURE_MATRIXSSL),1)
	MAKE_IFLAGS	+= $(BLD_MATRIXSSL_IFLAGS)
endif
ifeq ($(BLD_FEATURE_PHP),1)
	MAKE_IFLAGS	+= $(BLD_PHP_IFLAGS)
endif
MAKE_DFLAGS	+= -DSTATIC_OVERRIDE=1

compileExtra: prep $(PROJECTS)

prep:
	$(call log) "[Generate]" "appweb-static VS project"

appweb.vcproj: Makefile buildConfig.override
	@makeVsProj --project appweb.vcproj --exe appweb.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(EJS_HDR) $(APP_HDR) $(EJS_SRC) $(MPR_SRC) $(APPWEB_SRC) \
		$(SRC)/server/appweb.c $(SRC)/ejs/ejsAppweb.c

angel.vcproj: Makefile buildConfig.override
	@makeVsProj --project angel.vcproj --graphical --exe angel.exe --search "$(BLD_EJS_LIBPATHS)" \
		--dflags "$(DFLAGS)" --libs "$(LIBS)" $(MPR_HDR) $(APP_HDR) $(MPR_SRC) \
		$(SRC)/server/WIN/angel.c

appwebMonitor.vcproj: Makefile buildConfig.override
	@makeVsProj --project appwebMonitor.vcproj --graphical --exe appwebMonitor.exe --search "$(BLD_EJS_LIBPATHS)" \
		--dflags "$(DFLAGS)" --libs "$(LIBS)" $(MPR_HDR) $(APP_HDR) $(MPR_SRC) \
        $(SRC)/include/appwebMonitor.h $(SRC)/server/WIN/appwebMonitor.c

ajs.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajs.vcproj --exe ajs.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(EJS_HDR) $(EC_SRC) $(EJS_SRC) $(MPR_SRC) $(SRC)/ejs/ejs.c

ajsmod.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajsmod.vcproj --exe ajsmod.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(EJS_HDR) $(EC_HDR) $(EC_SRC) $(EJS_SRC) $(MPR_SRC) $(SRC)/ejs/ejsmod.c

ajsc.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajsc.vcproj --exe ajsc.exe --search "$(BLD_EJS_LIBPATHS)" --dflags "$(DFLAGS)" \
		--custom ajsc.custom --libs "$(LIBS)" $(MPR_HDR) $(EJS_HDR) $(EC_HDR) $(EC_SRC) $(EJS_SRC) $(MPR_SRC) \
		$(SRC)/ejs/ejsc.c

ajssql.vcproj: Makefile buildConfig.override
	@makeVsProj --project ajssql.vcproj --exe ajssql.exe --search "$(BLD_MPR_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(MPR_SRC) $(SRC)/ejs/ejssql.c $(SRC)/ejs/sqlite3.c

http.vcproj: Makefile buildConfig.override
	@makeVsProj --project http.vcproj --exe http.exe --search "$(BLD_MPR_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(MPR_SRC) $(SRC)/mpr/http.c

httpPassword.vcproj: Makefile buildConfig.override
	@makeVsProj --project httpPassword.vcproj --exe httpPassword.exe --search "$(BLD_MPR_LIBPATHS)" --dflags "$(DFLAGS)" \
		--libs "$(LIBS)" $(MPR_HDR) $(MPR_SRC) $(SRC)/utils/httpPassword.c

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
