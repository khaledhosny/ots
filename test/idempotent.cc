// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(_MSC_VER)
#ifdef __linux__
// Linux
#include <freetype/ftoutln.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#else
// Mac OS X
#include <ApplicationServices/ApplicationServices.h>  // g++ -framework Cocoa
#endif  // __linux__
#else
// Windows
// TODO(yusukes): Support Windows.
#endif  // _MSC_VER

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "opentype-sanitiser.h"
#include "ots-memory-stream.h"

namespace {

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s <ttf file>\n", argv0);
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) return Usage(argv[0]);

  const int fd = ::open(argv[1], O_RDONLY);
  if (fd < 0) {
    ::perror("open");
    return 1;
  }

  struct stat st;
  ::fstat(fd, &st);

  uint8_t *data = new uint8_t[st.st_size];
  if (::read(fd, data, st.st_size) != st.st_size) {
    ::close(fd);
    std::fprintf(stderr, "Failed to read file!\n");
    return 1;
  }
  ::close(fd);

  // A transcoded font is usually smaller than an original font.
  // However, it can be slightly bigger than the original one due to
  // name table replacement and/or padding for glyf table.
  //
  // However, a WOFF font gets decompressed and so can be *much* larger than
  // the original.
  uint8_t *result = new uint8_t[st.st_size * 8];
  ots::MemoryStream output(result, st.st_size * 8);

  bool r = ots::Process(&output, data, st.st_size);
  if (!r) {
    std::fprintf(stderr, "Failed to sanitise file!\n");
    return 1;
  }
  const size_t result_len = output.Tell();
  free(data);

  uint8_t *result2 = new uint8_t[result_len];
  ots::MemoryStream output2(result2, result_len);
  r = ots::Process(&output2, result, result_len);
  if (!r) {
    std::fprintf(stderr, "Failed to sanitise previous output!\n");
    return 1;
  }
  const size_t result2_len = output2.Tell();

  bool dump_results = false;
  if (result2_len != result_len) {
    std::fprintf(stderr, "Outputs differ in length\n");
    dump_results = true;
  } else if (std::memcmp(result2, result, result_len)) {
    std::fprintf(stderr, "Outputs differ in content\n");
    dump_results = true;
  }

  if (dump_results) {
    std::fprintf(stderr, "Dumping results to out1.tff and out2.tff\n");
    int fd1 = ::open("out1.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int fd2 = ::open("out2.ttf", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd1 < 0 || fd2 < 0) {
      ::perror("opening output file");
      return 1;
    }
    if ((::write(fd1, result, result_len) < 0) ||
        (::write(fd2, result2, result2_len) < 0)) {
      ::perror("writing output file");
      return 1;
    }
  }

  // Verify that the transcoded font can be opened by the font renderer for
  // Linux (FreeType2), Mac OS X, or Windows.
#if !defined(_MSC_VER)
#ifdef __linux__
  // Linux
  FT_Library library;
  FT_Error error = ::FT_Init_FreeType(&library);
  if (error) {
    std::fprintf(stderr, "Failed to initialize FreeType2!\n");
    return 1;
  }
  FT_Face dummy;
  error = ::FT_New_Memory_Face(library, result, result_len, 0, &dummy);
  if (error) {
    std::fprintf(stderr, "Failed to open the transcoded font\n");
    return 1;
  }
#else
  // Mac OS X
  ATSFontContainerRef container_ref = 0;
  ATSFontActivateFromMemory(result, result_len, 3, kATSFontFormatUnspecified,
                            NULL, kATSOptionFlagsDefault, &container_ref);
  if (!container_ref) {
    std::fprintf(stderr, "Failed to open the transcoded font\n");
    return 1;
  }

  ItemCount count;
  ATSFontFindFromContainer(
      container_ref, kATSOptionFlagsDefault, 0, NULL, &count);
  if (!count) {
    std::fprintf(stderr, "Failed to open the transcoded font\n");
    return 1;
  }

  ATSFontRef ats_font_ref = 0;
  ATSFontFindFromContainer(
      container_ref, kATSOptionFlagsDefault, 1, &ats_font_ref, NULL);
  if (!ats_font_ref) {
    std::fprintf(stderr, "Failed to open the transcoded font\n");
    return 1;
  }

  CGFontRef cg_font_ref = CGFontCreateWithPlatformFont(&ats_font_ref);
  if (!CGFontGetNumberOfGlyphs(cg_font_ref)) {
    std::fprintf(stderr, "Failed to open the transcoded font\n");
    return 1;
  }
#endif  // __linux__
#else
  // Windows
  // TODO(yusukes): Support Windows.
#endif  // _MSC_VER

  return 0;
}
