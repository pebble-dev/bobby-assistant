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
#include "../settings/settings.h"
#include "../util/thinking_layer.h"
#include "../util/style.h"
#include "../util/action_menu_crimes.h"
#include "../util/logging.h"
#include "../util/memory/malloc.h"
#include "../util/memory/sdk.h"
#include "../vibes/haptic_feedback.h"
#include "../features.h"

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
  int segments_deleted;
  bool dictation_pending;
  int content_height;
  int last_prompt_end_offset;
  time_t query_time;
  AppTimer *timeout_handle;
  int timeout;
  char* starting_prompt;
  char* last_prompt_label;
};

static void prv_window_load(Window *window);
static void prv_window_appear(Window *window);
static void prv_window_disappear(Window *window);
static void prv_window_unload(Window *window);
static void prv_destroy(SessionWindow *sw);
static void prv_dictation_status_callback(DictationSession *session, DictationSessionStatus status, char *transcription, void *context);
static void prv_conversation_manager_handler(bool entry_added, void* context);
static void prv_conversation_entry_deleted_handler(int index, void* context);
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
static void prv_action_menu_input(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_action_menu_report_thread(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_start_dictation(SessionWindow *sw);

void session_window_push(int timeout, char *starting_prompt) {
  Window *window = bwindow_create();
  SessionWindow *sw = bmalloc(sizeof(SessionWindow));
  memset(sw, 0, sizeof(SessionWindow));
  window_set_user_data(window, sw);
  sw->window = window;
  sw->timeout = timeout;
  if (starting_prompt != NULL) {
    sw->starting_prompt = bmalloc(strlen(starting_prompt) + 1);
    strncpy(sw->starting_prompt, starting_prompt, strlen(starting_prompt) + 1);
  }
  window_set_window_handlers(window, (WindowHandlers) {
      .load = prv_window_load,
      .unload = prv_window_unload,
      .appear = prv_window_appear,
      .disappear = prv_window_disappear,
  });
  window_stack_push(window, true);
}

static void prv_destroy(SessionWindow *sw) {
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "destroying SessionWindow %p.", sw);
  prv_cancel_timeout(sw);
  dictation_session_destroy(sw->dictation);
  for (int i = sw->segments_deleted; i < sw->segment_count; ++i) {
    segment_layer_destroy(sw->segment_layers[i]);
  }
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
  free(sw->segment_layers);
  window_destroy(sw->window);
  if (sw->starting_prompt) {
    free(sw->starting_prompt);
  }
  free(sw);
}

static void prv_window_load(Window *window) {
  Layer* root_layer = window_get_root_layer(window);
  GSize window_size = layer_get_frame(window_get_root_layer(window)).size;
  SessionWindow *sw = window_get_user_data(window);
  sw->dictation_pending = true;
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "created SessionWindow %p.", sw);
  sw->manager = conversation_manager_create();
  conversation_manager_set_handler(sw->manager, prv_conversation_manager_handler, sw);
  conversation_manager_set_deletion_handler(sw->manager, prv_conversation_entry_deleted_handler);
  sw->dictation = dictation_session_create(0, prv_dictation_status_callback, sw);
  dictation_session_enable_confirmation(sw->dictation, settings_get_should_confirm_transcripts());

  sw->segment_space = 3;
  sw->segment_count = 0;
  sw->segments_deleted = 0;
  sw->segment_layers = bmalloc(sizeof(SegmentLayer*) * sw->segment_space);

  sw->status_layer = bstatus_bar_layer_create();
  bobby_status_bar_config(sw->status_layer);
  layer_add_child(root_layer, (Layer *)sw->status_layer);

  sw->content_height = 0;
  sw->last_prompt_end_offset = 0;
  sw->scroll_indicator_down = blayer_create(GRect(0, window_size.h - STATUS_BAR_LAYER_HEIGHT, window_size.w, STATUS_BAR_LAYER_HEIGHT));
  sw->scroll_layer = bscroll_layer_create(GRect(0, STATUS_BAR_LAYER_HEIGHT, window_size.w, window_size.h - STATUS_BAR_LAYER_HEIGHT));
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
  sw->button_bitmap = bgbitmap_create_with_resource(RESOURCE_ID_BUTTON_INDICATOR);
  sw->button_layer = bbitmap_layer_create(GRect(window_size.w - 5, window_size.h / 2 - 10, 5, 20));
  bitmap_layer_set_bitmap(sw->button_layer, sw->button_bitmap);
  bitmap_layer_set_compositing_mode(sw->button_layer, GCompOpSet);
  layer_add_child(root_layer, (Layer *)sw->button_layer);

  // This must be added last.
  layer_add_child(root_layer, sw->scroll_indicator_down);
  window_set_user_data(sw->window, sw);
}

static void prv_window_appear(Window *window) {
  SessionWindow *sw = (SessionWindow *)window_get_user_data(window);
  if (sw->starting_prompt) {
    conversation_manager_add_input(sw->manager, sw->starting_prompt);
    sw->query_time = time(NULL);
    free(sw->starting_prompt);
    sw->starting_prompt = NULL;
    sw->dictation_pending = false;
  }
  if (sw->dictation_pending) {
    sw->dictation_pending = false;
    prv_start_dictation(sw);
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
    } else if (entry_type == EntryTypeWidget) {
      // Locally created widgets should still have a response coming after.
      visible = conversation_entry_get_widget(entry)->locally_created;
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
    if (sw->segment_count > sw->segments_deleted) {
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
  Conversation *conversation = conversation_manager_get_conversation(sw->manager);
  // If we have a new entry, we might just want to replace the old segment layer - we don't
  // keep old Thought segments around.
  if (sw->segment_count > sw->segments_deleted) {
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
      conversation_delete_last_thought(conversation);
    }
  }
  ConversationEntry* entry = conversation_peek(conversation);
  if (entry == NULL) {
    // ??????
    BOBBY_LOG(APP_LOG_LEVEL_ERROR, "We were told a new entry was added, but no entries actually exist????");
    return;
  }
  if (sw->segment_count == sw->segment_space) {
    // make room for a new segment
    SegmentLayer** new_block = bmalloc(sizeof(SegmentLayer*) * ++sw->segment_space);
    memcpy(new_block, sw->segment_layers, sizeof(SegmentLayer*) * sw->segment_count);
    free(sw->segment_layers);
    sw->segment_layers = new_block;
  }
  SegmentLayer* layer = segment_layer_create(GRect(0, prv_content_height(sw), holder_size.w, 10), entry, conversation_assistant_just_started(conversation));
  sw->segment_layers[sw->segment_count++] = layer;
  // It's possible that the content height changed *while the layer was being created*. In case this happened, move the
  // layer back to where it should be. Because segment layers are expected to adjust their own frame during
  // construction, we must read its size back first.
  GRect frame = layer_get_frame(layer);
  frame.origin.y = prv_content_height(sw);
  layer_set_frame(layer, frame);
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
    case EntryTypeDeleted:
      // nothing to do here.
      break;
  }
  if (entry_type == EntryTypeResponse || entry_type == EntryTypeWidget || entry_type == EntryTypeAction) {

  }
  // For now, whenever we add a new entry, we want to scroll to the top of it.
//  scroll_layer_set_content_offset(sw->scroll_layer, GPoint(0, layer_get_frame(layer).origin.y), true);
}

static void prv_conversation_entry_deleted_handler(int index, void* context) {
  if (index != 0) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Invalid index %d", index);
    return;
  }
  SessionWindow* sw = context;
  // We need to go through every segment that's left and shift it up by the height of the deleted segment
  SegmentLayer *to_delete = sw->segment_layers[sw->segments_deleted];
  int16_t removed_height = layer_get_frame(to_delete).size.h;
  for (int i = sw->segments_deleted + 1; i < sw->segment_count; ++i) {
    if (sw->segment_layers[i] == NULL) {
      BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Segment layer %d is NULL (not possible!?)", i);
      continue;
    }
    SegmentLayer *layer = sw->segment_layers[i];
    GRect frame = layer_get_frame(layer);
    frame.origin.y -= removed_height;
    layer_set_frame(layer, frame);
  }
  // We need to adjust the height of everything to compensate for the missing segment.
  sw->content_height -= removed_height;
  GPoint current_offset = scroll_layer_get_content_offset(sw->scroll_layer);
  GPoint new_offset = GPoint(current_offset.x, current_offset.y - removed_height);
  scroll_layer_set_content_offset(sw->scroll_layer, new_offset, false);
  GSize current_size = scroll_layer_get_content_size(sw->scroll_layer);
  GSize new_size = GSize(current_size.w, current_size.h - removed_height);
  scroll_layer_set_content_size(sw->scroll_layer, new_size);
  // We need to remove our first segment.
  layer_remove_from_parent(to_delete);
  segment_layer_destroy(sw->segment_layers[sw->segments_deleted]);
  sw->segment_layers[sw->segments_deleted] = NULL;
  sw->segments_deleted++;
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Removed top segment; adjusted upward by %d pixels.", removed_height);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_clicked);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_pressed, NULL);
}

static void prv_select_clicked(ClickRecognizerRef recognizer, void *context) {
  SessionWindow* sw = context;
  if (conversation_is_idle(conversation_manager_get_conversation(sw->manager))) {
    prv_start_dictation(sw);
  }
}

static void prv_destroy_action_menu(ActionMenu *action_menu, const ActionMenuItem *item, void *context) {
  SessionWindow *sw = context;
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
  if (sw->last_prompt_label) {
    free(sw->last_prompt_label);
    sw->last_prompt_label = NULL;
  }
}

static void prv_select_long_pressed(ClickRecognizerRef recognizer, void *context) {
  SessionWindow* sw = context;
  if (!conversation_is_idle(conversation_manager_get_conversation(sw->manager))) {
    return;
  }
  ActionMenuLevel *action_menu = baction_menu_level_create(5);
  action_menu_level_add_action(action_menu, "\"Yes.\"", prv_action_menu_input, "Yes.");
  action_menu_level_add_action(action_menu, "\"No.\"", prv_action_menu_input, "No.");
  Conversation *conversation = conversation_manager_get_conversation(sw->manager);
  ConversationEntry *entry = conversation_peek(conversation);
  EntryType type = conversation_entry_get_type(entry);
  int separator_index = 3;
  if (type == EntryTypeError) {
    ConversationEntry *last_prompt = conversation_get_last_of_type(conversation, EntryTypePrompt);
    if (last_prompt != NULL) {
      ConversationPrompt *prompt = conversation_entry_get_prompt(last_prompt);
      sw->last_prompt_label = bmalloc(strlen(prompt->prompt) + 3);
      snprintf(sw->last_prompt_label, strlen(prompt->prompt) + 3, "\"%s\"", prompt->prompt);
      action_menu_level_add_action(action_menu, sw->last_prompt_label, prv_action_menu_input, prompt->prompt);
      separator_index++;
    }
  }
  action_menu_level_add_action(action_menu, "Dictate", prv_action_menu_query, NULL);
  action_menu_level_set_separator_index(action_menu, separator_index);
  action_menu_level_add_action(action_menu, "Report conversation", prv_action_menu_report_thread, NULL);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = action_menu,
    .colors = {
      .background = BRANDED_BACKGROUND_COLOUR,
      .foreground = gcolor_legible_over(BRANDED_BACKGROUND_COLOUR),
    },
    .align = ActionMenuAlignTop,
    .context = sw,
    .did_close = prv_destroy_action_menu,
  };
  vibe_haptic_feedback();
  sw->query_time = time(NULL);
  free(bmalloc(750));
  action_menu_open(&config);
}

static void prv_action_menu_query(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  SessionWindow* sw = context;
  prv_start_dictation(sw);
}


static void prv_action_menu_input(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  SessionWindow* sw = context;
  const char* input = action_menu_item_get_action_data(action);
  conversation_manager_add_input(sw->manager, input);
  sw->query_time = time(NULL);
}

static void prv_action_menu_report_thread(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  SessionWindow* sw = context;
  report_window_push(conversation_get_thread_id(conversation_manager_get_conversation(sw->manager)));
}

static void prv_scrolled_handler(ScrollLayer* scroll_layer, void* context) {
  SessionWindow* sw = context;
  prv_refresh_timeout(sw);
}

static void prv_refresh_timeout(SessionWindow* sw) {
  if (sw->timeout == 0) {
    return;
  }
  if (sw->timeout_handle) {
    app_timer_cancel(sw->timeout_handle);
  }
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Refreshed timeout");
  sw->timeout_handle = app_timer_register(sw->timeout, prv_timed_out, sw);
}

static void prv_cancel_timeout(SessionWindow* sw) {
  if (sw->timeout_handle) {
    app_timer_cancel(sw->timeout_handle);
    sw->timeout_handle = NULL;
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Canceled timeout");
  }
}

static void prv_timed_out(void *ctx) {
  BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "Timed out");
  window_stack_pop(true);
}

static void prv_start_dictation(SessionWindow *sw) {
  // Dictation needs a ridiculous amount of memory to behave properly.
  free(bmalloc(2048));
#if !ENABLE_FEATURE_FIXED_PROMPT
  dictation_session_start(sw->dictation);
#else
  // skip this, just send some nonsense.
  prv_dictation_status_callback(sw->dictation, DictationSessionStatusSuccess, "This is just a test message", sw);
#endif
}
