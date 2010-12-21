#
#	Makefile for the http server library
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

PRE_DIRS		:= handlers filters connectors modules

LIBS			= appweb mpr
CGI				:= mod_cgi
FILE			:= mod_file
DIR				:= mod_dir
EGI				:= mod_egi
PHP				:= mod_php
UPLOAD			:= mod_upload
AUTH			:= mod_auth
CHUNK			:= mod_chunk
RANGE			:= mod_range
SSL				:= mod_ssl

include 		.makedep

TARGETS			+= $(BLD_LIB_DIR)/libappweb$(BLD_LIB)

ifeq ($(BLD_FEATURE_STATIC),1)
	SOURCES		= $(shell find . -name '*.c') 
else
	SOURCES		= $(shell ls *.c connectors/*.c handlers/passHandler.c) 
endif
OBJECTS			= $(patsubst %.c,$(BLD_OBJ_DIR)/%$(BLD_OBJ),$(notdir $(SOURCES)))

#
#	Order of modules matters. Dependencies must be earlier in the list.
#
ifeq	($(BLD_FEATURE_CGI),1)
	MODULES		+= $(BLD_MOD_DIR)/$(CGI)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_DIR),1)
	MODULES		+= $(BLD_MOD_DIR)/$(DIR)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_FILE),1)
	MODULES		+= $(BLD_MOD_DIR)/$(FILE)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_EGI),1)
	MODULES		+= $(BLD_MOD_DIR)/$(EGI)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_PHP),1)
	MODULES		+= $(BLD_MOD_DIR)/$(PHP)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_UPLOAD),1)
	MODULES		+= $(BLD_MOD_DIR)/$(UPLOAD)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_AUTH),1)
	MODULES		+= $(BLD_MOD_DIR)/$(AUTH)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_CHUNK),1)
	MODULES		+= $(BLD_MOD_DIR)/$(CHUNK)$(BLD_SHOBJ)
endif
ifeq	($(BLD_FEATURE_RANGE),1)
	MODULES		+= $(BLD_MOD_DIR)/$(RANGE)$(BLD_SHOBJ)
endif
ifeq ($(BLD_FEATURE_SSL),1)
	MODULES		+= $(BLD_MOD_DIR)/$(SSL)$(BLD_SHOBJ)
endif

compileExtra: $(TARGETS)

ifeq ($(BLD_FEATURE_STATIC),0)
compileFinal: $(MODULES)
endif

#
#	Build the appweb library
#
$(BLD_LIB_DIR)/libappweb$(BLD_LIB): Makefile $(OBJECTS) $(BLD_MPR_MAKEDEP)
	bld --library $(BLD_LIB_DIR)/libappweb --search "$(BLD_APPWEB_LIBPATHS)" --libs "$(BLD_APPWEB_WITHLIBS)" $(OBJECTS)

#
#	Build Modules -- Always build all modules -- even if statically linking
#
$(BLD_MOD_DIR)/$(CGI)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/cgiHandler$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(CGI) --libs "$(LIBS)" $(BLD_OBJ_DIR)/cgiHandler$(BLD_OBJ)

$(BLD_MOD_DIR)/$(DIR)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/dirHandler$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(DIR) --libs "$(LIBS)" $(BLD_OBJ_DIR)/dirHandler$(BLD_OBJ)

$(BLD_MOD_DIR)/$(EGI)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/egiHandler$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(EGI) --libs "$(LIBS)" $(BLD_OBJ_DIR)/egiHandler$(BLD_OBJ)

$(BLD_MOD_DIR)/$(FILE)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/fileHandler$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(FILE) --libs "$(LIBS)" $(BLD_OBJ_DIR)/fileHandler$(BLD_OBJ)

$(BLD_MOD_DIR)/$(PHP)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/phpHandler$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(PHP) --rpath "$(BLD_MOD_PREFIX)" \
		--search "$(BLD_PHP_LIBPATHS)" --libs "$(BLD_PHP_WITHLIBS) $(LIBS)" $(BLD_OBJ_DIR)/phpHandler$(BLD_OBJ)

$(BLD_MOD_DIR)/$(AUTH)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/authFilter$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(AUTH) --libs "$(LIBS)" $(BLD_OBJ_DIR)/authFilter$(BLD_OBJ)

$(BLD_MOD_DIR)/$(CHUNK)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/chunkFilter$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(CHUNK) --libs "$(LIBS)" $(BLD_OBJ_DIR)/chunkFilter$(BLD_OBJ)

$(BLD_MOD_DIR)/$(RANGE)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/rangeFilter$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(RANGE) --libs "$(LIBS)" $(BLD_OBJ_DIR)/rangeFilter$(BLD_OBJ)

$(BLD_MOD_DIR)/$(UPLOAD)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/uploadFilter$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(UPLOAD) --libs "$(LIBS)" $(BLD_OBJ_DIR)/uploadFilter$(BLD_OBJ)

$(BLD_MOD_DIR)/$(SSL)$(BLD_SHOBJ): $(BLD_OBJ_DIR)/sslModule$(BLD_OBJ) $(BLD_LIB_DIR)/libappweb$(BLD_LIB)
	bld --shared --library $(BLD_MOD_DIR)/$(SSL) --libs "appweb $(BLD_SSL_LIBS)" $(BLD_OBJ_DIR)/sslModule$(BLD_OBJ)

cleanExtra:
	rm -f $(MODULES)

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4
#
