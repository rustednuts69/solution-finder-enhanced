#include "game_core.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    SFTGameState state;
    SFTPCScoutCandidate candidates[2];
    char field[SFT_GAME_FIELD_TEXT_CAPACITY];
    char pattern[SFT_GAME_PATTERN_TEXT_CAPACITY];

    sft_game_init_seeded(&state, 12345u);
    memset(state.board, 0, sizeof(state.board));
    state.current = SFT_PIECE_T;
    state.queue_index = 0;
    state.queue_count = 4;
    state.queue[0] = SFT_PIECE_I;
    state.queue[1] = SFT_PIECE_O;
    state.queue[2] = SFT_PIECE_Z;
    state.queue[3] = SFT_PIECE_J;
    if (sft_game_write_active_pattern(&state, 5, pattern, sizeof(pattern)) != 5
        || strcmp(pattern, "TIOZJ") != 0) {
        fprintf(stderr, "Unexpected active PC Scout pattern: %s\n", pattern);
        return 1;
    }
    state.hold = SFT_PIECE_L;
    state.can_hold = 1;
    if (sft_game_write_active_patterns(&state, 2, pattern, sizeof(pattern)) <= 0
        || strcmp(pattern, "TI;TL;LI;LT") != 0) {
        fprintf(stderr, "Occupied hold was not represented correctly: %s\n", pattern);
        return 1;
    }
    state.hold = SFT_PIECE_NONE;
    if (sft_game_write_active_patterns(&state, 2, pattern, sizeof(pattern)) <= 0
        || strcmp(pattern, "TI;TO;IO;IT") != 0) {
        fprintf(stderr, "Empty hold was not represented correctly: %s\n", pattern);
        return 1;
    }
    state.custom_queue_enabled = 0;
    state.queue_index = SFT_GAME_BAG_SIZE - 1;
    state.queue_count = SFT_GAME_BAG_SIZE;
    state.queue[SFT_GAME_BAG_SIZE - 1] = SFT_PIECE_S;
    state.queue[0] = SFT_PIECE_NONE;
    state.rng_state = 123u;
    if (sft_game_queue_piece(&state, 0) != SFT_PIECE_S
        || sft_game_queue_piece(&state, 1) <= SFT_PIECE_NONE) {
        fprintf(stderr, "Queue lookahead did not predict the next bag\n");
        return 1;
    }
    {
        SFTGameState predicted;
        SFTGameState advanced;
        sft_game_init_seeded(&predicted, 98765u);
        advanced = predicted;
        for (int index = 0; index < SFT_GAME_QUEUE_SIZE; ++index) {
            memset(advanced.board, 0, sizeof(advanced.board));
            if (!sft_game_command(&advanced, SFT_CMD_HARD_DROP)
                || sft_game_queue_piece(&predicted, index) != advanced.current) {
                fprintf(stderr, "Queue prediction diverged at lookahead %d\n", index);
                return 1;
            }
        }
    }

    int count = sft_game_pc_scout_candidates(&state, 7, candidates, 2);
    if (count != 1 || candidates[0].pieces != 5 || candidates[0].clear_lines != 2) {
        fprintf(stderr, "Empty field PC window was not 5 pieces / 2 lines\n");
        return 1;
    }

    state.board[0] = SFT_PIECE_J;
    state.board[9] = SFT_PIECE_J;
    state.board[SFT_GAME_WIDTH + 5] = SFT_PIECE_T;
    if (sft_game_write_sfinder_field(&state, 3, field, sizeof(field)) <= 0
        || strcmp(field, "3\n_____X____\nX________X\n") != 0) {
        fprintf(stderr, "Unexpected sfinder field text:\n%s", field);
        return 1;
    }

    memset(state.board, 0, sizeof(state.board));
    state.board[0] = SFT_PIECE_J;
    state.board[1] = SFT_PIECE_J;
    count = sft_game_pc_scout_candidates(&state, 7, candidates, 2);
    if (count != 2
        || candidates[0].pieces != 2
        || candidates[0].clear_lines != 1
        || candidates[1].pieces != 7
        || candidates[1].clear_lines != 3) {
        fprintf(stderr, "Two-window PC candidate calculation failed\n");
        return 1;
    }

    state.board[4 * SFT_GAME_WIDTH] = SFT_PIECE_T;
    count = sft_game_pc_scout_candidates(&state, 7, candidates, 2);
    if (count != 0) {
        fprintf(stderr, "PC Scout accepted a target below the occupied field height\n");
        return 1;
    }

    return 0;
}
