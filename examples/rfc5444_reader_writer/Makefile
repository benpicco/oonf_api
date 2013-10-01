####
#### Sample Makefile for building apps with the RIOT OS
####
#### The Sample Filesystem Layout is:
#### /this makefile
#### ../../RIOT 
#### ../../boards   for board definitions (if you have one or more)
#### 

# name of your project
export PROJECT := $(shell basename $(CURDIR))

# for easy switching of boards
ifeq ($(strip $(BOARD)),)
	export BOARD = native
endif

# this has to be the absolute path of the RIOT-base dir
export RIOTBASE =/home/benpicco/µkleos/riot/RIOT
export OONFBASE =../..
export RIOTLIBS =$(RIOTBASE)/../riot-libs

EXTERNAL_MODULES +=$(OONFBASE)
EXTERNAL_MODULES +=$(RIOTLIBS)
export EXTERNAL_MODULES

export CFLAGS = -DRIOT -DOONF_LOG_INFO -DOONF_LOG_DEBUG_INFO

## Modules to include. 

USEMODULE += auto_init
USEMODULE += config
USEMODULE += uart0
USEMODULE += posix

USEMODULE += net_help
USEMODULE += cunit
USEMODULE += compat_misc
USEMODULE += oonf_common
USEMODULE += oonf_rfc5444

export INCLUDES += -I$(RIOTBASE)/sys/include -I$(RIOTBASE)/drivers/include -I$(RIOTBASE)/sys/net/include \
		-I$(OONFBASE)/src-api -I$(OONFBASE)/examples -I$(RIOTLIBS)

include $(RIOTBASE)/Makefile.include
