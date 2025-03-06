//
// Created by Katharine Berry on 3/5/25.
//

#include <pebble.h>
#include "style.h"

void bobby_status_bar_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, STATUS_BAR_COLOUR_BG, STATUS_BAR_COLOUR_FG);
  status_bar_layer_set_separator_mode(status_bar, STATUS_BAR_SEPARATOR);
}

void bobby_status_bar_menu_screen_config(StatusBarLayer *status_bar) {
  status_bar_layer_set_colors(status_bar, STATUS_BAR_COLOUR_BG, STATUS_BAR_COLOUR_FG);
  status_bar_layer_set_separator_mode(status_bar, STATUS_BAR_SEPARATOR);
}