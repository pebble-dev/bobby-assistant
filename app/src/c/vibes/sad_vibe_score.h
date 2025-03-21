//
// Created by Katharine Berry on 3/20/25.
//

#pragma once

#include <pebble.h>

typedef struct SadVibeScore SadVibeScore;

SadVibeScore* sad_vibe_score_create_with_resource(uint32_t resource_id);
void sad_vibe_score_destroy(SadVibeScore* score);
void sad_vibe_score_play(SadVibeScore* score);
void sad_vibe_score_stop();
