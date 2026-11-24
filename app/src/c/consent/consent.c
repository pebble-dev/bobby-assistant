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

#include "consent.h"
#include "../util/fonts.h"
#include "../util/persist_keys.h"
#include "../util/style.h"
#include "../util/logging.h"
#include "../util/memory/malloc.h"
#include "../util/memory/sdk.h"
#include "../version/version.h"
#include "../root_window.h"

#include <pebble.h>
#include <pebble-events/pebble-events.h>


#define MAX_CONSENT_PAGES 8
#define LOAD_TIMEOUT_MS 15000
#define REQUEST_RETRY_MS 1000

typedef enum {
  ConsentStateIntro,
  ConsentStateLoading,
  ConsentStateError,
  ConsentStatePage,
} ConsentState;

typedef struct {
  char *title;
  char *text;
} ConsentPage;

typedef struct {
  ScrollLayer *scroll_layer;
  TextLayer *title_layer;
  TextLayer *text_layer;
  Layer *content_indicator_layer;
  GBitmap* select_indicator_bitmap;
  BitmapLayer* select_indicator_layer;
  ActionMenu* action_menu;
  ConsentState state;
  // Index of the page being shown, when state is ConsentStatePage.
  int page;
  ConsentPage *pages;
  int page_count;
  int pages_received;
  int consent_version;
  AppTimer *load_timeout;
  AppTimer *request_retry;
  bool awaiting_location_reply;
  bool accepted;
  bool show_root_after;
  ConsentCompleteCallback callback;
  void *callback_context;
  EventHandle app_message_handle;
} ConsentWindowData;

static EventHandle s_required_version_handle;

static void prv_window_push(bool show_intro, bool show_root_after, ConsentCompleteCallback callback, void *context);
static void prv_window_load(Window *window);
static void prv_window_unload(Window *window);
static void prv_click_config_provider(void *context);
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context);
static bool prv_did_scroll_to_bottom(Window* window);
static void prv_show_text(Window *window, const char *title, const char *text);
static void prv_set_state(Window *window, ConsentState state);
static void prv_show_page(Window *window, int page);
static void prv_start_loading(Window *window);
static void prv_send_request(void *context);
static void prv_load_timed_out(void *context);
static void prv_free_pages(ConsentWindowData *data);
static void prv_present_consent_menu(Window* window);
static void prv_consent_menu_select_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context);
static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context);
static void prv_app_message_handler(DictionaryIterator *iter, void *context);
static void prv_app_message_failed(DictionaryIterator *iter, AppMessageResult reason, void *context);
static void prv_retry_request_soon(Window *window);
static void prv_handle_page(Window *window, DictionaryIterator *iter);
static void prv_handle_location_reply(Window *window, Tuple *tuple);
static void prv_required_version_handler(DictionaryIterator *iter, void *context);

void consent_init() {
  s_required_version_handle = events_app_message_register_inbox_received(prv_required_version_handler, NULL);
}

static void prv_required_version_handler(DictionaryIterator *iter, void *context) {
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_CONSENT_REQUIRED_VERSION);
  if (tuple == NULL) {
    return;
  }
  int required = tuple->value->int32;
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "Service requires consent version %d.", required);
  persist_write_int(PERSIST_KEY_CONSENT_REQUIRED_VERSION, required);
}

int consent_accepted_version() {
  return persist_read_int(PERSIST_KEY_CONSENTS_COMPLETED);
}

static int prv_required_version() {
  if (!persist_exists(PERSIST_KEY_CONSENT_REQUIRED_VERSION)) {
    return 1;
  }
  int required = persist_read_int(PERSIST_KEY_CONSENT_REQUIRED_VERSION);
  return required < 1 ? 1 : required;
}

bool must_present_consent() {
  return consent_accepted_version() < prv_required_version();
}

void consent_mark_outdated() {
  int accepted = consent_accepted_version();
  if (prv_required_version() <= accepted) {
    persist_write_int(PERSIST_KEY_CONSENT_REQUIRED_VERSION, accepted + 1);
  }
}

void consent_window_push_for_launch() {
  prv_window_push(false, true, NULL, NULL);
}

void consent_window_push_for_prompt(ConsentCompleteCallback callback, void *context) {
  vibes_double_pulse();
  prv_window_push(true, false, callback, context);
}

static void prv_window_push(bool show_intro, bool show_root_after, ConsentCompleteCallback callback, void *context) {
  Window* window = bwindow_create();
  ConsentWindowData* data = bmalloc(sizeof(ConsentWindowData));
  memset(data, 0, sizeof(ConsentWindowData));
  data->state = show_intro ? ConsentStateIntro : ConsentStateLoading;
  data->show_root_after = show_root_after;
  data->callback = callback;
  data->callback_context = context;
  window_set_user_data(window, data);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(window, true);
}

void consent_migrate() {
  if (version_is_updated() && !version_is_first_launch()) {
    // If we're updating from version 1.1 or older, consent agreement was implied by LOCATION_ENABLED being set
    // (either true or false).
    if (version_info_compare(version_get_last_launch(), (VersionInfo) {1, 1}) <= 0) {
      BOBBY_LOG(APP_LOG_LEVEL_INFO, "Performing consent migration from version 1.1.");
      // If the location enabled state is set, that's equivalent to consent agreement version 1.
      if (persist_exists(PERSIST_KEY_LOCATION_ENABLED)) {
        BOBBY_LOG(APP_LOG_LEVEL_INFO, "Marking consent as 1.");;
        persist_write_int(PERSIST_KEY_CONSENTS_COMPLETED, 1);
      } else {
        BOBBY_LOG(APP_LOG_LEVEL_INFO, "Not marking consent.");;
      }
    }
  }
}

static void prv_window_load(Window *window) {
  ConsentWindowData *data = window_get_user_data(window);
  Layer *root_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_frame(root_layer);
  const FontsConfig *fonts = fonts_get_config();
  data->scroll_layer = bscroll_layer_create(window_bounds);
  scroll_layer_set_click_config_onto_window(data->scroll_layer, window);
  data->title_layer = btext_layer_create(GRect(0, 0, window_bounds.size.w, 60));
  text_layer_set_text_alignment(data->title_layer, GTextAlignmentCenter);
  text_layer_set_font(data->title_layer, fonts->title_font);
  data->text_layer = btext_layer_create(GRect(10, 30, window_bounds.size.w - 20, window_bounds.size.h - 30));
  text_layer_set_font(data->text_layer, fonts->text_font);
  data->select_indicator_bitmap = bgbitmap_create_with_resource(RESOURCE_ID_BUTTON_INDICATOR);
  GRect select_indicator_size = gbitmap_get_bounds(data->select_indicator_bitmap);
  grect_align(&select_indicator_size, &window_bounds, GAlignRight, false);
  data->select_indicator_layer = bbitmap_layer_create(select_indicator_size);
  bitmap_layer_set_bitmap(data->select_indicator_layer, data->select_indicator_bitmap);
  bitmap_layer_set_compositing_mode(data->select_indicator_layer, GCompOpSet);
  data->content_indicator_layer = blayer_create(
    GRect(0, window_bounds.size.h - STATUS_BAR_LAYER_HEIGHT, window_bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
  scroll_layer_set_shadow_hidden(data->scroll_layer, true);
  ContentIndicator *indicator = scroll_layer_get_content_indicator(data->scroll_layer);
  const ContentIndicatorConfig content_indicator_config = (ContentIndicatorConfig) {
    .layer = data->content_indicator_layer,
    .alignment = GAlignCenter,
    .colors = {
      .background = GColorWhite,
      .foreground = GColorBlack,
    }
  };
  content_indicator_configure_direction(indicator, ContentIndicatorDirectionDown, &content_indicator_config);
  layer_add_child(root_layer, scroll_layer_get_layer(data->scroll_layer));
  layer_add_child(root_layer, (Layer *) data->select_indicator_layer);
  scroll_layer_add_child(data->scroll_layer, (Layer *) data->title_layer);
  scroll_layer_add_child(data->scroll_layer, (Layer *) data->text_layer);
  layer_add_child(root_layer, data->content_indicator_layer);
  scroll_layer_set_callbacks(data->scroll_layer, (ScrollLayerCallbacks) {
    .click_config_provider = prv_click_config_provider,
  });
  scroll_layer_set_context(data->scroll_layer, window);
  data->app_message_handle = events_app_message_subscribe_handlers((EventAppMessageHandlers) {
    .received = prv_app_message_handler,
    .failed = prv_app_message_failed,
  }, window);

  // The intro, if any, is shown while the pages load in the background.
  ConsentState initial_state = data->state;
  prv_start_loading(window);
  prv_set_state(window, initial_state);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_window_unload(Window *window) {
  ConsentWindowData* data = window_get_user_data(window);
  if (data->load_timeout) {
    app_timer_cancel(data->load_timeout);
  }
  if (data->request_retry) {
    app_timer_cancel(data->request_retry);
  }
  if (data->app_message_handle) {
    events_app_message_unsubscribe(data->app_message_handle);
  }
  scroll_layer_destroy(data->scroll_layer);
  text_layer_destroy(data->title_layer);
  text_layer_destroy(data->text_layer);
  gbitmap_destroy(data->select_indicator_bitmap);
  bitmap_layer_destroy(data->select_indicator_layer);
  layer_destroy(data->content_indicator_layer);
  prv_free_pages(data);
  ConsentCompleteCallback callback = data->callback;
  void *callback_context = data->callback_context;
  bool accepted = data->accepted;
  free(data);
  window_destroy(window);
  if (callback) {
    callback(accepted, callback_context);
  }
}

static void prv_free_pages(ConsentWindowData *data) {
  if (data->pages == NULL) {
    return;
  }
  for (int i = 0; i < data->page_count; ++i) {
    free(data->pages[i].title);
    free(data->pages[i].text);
  }
  free(data->pages);
  data->pages = NULL;
  data->page_count = 0;
  data->pages_received = 0;
}

static void prv_start_loading(Window *window) {
  ConsentWindowData *data = window_get_user_data(window);
  prv_free_pages(data);
  if (data->load_timeout) {
    app_timer_cancel(data->load_timeout);
  }
  data->load_timeout = app_timer_register(LOAD_TIMEOUT_MS, prv_load_timed_out, window);
  if (data->request_retry) {
    app_timer_cancel(data->request_retry);
    data->request_retry = NULL;
  }
  prv_send_request(window);
}

static void prv_send_request(void *context) {
  Window *window = context;
  ConsentWindowData *data = window_get_user_data(window);
  data->request_retry = NULL;
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_CONSENT_REQUEST, 1);
    result = app_message_outbox_send();
  }
  if (result != APP_MSG_OK) {
    // Most likely the outbox is busy. Keep trying until we time out.
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Requesting consent pages failed: %d. Retrying.", result);
    prv_retry_request_soon(window);
  }
}

static void prv_retry_request_soon(Window *window) {
  ConsentWindowData *data = window_get_user_data(window);
  // Only retry while we're still waiting for pages.
  if (data->load_timeout == NULL || data->request_retry != NULL) {
    return;
  }
  data->request_retry = app_timer_register(REQUEST_RETRY_MS, prv_send_request, window);
}

static void prv_app_message_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  if (iter == NULL || dict_find(iter, MESSAGE_KEY_CONSENT_REQUEST) == NULL) {
    return;
  }
  // This usually means the phone's JavaScript wasn't running yet.
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Consent page request wasn't delivered: %d. Retrying.", reason);
  prv_retry_request_soon(context);
}

static void prv_load_timed_out(void *context) {
  Window *window = context;
  ConsentWindowData *data = window_get_user_data(window);
  data->load_timeout = NULL;
  if (data->request_retry) {
    app_timer_cancel(data->request_retry);
    data->request_retry = NULL;
  }
  if (data->page_count > 0 && data->pages_received == data->page_count) {
    return;
  }
  BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Timed out waiting for consent pages.");
  prv_free_pages(data);
  if (data->state == ConsentStateLoading) {
    prv_set_state(window, ConsentStateError);
  }
}

static bool prv_pages_ready(ConsentWindowData *data) {
  return data->page_count > 0 && data->pages_received == data->page_count;
}

static void prv_set_state(Window *window, ConsentState state) {
  ConsentWindowData *data = window_get_user_data(window);
  data->state = state;
  switch (state) {
    case ConsentStateIntro:
      prv_show_text(window, "Hold up!", "Bobby's privacy terms have changed since you last agreed to them. Please review them to continue.");
      break;
    case ConsentStateLoading:
      if (prv_pages_ready(data)) {
        prv_show_page(window, 0);
      } else {
        prv_show_text(window, "Bobby", "Loading...");
      }
      break;
    case ConsentStateError:
      prv_show_text(window, "Bobby", "Couldn't load Bobby's terms from your phone. Make sure your phone is connected and the Pebble app is open, then press select to try again.");
      break;
    case ConsentStatePage:
      prv_show_page(window, data->page);
      break;
  }
}

static void prv_show_page(Window *window, int page) {
  ConsentWindowData *data = window_get_user_data(window);
  data->state = ConsentStatePage;
  data->page = page;
  prv_show_text(window, data->pages[page].title, data->pages[page].text);
}

static void prv_show_text(Window *window, const char *title, const char *text) {
  ConsentWindowData *data = window_get_user_data(window);
  GSize window_size = layer_get_frame(window_get_root_layer(window)).size;
  text_layer_set_text(data->title_layer, title);
  text_layer_set_size(data->title_layer, GSize(window_size.w, 100));
  GSize title_size = text_layer_get_content_size(data->title_layer);
  title_size.h += 10;
  text_layer_set_size(data->title_layer, GSize(window_size.w, title_size.h));
  text_layer_set_text(data->text_layer, text);
  text_layer_set_size(data->text_layer, GSize(window_size.w - 20, 1000));
  GSize text_size = text_layer_get_content_size(data->text_layer);
  text_size.h += 10;
  layer_set_frame(text_layer_get_layer(data->text_layer), GRect(10, title_size.h, window_size.w - 20, text_size.h));
  scroll_layer_set_content_size(data->scroll_layer, GSize(window_size.w, title_size.h + text_size.h));
  scroll_layer_set_content_offset(data->scroll_layer, GPoint(0, 0), false);
}

static bool prv_did_scroll_to_bottom(Window* window) {
  ConsentWindowData *data = window_get_user_data(window);
  ScrollLayer *scroll_layer = data->scroll_layer;
  int16_t offset = -scroll_layer_get_content_offset(data->scroll_layer).y;
  return (offset + layer_get_frame((Layer *)scroll_layer).size.h >= scroll_layer_get_content_size(scroll_layer).h - 10);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  Window* window = context;
  ConsentWindowData *data = window_get_user_data(window);
  if (!prv_did_scroll_to_bottom(window)) {
    BOBBY_LOG(APP_LOG_LEVEL_DEBUG, "User clicked select but hasn't scrolled to bottom; ignoring.");
    return;
  }
  switch (data->state) {
    case ConsentStateIntro:
      prv_set_state(window, ConsentStateLoading);
      break;
    case ConsentStateLoading:
      break;
    case ConsentStateError:
      prv_start_loading(window);
      prv_set_state(window, ConsentStateLoading);
      break;
    case ConsentStatePage:
      if (data->page < data->page_count - 1) {
        prv_show_page(window, data->page + 1);
      } else {
        // The final page is always the location page.
        prv_present_consent_menu(window);
      }
      break;
  }
}

static void prv_present_consent_menu(Window* window) {
  ConsentWindowData* data = window_get_user_data(window);
  ActionMenuLevel* root_level = baction_menu_level_create(2);
  action_menu_level_add_action(root_level, "Allow", prv_consent_menu_select_callback, (void *)true);
  action_menu_level_add_action(root_level, "Deny", prv_consent_menu_select_callback, (void *)false);
  ActionMenuConfig config = (ActionMenuConfig) {
    .root_level = root_level,
    .colors = {
      .background = ACCENT_COLOUR,
      .foreground = gcolor_legible_over(ACCENT_COLOUR),
    },
    .align = ActionMenuAlignCenter,
    .context = window,
    .did_close = prv_action_menu_close,
  };
  data->action_menu = action_menu_open(&config);
}

static void prv_consent_menu_select_callback(ActionMenu *action_menu, const ActionMenuItem *action, void *context) {
  Window *window = context;
  ConsentWindowData *data = window_get_user_data(window);
  data->awaiting_location_reply = true;
  bool choice = (int)action_menu_item_get_action_data(action);
  action_menu_freeze(action_menu);
  // We need to inform the phone of the user's choice.
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_int16(iter, MESSAGE_KEY_LOCATION_ENABLED, choice);
  app_message_outbox_send();
}

static void prv_app_message_handler(DictionaryIterator *iter, void *context) {
  Window* window = context;
  ConsentWindowData *data = window_get_user_data(window);
  if (dict_find(iter, MESSAGE_KEY_CONSENT_REQUIRED_VERSION) && data->pages_received == 0) {
    // The phone just started up, so it may have missed our request. Ask again.
    prv_retry_request_soon(window);
  }
  if (dict_find(iter, MESSAGE_KEY_CONSENT_PAGE_INDEX) || dict_find(iter, MESSAGE_KEY_CONSENT_ERROR)) {
    prv_handle_page(window, iter);
    return;
  }
  Tuple *tuple = dict_find(iter, MESSAGE_KEY_LOCATION_ENABLED);
  if (tuple != NULL) {
    prv_handle_location_reply(window, tuple);
  }
}

static char *prv_copy_string(const char *str) {
  char *copy = bmalloc(strlen(str) + 1);
  strcpy(copy, str);
  return copy;
}

static void prv_handle_page(Window *window, DictionaryIterator *iter) {
  ConsentWindowData *data = window_get_user_data(window);
  if (data->state == ConsentStatePage || data->awaiting_location_reply) {
    // We already have everything we need; this is probably a late duplicate.
    return;
  }
  if (dict_find(iter, MESSAGE_KEY_CONSENT_ERROR)) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Phone couldn't provide consent pages.");
    if (data->load_timeout) {
      app_timer_cancel(data->load_timeout);
      data->load_timeout = NULL;
    }
    prv_free_pages(data);
    if (data->state == ConsentStateLoading) {
      prv_set_state(window, ConsentStateError);
    }
    return;
  }
  Tuple *version = dict_find(iter, MESSAGE_KEY_CONSENT_VERSION);
  Tuple *count = dict_find(iter, MESSAGE_KEY_CONSENT_PAGE_COUNT);
  Tuple *index = dict_find(iter, MESSAGE_KEY_CONSENT_PAGE_INDEX);
  Tuple *title = dict_find(iter, MESSAGE_KEY_CONSENT_PAGE_TITLE);
  Tuple *text = dict_find(iter, MESSAGE_KEY_CONSENT_PAGE_TEXT);
  if (!version || !count || !index || !title || !text) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Ignoring incomplete consent page.");
    return;
  }
  int page_count = count->value->int32;
  int page_index = index->value->int32;
  if (page_count < 1 || page_count > MAX_CONSENT_PAGES || page_index < 0 || page_index >= page_count) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Ignoring consent page %d of %d.", page_index, page_count);
    return;
  }
  if (data->pages && (data->consent_version != version->value->int32 || data->page_count != page_count)) {
    // A retry produced a different set of pages; start over with the new ones.
    prv_free_pages(data);
  }
  if (data->pages == NULL) {
    data->pages = bmalloc(sizeof(ConsentPage) * page_count);
    memset(data->pages, 0, sizeof(ConsentPage) * page_count);
    data->page_count = page_count;
    data->pages_received = 0;
    data->consent_version = version->value->int32;
  }
  ConsentPage *page = &data->pages[page_index];
  if (page->text != NULL) {
    return;
  }
  page->title = prv_copy_string(title->value->cstring);
  page->text = prv_copy_string(text->value->cstring);
  data->pages_received++;
  if (!prv_pages_ready(data)) {
    return;
  }
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "Received all %d pages of consent version %d.", page_count, data->consent_version);
  if (data->load_timeout) {
    app_timer_cancel(data->load_timeout);
    data->load_timeout = NULL;
  }
  if (data->state == ConsentStateLoading || data->state == ConsentStateError) {
    prv_show_page(window, 0);
  }
}

static void prv_handle_location_reply(Window *window, Tuple *tuple) {
  ConsentWindowData* data = window_get_user_data(window);
  if (!data->awaiting_location_reply) {
    BOBBY_LOG(APP_LOG_LEVEL_WARNING, "Ignoring unexpected location consent response.");
    return;
  }
  data->awaiting_location_reply = false;
  BOBBY_LOG(APP_LOG_LEVEL_INFO, "Got location enabled reply, dismissing dialog.");
  events_app_message_unsubscribe(data->app_message_handle);
  data->app_message_handle = NULL;
  bool location_enabled = tuple->value->int16;
  persist_write_bool(PERSIST_KEY_LOCATION_ENABLED, location_enabled);
  persist_write_int(PERSIST_KEY_CONSENTS_COMPLETED, data->consent_version);
  data->accepted = true;
  if (data->show_root_after) {
    RootWindow *root_window = root_window_create();
    action_menu_set_result_window(data->action_menu, root_window_get_window(root_window));
  }
  action_menu_close(data->action_menu, true);
  window_stack_remove(window, false);
}

static void prv_action_menu_close(ActionMenu* action_menu, const ActionMenuItem* item, void* context) {
  action_menu_hierarchy_destroy(action_menu_get_root_level(action_menu), NULL, NULL);
}
