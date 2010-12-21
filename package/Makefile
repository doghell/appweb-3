#
#	Makefile for the packages
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include 		.makedep 

PRE_DIRS		= $(BLD_HOST_OS)
BASE_NAME		:= "$(BLD_PRODUCT)-$(BLD_VERSION)-$(BLD_NUMBER)-$(BLD_HOST_DIST)-$(BLD_HOST_OS)-$(BLD_HOST_CPU)"
PACKS			:= binary dev source
PKG_DIR			:= `pwd`/../staging
INS_DIR 		:= c:/tmp/testInsAppweb

install install-%:
	packs="binary dev"; \
	[ "$$packs" = "binary dev" ] && BROWSER=--browser ; \
	priv OPTIONS=$(OPTIONS) QUIET=$(QUIET) TRACE=$(TRACE) $(BLD_TOP)/build/bin/makeInstall \
	$$BROWSER --root=$(ROOT_DIR) --install --with-conf="$(CONF)" --with-testweb pre $$packs post

uninstall uninstall-%:
	packs="binary dev" ; \
	priv OPTIONS=$(OPTIONS) QUIET=$(QUIET) TRACE=$(TRACE) $(BLD_TOP)/build/bin/makeInstall \
	--root=$(ROOT_DIR) --remove --with-testweb pre $$packs post
	
packageExtra:
	export OPTIONS=$(OPTIONS) TRACE=$(TRACE) ; \
	fakePriv "$(BLD_TOP)/build/bin/makeInstall --root=$(PKG_DIR) --with-testweb --package $(PACKS) && \
        $(BLD_TOOLS_DIR)/makePackage --root=$(PKG_DIR) $(PACKS)"

packageOnly:
	fakePriv $(BLD_TOOLS_DIR)/makePackage --root=$(PKG_DIR) $(PACKS)

test-install-package:
test-uninstall-package: 
test-install-native-package:
test-uninstall-native-package:
test-installed:
test-removed:

checkInstalled: installed-files

installed-files:
	( \
		for d in /etc /usr/src /usr/share /usr/share/doc /usr/lib /usr/lib/$(BLD_PRODUCT) /usr/include \
			/usr/local/bin /usr/local/bin/$(BLD_PRODUCT) /usr/bin /usr/bin/$(BLD_PRODUCT) \
			/usr/include/$(BLD_PRODUCT) /var/log /var/www ; \
		do \
			if [ "`echo $$d/$(BLD_PRODUCT)*`" != $$d/$(BLD_PRODUCT)'*' ] ; then \
				eval echo "$$d/$(BLD_PRODUCT)*" ; \
				continue ; \
			fi ; \
			if [ -x "$$d/$(BLD_PRODUCT)" ] ; then \
				echo "$$d/$(BLD_PRODUCT)" ; \
				continue ; \
			fi ; \
		done ; \
		for d in "$(BLD_CFG_PREFIX)" "$(BLD_DOC_PREFIX)" "$(BLD_INC_PREFIX)" "$(BLD_LIB_PREFIX)" "$(BLD_LOG_PREFIX)" \
			"$(BLD_SAM_PREFIX)" "$(BLD_SRC_PREFIX)" ; \
		do \
			[ -x "$$d" ] && echo "$$d" || true; \
		done ; \
	) | sort | uniq

dependExtra:
	[ "$(shell echo *.sh)" != "*.sh" ] && chmod +x *.sh ; true
	mkdir -p ../releases
