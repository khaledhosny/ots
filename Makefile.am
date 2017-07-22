NULL =

ACLOCAL_AMFLAGS = -I m4

TESTS = \
  cff_type2_charstring_test \
  layout_common_table_test \
  tests/test_bad_fonts.sh \
  tests/test_fuzzed_fonts.sh \
  $(NULL)


bin_PROGRAMS = \
  ots-sanitize \
  $(NULL)

if HAVE_SYSTIME
bin_PROGRAMS += ots-perf
endif

if HAVE_FREETYPE
bin_PROGRAMS += \
  ots-idempotent \
  ots-validator-checker \
  ots-side-by-side \
  $(NULL)

TESTS += tests/test_good_fonts.sh
else
if HAVE_CORETEXT
bin_PROGRAMS += \
  ots-idempotent \
  ots-validator-checker \
  $(NULL)

TESTS += tests/test_good_fonts.sh
else
if HAVE_WIN32
bin_PROGRAMS += \
  ots-idempotent \
  $(NULL)

TESTS += tests/test_good_fonts.sh
endif # HAVE_WIN32
endif # HAVE_CORETEXT
endif # HAVE_FREETYPE

noinst_PROGRAMS = ots-fuzzer ots-testfuzz

lib_LIBRARIES = \
  libots.a \
  libbrotli.a \
  libwoff2.a \
  $(NULL)

check_PROGRAMS = \
  cff_type2_charstring_test \
  layout_common_table_test \
  $(NULL)

check_LIBRARIES = \
  libgtest.a \
  $(NULL)


libots_a_SOURCES = \
  src/cff.cc \
  src/cff.h \
  src/cff_type2_charstring.cc \
  src/cff_type2_charstring.h \
  src/cmap.cc \
  src/cmap.h \
  src/cvt.cc \
  src/cvt.h \
  src/feat.cc \
  src/feat.h \
  src/fpgm.cc \
  src/fpgm.h \
  src/gasp.cc \
  src/gasp.h \
  src/gdef.cc \
  src/gdef.h \
  src/glat.cc \
  src/glat.h \
  src/gloc.cc \
  src/gloc.h \
  src/glyf.cc \
  src/glyf.h \
  src/gpos.cc \
  src/gpos.h \
  src/gsub.cc \
  src/gsub.h \
  src/hdmx.cc \
  src/hdmx.h \
  src/head.cc \
  src/head.h \
  src/hhea.cc \
  src/hhea.h \
  src/hmtx.cc \
  src/hmtx.h \
  src/kern.cc \
  src/kern.h \
  src/layout.cc \
  src/layout.h \
  src/loca.cc \
  src/loca.h \
  src/ltsh.cc \
  src/ltsh.h \
  src/maxp.cc \
  src/maxp.h \
  src/math.cc \
  src/math_.h \
  src/metrics.cc \
  src/metrics.h \
  src/name.cc \
  src/name.h \
  src/os2.cc \
  src/os2.h \
  src/ots.cc \
  src/ots.h \
  src/post.cc \
  src/post.h \
  src/prep.cc \
  src/prep.h \
  src/sile.h \
  src/sile.cc \
  src/silf.h \
  src/silf.cc \
  src/sill.h \
  src/sill.cc \
  src/vdmx.cc \
  src/vdmx.h \
  src/vhea.cc \
  src/vhea.h \
  src/vmtx.cc \
  src/vmtx.h \
  src/vorg.cc \
  src/vorg.h \
  $(NULL)

libots_a_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/third_party/woff2/src \
  $(NULL)


pkginclude_HEADERS = \
  include/ots-memory-stream.h \
  include/opentype-sanitiser.h \
  $(NULL)


libbrotli_a_SOURCES = \
  third_party/brotli/dec/bit_reader.c \
  third_party/brotli/dec/bit_reader.h \
  third_party/brotli/dec/context.h \
  third_party/brotli/dec/decode.c \
  third_party/brotli/dec/decode.h \
  third_party/brotli/dec/dictionary.c \
  third_party/brotli/dec/dictionary.h \
  third_party/brotli/dec/huffman.c \
  third_party/brotli/dec/huffman.h \
  third_party/brotli/dec/port.h \
  third_party/brotli/dec/prefix.h \
  third_party/brotli/dec/state.c \
  third_party/brotli/dec/state.h \
  third_party/brotli/dec/transform.h \
  third_party/brotli/dec/types.h \
  $(NULL)

libbrotli_a_CPPFLAGS = \
  -I$(top_srcdir)/third_party/brotli/dec \
  $(NULL)


libwoff2_a_SOURCES = \
  third_party/woff2/src/buffer.h \
  third_party/woff2/src/port.h \
  third_party/woff2/src/round.h \
  third_party/woff2/src/store_bytes.h \
  third_party/woff2/src/table_tags.cc \
  third_party/woff2/src/table_tags.h \
  third_party/woff2/src/variable_length.cc \
  third_party/woff2/src/variable_length.h \
  third_party/woff2/src/woff2_common.cc \
  third_party/woff2/src/woff2_common.h \
  third_party/woff2/src/woff2_dec.cc \
  third_party/woff2/src/woff2_dec.h \
  third_party/woff2/src/woff2_out.cc \
  third_party/woff2/src/woff2_out.h \
  $(NULL)

libwoff2_a_CPPFLAGS = \
  -I$(top_srcdir)/third_party/brotli/dec \
  -I$(top_srcdir)/third_party/woff2/src \
  $(NULL)


ots_sanitize_SOURCES = \
  util/ots-sanitize.cc \
  util/test-context.h \
  $(NULL)


ots_perf_SOURCES = \
  util/ots-perf.cc \
  $(NULL)


ots_idempotent_SOURCES = \
  util/ots-idempotent.cc \
  util/test-context.h \
  $(NULL)

ots_idempotent_CXXFLAGS = \
  $(CORETEXT_CFLAGS) \
  $(FREETYPE_CFLAGS) \
  $(NULL)

ots_idempotent_LDADD = \
  $(LDADD) \
  $(CORETEXT_LIBS) \
  $(FREETYPE_LIBS) \
  $(NULL)


ots_validator_checker_SOURCES = \
  util/ots-validator-checker.cc \
  $(NULL)

ots_validator_checker_CXXFLAGS = \
  $(CORETEXT_CFLAGS) \
  $(FREETYPE_CFLAGS) \
  $(NULL)

ots_validator_checker_LDADD = \
  $(LDADD) \
  $(CORETEXT_LIBS) \
  $(FREETYPE_LIBS) \
  $(NULL)


ots_side_by_side_SOURCES = \
  util/ots-side-by-side.cc \
  $(NULL)

ots_side_by_side_CXXFLAGS = \
  $(FREETYPE_CFLAGS) \
  $(NULL)

ots_side_by_side_LDADD = \
  $(LDADD) \
  $(FREETYPE_LIBS) \
  $(NULL)


# Used by OSS-Fuzz, should not directly use -fsanitize
ots_fuzzer_SOURCES = \
  util/ots-fuzzer.cc \
  $(NULL)

ots_fuzzer_CPPFLAGS = \
  -I$(top_srcdir)/src \
  $(AM_CPPFLAGS) \
  $(NULL)


ots_testfuzz_SOURCES = \
  util/ots-fuzzer.cc \
  $(libbrotli_a_SOURCES) \
  $(libots_a_SOURCES) \
  $(libwoff2_a_SOURCES) \
  $(NULL)

ots_testfuzz_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/third_party/brotli/dec \
  -I$(top_srcdir)/third_party/woff2/src \
  $(AM_CPPFLAGS) \
  $(NULL)

ots_testfuzz_CXXFLAGS = \
  $(ASAN_CFLAGS) \
  $(UBSAN_CFLAGS) \
  $(NULL)

ots_testfuzz_LDADD = \
  -lz \
  $(NULL)

cff_type2_charstring_test_SOURCES = \
  third_party/googletest/googletest/src/gtest_main.cc \
  tests/cff_type2_charstring_test.cc \
  $(NULL)

cff_type2_charstring_test_CPPFLAGS = \
  -I$(top_srcdir)/src \
  $(GTEST_CPPFLAGS) \
  $(AM_CPPFLAGS) \
  $(NULL)

cff_type2_charstring_test_CXXFLAGS = \
  $(PTHREAD_CFLAGS) \
  $(NULL)

cff_type2_charstring_test_LDADD = \
  libgtest.a \
  $(LDADD) \
  $(PTHREAD_LIBS) \
  $(NULL)


layout_common_table_test_SOURCES = \
  third_party/googletest/googletest/src/gtest_main.cc \
  tests/layout_common_table_test.cc \
  $(NULL)

layout_common_table_test_CPPFLAGS = \
  -I$(top_srcdir)/src \
  $(GTEST_CPPFLAGS) \
  $(AM_CPPFLAGS) \
  $(NULL)

layout_common_table_test_CXXFLAGS = \
  $(PTHREAD_CFLAGS) \
  $(NULL)

layout_common_table_test_LDADD = \
  libgtest.a \
  $(LDADD) \
  $(PTHREAD_LIBS) \
  $(NULL)


libgtest_a_SOURCES = \
  third_party/googletest/googletest/include/gtest/gtest-death-test.h \
  third_party/googletest/googletest/include/gtest/gtest.h \
  third_party/googletest/googletest/include/gtest/gtest-message.h \
  third_party/googletest/googletest/include/gtest/gtest-param-test.h \
  third_party/googletest/googletest/include/gtest/gtest_pred_impl.h \
  third_party/googletest/googletest/include/gtest/gtest-printers.h \
  third_party/googletest/googletest/include/gtest/gtest_prod.h \
  third_party/googletest/googletest/include/gtest/gtest-spi.h \
  third_party/googletest/googletest/include/gtest/gtest-test-part.h \
  third_party/googletest/googletest/include/gtest/gtest-typed-test.h \
  third_party/googletest/googletest/include/gtest/internal/custom/gtest.h \
  third_party/googletest/googletest/include/gtest/internal/custom/gtest-port.h \
  third_party/googletest/googletest/include/gtest/internal/custom/gtest-printers.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-death-test-internal.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-filepath.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-internal.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-linked_ptr.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-param-util-generated.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-param-util.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-port-arch.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-port.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-string.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-tuple.h \
  third_party/googletest/googletest/include/gtest/internal/gtest-type-util.h \
  third_party/googletest/googletest/src/gtest.cc \
  third_party/googletest/googletest/src/gtest-death-test.cc \
  third_party/googletest/googletest/src/gtest-filepath.cc \
  third_party/googletest/googletest/src/gtest-internal-inl.h \
  third_party/googletest/googletest/src/gtest-port.cc \
  third_party/googletest/googletest/src/gtest-printers.cc \
  third_party/googletest/googletest/src/gtest-test-part.cc \
  third_party/googletest/googletest/src/gtest-typed-test.cc \
  $(NULL)

libgtest_a_CPPFLAGS = \
  $(AM_CPPFLAGS) \
  $(GTEST_CPPFLAGS) \
  $(NULL)

libgtest_a_CXXFLAGS = \
  $(PTHREAD_CFLAGS) \
  $(NULL)

libgtest_a_LIBADD = \
  $(PTHREAD_LIBS) \
  $(NULL)

GTEST_CPPFLAGS += \
  -I$(top_srcdir)/third_party/googletest/googletest/include \
  -I$(top_srcdir)/third_party/googletest/googletest/ \
  $(NULL)

if HAVE_WIN32
GTEST_CPPFLAGS += -DGTEST_OS_WINDOWS
endif


AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  $(NULL)

LDADD = \
  libots.a \
  libwoff2.a \
  libbrotli.a \
  -lz \
  $(NULL)

if HAVE_WIN32
LDADD += -lgdi32
endif

AM_TESTS_ENVIRONMENT = \
  EXEEXT="$(EXEEXT)"; \
  export EXEEXT; \
  top_srcdir="$(top_srcdir)"; \
  export top_srcdir; \
  top_builddir="$(top_builddir)"; \
  export top_builddir; \
  $(NULL)


EXTRA_DIST = \
  LICENSE \
  docs/DesignDoc.md \
  docs/HowToFix.md \
  docs/HowToTest.md \
  tests/BLACKLIST.txt \
  tests/fonts/bad/.dummy \
  tests/test_fuzzed_fonts.sh \
  tests/test_bad_fonts.sh \
  tests/test_good_fonts.sh \
  third_party/brotli/LICENSE \
  third_party/googletest/googletest/LICENSE \
  third_party/woff2/LICENSE \
  $(NULL)


MAINTAINERCLEANFILES = \
  $(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
  $(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
  $(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
  $(NULL)

-include $(top_srcdir)/git.mk
