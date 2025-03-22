/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "reminders_menu.h"
#include "../util/style.h"
#include "../util/vector_sequence_layer.h"
#include "../util/vector_layer.h"
#include "../util/time.h"
#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  char *text;
  char *id;
  time_t time;
} Reminder;

typedef struct {
  Window *window;
  MenuLayer *menu_layer;
  StatusBarLayer *status_bar;
  VectorSequenceLayer *loading_layer;
  GDrawCommandSequence *loading_sequence;
  TextLayer *empty_text_layer;
  GDrawCommandImage *sleeping_horse_image;
  VectorLayer *sleeping_horse_layer;
  EventHandle app_message_handle;
  Reminder *reminders;
  uint16_t num_reminders;
  uint16_t reminders_capacity;
  bool loading;
} RemindersMenuData;

static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_fetch_reminders(Window *window);
static void prv_app_message_received(DictionaryIterator *iter, void *context);
static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context);
static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context);
static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context);
static void prv_delete_reminder_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_show_empty(Window *window);
static void prv_action_menu_did_close(ActionMenu *action_menu, const ActionMenuItem *menu_item, void *context);

void reminders_menu_push() {
  Window *window = window_create();
  RemindersMenuData *data = malloc(sizeof(RemindersMenuData));
  memset(data, 0, sizeof(RemindersMenuData));
  data->window = window;
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}

static void prv_show_empty(Window *window) {
  RemindersMenuData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  // Remove the menu and loading animation if present
  layer_remove_from_parent(menu_layer_get_layer(data->menu_layer));
  if (data->loading_layer) {
    vector_sequence_layer_stop(data->loading_layer);
    layer_remove_from_parent(data->loading_layer);
  }
  window_set_click_config_provider(window, NULL);

  // Create empty state text if not exists
  if (!data->empty_text_layer) {
    data->empty_text_layer = text_layer_create(GRect(10, 20, bounds.size.w - 20, 80));
    text_layer_set_text_color(data->empty_text_layer, gcolor_legible_over(ACCENT_COLOUR));
    text_layer_set_background_color(data->empty_text_layer, GColorClear);
    text_layer_set_font(data->empty_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(data->empty_text_layer, GTextAlignmentCenter);
    text_layer_set_text(data->empty_text_layer, "No reminders.\nAsk Bobby to set some.");
  }

  // Create sleeping horse if not exists
  if (!data->sleeping_horse_image) {
    data->sleeping_horse_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SLEEPING_PONY);
    data->sleeping_horse_layer = vector_layer_create(GRect(bounds.size.w / 2 - 25, bounds.size.h - 55, 50, 50));
    vector_layer_set_vector(data->sleeping_horse_layer, data->sleeping_horse_image);
  }

  window_set_background_color(window, BRANDED_BACKGROUND_COLOUR);
  bobby_status_bar_result_pane_config(data->status_bar);
  layer_add_child(root_layer, text_layer_get_layer(data->empty_text_layer));
  layer_add_child(root_layer, vector_layer_get_layer(data->sleeping_horse_layer));
}

static void prv_window_load(Window *window) {
  RemindersMenuData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root_layer);

  window_set_background_color(window, BRANDED_BACKGROUND_COLOUR);

  data->status_bar = status_bar_layer_create();
  bobby_status_bar_result_pane_config(data->status_bar);
  layer_add_child(root_layer, status_bar_layer_get_layer(data->status_bar));

  // Create menu layer but don't add it yet
  data->menu_layer = menu_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, 
    bounds.size.w, bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  menu_layer_set_callbacks(data->menu_layer, window, (MenuLayerCallbacks) {
    .get_num_rows = prv_get_num_rows,
    .draw_row = prv_draw_row,
    .select_click = prv_select_click,
  });
  menu_layer_set_highlight_colors(data->menu_layer, SELECTION_HIGHLIGHT_COLOUR, 
    gcolor_legible_over(SELECTION_HIGHLIGHT_COLOUR));

  // Show loading animation
  data->loading_sequence = gdraw_command_sequence_create_with_resource(RESOURCE_ID_RUNNING_PONY);
  GSize pony_size = gdraw_command_sequence_get_bounds_size(data->loading_sequence);
  data->loading_layer = vector_sequence_layer_create(GRect(
    bounds.size.w / 2 - pony_size.w / 2,
    bounds.size.h / 2 - pony_size.h / 2,
    pony_size.w, pony_size.h));
  vector_sequence_layer_set_sequence(data->loading_layer, data->loading_sequence);
  layer_add_child(root_layer, data->loading_layer);
  vector_sequence_layer_play(data->loading_layer);

  data->loading = true;
  data->app_message_handle = events_app_message_register_inbox_received(prv_app_message_received, window);
  prv_fetch_reminders(window);
}

static void prv_window_unload(Window *window) {
  RemindersMenuData *data = window_get_user_data(window);
  menu_layer_destroy(data->menu_layer);
  status_bar_layer_destroy(data->status_bar);
  if (data->loading_layer) {
    vector_sequence_layer_destroy(data->loading_layer);
    gdraw_command_sequence_destroy(data->loading_sequence);
  }
  if (data->empty_text_layer) {
    text_layer_destroy(data->empty_text_layer);
  }
  if (data->sleeping_horse_layer) {
    vector_layer_destroy(data->sleeping_horse_layer);
    gdraw_command_image_destroy(data->sleeping_horse_image);
  }
  events_app_message_unsubscribe(data->app_message_handle);
  
  // Free all reminder texts and the reminders array
  for (uint16_t i = 0; i < data->num_reminders; i++) {
    free(data->reminders[i].text);
    free(data->reminders[i].id);
  }
  free(data->reminders);
  
  free(data);
  window_destroy(window);
}

static void prv_fetch_reminders(Window *window) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_REMINDER_LIST_REQUEST, 1);
  app_message_outbox_send();
}

static void prv_app_message_received(DictionaryIterator *iter, void *context) {
  Window *window = (Window *)context;
  RemindersMenuData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);

  Tuple *count_tuple = dict_find(iter, MESSAGE_KEY_REMINDER_COUNT);
  if (count_tuple) {
    // Allocate space for reminders
    data->reminders_capacity = count_tuple->value->uint16;
    if (data->reminders_capacity == 0) {
      // No reminders - show empty state
      prv_show_empty(window);
      return;
    }
    data->reminders = malloc(sizeof(Reminder) * data->reminders_capacity);
    data->num_reminders = 0;
    return;
  }

  Tuple *text_tuple = dict_find(iter, MESSAGE_KEY_REMINDER_TEXT);
  Tuple *id_tuple = dict_find(iter, MESSAGE_KEY_REMINDER_ID);
  Tuple *time_tuple = dict_find(iter, MESSAGE_KEY_REMINDER_TIME);

  if (text_tuple && id_tuple && time_tuple && data->num_reminders < data->reminders_capacity) {
    // Allocate and copy the text
    size_t text_len = strlen(text_tuple->value->cstring) + 1;
    data->reminders[data->num_reminders].text = malloc(text_len);
    strncpy(data->reminders[data->num_reminders].text, text_tuple->value->cstring, text_len);

    // Allocate and copy the id
    size_t id_len = strlen(id_tuple->value->cstring) + 1;
    data->reminders[data->num_reminders].id = malloc(id_len);
    strncpy(data->reminders[data->num_reminders].id, id_tuple->value->cstring, id_len);
    
    // Copy the time
    data->reminders[data->num_reminders].time = (time_t)time_tuple->value->int32;
    data->num_reminders++;

    // If we've received all reminders, show the menu
    if (data->num_reminders == data->reminders_capacity) {
      window_set_background_color(window, GColorWhite);
      bobby_status_bar_config(data->status_bar);
      data->loading = false;
      vector_sequence_layer_stop(data->loading_layer);
      layer_remove_from_parent(data->loading_layer);
      layer_add_child(root_layer, menu_layer_get_layer(data->menu_layer));
      menu_layer_set_click_config_onto_window(data->menu_layer, window);
      menu_layer_reload_data(data->menu_layer);
    }
  }
}

static uint16_t prv_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  RemindersMenuData *data = window_get_user_data(context);
  return data->num_reminders;
}

static void prv_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  RemindersMenuData *data = window_get_user_data(context);
  Reminder *reminder = &data->reminders[cell_index->row];

  char time_text[32];
  format_datetime(time_text, sizeof(time_text), reminder->time);

  GRect bounds = layer_get_bounds(cell_layer);
  graphics_draw_text(ctx, reminder->text,
    fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
    GRect(4, -4, bounds.size.w - 8, 24),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
  graphics_draw_text(ctx, time_text,
    fonts_get_system_font(FONT_KEY_GOTHIC_18),
    GRect(4, 20, bounds.size.w - 8, 18),
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentLeft,
    NULL);
}

static void prv_action_menu_did_close(ActionMenu *action_menu, const ActionMenuItem *menu_item, void *context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
}

static void prv_select_click(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  RemindersMenuData *data = window_get_user_data(context);
  Reminder *reminder = &data->reminders[cell_index->row];
  
  // Show action menu for deletion
  ActionMenuLevel *root_level = action_menu_level_create(1);
  action_menu_level_add_action(root_level, "Delete", prv_delete_reminder_callback, reminder);
  
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = BRANDED_BACKGROUND_COLOUR,
      .foreground = gcolor_legible_over(BRANDED_BACKGROUND_COLOUR),
    },
    .align = ActionMenuAlignCenter,
    .context = data,
    .did_close = prv_action_menu_did_close,
  };
  
  action_menu_open(&config);
}

static void prv_delete_reminder_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  Reminder *reminder = (Reminder *)action_menu_item_get_action_data(action);
  RemindersMenuData *data = action_menu_get_context(action_menu);
  
  // Send delete message to phone
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, MESSAGE_KEY_REMINDER_DELETE, reminder->id);
  app_message_outbox_send();
  
  // Update locally
  for (uint16_t i = 0; i < data->num_reminders; i++) {
    if (strcmp(data->reminders[i].id, reminder->id) == 0) {
      // Free the text of the deleted reminder
      free(data->reminders[i].text);
      free(data->reminders[i].id);
      // Move remaining reminders up
      memmove(&data->reminders[i], &data->reminders[i + 1], 
              (data->num_reminders - i - 1) * sizeof(Reminder));
      data->num_reminders--;
      
      if (data->num_reminders == 0) {
        prv_show_empty(data->window);
      } else {
        menu_layer_reload_data(data->menu_layer);
      }
      break;
    }
  }
} 