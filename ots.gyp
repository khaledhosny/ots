# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'ots-common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ots',
      'type': '<(library)',
      'sources': [
        '<@(ots_sources)',
      ],
      'include_dirs': [
        '<@(ots_include_dirs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<@(ots_include_dirs)',
        ],
      },
      'dependencies': [
        '../zlib/zlib.gyp:zlib',
      ],
    },
  ],
}
