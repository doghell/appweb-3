#
#	Makefile for the simpleClient sample
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

NAME			:= simpleClient

include 		.makedep

TARGETS			+= $(NAME)$(BLD_EXE)

compileExtra: 	$(TARGETS)

$(NAME)$(BLD_EXE): $(OBJECTS)
	bld --exe $(NAME)$(BLD_EXE) --libs "$(BLD_MPR_LIBS)" $(OBJECTS)

run:
	./$(NAME)$(BLD_EXE)

projectExtra:
	@makeVsProj --project client.vcproj --exe client.exe --libs "$(BLD_MPR_LIBS)" *.c

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
