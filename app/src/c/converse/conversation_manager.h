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

#ifndef CONVERSATION_MANAGER_H
#define CONVERSATION_MANAGER_H

#include <pebble.h>
#include "conversation.h"

typedef struct ConversationManager ConversationManager;
typedef void (*ConversationManagerUpdateHandler)(bool entry_added, void* context);

void conversation_manager_init();
ConversationManager* conversation_manager_create();
ConversationManager* conversation_manager_get_current();
void conversation_manager_destroy(ConversationManager* manager);
void conversation_manager_set_handler(ConversationManager* manager, ConversationManagerUpdateHandler handler, void* context);
void conversation_manager_add_input(ConversationManager* manager, const char* input);
void conversation_manager_add_action(ConversationManager* manager, ConversationAction* action);
void conversation_manager_add_widget(ConversationManager* manager, ConversationWidget* widget);
Conversation* conversation_manager_get_conversation(ConversationManager* manager);

#endif
