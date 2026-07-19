#include "game_core.h"

#include <stdio.h>

int main(void) {
    static const int expected_rows_in_ten_seconds[20] = {
        10, 12, 16, 21, 28, 38, 52, 74, 106, 155,
        232, 354, 550, 874, 1416, 2345, 3968, 6862, 12000, 12000
    };

    for (int level = 1; level <= 19; ++level) {
        SFTGameState state;
        sft_game_init_seeded(&state, 12345u);
        sft_game_set_gravity_level(&state, level);
        state.y = 50000;
        const int start_y = state.y;

        sft_game_tick(&state, 10000, 0);

        const int moved = start_y - state.y;
        if (moved != expected_rows_in_ten_seconds[level - 1]) {
            fprintf(stderr, "Level %d moved %d rows; expected %d\n",
                    level, moved, expected_rows_in_ten_seconds[level - 1]);
            return 1;
        }
    }

    SFTGameState instant_state;
    SFTGameState soft_drop_state;
    sft_game_init_seeded(&instant_state, 12345u);
    sft_game_init_seeded(&soft_drop_state, 12345u);
    sft_game_set_options(&soft_drop_state, 0, 0, 0);
    sft_game_set_gravity_level(&instant_state, 20);
    while (sft_game_command(&soft_drop_state, SFT_CMD_SOFT_DROP)) {}

    if (!instant_state.grounded || instant_state.pieces_locked != 0) {
        fprintf(stderr, "Level 20 did not settle without locking\n");
        return 1;
    }
    if (instant_state.y != soft_drop_state.y) {
        fprintf(stderr, "Level 20 settled at y=%d; 0 ms soft drop settled at y=%d\n",
                instant_state.y, soft_drop_state.y);
        return 1;
    }

    const int settled_y = instant_state.y;
    if (!sft_game_command(&instant_state, SFT_CMD_LEFT)) {
        fprintf(stderr, "Level 20 horizontal movement failed\n");
        return 1;
    }
    if (!instant_state.grounded || instant_state.y > settled_y) {
        fprintf(stderr, "Level 20 did not immediately settle after horizontal movement\n");
        return 1;
    }
    if (!sft_game_command(&instant_state, SFT_CMD_ROTATE_CW) || !instant_state.grounded) {
        fprintf(stderr, "Level 20 did not immediately settle after rotation\n");
        return 1;
    }
    if (!sft_game_command(&instant_state, SFT_CMD_HOLD) || !instant_state.grounded) {
        fprintf(stderr, "Level 20 did not immediately settle after hold\n");
        return 1;
    }
    if (!sft_game_command(&instant_state, SFT_CMD_HARD_DROP)
        || instant_state.pieces_locked != 1
        || !instant_state.grounded) {
        fprintf(stderr, "Level 20 did not immediately settle the next spawned piece\n");
        return 1;
    }

    SFTGameState state;
    sft_game_init_seeded(&state, 12345u);
    sft_game_set_gravity_level(&state, 0);
    if (state.gravity_level != 1) {
        fprintf(stderr, "Gravity level did not clamp to 1\n");
        return 1;
    }
    sft_game_set_gravity_level(&state, 31);
    if (state.gravity_level != 30) {
        fprintf(stderr, "Gravity level did not clamp to 30\n");
        return 1;
    }
    sft_game_set_gravity_level(&state, 21);
    if (state.gravity_level != 21 || !state.grounded) {
        fprintf(stderr, "Level 21 did not use instant gravity\n");
        return 1;
    }

    SFTGameState progression_state;
    sft_game_init_seeded(&progression_state, 12345u);
    sft_game_set_options(&progression_state, 0, 0, 0);
    sft_game_set_gravity_level(&progression_state, 19);
    sft_game_set_level_progression(&progression_state, 1);
    progression_state.lines_cleared = 9;
    for (int x = 0; x < SFT_GAME_WIDTH; ++x) {
        progression_state.board[x] = (x >= 3 && x <= 6) ? 0 : SFT_PIECE_J;
    }
    progression_state.current = SFT_PIECE_I;
    progression_state.rotation = 0;
    progression_state.x = 3;
    progression_state.y = 17;
    if (!sft_game_command(&progression_state, SFT_CMD_HARD_DROP)
        || progression_state.lines_cleared != 10
        || progression_state.gravity_level != 20
        || progression_state.lock_delay_ms != 450) {
        fprintf(stderr, "Level progression did not advance from 19 to 20 after 10 lines\n");
        return 1;
    }

    SFTGameState level_twenty_state;
    sft_game_init_seeded(&level_twenty_state, 12345u);
    sft_game_set_options(&level_twenty_state, 0, 0, 0);
    sft_game_set_gravity_level(&level_twenty_state, 20);
    sft_game_set_level_progression(&level_twenty_state, 1);
    level_twenty_state.lines_cleared = 9;
    for (int x = 0; x < SFT_GAME_WIDTH; ++x) {
        level_twenty_state.board[x] = (x >= 3 && x <= 6) ? 0 : SFT_PIECE_J;
    }
    level_twenty_state.current = SFT_PIECE_I;
    level_twenty_state.rotation = 0;
    level_twenty_state.x = 3;
    level_twenty_state.y = 17;
    if (!sft_game_command(&level_twenty_state, SFT_CMD_HARD_DROP)
        || level_twenty_state.lines_cleared != 10
        || level_twenty_state.gravity_level != 21
        || level_twenty_state.lock_delay_ms != 400) {
        fprintf(stderr, "Level progression did not advance from 20 to 21 after 10 lines\n");
        return 1;
    }
    sft_game_reset_level_progression(&level_twenty_state, 20);
    if (level_twenty_state.gravity_level != 20
        || level_twenty_state.gravity_base_level != 20
        || level_twenty_state.progression_start_lines != 10
        || level_twenty_state.lock_delay_ms != 450) {
        fprintf(stderr, "Manual level selection did not restart progression at level 20\n");
        return 1;
    }

    return 0;
}
