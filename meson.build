project('ots', 'c', 'cpp',
  version: '7.1.8',
  default_options : ['cpp_std=c++11', 'default_library=static'],
)

cxx = meson.get_compiler('cpp')

conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())

if get_option('debug')
  conf.set('OTS_DEBUG', 1)
endif

if get_option('graphite')
  conf.set('OTS_GRAPHITE', 1)
endif

if get_option('variations')
  conf.set('OTS_VARIATIONS', 1)
endif

freetype = dependency('freetype2', required: false)
if freetype.found()
  conf.set('HAVE_FREETYPE', 1)
endif

coretext = dependency('appleframeworks', modules: 'applicationservices',
                      required: false)
if coretext.found()
  conf.set('HAVE_CORETEXT', 1)
endif

gdi32 = cxx.find_library('gdi32', required: false)
if gdi32.found()
  conf.set('HAVE_WIN32', 1)
endif


configure_file(output: 'config.h',
               configuration: conf)


brotli_includes = ['third_party/brotli/c/include']
libbrotli = library('brotli',
  'third_party/brotli/c/common/constants.h',
  'third_party/brotli/c/common/dictionary.c',
  'third_party/brotli/c/common/dictionary.h',
  'third_party/brotli/c/common/transform.c',
  'third_party/brotli/c/common/transform.h',
  'third_party/brotli/c/common/version.h',
  'third_party/brotli/c/dec/bit_reader.c',
  'third_party/brotli/c/dec/bit_reader.h',
  'third_party/brotli/c/dec/decode.c',
  'third_party/brotli/c/dec/huffman.c',
  'third_party/brotli/c/dec/huffman.h',
  'third_party/brotli/c/dec/prefix.h',
  'third_party/brotli/c/dec/state.c',
  'third_party/brotli/c/dec/state.h',
  'third_party/brotli/c/include/brotli/decode.h',
  'third_party/brotli/c/include/brotli/port.h',
  'third_party/brotli/c/include/brotli/types.h',
  include_directories: include_directories(brotli_includes),
)


woff2_includes = ['third_party/brotli/c/include', 'third_party/woff2/include']
libwoff2 = library('woff2',
  'third_party/woff2/include/woff2/decode.h',
  'third_party/woff2/include/woff2/output.h',
  'third_party/woff2/src/buffer.h',
  'third_party/woff2/src/port.h',
  'third_party/woff2/src/round.h',
  'third_party/woff2/src/store_bytes.h',
  'third_party/woff2/src/table_tags.cc',
  'third_party/woff2/src/table_tags.h',
  'third_party/woff2/src/variable_length.cc',
  'third_party/woff2/src/variable_length.h',
  'third_party/woff2/src/woff2_common.cc',
  'third_party/woff2/src/woff2_common.h',
  'third_party/woff2/src/woff2_dec.cc',
  'third_party/woff2/src/woff2_out.cc',
  include_directories: include_directories(woff2_includes),
)


ots_includes = [
  'include',
  'third_party/woff2/include',
]

ots_sources = [
  'src/cff.cc',
  'src/cff.h',
  'src/cff_type2_charstring.cc',
  'src/cff_type2_charstring.h',
  'src/cmap.cc',
  'src/cmap.h',
  'src/cvt.cc',
  'src/cvt.h',
  'src/fpgm.cc',
  'src/fpgm.h',
  'src/gasp.cc',
  'src/gasp.h',
  'src/gdef.cc',
  'src/gdef.h',
  'src/glyf.cc',
  'src/glyf.h',
  'src/gpos.cc',
  'src/gpos.h',
  'src/gsub.cc',
  'src/gsub.h',
  'src/hdmx.cc',
  'src/hdmx.h',
  'src/head.cc',
  'src/head.h',
  'src/hhea.cc',
  'src/hhea.h',
  'src/hmtx.cc',
  'src/hmtx.h',
  'src/kern.cc',
  'src/kern.h',
  'src/layout.cc',
  'src/layout.h',
  'src/loca.cc',
  'src/loca.h',
  'src/ltsh.cc',
  'src/ltsh.h',
  'src/maxp.cc',
  'src/maxp.h',
  'src/math.cc',
  'src/math_.h',
  'src/metrics.cc',
  'src/metrics.h',
  'src/name.cc',
  'src/name.h',
  'src/os2.cc',
  'src/os2.h',
  'src/ots.cc',
  'src/ots.h',
  'src/post.cc',
  'src/post.h',
  'src/prep.cc',
  'src/prep.h',
  'src/vdmx.cc',
  'src/vdmx.h',
  'src/vhea.cc',
  'src/vhea.h',
  'src/vmtx.cc',
  'src/vmtx.h',
  'src/vorg.cc',
  'src/vorg.h',
]

ots_libs = [libbrotli, libwoff2]

if get_option('graphite')
  ots_includes += ['third_party/lz4/lib']
  ots_sources += [
    'src/feat.cc',
    'src/feat.h',
    'src/glat.cc',
    'src/glat.h',
    'src/gloc.cc',
    'src/gloc.h',
    'src/graphite.h',
    'src/sile.h',
    'src/sile.cc',
    'src/silf.h',
    'src/silf.cc',
    'src/sill.h',
    'src/sill.cc',
  ]
  liblz4 = library('lz4',
    'third_party/lz4/lib/lz4.c',
    'third_party/lz4/lib/lz4.h',
  )
  ots_libs += [liblz4]
endif

if get_option('variations')
  ots_sources += [
    'src/avar.cc',
    'src/avar.h',
    'src/cvar.cc',
    'src/cvar.h',
    'src/fvar.cc',
    'src/fvar.h',
    'src/gvar.cc',
    'src/gvar.h',
    'src/hvar.cc',
    'src/hvar.h',
    'src/mvar.cc',
    'src/mvar.h',
    'src/stat.cc',
    'src/stat.h',
    'src/variations.cc',
    'src/variations.h',
    'src/vvar.cc',
    'src/vvar.h',
  ]
endif

zlib = dependency('zlib', fallback : ['zlib', 'zlib_dep'])

libots = library('ots',
  ots_sources,
  include_directories: include_directories(ots_includes),
  link_with: ots_libs,
  cpp_args : '-DHAVE_CONFIG_H',
  dependencies: zlib,
)


executable('ots-sanitize',
  'util/ots-sanitize.cc',
  'util/test-context.h',
  include_directories: include_directories('include'),
  link_with: libots,
  install: true,
)


executable('ots-fuzzer',
  'util/ots-fuzzer.cc',
  include_directories: include_directories(['include', 'src']),
  link_with: libots,
)


if freetype.found() or coretext.found() or gdi32.found()
  deps = [freetype, gdi32]
  if coretext.found()
    deps += [coretext]
  endif
  executable('ots-idempotent',
    'util/ots-idempotent.cc',
    'util/test-context.h',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: deps,
    install: true,
  )
  test_good_fonts = find_program('tests/test_good_fonts.sh')
  test('test_good_fonts',
    test_good_fonts,
    timeout: 9999,
    env: [
      'top_srcdir=@0@'.format(meson.current_source_dir()),
      'top_builddir=@0@'.format(meson.current_build_dir()),
    ]
  )
endif


if freetype.found() or coretext.found()
  deps = [freetype]
  if coretext.found()
    deps += [coretext]
  endif
  executable('ots-validator-checker',
    'util/ots-validator-checker.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: deps,
    install: true,
  )
endif


if freetype.found()
  executable('ots-side-by-side',
    'util/ots-side-by-side.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    dependencies: freetype,
    install: true,
  )
endif


if cxx.has_header('sys/time.h')
  executable('ots-perf',
    'util/ots-perf.cc',
    include_directories: include_directories('include'),
    link_with: libots,
    install: true,
  )
endif


gtest = dependency('gtest', fallback: ['gtest', 'gtest_main_dep'])

cff_type2_charstring = executable('cff_type2_charstring',
  'tests/cff_type2_charstring_test.cc',
  include_directories: include_directories(['include', 'src']),
  link_with: libots,
  dependencies: gtest,
)

test('cff_type2_charstring', cff_type2_charstring)


layout_common_table = executable('layout_common_table',
  'tests/layout_common_table_test.cc',
  include_directories: include_directories(['include', 'src']),
  link_with: libots,
  dependencies: gtest,
)

test('layout_common_table', layout_common_table)


test('test_bad_fonts',
  find_program('tests/test_bad_fonts.sh'),
  env: [
    'top_srcdir=@0@'.format(meson.current_source_dir()),
    'top_builddir=@0@'.format(meson.current_build_dir()),
  ]
)


test('test_fuzzed_fonts',
  find_program('tests/test_fuzzed_fonts.sh'),
  timeout: 9999,
  env: [
    'top_srcdir=@0@'.format(meson.current_source_dir()),
    'top_builddir=@0@'.format(meson.current_build_dir()),
  ]
)
