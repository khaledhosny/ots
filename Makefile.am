NULL =

bin_PROGRAMS = \
  ot-sanitise \
  perf \
  $(NULL)

if HAVE_FREETYPE
bin_PROGRAMS += \
  idempotent \
  validator-checker \
  side-by-side \
  $(NULL)
endif

lib_LIBRARIES = \
  libots.a \
  libbrotli.a \
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
  src/woff2.cc \
  src/woff2.h \
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
  third_party/brotli/dec/dictionary.h \
  third_party/brotli/dec/huffman.c \
  third_party/brotli/dec/huffman.h \
  third_party/brotli/dec/prefix.h \
  third_party/brotli/dec/safe_malloc.c \
  third_party/brotli/dec/safe_malloc.h \
  third_party/brotli/dec/streams.c \
  third_party/brotli/dec/streams.h \
  third_party/brotli/dec/transform.h \
  third_party/brotli/dec/types.h \
  $(NULL)

ot_sanitise_SOURCES = \
  util/ot-sanitise.cc \
  util/file-stream.h \
  $(NULL)

ot_sanitise_LDADD = \
  libots.a \
  libbrotli.a \
  -lz \
  $(NULL)

perf_SOURCES = \
  util/perf.cc \
  $(NULL)

perf_LDADD = \
  libots.a \
  libbrotli.a \
  -lz \
  $(NULL)

idempotent_SOURCES = \
  util/idempotent.cc \
  $(NULL)

idempotent_LDADD = \
  libots.a \
  libbrotli.a \
  -lz \
  $(FREETYPE_LIBS) \
  $(NULL)

validator_checker_SOURCES = \
  util/validator-checker.cc \
  $(NULL)

validator_checker_LDADD = \
  libots.a \
  libbrotli.a \
  -lz \
  $(FREETYPE_LIBS) \
  $(NULL)

side_by_side_SOURCES = \
  util/side-by-side.cc \
  $(NULL)

side_by_side_LDADD = \
  libots.a \
  libbrotli.a \
  -lz \
  $(FREETYPE_LIBS) \
  $(NULL)

AM_CPPFLAGS = \
  -I$(top_srcdir)/include \
  -I$(top_srcdir)/third_party/brotli/dec/ \
  $(FREETYPE_CFLAGS) \
  $(NULL)

-include $(top_srcdir)/git.mk
