#
# to use oonf_api in your RIOT project,
# add the following to your Makefile:
#
#	export OONFBASE   = /path/to/this/directory
#	EXTERNAL_MODULES += $(OONFBASE)
#
# this provides the following modules:
#	oonf_common	-	avl tree, list, netaddr, regex, string functions
#	oonf_rfc5444	-	packetBB implementation, requires oonf_common

ifneq (,$(findstring oonf_common,$(USEMODULE)))
	DIRS += src-api/common
endif
ifneq (,$(findstring oonf_rfc5444,$(USEMODULE)))
	DIRS += src-api/rfc5444
endif
ifneq (,$(findstring cunit,$(USEMODULE)))
	DIRS += tests/cunit
endif

all:
	mkdir -p $(BINDIR)
	@for i in $(DIRS) ; do $(MAKE) -C $$i ; done ;

clean:
