#include "render.h"
#include "config.h"
#include "ssd1306.h"

void render_frame(const game_state_t *s)
{
    oled_clear();

    /* lưới giữa sân */
    for (int x = 0; x < OLED_W; x += 8)
        oled_fill_rect(x, OLED_H / 2, 4, 1);

    /* khi kết thúc: paddle của người thắng nhấp nháy */
    int blink_off = (s->phase == PHASE_OVER) && ((s->timer / 16) & 1);
    int p1_won = s->score1 >= WIN_SCORE;
    int p2_won = s->score2 >= WIN_SCORE;

    if (!(blink_off && p1_won))
        oled_fill_rect(s->p1_x, 0, PADDLE_W, PADDLE_H);
    if (!(blink_off && p2_won))
        oled_fill_rect(s->p2_x, OLED_H - PADDLE_H, PADDLE_W, PADDLE_H);

    oled_fill_rect(s->ball_x, s->ball_y, BALL_SIZE, BALL_SIZE);

    /* điểm: P1 nửa trên, P2 nửa dưới, canh trái */
    oled_draw_digit(4, 16, s->score1 % 10, 2);
    oled_draw_digit(4, OLED_H / 2 + 6, s->score2 % 10, 2);
}
