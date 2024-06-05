
//#include <algorithm>
//#include <fstream>
//#include <streambuf>
//#include <string>

//#include <getopt.h>
//#include <math.h>
//#include <signal.h>
//#include <stdint.h>
#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
//#include <sys/stat.h>
//#include <time.h>
//#include <unistd.h>

//#include "graphics.h"

#include <bitset>
#include <vector>

static int usage(const char *progname) {
  /* ToDo
  fprintf(stderr, "usage: %s [options] [<text>| -i <filename>]\n", progname);
  fprintf(stderr, "Takes text and scrolls it with speed -s\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr,
          "\t-f <font-file>    : Path to *.bdf-font to be used.\n"
          "\t-i <textfile>     : Input from file.\n"
          "\t-s <speed>        : Approximate letters per second. \n"
          "\t                    Positive: scroll right to left; Negative: scroll left to right\n"
          "\t                    (Zero for no scrolling)\n"
          "\t-l <loop-count>   : Number of loops through the text. "
          "-1 for endless (default)\n"
          "\t-b <on-time>,<off-time>  : Blink while scrolling. Keep "
          "on and off for these amount of scrolled pixels.\n"
          "\t-x <x-origin>     : Shift X-Origin of displaying text (Default: 0)\n"
          "\t-y <y-origin>     : Shift Y-Origin of displaying text (Default: 0)\n"
          "\t-t <track-spacing>: Spacing pixels between letters (Default: 0)\n"
          "\n"
          "\t-C <r,g,b>        : Text Color. Default 255,255,255 (white)\n"
          "\t-B <r,g,b>        : Background-Color. Default 0,0,0\n"
          "\t-O <r,g,b>        : Outline-Color, e.g. to increase contrast.\n"
          );
  */
  return 1;
}

// Bitmap for one row. This limits the number of available columns.
// Make wider if running into trouble.
static constexpr int kMaxFontWidth = 196;
typedef std::bitset<kMaxFontWidth> rowbitmap_t;

struct Glyph {
  int device_width, device_height;
  int width, height;
  int x_offset, y_offset;
  std::vector<rowbitmap_t> bitmap;  // contains 'height' elements.
};

static bool readNibble(char c, uint8_t* val) {
  if (c >= '0' && c <= '9') { *val = c - '0'; return true; }
  if (c >= 'a' && c <= 'f') { *val = c - 'a' + 0xa; return true; }
  if (c >= 'A' && c <= 'F') { *val = c - 'A' + 0xa; return true; }
  return false;
}

static bool parseBitmap(const char* buffer, rowbitmap_t* result) {
  // Read the bitmap left-aligned to our buffer.
  for (int pos = result->size() - 1; *buffer && pos >= 3; buffer+=1) {
    uint8_t val;
    if (!readNibble(*buffer, &val))
      break;
    (*result)[pos--] = val & 0x8;
    (*result)[pos--] = val & 0x4;
    (*result)[pos--] = val & 0x2;
    (*result)[pos--] = val & 0x1;
  }
  return true;
}

static void writeNibble(uint8_t val, char* c) {
  if (val <= 0x9) *c = '0' + val;
  if (val >= 0xA && val <= 0xF) *c = 'A' + (val - 0xA);
}

static char* composeBitmap(char* buffer, const rowbitmap_t& row, int width) {
  char* line = buffer;
  int x = 0;
  while (x < width) {
    uint8_t val = 0;
    for (int i = 3; i >= 0; i--) {
      val |= row.test(kMaxFontWidth - 1 - x) << i;
      x++;
    }
    writeNibble(val, buffer);
    buffer++;
  }
  *buffer = '\0';
  return line;
}

static int getFirstUsedColumn(const Glyph& g) {
  int first_used_column = g.width;
  for(const auto& row : g.bitmap) {
    for (int column = 0; column < g.width; column++) {
      if (row.test(kMaxFontWidth - 1 - column)) {
        first_used_column = std::min(first_used_column, column);
        //break;
      }
    }
  }
  return first_used_column;
}

static int getLastUsedColumn(const Glyph& g) {
  int last_used_column = 0;
  for(const auto& row : g.bitmap) {
    for (int column = g.width - 1; column >= 0 ; column--) {
      if (row.test(kMaxFontWidth - 1 - column)) {
        last_used_column = std::max(last_used_column, column);
        break;
      }
    }
  }
  return last_used_column;
}

void shiftBitmap(Glyph* g, int i) {
  for(int row = 0; row < g->height; row++) {
    g->bitmap[row] <<= i;
  }
}

void modifyGlyph(Glyph* g) {
  int first_used_column = getFirstUsedColumn(*g);
  int last_used_column = getLastUsedColumn(*g);
  if (first_used_column == g->device_width) {
    return;
  }
  int used_width = last_used_column - first_used_column + 1;
  g->device_width = used_width;
  g->width = used_width;
  shiftBitmap(g, first_used_column);
}

int main(int argc, char *argv[]) {
  const char *input_bdf_font_file = NULL;
  const char *output_bdf_font_file = NULL;

  if (argc != 3) {
    return usage(argv[0]);
  }

  input_bdf_font_file = argv[1];
  output_bdf_font_file = argv[2];

  printf("input filename = %s\n", input_bdf_font_file);
  printf("Output filename = %s\n", output_bdf_font_file);

  if (!input_bdf_font_file || !*input_bdf_font_file) {
    return usage(argv[0]);
  }

  FILE *f_in = fopen(input_bdf_font_file, "r");
  if (f_in == NULL) {
    return usage(argv[0]);
  }

  FILE *f_out = fopen(output_bdf_font_file, "w");
  if (f_out == NULL) {
    return usage(argv[0]);
  }

  char buffer[1024];
  bool startchar = false;
  bool dwidth = false;
  bool bbx = false;
  Glyph g;
  int row = -1;

  while (fgets(buffer, sizeof(buffer), f_in)) {
    if (strncmp(buffer, "STARTCHAR", strlen("STARTCHAR")) == 0) {
      startchar = true;
      fputs(buffer, f_out);
      dwidth = false;
      bbx = false;
      row = -1;
    }
    else if (startchar == false) {
      fputs(buffer, f_out);
    }
    else if (sscanf(buffer, "DWIDTH %d %d", &g.device_width, &g.device_height) == 2) {
      dwidth = true;
    }
    else if (sscanf(buffer, "BBX %d %d %d %d", &g.width, &g.height, &g.x_offset, &g.y_offset) == 4) {
      g.bitmap.resize(g.height);
      bbx = true;
    }
    else if (dwidth == false || bbx == false) {
      fputs(buffer, f_out);
    }
    else if (bbx && strncmp(buffer, "BITMAP", strlen("BITMAP")) == 0) {
      row = 0;
    }
    else if (row >= 0 && row < g.height
             && parseBitmap(buffer, &g.bitmap[row])) {
      row++;
    }
    else if (strncmp(buffer, "ENDCHAR", strlen("ENDCHAR")) == 0) {
      if (row == g.height) {
        fprintf(f_out, "First %d\n", getFirstUsedColumn(g));
        fprintf(f_out, "Last %d\n", getLastUsedColumn(g));
        modifyGlyph(&g);
        // Write out current glyph
        fprintf(f_out, "DWIDTH %d %d\n", g.device_width, g.device_height);
        fprintf(f_out, "BBX %d %d %d %d\n", g.width, g.height, g.x_offset, g.y_offset);
        fprintf(f_out, "BITMAP\n");
        for(const auto &row : g.bitmap) {
          fprintf(f_out, "%s\n", composeBitmap(buffer, row, g.width));
        }
        fprintf(f_out, "First %d\n", getFirstUsedColumn(g));
        fprintf(f_out, "Last %d\n", getLastUsedColumn(g));
        fprintf(f_out, "ENDCHAR\n");
      }
      startchar = false;
    } else {
      fputs(buffer, f_out);
    }
  }
  fclose(f_in);
  fclose(f_out);

  return 0;
}
