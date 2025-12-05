#pragma once

#include <stdint.h>

void touch_overlay_init(void);
void touch_overlay_update(void);
void touch_overlay_enable_2p(bool enable);
void touch_overlay_get_state(uint8_t player_index, uint8_t state[7]);
