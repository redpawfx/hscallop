OS_NAME := $(shell uname -s)
SOURCES = sop_scallop.cpp
ICONS = icon/SOP_path_deform.svg
INSTDIR = $(HOME)/houdini15.0

ifeq ($(OS_NAME),Darwin)
	DSONAME = sop_pathdeform.dylib
	OPTIMIZER = -O1
else
	DSONAME = sop_scallop.so
	OPTIMIZER = -O2
endif
# OPTIMIZER = -g

include $(HFS)/toolkit/makefiles/Makefile.gnu

all:	install clean
install:	default	icons clean
	@if [ ! -d $(INSTDIR)/dso ]; then mkdir $(INSTDIR)/dso; fi
	@mv $(DSONAME) $(INSTDIR)/dso
clean:
	@rm *.o
