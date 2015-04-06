NULL =

TESTS = \
  cff_type2_charstring_test \
  layout_common_table_test \
  table_dependencies_test \
  $(NULL)

check_PROGRAMS = \
  cff_type2_charstring_test \
  layout_common_table_test \
  table_dependencies_test \
  $(NULL)

check_LIBRARIES = \
  libgtest.a \
  libgtest_main.a \
  $(NULL)

cff_type2_charstring_test_SOURCES = \
  cff_type2_charstring_test.cc \
  $(NULL)

layout_common_table_test_SOURCES = \
  layout_common_table_test.cc \
  $(NULL)

table_dependencies_test_SOURCES = \
  table_dependencies_test.cc \
  $(NULL)

libgtest_a_SOURCES = \
  ../third_party/gtest/src/gtest-all.cc \
  $(NULL)

libgtest_main_a_SOURCES = \
  ../third_party/gtest/src/gtest_main.cc \
  $(NULL)

libgtest_main_a_LIBADD = \
  libgtest.a \
  $(NULL)

AM_CPPFLAGS = \
  -I$(top_srcdir)/third_party/gtest/include \
  -I$(top_srcdir)/third_party/gtest/ \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/include \
  $(GTEST_CPPFLAGS) \
  $(NULL)

AM_CXXFLAGS = \
  $(PTHREAD_CFLAGS) \
  $(NULL)

LDADD = \
  $(PTHREAD_LIBS) \
  $(top_builddir)/libots.a \
  $(top_builddir)/libbrotli.a \
  libgtest_main.a \
  libgtest.a \
  -lz \
  $(NULL)

-include $(top_srcdir)/git.mk
