#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

test "x$top_srcdir" = x && top_srcdir=../..
test "x$top_builddir" = x && top_builddir=../..

# Usage: ./test_unmalicious_fonts.sh [ttf_or_otf_file_name]

BLACKLIST=$top_srcdir/tests/BLACKLIST.txt
CHECKER=$top_builddir/idempotent$EXEEXT

if [ ! -r "$BLACKLIST" ] ; then
  echo "$BLACKLIST is not found."
  exit 1
fi

if [ ! -x "$CHECKER" ] ; then
  echo "$CHECKER is not found."
  exit 1
fi

if [ $# -eq 0 ] ; then
  # No font file is specified. Apply this script to all TT/OT files under the
  # BASE_DIR below.

  # On Ubuntu Linux (>= 8.04), You can install ~1800 TrueType/OpenType fonts
  # to /usr/share/fonts/truetype by:
  #   % sudo apt-get install ttf-.*[^0]$
  BASE_DIR=/usr/share/fonts/
  if [ ! -d $BASE_DIR ] ; then
    # Mac OS X
    BASE_DIR="/Library/Fonts/ /System/Library/Fonts/"
  fi
  # TODO(yusukes): Support Cygwin.

  # Recursively call this script.
  fails=0
  fonts=`find $BASE_DIR -type f -name '*tf'`
  for f in $fonts; do
    $0 "$f"
    fails=$((fails+$?))
  done
  if [ $fails != 0 ]; then
    echo "$fails fonts failed."
    exit 1
  else
    echo "All fonts passed"
    exit 0
  fi
fi

if [ $# -gt 1 ] ; then
  echo "Usage: $0 [ttf_or_otf_file_name]"
  exit 1
fi

# Check the font file using idempotent if the font is not blacklisted.
base=`basename "$1"`
skip=`egrep -i -e "^$base" "$BLACKLIST"`

if [ "x$skip" = "x" ]; then
  $CHECKER "$1"
  ret=$?
  if [ $ret != 0 ]; then
    echo "FAILED: $1"
  fi
  exit $ret
fi
