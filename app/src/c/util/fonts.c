#include <pebble.h>
#include "fonts.h"

typedef struct {
  const char *title_font;
  uint32_t title_font_custom;
  uint16_t title_font_cap; // https://en.wikipedia.org/wiki/Cap_height
  const char *text_font;
  uint32_t text_font_custom;
  uint16_t text_font_cap;
  const char *small_font;
  uint16_t small_font_cap;
  const char *content_font;
  uint16_t content_font_cap;
  const char *small_content_font;
  uint16_t small_content_font_cap;
} FontsSpec;

FontsSpec s_specs[] = {
  [PreferredContentSizeSmall] = {
    .title_font = FONT_KEY_GOTHIC_18_BOLD,
    .title_font_cap = 11,
    .text_font  = FONT_KEY_GOTHIC_18,
    .text_font_cap = 11,
    .small_font = FONT_KEY_GOTHIC_14_BOLD,
    .small_font_cap = 9,
    .content_font = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    .content_font_cap = 18,
    .small_content_font = FONT_KEY_LECO_20_BOLD_NUMBERS,
    .small_content_font_cap = 14,
  },
  [PreferredContentSizeMedium] = {
    .title_font = FONT_KEY_GOTHIC_24_BOLD,
    .title_font_cap = 14,
    .text_font  = FONT_KEY_GOTHIC_24_BOLD,
    .text_font_cap = 14,
    .small_font = FONT_KEY_GOTHIC_18_BOLD,
    .small_font_cap = 11,
    .content_font = FONT_KEY_LECO_32_BOLD_NUMBERS,
    .content_font_cap = 22,
    .small_content_font = FONT_KEY_LECO_20_BOLD_NUMBERS,
    .small_content_font_cap = 14,
  },
  [PreferredContentSizeLarge] = {
    .title_font = FONT_KEY_GOTHIC_28_BOLD,
    .title_font_cap = 18,
    .text_font  = FONT_KEY_GOTHIC_28,
    .text_font_cap = 18,
    .small_font = FONT_KEY_GOTHIC_24_BOLD,
    .small_font_cap = 14,
    .content_font = FONT_KEY_LECO_36_BOLD_NUMBERS,
    .content_font_cap = 25,
    .small_content_font = FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,
    .small_content_font_cap = 18,
  },
#if defined(RESOURCE_ID_FONT_GOTHIC_36)
  [PreferredContentSizeExtraLarge] = {
    .title_font = NULL,
    .title_font_custom = RESOURCE_ID_FONT_GOTHIC_36_BOLD,
    .title_font_cap = 22,
    .text_font = NULL,
    .text_font_custom  = RESOURCE_ID_FONT_GOTHIC_36,
    .text_font_cap = 22,
    .small_font = FONT_KEY_GOTHIC_28_BOLD,
    .small_font_cap = 18,
    .content_font = FONT_KEY_LECO_42_NUMBERS,
    .content_font_cap = 29,
    .small_content_font = FONT_KEY_LECO_32_BOLD_NUMBERS,
    .small_content_font_cap = 22,
  },
#endif
};

static FontsConfig s_config;

static GFont prv_load_font(const char *system_key, uint32_t resource_id) {
  if (system_key) {
    return fonts_get_system_font(system_key);
  }
  return fonts_load_custom_font(resource_get_handle(resource_id));
}

static void prv_unload_font(const char *system_key, GFont font) {
  if (!system_key) {
    fonts_unload_custom_font(font);
  }
}

void fonts_load(void) {
  const FontsSpec *spec = &s_specs[preferred_content_size()];
  s_config = (FontsConfig) {
    .title_font = prv_load_font(spec->title_font, spec->title_font_custom),
    .title_font_cap = spec->title_font_cap,
    .text_font  = prv_load_font(spec->text_font, spec->text_font_custom),
    .text_font_cap = spec->text_font_cap,
    .small_font = fonts_get_system_font(spec->small_font),
    .small_font_cap = spec->small_font_cap,
    .content_font = fonts_get_system_font(spec->content_font),
    .content_font_cap = spec->content_font_cap,
    .small_content_font = fonts_get_system_font(spec->small_content_font),
    .small_content_font_cap = spec->small_content_font_cap,
  };
}

void fonts_unload(void) {
  const FontsSpec *spec = &s_specs[preferred_content_size()];
  prv_unload_font(spec->title_font, s_config.title_font);
  prv_unload_font(spec->text_font, s_config.text_font);
}

const FontsConfig *fonts_get_config(void) {
  return &s_config;
}
