
EXPORT_OBJECTS	:= 0
BUILD_CROSS		:= 0

include 		.makedep

compileFirst: 	Acme.mod

Acme.mod: Acme.es
	$(call setlibpath) ; ajsc --debug Acme.es
	$(call setlibpath) ; ajsmod --cslots Acme.mod
	$(call setlibpath) ; ajsmod --listing Acme.mod
	mv Acme.mod ..

compileExtra: Acme.mod Acme$(BLD_SHOBJ)

Acme$(BLD_SHOBJ) : $(OBJECTS) $(BLD_EJS_MAKEDEP) Acme.slots.h
	@bld --shared --library Acme --shared --search "$(BLD_LIB_DIR) $(BLD_LIB_PREFIX)" \
		--libs "$(BLD_EJS_LIBS)" $(OBJECTS)

cleanExtra:
	@rm -f *.lst *.mod *$(BLD_LIB)

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
