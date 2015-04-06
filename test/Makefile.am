NULL =

check_LIBRARIES = \
  libgtest.a \
  libgtest_main.a \
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
  $(GTEST_CPPFLAGS) \
  $(NULL)

AM_CXXFLAGS = \
  $(PTHREAD_CFLAGS) \
  $(NULL)

LDADD = \
  $(PTHREAD_LIBS) \
  libgtest_main.a \
  libgtest.a \
  $(NULL)

-include $(top_srcdir)/git.mk
