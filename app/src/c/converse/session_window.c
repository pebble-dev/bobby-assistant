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

#include "session_window.h"

#include "conversation.h"
#include "conversation_manager.h"
#include "segments/segment_layer.h"
#include "../util/thinking_layer.h"
#include "../util/style.h"
#include "../vibes/haptic_feedback.h"

#include <pebble.h>

#include "report_window.h"

#define PADDING 5

struct SessionWindow {
  Window* window;
  DictationSession* dictation;
  ConversationManager* manager;
  ScrollLayer* scroll_layer;
  StatusBarLayer* status_layer;
  Layer* scroll_indicator_down;
  SegmentLayer** segment_layers;
  ThinkingLayer* thinking_layer;
  GBitmap* button_bitmap;
  BitmapLayer* button_layer;
  int segment_space;
  int segment_count;
  bool dictation_pending;
  int content_height;
  int last_prompt_end_offset;
  time_t query_time;
  AppTimer *timeout_handle;
  ActionMenuLevel *action_menu;
};

// this is a stupid hack because the way the session window is pushed is also stupid for some reason
// since we only ever have one session window, we get away with it. but ew.
static int s_timeout = 0;

static void prv_window_load(Window *window);
static void prv_window_appear(Window *window);
static void prv_window_disappear(Window *window);
static void prv_window_unload(Window *window);
static void prv_destroy(SessionWindow *sw);
static void prv_dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context);
static void prv_conversation_manager_handler(bool entry_added, void* context);
static void prv_click_config_provider(void *context);
static void prv_select_clicked(ClickRecognizerRef recognizer, void *context);
static void prv_select_long_pressed(ClickRecognizerRef recognizer, void *context);
static void prv_update_thinking_layer(SessionWindow* sw);
static int16_t prv_content_height(const SessionWindow* sw);
static void prv_scrolled_handler(ScrollLayer* scroll_layer, void* context);
static void prv_refresh_timeout(SessionWindow* sw);
static void prv_timed_out(void *ctx);
static void prv_cancel_timeout(SessionWindow* sw);
static void prv_action_menu_query(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_action_menu_report_thread(ActionMenu *action_menu, const ActionMenuItem *action, void *context);

void session_window_push(int timeout) {
  Window *window = window_create();
  window_set_user_data(window, (void *)1);
  s_timeout = timeout;
  window_set_window_handlers(window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
  });
  window_stack_push(window, true);
}

static void prv_destroy(SessionWindow *sw) {
  APP_LOG(APP_LOG_LEVEL_INFO, "destroying SessionWindow %p.", sw);
  prv_cancel_timeout(sw);
  dictation_session_destroy(sw->dictation);
  conversation_manager_destroy(sw->manager);
  status_bar_layer_destroy(sw->status_layer);
  scroll_layer_destroy(sw->scroll_layer);
  bitmap_layer_destroy(sw->button_layer);
  gbitmap_destroy(sw->button_bitmap);
  layer_destroy(sw->scroll_indicator_down);
  if (sw->thinking_layer) {
    thinking_layer_destroy(sw->thinking_layer);
    sw->thinking_layer = NULL;
  }
  for (int i = 0; i < sw->segment_count; ++i) {
    segment_layer_destroy(sw->segment_layers[i]);
  }
  free(sw->segment_layers);
  window_destroy(sw->window);
  free(sw);
}

static void prv_window_load(Window *window) {
  Layer* root_layer = window_get_root_layer(window);
  bool start_dictation = (bool)window_get_user_data(window);
  GSize window_size = layer_get_frame(window_get_root_layer(window)).size;
  SessionWindow *sw = malloc(sizeof(SessionWindow));
  memset(sw, 0, sizeof(SessionWindow));
  sw->dictation_pending = start_dictation;
  APP_LOG(APP_LOG_LEVEL_INFO, "created SessionWindow %p.", sw);
  sw->window = window;
  sw->manager = conversation_manager_create();
  conversation_manager_set_handler(sw->manager, prv_conversation_manager_handler, sw);
  sw->dictation = dictation_session_create(0, prv_dictation_status_callback, sw);
  dictation_session_enable_confirmation(sw->dictation, false);

  sw->segment_space = 3;
  sw->segment_count = 0;
  sw->segment_layers = malloc(sizeof(SegmentLayer*) * sw->segment_space);

  sw->status_layer = status_bar_layer_create();
  bobby_status_bar_config(sw->status_layer);
  layer_add_child(root_layer, (Layer *)sw->status_layer);

  sw->content_height = 0;
  sw->last_prompt_end_offset = 0;
  sw->scroll_indicator_down = layer_create(GRect(0, window_size.h - STATUS_BAR_LAYER_HEIGHT, window_size.w, STATUS_BAR_LAYER_HEIGHT));
  sw->scroll_layer = scroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_size.w, window_size.h - STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_shadow_hidden(sw->scroll_layer, true);
  ContentIndicator* indicator = scroll_layer_get_content_indicator(sw->scroll_layer);
  const ContentIndicatorConfig up_config = (ContentIndicatorConfig) {
    .layer = status_bar_layer_get_layer(sw->status_layer),
    .times_out = true,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = GColorBlack,
      .background = GColorWhite,
    }
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionUp, &up_config);
  const ContentIndicatorConfig down_config = (ContentIndicatorConfig) {
    .layer = sw->scroll_indicator_down,
    .times_out = true,
    .alignment = GAlignCenter,
    .colors = {
      .foreground = GColorBlack,
      .background = GColorWhite,
    },
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionDown, &down_config);
  layer_add_child(root_layer, (Layer *)sw->scroll_layer);
  scroll_layer_set_context(sw->scroll_layer, sw);
  scroll_layer_set_callbacks(sw->scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = prv_click_config_provider,
    .content_offset_changed_handler = prv_scrolled_handler,
  });
  scroll_layer_set_click_config_onto_window(sw->scroll_layer, window);

  // This must be added after the scroll layer, to always appear on top.
  sw->button_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BUTTON_INDICATOR);
  sw->button_layer = bitmap_layer_create(GRect(window_size.w - 5, window_size.h / 2 - 10, 5, 20));
  bitmap_layer_set_bitmap(sw->button_layer, sw->button_bitmap);
  bitmap_layer_set_compositing_mode(sw->button_layer, GCompOpSet);
  layer_add_child(root_layer, (Layer *)sw->button_layer);

  // This must be added last.
  layer_add_child(root_layer, sw->scroll_indicator_down);
  window_set_user_data(sw->window, sw);
}

static void prv_window_appear(Window *window) {
  SessionWindow *sw = (SessionWindow *)window_get_user_data(window);
  if (sw->dictation_pending) {
    sw->dictation_pending = false;
    dictation_session_start(sw->dictation);
  }
}

static void prv_window_disappear(Window *window) {
  SessionWindow *sw = (SessionWindow *)window_get_user_data(window);
  prv_cancel_timeout(sw);
}

static void prv_window_unload(Window *window) {
  SessionWindow *sw = (SessionWindow *)window_get_user_data(window);
  window_set_user_data(window, (void*)0);
  prv_destroy(sw);
}

static void prv_dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcript, void *context) {
  SessionWindow *sw = context;
  switch (status) {
  case DictationSessionStatusSuccess:
    conversation_manager_add_input(sw->manager, transcript);
    sw->query_time = time(NULL);
    break;
  default:
    if (conversation_peek(conversation_manager_get_conversation(sw->manager)) == NULL) {
      window_stack_pop(true);
    }
    break;
  }
}

static void prv_set_scroll_height(SessionWindow* sw) {
  GSize old_size = scroll_layer_get_content_size(sw->scroll_layer);
  GSize new_size = GSize(old_size.w, sw->content_height + PADDING);
  if (old_size.h >= new_size.h) {
    return;
  }
  scroll_layer_set_content_size(sw->scroll_layer, new_size);
  GPoint offset = scroll_layer_get_content_offset(sw->scroll_layer);
  if (offset.y > -sw->last_prompt_end_offset) {
    int scroll_target = -sw->last_prompt_end_offset;
    scroll_layer_set_content_offset(sw->scroll_layer, GPoint(0, scroll_target), false);
  }
}

static void prv_update_thinking_layer(SessionWindow* sw) {
  ConversationEntry* entry = conversation_peek(conversation_manager_get_conversation(sw->manager));
  bool visible = false;
  if (entry != NULL) {
    EntryType entry_type = conversation_entry_get_type(entry);
    if (entry_type == EntryTypePrompt || entry_type == EntryTypeAction || entry_type == EntryTypeThought) {
      visible = true;
    } else if (entry_type == EntryTypeResponse) {
      visible = !conversation_entry_get_response(entry)->complete;
    }
  }

  if (!visible) {
    if (sw->thinking_layer) {
      sw->content_height -= THINKING_LAYER_HEIGHT + 5;
      layer_remove_from_parent(sw->thinking_layer);
      thinking_layer_destroy(sw->thinking_layer);
      sw->thinking_layer = NULL;
    }
    return;
  }

  GSize holder_size = scroll_layer_get_content_size(sw->scroll_layer);
  if (!sw->thinking_layer) {
    sw->thinking_layer = thinking_layer_create(GRect((holder_size.w - THINKING_LAYER_WIDTH) / 2, sw->content_height + 5, THINKING_LAYER_WIDTH, THINKING_LAYER_HEIGHT));
    scroll_layer_add_child(sw->scroll_layer, sw->thinking_layer);
    sw->content_height += THINKING_LAYER_HEIGHT + 5;
    return;
  }

  GRect frame = layer_get_frame(sw->thinking_layer);
  frame.origin.y = sw->content_height - THINKING_LAYER_HEIGHT - 5;
  layer_set_frame(sw->thinking_layer, frame);
}

static int16_t prv_content_height(const SessionWindow* sw) {
  if (sw->thinking_layer) {
    return sw->content_height - THINKING_LAYER_HEIGHT - 5;
  }
  return sw->content_height;
}

static void prv_conversation_manager_handler(bool entry_added, void* context) {
  SessionWindow* sw = context;
  GSize holder_size = scroll_layer_get_content_size(sw->scroll_layer);
  if (!entry_added) {
    if (sw->segment_count > 0) {
      SegmentLayer *layer = sw->segment_layers[sw->segment_count-1];
      int old_height = layer_get_frame(layer).size.h;
      segment_layer_update(layer);
      int new_height = layer_get_frame(layer).size.h;
      sw->content_height = sw->content_height - old_height + new_height;
      prv_update_thinking_layer(sw);
      prv_set_scroll_height(sw);
      light_enable_interaction();
    }
    return;
  }
  // If we have a new entry, we might just want to replace the old segment layer - we don't
  // keep old Thought segments around.
  if (sw->segment_count > 0) {
    SegmentLayer* last_layer = sw->segment_layers[sw->segment_count-1];
    EntryType type = conversation_entry_get_type(segment_layer_get_entry(last_layer));
    if (type == EntryTypeThought) {
      // clean it up
      sw->content_height -= layer_get_frame(last_layer).size.h;
      prv_update_thinking_layer(sw);
      prv_set_scroll_height(sw);
      layer_remove_from_parent(last_layer);
      segment_layer_destroy(last_layer);
      sw->segment_layers[--sw->segment_count] = NULL;
    }
  }
  Conversation *conversation = conversation_manager_get_conversation(sw->manager);
  ConversationEntry* entry = conversation_peek(conversation);
  if (entry == NULL) {
    // ??????
    APP_LOG(APP_LOG_LEVEL_ERROR, "We were told a new entry was added, but no entries actually exist????");
    return;
  }
  if (sw->segment_count == sw->segment_space) {
    // make room for a new segment
    SegmentLayer** new_block = malloc(sizeof(SegmentLayer*) * ++sw->segment_space);
    memcpy(new_block, sw->segment_layers, sizeof(SegmentLayer*) * sw->segment_count);
    free(sw->segment_layers);
    sw->segment_layers = new_block;
  }
  SegmentLayer* layer = segment_layer_create(GRect(0, prv_content_height(sw), holder_size.w, 10), entry, conversation_assistant_just_started(conversation));
  sw->segment_layers[sw->segment_count++] = layer;
  scroll_layer_add_child(sw->scroll_layer, layer);
  int layer_height = layer_get_frame(layer).size.h;
  sw->content_height += layer_height;
  EntryType entry_type = conversation_entry_get_type(entry);
  if (entry_type == EntryTypePrompt) {
    sw->last_prompt_end_offset = prv_content_height(sw);
  }
  prv_update_thinking_layer(sw);
  prv_set_scroll_height(sw);
  light_enable_interaction();
  prv_refresh_timeout(sw);
  // For responses that took longer than five seconds, pulse the vibe when we get useful data.
  switch (entry_type) {
    case EntryTypeResponse:
    case EntryTypeWidget:
    case EntryTypeAction:
    case EntryTypeError:
      if (sw->query_time > 0) {
        if (time(NULL) >= sw->query_time + 5) {
          vibe_haptic_feedback();
        }
        sw->query_time = 0;
      }
      break;
    case EntryTypePrompt:
    case EntryTypeThought:
      // nothing to do here.
      break;
  }
  if (entry_type == EntryTypeResponse || entry_type == EntryTypeWidget || entry_type == EntryTypeAction) {

  }
  // For now, whenever we add a new entry, we want to scroll to the top of it.
//  scroll_layer_set_content_offset(sw->scroll_layer, GPoint(0, layer_get_frame(layer).origin.y), true);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_clicked);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_pressed, NULL);
}

static void prv_select_clicked(ClickRecognizerRef recognizer, void *context) {
  SessionWindow* sw = context;
  if (conversation_is_idle(conversation_manager_get_conversation(sw->manager))) {
    dictation_session_start(sw->dictation);
  }
}

static void prv_select_long_pressed(ClickRecognizerRef recognizer, void *context) {
  SessionWindow* sw = context;
  if (!conversation_is_idle(conversation_manager_get_conversation(sw->manager))) {
    return;
  }
  ActionMenuLevel *action_menu = action_menu_level_create(2);
  action_menu_level_add_action(action_menu, "Prompt", prv_action_menu_query, sw);
  action_menu_level_add_action(action_menu, "Report conversation", prv_action_menu_report_thread, sw);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = action_menu,
    .colors = {
      .background = BRANDED_BACKGROUND_COLOUR,
      .foreground = gcolor_legible_over(BRANDED_BACKGROUND_COLOUR),
    },
    .align = ActionMenuAlignCenter,
    .context = sw
  };
  vibe_haptic_feedback();
  action_menu_open(&config);
}

static void prv_action_menu_query(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  SessionWindow* sw = context;
  dictation_session_start(sw->dictation);
  action_menu_hierarchy_destroy(sw->action_menu, NULL, NULL);
  sw->action_menu = NULL;
}

static void prv_action_menu_report_thread(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  SessionWindow* sw = context;
  action_menu_hierarchy_destroy(sw->action_menu, NULL, NULL);
  sw->action_menu = NULL;
  report_window_push(conversation_get_thread_id(conversation_manager_get_conversation(sw->manager)));
}

static void prv_scrolled_handler(ScrollLayer* scroll_layer, void* context) {
  SessionWindow* sw = context;
  prv_refresh_timeout(sw);
}

static void prv_refresh_timeout(SessionWindow* sw) {
  if (s_timeout == 0) {
    return;
  }
  if (sw->timeout_handle) {
    app_timer_cancel(sw->timeout_handle);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Refreshed timeout");
  sw->timeout_handle = app_timer_register(s_timeout, prv_timed_out, sw);
}

static void prv_cancel_timeout(SessionWindow* sw) {
  if (sw->timeout_handle) {
    app_timer_cancel(sw->timeout_handle);
    sw->timeout_handle = NULL;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Canceled timeout");
  }
}

static void prv_timed_out(void *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Timed out");
  window_stack_pop(true);
}
