#
#	Makefile to build the all-in-one Appweb distribution
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include 		.makedep

COMPILE			= 
TARGETS			+= appweb.h appwebLib.c appweb.c angel.c

compileExtra:	$(TARGETS) dummy

APPWEB_HEADERS	+= $(patsubst %,../src/include/%.h, appweb appwebMonitor httpTune http)
LIB_SOURCES		+= $(shell find ../src/http -name '*.c') 

appweb.h: Makefile $(APPWEB_HEADERS)
	$(call log) "[Generate]" all/appweb.h
	echo '#include "appwebConfig.h"' >appweb.h
	cat $(APPWEB_HEADERS) | egrep -v '#include.*appweb|#include.*http' >>appweb.h

appwebLib.c: Makefile appweb.h $(LIB_SOURCES)
	$(call log) "[Generate]" all/appwebLib.c
	echo '#include "appweb.h"' >appwebLib.c
	all-in-one $(LIB_SOURCES) | egrep -v '#include.*appweb|#include.*http' >>appwebLib.c

appweb.c: Makefile appweb.h ../src/server/appweb.c
	$(call log) "[Generate]" all/appweb.c
	echo '#include "appweb.h"' >appweb.c
	all-in-one ../src/server/appweb.c | egrep -v '#include.*appweb|#include.*http' >>appweb.c

angel.c: Makefile ../src/server/UNIX/angel.c
	$(call log) "[Generate]" all/angel.c
	echo '#include "mpr.h"' >angel.c
	all-in-one ../src/server/UNIX/angel.c | egrep -v '#include.*appweb|#include.*http' >>angel.c

cleanExtra:
	rm -f *.c *.h
