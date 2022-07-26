TESTS = check_ftp check_parse
check_PROGRAMS = check_ftp check_parse
check_ftp_SOURCES = check_ftp.c \
                    $(top_builddir)/src/ftp.h $(top_builddir)/src/error.h \
                    $(top_builddir)/src/cmd.h

check_ftp_CFLAGS = @CHECK_CFLAGS@
check_ftp_LDADD = $(top_builddir)/src/libwaftp.la @CHECK_LIBS@

check_parse_SOURCES = check_parse.c $(top_builddir)/src/parse.h
check_parse_CFLAGS = $(check_ftp_CFLAGS)
check_parse_LDADD = $(check_ftp_LDADD)
