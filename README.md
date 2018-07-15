[![Build Status](https://travis-ci.org/khaledhosny/ots.svg?branch=master)](https://travis-ci.org/khaledhosny/ots)
[![Build status](https://ci.appveyor.com/api/projects/status/0l9ms6g47corescm?svg=true)](https://ci.appveyor.com/project/khaledhosny/ots)

OpenType Sanitizer
==================

The OpenType Sanitizer (OTS) parses and serializes OpenType files (OTF, TTF)
and WOFF and WOFF2 font files, validating them and sanitizing them as it goes.

The C library is integrated into Chromium and Firefox, and also simple
command line tools to check files offline in a Terminal.

The CSS [font-face property][1] is great for web typography. Having to use images
in order to get the correct typeface is a great sadness; one should be able to
use vectors.

However, on many platforms the system-level TrueType font renderers have never
been part of the attack surface before, and putting them on the front line is
a scary proposition... Especially on platforms like Windows, where it's a
closed-source blob running with high privilege.

Building from source
--------------------

Instructions below are for building standalone OTS utilities, if you want to
use OTS as a library then the recommended way is to copy the source code and
integrate it into your existing build system. Our build system does not build a
shared library intentionally.

### General build instructions

Build OTS:

    $ meson build
    $ ninja -C build

Run the tests (if you wish):

    $ ninja -C build test

### Developer build instructions

If you would like to see the source code lines related to reported errors, then
replace meson call above with:

    $ meson -Ddebug=true build

For example:

    $ ./ots-sanitize ~/fonts/ofl/merriweathersans/MerriweatherSans-Bold.ttf
    ERROR at src/layout.cc:100 (ParseScriptTable)
    ERROR: Layout: DFLT table doesn't satisfy the spec. for script tag DFLT
    ERROR at src/layout.cc:1247 (ParseScriptListTable)
    ERROR: Layout: Failed to parse script table 0
    ERROR at src/gsub.cc:642 (ots_gsub_parse)
    ERROR: GSUB: Failed to parse script list table
    ERROR at src/ots.cc:669 (ProcessGeneric)
    Failed to sanitize file!

Usage
-----

See [docs](docs)

* * *

Thanks to Alex Russell for the original idea.

[1]: http://www.w3.org/TR/CSS2/fonts.html#font-descriptions
