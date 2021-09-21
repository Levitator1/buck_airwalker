# Assume "build" is the build directory and launch the build
#
#

MAKECMD=make -C build

.PHONY:% all

all:
	$(MAKECMD)

%:
	$(MAKECMD) $@

