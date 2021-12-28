#!/bin/bash

# Copyright (c) 2009-2021 The OTS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage: ./test_bad_fonts.sh ots-sanitize fonts_dir [ttf_or_otf_file_name]

CHECKER=$1
BASE_DIR=$2

if [ ! -x "$CHECKER" ] ; then
  echo "$CHECKER is not found."
  exit 1
fi

if [ $# -eq 2 ] ; then
  # No font file is specified. Apply this script to all TT/OT files under the
  # BASE_DIR.
  if [ ! -d "$BASE_DIR" ] ; then
    echo "$BASE_DIR does not exist."
    exit 1
  fi

  FONTS=$(find "$BASE_DIR" -type f)
  # Recursively call this script.
  FAILS=0
  IFS=$'\n'
  for f in $FONTS; do
    $0 "$CHECKER" "$BASE_DIR" "$f"
    FAILS=$((FAILS+$?))
  done

  if [ $FAILS != 0 ]; then
    echo "$FAILS fonts failed."
    exit 1
  else
    echo "All fonts passed"
    exit 0
  fi
fi

if [ $# -gt 3 ] ; then
  echo "Usage: $0 ots-sanitize fonts_dir [ttf_or_otf_file_name]"
  exit 1
fi

# Confirm that the bad font file is rejected by OTS.
$CHECKER "$3" 2>&1
if [ $? = 0 ]; then
  echo "FAILED: $1"
  exit 1
else
  echo "PASSED: $1"
  exit 0
fi
