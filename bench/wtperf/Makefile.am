LDADD = $(top_builddir)/libwiredtiger.la

noinst_PROGRAMS = wtperf

# The benchmark can be run with no arguments as simple smoke tests
TESTS = $(noinst_PROGRAMS)

TESTS_ENVIRONMENT = rm -rf WT_TEST ; mkdir WT_TEST ;

clean-local:
	rm -rf WT_TEST
