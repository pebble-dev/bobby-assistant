#pragma once
#include <pebble.h>

typedef struct {
  GFont title_font;
  uint16_t title_font_cap;
  GFont text_font;
  uint16_t text_font_cap;
  GFont small_font;
  uint16_t small_font_cap;
  GFont content_font;
  uint16_t content_font_cap;
  GFont small_content_font;
  uint16_t small_content_font_cap;
} FontsConfig;

void fonts_load(void);
void fonts_unload(void);
const FontsConfig *fonts_get_config(void);
