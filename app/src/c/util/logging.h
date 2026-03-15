//
// Created by Katharine Berry on 4/9/25.
//

#pragma once

#include <pebble.h>

#include "debug_state.h"

#ifdef CLAWD_DEBUG_LEVEL
#define CLAWD_LOG(level, ...) do {if (level <= CLAWD_DEBUG_LEVEL) APP_LOG(level, __VA_ARGS__);} while (0)
#else
#define CLAWD_LOG(level, ...) do {} while (0)
#endif
