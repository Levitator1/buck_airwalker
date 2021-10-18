LIBUTIL_PATH = $(top_builddir)/../utillib/source/libutil.a
bin_PROGRAMS = baw
baw_SOURCES = main.cpp baw.cpp BawConfig.cpp state_file.cpp
baw_DEPENDENCIES = $(LIBUTIL_PATH)
AM_CPPFLAGS = -std=c++2a -Wno-subobject-linkage -I$(srcdir)/../../utillib/include/ -I$(srcdir)/../include/ -I$(srcdir)/../
LDADD = $(LIBUTIL_PATH) -lstdc++ -lpthread

# Phony so it will always get called and its own makefile decides whether building is needed
.PHONY: $(LIBUTIL_PATH)
$(LIBUTIL_PATH):
	make -j `nproc` -C `dirname $(LIBUTIL_PATH)`
