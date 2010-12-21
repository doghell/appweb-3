#
# 	Makefile for the Appweb unit test source code
#
# Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include 		.makedep

TARGETS			:= $(BLD_BIN_DIR)/testAppweb$(BLD_EXE) 

compileExtra: 	$(TARGETS)

#
#	Build testAppweb
#
$(BLD_BIN_DIR)/testAppweb$(BLD_EXE): $(OBJECTS) $(BLD_LIB_DIR)/libmpr$(BLD_LIB)
	bld --exe $(BLD_BIN_DIR)/testAppweb$(BLD_EXE) --libs "$(BLD_MPR_LIBS) $(BLD_SSL_LIBS)" $(OBJECTS)


#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4
#
