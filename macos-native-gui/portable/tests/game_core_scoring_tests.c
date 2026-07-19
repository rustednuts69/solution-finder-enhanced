#include "game_core.h"

#include <stdio.h>

static int expect_score(
    const char *name,
    int expected,
    int lines,
    int t_spin,
    int mini,
    int back_to_back,
    int combo,
    int perfect_clear,
    int level
) {
    int actual = sft_game_score_action(
        lines, t_spin, mini, back_to_back, combo, perfect_clear, level);
    if (actual != expected) {
        fprintf(stderr, "%s scored %d; expected %d\n", name, actual, expected);
        return 0;
    }
    return 1;
}

int main(void) {
    if (!expect_score("single", 100, 1, 0, 0, 0, 0, 0, 1)
        || !expect_score("level 3 double", 900, 2, 0, 0, 0, 0, 0, 3)
        || !expect_score("back-to-back Tetris", 1200, 4, 0, 0, 1, 0, 0, 1)
        || !expect_score("T-spin double", 1200, 2, 1, 0, 0, 0, 0, 1)
        || !expect_score("mini T-spin double", 400, 2, 1, 1, 0, 0, 0, 1)
        || !expect_score("second-clear combo", 150, 1, 0, 0, 0, 1, 0, 1)
        || !expect_score("single perfect clear", 900, 1, 0, 0, 0, 0, 1, 1)
        || !expect_score("back-to-back Tetris perfect clear", 4400, 4, 0, 0, 1, 0, 1, 1)) {
        return 1;
    }

    SFTGameState state;
    sft_game_init_seeded(&state, 12345u);
    state.gravity_enabled = 0;
    long long before = state.score;
    if (!sft_game_command(&state, SFT_CMD_SOFT_DROP)
        || state.score != before + 1) {
        fprintf(stderr, "Soft drop did not award one point per cell\n");
        return 1;
    }

    SFTGameState distance_state = state;
    int hard_drop_distance = sft_game_drop_to_surface(&distance_state);
    before = state.score;
    sft_game_command(&state, SFT_CMD_HARD_DROP);
    if (hard_drop_distance <= 0 || state.score != before + hard_drop_distance * 2) {
        fprintf(stderr, "Hard drop did not award distance points\n");
        return 1;
    }

    return 0;
}
