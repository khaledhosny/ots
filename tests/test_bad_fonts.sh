#!/bin/bash

# Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

test "x$top_srcdir" = x && top_srcdir=.
test "x$top_builddir" = x && top_builddir=.

# Usage: ./test_bad_fonts.sh [ttf_or_otf_file_name]

BASE_DIR=$top_srcdir/tests/fonts/bad/
CHECKER=$top_builddir/ots-validator-checker$EXEEXT

if [ ! -x "$CHECKER" ] ; then
  echo "$CHECKER is not found."
  exit 1
fi

if [ $# -eq 0 ] ; then
  # No font file is specified. Apply this script to all TT/OT files under the
  # BASE_DIR.
  if [ ! -d $BASE_DIR ] ; then
    echo "$BASE_DIR does not exist."
    exit 1
  fi

  # Recursively call this script.
  find $BASE_DIR -type f -name '*tf' -o -name '*tc' -exec "$0" {} \;
  echo
  exit 0
fi

if [ $# -gt 1 ] ; then
  echo "Usage: $0 [ttf_or_otf_file_name]"
  exit 1
fi

# Confirm that the bad font file does not crash OTS nor OS font renderer.
base=`basename "$1"`
"$CHECKER" "$1" > /dev/null 2>&1 || (echo ; echo "\nFAIL: $1 (Run $CHECKER $1 for more information.)")
echo -n "."
