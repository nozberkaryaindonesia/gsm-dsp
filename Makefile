export DEVICETYPE := DSP

ifneq ("$(ComSpec)", "")
    ifeq ("$(OSTYPE)", "cygwin")
        DIRSEP ?=/
    else
        DIRSEP ?=\\
    endif
else
    DIRSEP ?= /
endif

include $(DSPLINK)$(DIRSEP)make$(DIRSEP)start.mk
