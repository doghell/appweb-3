#
#	Makefile -- Documentation makefile
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include		.makedep

compileExtra: index.html

gen: index.html

index.html: $(BLD_TOP)/buildConfig.make Makefile
	cp version.html version.new
	sed 's/<p>.*<\/p>/<p>$(BLD_VERSION)<\/p>/' < version.new >version.html
	cp index.html index.new
	sed "s/Ejscript Documentation.*</Ejscript Documentation $(BLD_VERSION)</" < index.new >index.html
	rm -f index.new version.new

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
