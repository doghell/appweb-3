#
# Makefile for the Appweb unit tests
#
# Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

BUILD_CROSS		:= 0
BUILD_DEPTH		?= 2
TEST_SYSTEM		:= 127.0.0.1:4100

include 		.makedep

CGIP			:= $(BLD_BIN_DIR)/cgiProgram$(BLD_EXE)

ifeq	($(BLD_FEATURE_TEST),1)
compileExtra: 	appweb.conf cgi-bin/testScript cgi-bin/cgiProgram$(BLD_EXE)

run:
	$(BLD_BIN_DIR)/appweb --log stdout:3 --config appweb.conf

testExtra:
ifeq	($(BLD_FEATURE_TEST),1)
ifeq	($(BLD_FEATURE_MULTITHREAD),1)
	mkdir -p web/tmp
	find . -name '*.mod' | xargs rm -f >/dev/null 2>&1 ; true
	if [ ! -f $(BLD_BIN_DIR)/ajs ] ; then \
		if ! which ejs >/dev/null ; then \
			$(call log) "[Test]" "Skip unit tests. Required \"ejs\" is installed." ; \
		else \
			ejs $(BLD_TOOLS_DIR)/utest -v ; \
		fi ; \
	else \
		$(BLD_BIN_DIR)/ajs $(BLD_TOOLS_DIR)/utest -v ; \
	fi
else
	$(call log) "[Notice]" "Test requires multithread configuration"
endif
endif

#
#	Update the shbang path in testScript. Must be an absolute path on some systems and for valgrind.
#
cgi-bin/testScript: $(CGIP) Makefile $(BLD_TOP)/src/include/buildConfig.h
	echo "#!$(BLD_ABS_BIN_DIR)/cgiProgram$(BLD_EXE)" >cgi-bin/testScript
	chmod +x cgi-bin/testScript

#
#	cgiProgram
#
cgi-bin/cgiProgram$(BLD_EXE): $(BLD_BIN_DIR)/cgiProgram$(BLD_EXE)
	cp $(CGIP)  cgi-bin/cgiProgram$(BLD_EXE)
	cp $(CGIP)  cgi-bin/nph-cgiProgram$(BLD_EXE)
	cp $(CGIP) 'cgi-bin/cgi Program$(BLD_EXE)'
	cp $(CGIP)  web/cgiProgram.cgi
	chmod +x cgi-bin/* web/cgiProgram.cgi

valgrindTest:
	if type valgrind >/dev/null 2>/dev/null ; then \
		$(call log) "[Test]" "Valgrind tests" ; \
		valgrind -q --tool=memcheck --suppressions=appweb.supp $(BLD_BIN_DIR)/appweb --log stdout:0 --timeout 20 ; \
	fi
endif

server: 
	$(BLD_BIN_DIR)/appweb --log stdout:1

first:
	iterations=4000 ; \
	set -e ; while : ; \
	do \
		set -x ; \
		utest -n -v -i 9999999 --threads 1 ; \
		set +x ; \
	done

second:
	iterations=4000 ; \
	set -e ; while : ; \
	do \
		set -x ; \
		utest -2 -v -i 9999999 --threads 8 ; \
		set +x ; \
	done

third:
	iterations=4000 ; \
	set -e ; while : ; \
	do \
		set -x ; \
		$(BLD_BIN_DIR)/http -v -t 1 -w 1 -i $$iterations -q $(TEST_SYSTEM)/index.html ; \
		$(BLD_BIN_DIR)/http -v -t 2 -w 2 -i $$iterations -q $(TEST_SYSTEM)/index.html ; \
		$(BLD_BIN_DIR)/http -v -t 4 -w 4 -i $$iterations -q $(TEST_SYSTEM)/index.html ; \
		$(BLD_BIN_DIR)/http -v -t 8 -w 8 -i $$iterations -q $(TEST_SYSTEM)/index.html ; \
		set +x ; \
	done

mem:
	ejs $(BLD_TOOLS_DIR)/memmon APPWEB

users.db:
	echo "# Make authorization databases"
	rm -f users.db groups.db
	@set -e ; \
	httpPassword -c -p pass1 users.db "Acme Inc" joshua ; \
	httpPassword -p pass2 users.db "Acme Inc" mary ; \
	httpPassword -p pass3 users.db "Coyote Corp" peter ; \
	httpPassword -p pass4 users.db "Coyote Corp" julie ; \
	echo "1: 1023: coyoteUsers: peter julie" >groups.db ; \
	echo "1: FFFF: acmeUsers: joshua mary" >>groups.db ; \
	echo "1: FFFF: acmeExec: mary" >>groups.db

scan:
	cd ~/src/nikto ; perl nikto.pl -h 127.0.0.1 -p 4010
	# cd ~/src/nikto ; perl nikto.pl -h 127.0.0.1 -p 4010,4011,4012,4443

scanUpdate:
	perl ~/src/nikto/nikto.pl -update

apacheBench:
	ab -k -c 10 -n 1000 http://127.0.0.1:4010/bench.html 

cleanExtra:
	rm -f *.obj *.pdb
	rm -f access.log error.log leak.log
	rm -f $(MODULES) $(BLD_BIN_DIR)/testAppweb$(BLD_EXE) testAppweb$(BLD_EXE)
	rm -f *.o *.lo *.obj *.out */*.mod */*/*.mod
	rm -f ../cgi-bin/*cgi* ../cgi-bin/testScript

#
#	Don't copy this. Ensure you set minimal permissions on upload and temp directories
#
dependExtra:
	mkdir -p web/upload web/tmp
	chmod 777 web/upload web/tmp
