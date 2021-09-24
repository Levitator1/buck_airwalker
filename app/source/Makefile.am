LIBUTIL_PATH = $(top_builddir)/../utillib/source/libutil.a
bin_PROGRAMS = baw
baw_SOURCES = main.cpp baw.cpp
baw_DEPENDENCIES = $(LIBUTIL_PATH)
AM_CPPFLAGS = -std=c++2a -I$(srcdir)/../include/
LDADD = $(LIBUTIL_PATH)

# Phony so it will always get called and its own makefile decides whether building is needed
.PHONY: $(LIBUTIL_PATH)
$(LIBUTIL_PATH):
	make -C `dirname $(LIBUTIL_PATH)`

