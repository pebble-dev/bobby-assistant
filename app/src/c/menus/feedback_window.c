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

#include "feedback_window.h"
#include "../util/vector_sequence_layer.h"
#include "../util/result_window.h"
#include "../util/style.h"
#include "../alarms/manager.h"
#include "../version/version.h"
#include <pebble.h>
#include <pebble-events/pebble-events.h>

typedef struct {
  DictationSession *dict_session;
  ScrollLayer *scroll_layer;
  TextLayer *text_layer;
  GBitmap *select_indicator;
  BitmapLayer *select_indicator_layer;
  char *blurb;
  EventHandle event_handle;
  GDrawCommandSequence *loading_sequence;
  VectorSequenceLayer *loading_layer;
  Layer *scroll_indicator_down;
  StatusBarLayer *status_bar_layer;
} FeedbackWindowData;

static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context);
static void prv_click_config_provider();
static void prv_select_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_app_message_received(DictionaryIterator *iterator, void *context);

void feedback_window_push() {
  Window *window = window_create();
  FeedbackWindowData *data = malloc(sizeof(FeedbackWindowData));
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}

static void prv_window_load(Window *window) {
  FeedbackWindowData *data = window_get_user_data(window);
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  Layer *layer = window_get_root_layer(window);

  data->status_bar_layer = status_bar_layer_create();
  layer_add_child(layer, status_bar_layer_get_layer(data->status_bar_layer));
  bobby_status_bar_config(data->status_bar_layer);

  data->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, bounds.size.w, bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_callbacks(data->scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = prv_click_config_provider,
  });
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  scroll_layer_set_context(data->scroll_layer, window);
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  layer_add_child(layer, scroll_layer_get_layer(data->scroll_layer));
  data->scroll_indicator_down = layer_create(GRect(0, bounds.size.h - STATUS_BAR_LAYER_HEIGHT, bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  layer_add_child(layer, data->scroll_indicator_down);
  ContentIndicator* indicator = scroll_layer_get_content_indicator(data->scroll_layer);
  const ContentIndicatorConfig up_config = (ContentIndicatorConfig) {
    .layer = status_bar_layer_get_layer(data->status_bar_layer),
    .times_out = true,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = GColorBlack,
      .background = GColorWhite,
    }
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionUp, &up_config);
  const ContentIndicatorConfig down_config = (ContentIndicatorConfig) {
    .layer = data->scroll_indicator_down,
    .times_out = true,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = GColorBlack,
      .background = GColorWhite,
    },
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionDown, &down_config);

  ResHandle blurb_handle = resource_get_handle(RESOURCE_ID_FEEDBACK_BLURB);
  size_t blurb_length = resource_size(blurb_handle);
  data->blurb = malloc(blurb_length + 1);
  resource_load(blurb_handle, (uint8_t *)data->blurb, blurb_length);
  data->blurb[blurb_length] = '\0';

  data->text_layer = text_layer_create(GRect(5, 5, bounds.size.w - 10, 2000));
  text_layer_set_text(data->text_layer, data->blurb);
  text_layer_set_font(data->text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_overflow_mode(data->text_layer, GTextOverflowModeWordWrap);
  GSize text_size = text_layer_get_content_size(data->text_layer);
  layer_set_frame(text_layer_get_layer(data->text_layer), GRect(5, 5, bounds.size.w - 10, text_size.h));
  scroll_layer_add_child(data->scroll_layer, text_layer_get_layer(data->text_layer));
  scroll_layer_set_content_size(data->scroll_layer, GSize(bounds.size.w, text_size.h + 10));

  data->select_indicator = gbitmap_create_with_resource(RESOURCE_ID_BUTTON_INDICATOR);
  data->select_indicator_layer = bitmap_layer_create(GRect(bounds.size.w - 5, bounds.size.h / 2 - 10, 5, 20));
  layer_add_child(layer, bitmap_layer_get_layer(data->select_indicator_layer));
  bitmap_layer_set_bitmap(data->select_indicator_layer, data->select_indicator);
  bitmap_layer_set_compositing_mode(data->select_indicator_layer, GCompOpSet);

  data->loading_sequence = gdraw_command_sequence_create_with_resource(RESOURCE_ID_RUNNING_PONY);
  GSize pony_size = gdraw_command_sequence_get_bounds_size(data->loading_sequence);
  data->loading_layer = vector_sequence_layer_create(GRect(bounds.size.w / 2 - pony_size.w / 2, bounds.size.h / 2 - pony_size.h / 2, pony_size.w, pony_size.h));
  vector_sequence_layer_set_sequence(data->loading_layer, data->loading_sequence);

  data->dict_session = dictation_session_create(0, prv_dictation_status_callback, window);
  dictation_session_enable_error_dialogs(data->dict_session, true);
  dictation_session_enable_confirmation(data->dict_session, true);

  data->event_handle = events_app_message_register_inbox_received(prv_app_message_received, window);
}

static void prv_window_unload(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window unloading");
  FeedbackWindowData *data = window_get_user_data(window);
  dictation_session_destroy(data->dict_session);
  text_layer_destroy(data->text_layer);
  scroll_layer_destroy(data->scroll_layer);
  gbitmap_destroy(data->select_indicator);
  bitmap_layer_destroy(data->select_indicator_layer);
  gdraw_command_sequence_destroy(data->loading_sequence);
  vector_sequence_layer_destroy(data->loading_layer);
  layer_destroy(data->scroll_indicator_down);
  status_bar_layer_destroy(data->status_bar_layer);
  events_app_message_unsubscribe(data->event_handle);
  free(data->blurb);
  free(data);
  window_destroy(window);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Window unloaded");
}

static void prv_click_config_provider() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Click menu configuration");
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_clicked);
}

static void prv_select_clicked(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Click menu selection");
  Window *window = context;
  FeedbackWindowData *data = window_get_user_data(window);
  dictation_session_start(data->dict_session);
}

static void prv_dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context) {
  Window *window = context;
  FeedbackWindowData *data = window_get_user_data(window);
  if (status != DictationSessionStatusSuccess) {
    return;
  }
  layer_remove_from_parent(scroll_layer_get_layer(data->scroll_layer));
  layer_add_child(window_get_root_layer(window), vector_sequence_layer_get_layer(data->loading_layer));
  vector_sequence_layer_play(data->loading_layer);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, MESSAGE_KEY_FEEDBACK_TEXT, transcription);
  VersionInfo version = version_get_current();
  dict_write_int8(iter, MESSAGE_KEY_FEEDBACK_APP_MAJOR, version.major);
  dict_write_int8(iter, MESSAGE_KEY_FEEDBACK_APP_MINOR, version.minor);
  dict_write_int8(iter, MESSAGE_KEY_FEEDBACK_ALARM_COUNT, alarm_manager_get_alarm_count());
  app_message_outbox_send();
}

static void prv_app_message_received(DictionaryIterator *iter, void *context) {
  Window *window = context;
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_FEEDBACK_SEND_RESULT);
  if (!tuple) {
    return;
  }
  int result = tuple->value->int32;
  if (result == 0) {
    GDrawCommandImage *image = gdraw_command_image_create_with_resource(RESOURCE_ID_SENT_IMAGE);
    result_window_push("Sent", "Thank you!", image, BRANDED_BACKGROUND_COLOUR);
  } else {
    GDrawCommandImage *image = gdraw_command_image_create_with_resource(RESOURCE_ID_FAILED_PONY);
    result_window_push("Error", "There was a problem 🙁", image, COLOR_FALLBACK(GColorSunsetOrange, GColorWhite));
  }
  window_stack_remove(window, false);
}
