#include "stdlib.h"
#include "game.h"
#include "config.h"
#include "esp_random.h"

static void serve(game_state_t *s, int dir_y)
{
    static const int8_t vxs[4] = { -2, -1, 1, 2 };
    s->ball_x = OLED_W / 2 - BALL_SIZE / 2;
    s->ball_y = OLED_H / 2 - BALL_SIZE / 2;
    s->vx = vxs[esp_random() % 4];
    s->vy = dir_y * BALL_SPEED_Y;
    s->phase = PHASE_SERVE;
    s->timer = SERVE_TICKS;
}

void game_init(game_state_t *s)
{
    s->p1_x = (OLED_W - PADDLE_W) / 2;
    s->p2_x = (OLED_W - PADDLE_W) / 2;
    s->score1 = s->score2 = 0;
    serve(s, +1);
}

static void move_paddle(int16_t *x, int8_t axis)
{
    if (axis == 0) {
        return;   // joystick ở giữa -> đứng yên
    }

    int speed = (abs(axis) * PADDLE_MAX_SPEED) / 100;

    if (speed < 1) {
        speed = 1;
    }

    if (axis > 0) {
        *x += speed;
    } else {
        *x -= speed;
    }

    if (*x < 0)
        *x = 0;

    if (*x > OLED_W - PADDLE_W)
        *x = OLED_W - PADDLE_W;
}

static int overlap_x(int ball_x, int pad_x)
{
    return ball_x + BALL_SIZE > pad_x && ball_x < pad_x + PADDLE_W;
}

/* Góc nảy phụ thuộc vị trí chạm trên paddle */
static int16_t deflect(int ball_x, int pad_x)
{
    int off = (ball_x + BALL_SIZE / 2) - (pad_x + PADDLE_W / 2);
    return off * BALL_MAX_VX / (PADDLE_W / 2);
}

static void point(game_state_t *s, int p1_scored)
{
    if (p1_scored) s->score1++; else s->score2++;

    if (s->score1 >= WIN_SCORE || s->score2 >= WIN_SCORE) {
        s->phase = PHASE_OVER;
        s->timer = OVER_TICKS;
        s->vx = s->vy = 0;
    } else {
        /* giao bóng về phía người vừa để mất điểm */
        serve(s, p1_scored ? +1 : -1);
    }
}

static void step_ball(game_state_t *s)
{
    s->ball_x += s->vx;
    s->ball_y += s->vy;

    if (s->ball_x < 0) { s->ball_x = 0; s->vx = -s->vx; }
    else if (s->ball_x > OLED_W - BALL_SIZE) { s->ball_x = OLED_W - BALL_SIZE; s->vx = -s->vx; }

    /* paddle trên (P1) */
    if (s->vy < 0 && s->ball_y >= 0 && s->ball_y <= PADDLE_H && overlap_x(s->ball_x, s->p1_x)) {
        s->ball_y = PADDLE_H;
        s->vy = -s->vy;
        s->vx = deflect(s->ball_x, s->p1_x);
    }
    /* paddle dưới (P2) */
    else if (s->vy > 0 && s->ball_y + BALL_SIZE >= OLED_H - PADDLE_H &&
             s->ball_y + BALL_SIZE <= OLED_H && overlap_x(s->ball_x, s->p2_x)) {
        s->ball_y = OLED_H - PADDLE_H - BALL_SIZE;
        s->vy = -s->vy;
        s->vx = deflect(s->ball_x, s->p2_x);
    }

    if (s->ball_y + BALL_SIZE <= 0)      point(s, 0);   /* bóng lọt phía P1 -> P2 ghi điểm */
    else if (s->ball_y >= OLED_H)        point(s, 1);   /* bóng lọt phía P2 -> P1 ghi điểm */
}

void game_update(game_state_t *s, const input_msg_t *in)
{
    move_paddle(&s->p1_x, in->p1);
    move_paddle(&s->p2_x, in->p2);

    switch (s->phase) {
    case PHASE_SERVE:
        if (s->timer > 0) s->timer--;
        if (s->timer == 0) s->phase = PHASE_PLAY;
        break;
    case PHASE_PLAY:
        step_ball(s);
        break;
    case PHASE_OVER:
        if (s->timer > 0) s->timer--;
        if (s->timer == 0) { s->score1 = s->score2 = 0; serve(s, +1); }
        break;
    }
}
