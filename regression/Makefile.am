#AX_CHECK_ENABLE_DEBUG()
LIBUTIL_PATH = $(top_builddir)/../utillib/source/libutil.a

bin_PROGRAMS = regression
regression_SOURCES = main.cpp io.cpp test.cpp
regression_DEPENDENCIES = $(LIBUTIL_PATH)
AM_CPPFLAGS = -std=c++2a -I$(srcdir)/../utillib/include/

LDADD = $(LIBUTIL_PATH)

.PHONY: $(LIBUTIL_PATH)
$(LIBUTIL_PATH):
	make -j `nproc` -C `dirname $(LIBUTIL_PATH)`
