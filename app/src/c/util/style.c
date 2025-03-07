//
// Created by Katharine Berry on 3/5/25.
//

#include <pebble.h>
#include "style.h"

void bobby_status_bar_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, GColorWhite, GColorBlack);
  status_bar_layer_set_separator_mode(status_bar, StatusBarLayerSeparatorModeDotted);
}

void bobby_status_bar_result_pane_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, COLOR_FALLBACK(ACCENT_COLOUR, GColorWhite), gcolor_legible_over(ACCENT_COLOUR));
  status_bar_layer_set_separator_mode(status_bar, PBL_IF_COLOR_ELSE(StatusBarLayerSeparatorModeNone, StatusBarLayerSeparatorModeDotted));
}