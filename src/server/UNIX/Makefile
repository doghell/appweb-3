#
# Makefile for the UNIX Appweb server components
#
# Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include 		.makedep 

MAKE_IFLAGS		:= -I$(BLD_TOP)/include

ifeq ($(BLD_FEATURE_ANGEL),1)
ifeq ($(BUILD_CROSS_OR_COMPLETE_NATIVE),1)
	TARGETS		+= $(BLD_BIN_DIR)/angel$(BLD_EXE)
endif
endif

compileExtra: 	$(TARGETS) 

$(BLD_BIN_DIR)/angel$(BLD_EXE): $(OBJECTS) $(BLD_MPR_MAKEDEP)
	@bld --exe $(BLD_BIN_DIR)/angel$(BLD_EXE) --search "$(BLD_MPR_LIBPATHS)" --libs "$(BLD_MPR_LIBS)" $(OBJECTS)

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
