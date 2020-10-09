#
# Component Makefile
#

# Set simple includes as default
ifndef CONFIG_VAR
CFLAGS += -DCONFIG_VAR
endif

COMPONENT_SRCDIRS := md5/ 
COMPONENT_ADD_INCLUDEDIRS := $(COMPONENT_SRCDIRS) .