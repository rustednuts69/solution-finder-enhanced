#include "game_core.h"

#include <stdio.h>

static int settle_piece(SFTGameState *state) {
    sft_game_set_options(state, 0, 0, 0);
    return sft_game_drop_to_surface(state) > 0 && state->grounded;
}

int main(void) {
    SFTGameState move_state;
    sft_game_init_seeded(&move_state, 12345u);
    sft_game_set_lock_delay(&move_state, 1000);
    sft_game_set_lock_reset(&move_state, SFT_LOCK_RESET_MOVE, 2);
    if (!settle_piece(&move_state)) {
        fprintf(stderr, "Could not settle move-reset test piece\n");
        return 1;
    }
    sft_game_tick(&move_state, 200, 0);
    if (!sft_game_command(&move_state, SFT_CMD_LEFT)
        || move_state.lock_elapsed_ms != 0
        || move_state.lock_resets_used != 1) {
        fprintf(stderr, "First move reset was not applied\n");
        return 1;
    }
    sft_game_tick(&move_state, 200, 0);
    if (!sft_game_command(&move_state, SFT_CMD_RIGHT)
        || move_state.lock_elapsed_ms != 0
        || move_state.lock_resets_used != 2) {
        fprintf(stderr, "Second move reset was not applied\n");
        return 1;
    }
    sft_game_tick(&move_state, 200, 0);
    if (!sft_game_command(&move_state, SFT_CMD_LEFT)
        || move_state.lock_elapsed_ms != 200
        || move_state.lock_resets_used != 2) {
        fprintf(stderr, "Move-reset limit was not enforced\n");
        return 1;
    }

    SFTGameState step_state;
    sft_game_init_seeded(&step_state, 12345u);
    sft_game_set_lock_delay(&step_state, 1000);
    sft_game_set_lock_reset(&step_state, SFT_LOCK_RESET_STEP, 15);
    if (!settle_piece(&step_state)) {
        fprintf(stderr, "Could not settle step-reset test piece\n");
        return 1;
    }
    sft_game_tick(&step_state, 200, 0);
    if (!sft_game_command(&step_state, SFT_CMD_LEFT)
        || step_state.lock_elapsed_ms != 200) {
        fprintf(stderr, "Step reset incorrectly reset after a horizontal move\n");
        return 1;
    }
    step_state.y += 2;
    step_state.lock_elapsed_ms = 200;
    if (!sft_game_command(&step_state, SFT_CMD_SOFT_DROP)
        || step_state.lock_elapsed_ms != 0) {
        fprintf(stderr, "Step reset did not reset after descending\n");
        return 1;
    }

    SFTGameState none_state;
    sft_game_init_seeded(&none_state, 12345u);
    sft_game_set_lock_delay(&none_state, 1000);
    sft_game_set_lock_reset(&none_state, SFT_LOCK_RESET_NONE, 15);
    if (!settle_piece(&none_state)) {
        fprintf(stderr, "Could not settle no-reset test piece\n");
        return 1;
    }
    sft_game_tick(&none_state, 200, 0);
    if (!sft_game_command(&none_state, SFT_CMD_LEFT)
        || none_state.lock_elapsed_ms != 200) {
        fprintf(stderr, "Disabled reset changed the lock timer\n");
        return 1;
    }

    static const int expected_delays[11] = {
        450, 400, 350, 300, 250, 200, 195, 184, 167, 151, 150
    };
    for (int level = 20; level <= 30; ++level) {
        int actual = sft_game_lock_delay_for_level(level);
        if (actual != expected_delays[level - 20]) {
            fprintf(stderr, "Level %d lock delay was %d; expected %d\n",
                    level, actual, expected_delays[level - 20]);
            return 1;
        }
    }

    return 0;
}
