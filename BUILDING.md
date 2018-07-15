Instructions below are for building standalone OTS utilities, if you want to
use OTS as a library then the recommended way is to copy the source code and
integrate it into your existing build system. Our build system does not build
a shared library intentionally.

General build instructions
--------------------------

Build OTS: 
    
    $ meson build
    $ ninja -C build

Run the tests (if you wish):
    
    $ ninja -C build test

Developer build instructions
----------------------------

If you would like to see the source code lines related to reported 
errors, then replace meson call above with:

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
