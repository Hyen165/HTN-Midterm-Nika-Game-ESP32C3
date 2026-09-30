#pragma once
#include <stdint.h>
#include "input.h"

typedef enum { PHASE_SERVE, PHASE_PLAY, PHASE_OVER } game_phase_t;

typedef struct {
    int16_t p1_x;               /* paddle trên  (người chơi 1) */
    int16_t p2_x;               /* paddle dưới  (người chơi 2) */
    int16_t ball_x, ball_y;     /* góc trên-trái của bóng */
    int16_t vx, vy;
    uint8_t score1, score2;
    game_phase_t phase;
    uint16_t timer;
} game_state_t;

void game_init(game_state_t *s);
void game_update(game_state_t *s, const input_msg_t *in);
