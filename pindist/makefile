#export PIN_ROOT=~/MA/pin-3.18-98332-gaebd7b1e6-gcc-linux
PIN_ROOT?=/opt/pin-3.18-98332-gaebd7b1e6-gcc-linux
ifdef PIN_ROOT
CONFIG_ROOT := $(PIN_ROOT)/source/tools/Config
else
CONFIG_ROOT := ../Config
endif
include $(CONFIG_ROOT)/makefile.config
include makefile.rules
include $(TOOLS_ROOT)/Config/makefile.default.rules

