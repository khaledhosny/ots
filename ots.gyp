# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ots',
      'type': '<(library)',
      'msvs_guid': '529831C4-5E17-4B0F-814E-08DD1D5AFC0B',
      'sources': [
        'include/ots-memory-stream.h',
        'include/opentype-sanitiser.h',
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
        'src/glyf.cc',
        'src/glyf.h',
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
        'src/loca.cc',
        'src/loca.h',
        'src/ltsh.cc',
        'src/ltsh.h',
        'src/maxp.cc',
        'src/maxp.h',
        'src/name.cc',
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
        'src/vorg.cc',
        'src/vorg.h',
      ],
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
    },
  ],
}
