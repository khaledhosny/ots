NULL =

ACLOCAL_AMFLAGS = -I m4

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
else
if HAVE_CORETEXT
bin_PROGRAMS += \
  ots-idempotent \
  ots-validator-checker \
  $(NULL)
else
if HAVE_WIN32
bin_PROGRAMS += \
  ots-idempotent \
  $(NULL)
endif # HAVE_WIN32
endif # HAVE_CORETEXT
endif # HAVE_FREETYPE

noinst_PROGRAMS = ots-fuzzer

lib_LIBRARIES = \
  libots.a \
  libbrotli.a \
  libwoff2.a \
  $(NULL)

TESTS = \
  cff_type2_charstring_test \
  layout_common_table_test \
  tests/test_fuzzed_fonts.sh \
  $(NULL)

if HAVE_FREETYPE
TESTS += \
  tests/test_bad_fonts.sh \
  tests/test_good_fonts.sh \
  $(NULL)
else
if HAVE_CORETEXT
TESTS += \
  tests/test_bad_fonts.sh \
  tests/test_good_fonts.sh \
  $(NULL)
endif # HAVE_CORETEXT
endif # HAVE_FREETYPE

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
  src/fpgm.cc \
  src/fpgm.h \
  src/gasp.cc \
  src/gasp.h \
  src/gdef.cc \
  src/gdef.h \
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
  src/vdmx.cc \
  src/vdmx.h \
  src/vhea.cc \
  src/vhea.h \
  src/vmtx.cc \
  src/vmtx.h \
  src/vorg.cc \
  src/vorg.h \
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


ots_validator_checker_SOURCES = \
  util/ots-validator-checker.cc \
  $(NULL)

ots_side_by_side_SOURCES = \
  util/ots-side-by-side.cc \
  $(NULL)


ots_fuzzer_SOURCES = \
  util/ots-fuzzer.cc \
  $(NULL)


cff_type2_charstring_test_SOURCES = \
  tests/cff_type2_charstring_test.cc \
  $(NULL)

cff_type2_charstring_test_LDADD = \
  libgtest.a \
  $(LDADD) \
  $(NULL)


layout_common_table_test_SOURCES = \
  tests/layout_common_table_test.cc \
  $(NULL)

layout_common_table_test_LDADD = \
  libgtest.a \
  $(LDADD) \
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
  third_party/googletest/googletest/src/gtest_main.cc \
  third_party/googletest/googletest/src/gtest-port.cc \
  third_party/googletest/googletest/src/gtest-printers.cc \
  third_party/googletest/googletest/src/gtest-test-part.cc \
  third_party/googletest/googletest/src/gtest-typed-test.cc \
  $(NULL)

libgtest_a_LIBADD = \
  $(PTHREAD_LIBS) \
  $(NULL)


AM_CPPFLAGS = \
  -I$(top_srcdir)/third_party/googletest/googletest/include \
  -I$(top_srcdir)/third_party/googletest/googletest/ \
  -I$(top_srcdir)/src \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/third_party/brotli/dec/ \
  -I$(top_srcdir)/third_party/woff2/src \
  $(GTEST_CPPFLAGS) \
  $(NULL)

AM_CXXFLAGS = \
## for woff2
  -std=c++11 \
  $(CORETEXT_CFLAGS) \
  $(FREETYPE_CFLAGS) \
  $(PTHREAD_CFLAGS) \
  $(NULL)

LDADD = \
  libots.a \
  libwoff2.a \
  libbrotli.a \
  -lz \
  $(CORETEXT_LIBS) \
  $(FREETYPE_LIBS) \
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
