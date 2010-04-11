#
#	Makefile for the Appweb source code.
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

include 		.makedep

PRE_DIRS		+= include
ifeq ($(BLD_MPR_DIR),)
	PRE_DIRS	+= mpr
endif
ifeq ($(BLD_FEATURE_EJS),1)
ifeq ($(BLD_EJS_DIR),)
	PRE_DIRS	+= ejs 
endif
endif
PRE_DIRS		+= http 

POST_DIRS		+= utils server
ifeq ($(BLD_FEATURE_TEST),1)
	POST_DIRS	+= test
endif

compileExtra:
ifeq ($(BLD_FEATURE_STATIC),0)
	@T=modules D=ejs ; $(DO_RECURSE)
endif
