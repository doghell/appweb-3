#
#  Makefile -- Binaries Makefile
#
# Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include 	.makedep

dependExtra:
	mkdir -p $(BLD_BIN_DIR)

cleanExtra:
	rm -f angel* cgi* http* makerom* testAppweb* appweb* ajs* *.out *.def *.exe *.lib *.dll *.dylib *.a *.so *.mod
	rm -f *.manifest *.exp *.pdb
	rm -f $(BLD_BIN_DIR)/ajs* $(BLD_BIN_DIR)/ajs* 
	rm -f $(BLD_BIN_DIR)/*.def $(BLD_BIN_DIR)/*.out $(BLD_BIN_DIR)/*.exe $(BLD_BIN_DIR)/*.lib 
	rm -f $(BLD_BIN_DIR)/*.dylib $(BLD_BIN_DIR)/*.a $(BLD_BIN_DIR)/*.so $(BLD_BIN_DIR)/*.mod
	[ "$(BUILD_CROSS)" = 1 ] && rm -f "./$(BLD_HOST_SYSTEM)/*" ; true

clobberExtra: cleanExtra
	[ "$(BUILD_CROSS)" = 1 ] && rm -fr "./$(BLD_HOST_SYSTEM)" ; true

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
